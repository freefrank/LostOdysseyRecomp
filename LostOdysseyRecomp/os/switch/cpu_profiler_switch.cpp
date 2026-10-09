#include <os/switch_cpu_profiler.h>

#include <switch.h>

#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// The executable's first instruction (libnx crt0), address 0 of the ELF: code offsets are relative to it.
// (__start__, used before, is an absolute linker symbol that is never relocated: it gave a base of 0, so
// every sample fell outside the module and was dropped.)
extern "C" char _start;

namespace
{
    constexpr int MAX_THREADS = 64;
    constexpr uint64_t SAMPLE_INTERVAL_NS = 2'000'000;   // 500 samples per second per thread
    constexpr uint64_t REPORT_INTERVAL_NS = 30'000'000'000ull;
    constexpr uint32_t TABLE_SIZE = 1u << 17;            // (thread, 16-byte code line) -> samples
    constexpr uint32_t REPORT_LINES_PER_THREAD = 150;
    constexpr uint32_t REPORT_MIN_SAMPLES = 50;         // threads that ran less are listed without lines

    // Samples taken while a thread sat in a system call (waits, sleeps, IPC): where it came from, as its
    // return address (LR) and the first return addresses found on its stack, so that a wait can be traced
    // to the code that waits (tools/switch-cpu-profile.py names them).
    constexpr uint32_t WAIT_TABLE_SIZE = 1u << 12;
    constexpr uint32_t WAIT_CALLERS = 4;                 // LR + 3 return addresses from the stack
    constexpr uint32_t WAIT_STACK_BYTES = 1024;          // scanned above SP, while the thread is paused
    constexpr uint32_t REPORT_WAIT_LINES_PER_THREAD = 12;

    struct ThreadSlot
    {
        Handle handle = 0;
        char name[48]{};
        bool active = false;
        uint64_t usageTicks = 0;    // CPU ticks at the previous AppendThreadCpuUsage
        uint64_t sampleTicks = 0;   // CPU ticks at the previous sample
        uint32_t samples = 0;       // samples taken while it had run since the previous one
        uint32_t idleSamples = 0;   // samples taken while it had not run
        uint32_t syscallSamples = 0;
    };

    struct Bucket
    {
        uint32_t key; // 0 = empty; otherwise ((slot << 26) | (offset >> 4)) + 1
        uint32_t count;
    };

    struct WaitBucket
    {
        uint32_t slot;
        uint32_t count;             // 0 = empty
        uint32_t callers[WAIT_CALLERS]; // module offsets (0 = none)
    };

    ThreadSlot g_slots[MAX_THREADS];
    Mutex g_slotsMutex;
    uint64_t g_usageWindowStart = 0;

    Bucket g_table[TABLE_SIZE];
    uint32_t g_tableDropped = 0;
    WaitBucket g_waitTable[WAIT_TABLE_SIZE];
    uint32_t g_waitDropped = 0;
    Thread g_samplerThread;
    bool g_samplerStarted = false;

    uint64_t ThreadTicks(Handle handle)
    {
        uint64_t ticks = 0;
        if (R_FAILED(svcGetInfo(&ticks, InfoType_ThreadTickCount, handle, UINT64_MAX)))
            return 0;
        return ticks;
    }

    uint64_t g_moduleBase = 0;
    uint64_t g_moduleSize = 0;

    void FindModule()
    {
        if (g_moduleBase != 0)
            return;

        g_moduleBase = reinterpret_cast<uint64_t>(&_start);

        // The size of the code mapping that holds it, for the report.
        MemoryInfo info{};
        u32 pageInfo = 0;
        if (R_SUCCEEDED(svcQueryMemory(&info, &pageInfo, g_moduleBase)))
            g_moduleSize = info.addr + info.size - g_moduleBase;
    }

    bool InModule(uint64_t address)
    {
        return address >= g_moduleBase && address - g_moduleBase < g_moduleSize;
    }

    uint32_t ReadCode(uint64_t address)
    {
        uint32_t instruction;
        memcpy(&instruction, reinterpret_cast<const void*>(address), sizeof(instruction));
        return instruction;
    }

    // A return address: inside the code and right after a BL or BLR.
    bool IsReturnAddress(uint64_t address)
    {
        if ((address & 3) != 0 || !InModule(address) || address - g_moduleBase < 4)
            return false;

        const uint32_t previous = ReadCode(address - 4);
        return (previous & 0xFC000000u) == 0x94000000u || (previous & 0xFFFFFC1Fu) == 0xD63F0000u;
    }

    // The thread is in a system call: Horizon reports a thread blocked in one with its PC at the SVC instruction
    // itself (round 9: the check only looked right after it, so no wait was ever attributed), and one stopped
    // on its way out right after it.
    bool InSystemCall(uint64_t pc)
    {
        if (!InModule(pc) || (pc & 3) != 0)
            return false;

        if ((ReadCode(pc) & 0xFFE0001Fu) == 0xD4000001u)
            return true;

        return pc - g_moduleBase >= 4 && (ReadCode(pc - 4) & 0xFFE0001Fu) == 0xD4000001u;
    }

    void AddSample(uint32_t slot, uint64_t pc)
    {
        const uint64_t base = g_moduleBase;
        if (pc < base || pc - base >= (uint64_t(1) << 30))
            return;

        const uint32_t key = ((slot << 26) | uint32_t((pc - base) >> 4)) + 1;
        uint32_t index = (key * 2654435761u) & (TABLE_SIZE - 1);
        for (uint32_t probe = 0; probe < 64; probe++)
        {
            Bucket& bucket = g_table[(index + probe) & (TABLE_SIZE - 1)];
            if (bucket.key == key)
            {
                bucket.count++;
                return;
            }
            if (bucket.key == 0)
            {
                bucket.key = key;
                bucket.count = 1;
                return;
            }
        }

        g_tableDropped++;
    }

    void AddWaitSample(uint32_t slot, const uint32_t (&callers)[WAIT_CALLERS])
    {
        uint32_t hash = slot * 2654435761u;
        for (uint32_t caller : callers)
            hash = (hash ^ caller) * 2654435761u;

        for (uint32_t probe = 0; probe < 32; probe++)
        {
            WaitBucket& bucket = g_waitTable[(hash + probe) & (WAIT_TABLE_SIZE - 1)];
            if (bucket.count != 0 && bucket.slot == slot && memcmp(bucket.callers, callers, sizeof(callers)) == 0)
            {
                bucket.count++;
                return;
            }
            if (bucket.count == 0)
            {
                bucket.slot = slot;
                bucket.count = 1;
                memcpy(bucket.callers, callers, sizeof(callers));
                return;
            }
        }

        g_waitDropped++;
    }

    // Report data copied out of the tables under the lock; formatted and written without it.
    struct ReportThread
    {
        uint32_t slot;
        char name[48];
        uint32_t samples;
        uint32_t idleSamples;
        uint32_t syscallSamples;
    };

    void FormatReport(std::string& report, uint64_t windowTicks, const std::vector<ReportThread>& threads,
        std::vector<Bucket>& buckets, std::vector<WaitBucket>& waits, uint32_t dropped, uint32_t waitDropped)
    {
        report.reserve(65536);

        char line[256];
        snprintf(line, sizeof(line), "[cpu profile] %.1f s, module base 0x%llx (code 0x%llx bytes), %u samples dropped; "
            "run tools/switch-cpu-profile.py on this log to name the addresses\n",
            double(windowTicks) / double(armGetSystemTickFreq()), (unsigned long long)g_moduleBase,
            (unsigned long long)g_moduleSize, dropped);
        report += line;

        std::sort(buckets.begin(), buckets.end(), [](const Bucket& a, const Bucket& b)
            {
                const uint32_t slotA = (a.key - 1) >> 26;
                const uint32_t slotB = (b.key - 1) >> 26;
                return slotA != slotB ? slotA < slotB : a.count > b.count;
            });

        std::sort(waits.begin(), waits.end(), [](const WaitBucket& a, const WaitBucket& b)
            {
                return a.slot != b.slot ? a.slot < b.slot : a.count > b.count;
            });

        for (const ReportThread& thread : threads)
        {
            snprintf(line, sizeof(line), "  thread %s: %u samples while running, %u while waiting\n",
                thread.name, thread.samples, thread.idleSamples);
            report += line;

            if (thread.samples >= REPORT_MIN_SAMPLES)
            {
                uint32_t shown = 0;
                for (const Bucket& bucket : buckets)
                {
                    if (((bucket.key - 1) >> 26) != thread.slot || shown++ == REPORT_LINES_PER_THREAD)
                        continue;

                    snprintf(line, sizeof(line), "    %5.1f%%  +0x%07x\n", 100.0 * bucket.count / thread.samples,
                        ((bucket.key - 1) & ((1u << 26) - 1)) << 4);
                    report += line;
                }
            }

            // Where its system calls (waits, sleeps, IPC) came from.
            if (thread.syscallSamples >= REPORT_MIN_SAMPLES / 5)
            {
                snprintf(line, sizeof(line), "    system calls in %u samples; from (return address, then stack):\n",
                    thread.syscallSamples);
                report += line;

                uint32_t shown = 0;
                for (const WaitBucket& wait : waits)
                {
                    if (wait.slot != thread.slot || shown++ == REPORT_WAIT_LINES_PER_THREAD)
                        continue;

                    int length = snprintf(line, sizeof(line), "      %5.1f%%  wait", 100.0 * wait.count / thread.syscallSamples);
                    for (uint32_t caller : wait.callers)
                    {
                        if (caller != 0 && length > 0 && size_t(length) < sizeof(line) - 16)
                            length += snprintf(line + length, sizeof(line) - length, " +0x%07x", caller);
                    }
                    report += line;
                    report += '\n';
                }
            }
        }

        if (waitDropped != 0)
        {
            snprintf(line, sizeof(line), "  %u system call samples dropped (table full)\n", waitDropped);
            report += line;
        }
    }

    void SamplerThread(void*)
    {
        const Handle self = threadGetCurHandle();
        uint64_t reportStart = armGetSystemTick();
        const uint64_t reportTicks = armNsToTicks(REPORT_INTERVAL_NS);

        std::vector<ReportThread> reportThreads;
        std::vector<Bucket> reportBuckets;
        std::vector<WaitBucket> reportWaits;
        static uint8_t stackCopy[WAIT_STACK_BYTES];

        while (true)
        {
            svcSleepThread(SAMPLE_INTERVAL_NS);

            mutexLock(&g_slotsMutex);
            for (uint32_t slot = 0; slot < MAX_THREADS; slot++)
            {
                ThreadSlot& thread = g_slots[slot];
                if (!thread.active || thread.handle == self)
                    continue;

                // A thread that has not run since the last sample is waiting: its position is where it
                // blocked, which says nothing about where CPU time goes.
                const uint64_t ticks = ThreadTicks(thread.handle);
                if (ticks == thread.sampleTicks)
                {
                    thread.idleSamples++;
                    continue;
                }
                thread.sampleTicks = ticks;

                // Nothing between pause and resume may allocate, lock or log: the paused thread may hold
                // any of those locks. Its stack is only read while it is paused (it cannot exit meanwhile).
                if (R_FAILED(svcSetThreadActivity(thread.handle, ThreadActivity_Paused)))
                    continue;

                ThreadContext context;
                const Result result = svcGetThreadContext3(&context, thread.handle);

                bool inSystemCall = false;
                uint32_t callers[WAIT_CALLERS]{};
                if (R_SUCCEEDED(result) && InSystemCall(context.pc.x))
                {
                    inSystemCall = true;
                    uint32_t count = 0;
                    if (IsReturnAddress(context.lr))
                        callers[count++] = uint32_t(context.lr - g_moduleBase);

                    // The stack above SP, if it is readable memory.
                    MemoryInfo info{};
                    u32 pageInfo = 0;
                    const uint64_t sp = context.sp;
                    if (count < WAIT_CALLERS && R_SUCCEEDED(svcQueryMemory(&info, &pageInfo, sp)) &&
                        info.type != MemType_Unmapped && (info.perm & Perm_R) != 0)
                    {
                        const uint64_t end = std::min<uint64_t>(info.addr + info.size, sp + WAIT_STACK_BYTES);
                        if (end > sp)
                        {
                            memcpy(stackCopy, reinterpret_cast<const void*>(sp), size_t(end - sp));
                            for (uint64_t offset = 0; offset + 8 <= end - sp && count < WAIT_CALLERS; offset += 8)
                            {
                                uint64_t value;
                                memcpy(&value, stackCopy + offset, sizeof(value));
                                if (IsReturnAddress(value) && uint32_t(value - g_moduleBase) != callers[count ? count - 1 : 0])
                                    callers[count++] = uint32_t(value - g_moduleBase);
                            }
                        }
                    }
                }

                svcSetThreadActivity(thread.handle, ThreadActivity_Runnable);

                if (R_SUCCEEDED(result))
                {
                    AddSample(slot, context.pc.x);
                    thread.samples++;
                    if (inSystemCall)
                    {
                        AddWaitSample(slot, callers);
                        thread.syscallSamples++;
                    }
                }
            }
            mutexUnlock(&g_slotsMutex);

            const uint64_t now = armGetSystemTick();
            if (now - reportStart >= reportTicks)
            {
                // Copy and reset under the lock; format and write without it.
                reportThreads.clear();
                reportBuckets.clear();
                reportWaits.clear();

                mutexLock(&g_slotsMutex);
                for (uint32_t slot = 0; slot < MAX_THREADS; slot++)
                {
                    ThreadSlot& thread = g_slots[slot];
                    if (thread.samples != 0)
                    {
                        ReportThread copy{};
                        copy.slot = slot;
                        memcpy(copy.name, thread.name, sizeof(copy.name));
                        copy.samples = thread.samples;
                        copy.idleSamples = thread.idleSamples;
                        copy.syscallSamples = thread.syscallSamples;
                        reportThreads.push_back(copy);
                    }

                    thread.samples = 0;
                    thread.idleSamples = 0;
                    thread.syscallSamples = 0;
                }

                for (Bucket& bucket : g_table)
                {
                    if (bucket.key != 0)
                        reportBuckets.push_back(bucket);
                }
                for (WaitBucket& wait : g_waitTable)
                {
                    if (wait.count != 0)
                        reportWaits.push_back(wait);
                }

                const uint32_t dropped = g_tableDropped;
                const uint32_t waitDropped = g_waitDropped;
                memset(g_table, 0, sizeof(g_table));
                memset(g_waitTable, 0, sizeof(g_waitTable));
                g_tableDropped = 0;
                g_waitDropped = 0;
                mutexUnlock(&g_slotsMutex);

                std::string report;
                FormatReport(report, now - reportStart, reportThreads, reportBuckets, reportWaits, dropped, waitDropped);
                os::switch_cpu_profiler::WriteLog(report);

                reportStart = armGetSystemTick();
            }
        }
    }

    int FindSlot(Handle handle)
    {
        for (int i = 0; i < MAX_THREADS; i++)
        {
            if (g_slots[i].active && g_slots[i].handle == handle)
                return i;
        }
        return -1;
    }

    struct MutexInitializer
    {
        MutexInitializer() { mutexInit(&g_slotsMutex); }
    } g_mutexInitializer;
}

void os::switch_cpu_profiler::RegisterCurrentThread(const char* name)
{
    const Handle handle = threadGetCurHandle();

    mutexLock(&g_slotsMutex);
    int slot = FindSlot(handle);
    if (slot < 0)
    {
        for (int i = 0; i < MAX_THREADS; i++)
        {
            if (!g_slots[i].active && g_slots[i].samples == 0)
            {
                slot = i;
                break;
            }
        }
    }

    if (slot >= 0)
    {
        ThreadSlot& thread = g_slots[slot];
        thread.handle = handle;
        snprintf(thread.name, sizeof(thread.name), "%s", name);
        thread.usageTicks = ThreadTicks(handle);
        thread.sampleTicks = thread.usageTicks;
        thread.active = true;
    }
    mutexUnlock(&g_slotsMutex);
}

void os::switch_cpu_profiler::RegisterCurrentThreadWithAddress(const char* prefix, uint32_t guestAddress)
{
    // Kept per thread: the slot copies it anyway.
    char name[48];
    snprintf(name, sizeof(name), "%s %08X", prefix, guestAddress);
    RegisterCurrentThread(name);
}

void os::switch_cpu_profiler::UnregisterCurrentThread()
{
    const Handle handle = threadGetCurHandle();

    mutexLock(&g_slotsMutex);
    const int slot = FindSlot(handle);
    if (slot >= 0)
        g_slots[slot].active = false; // Its samples, if any, are still reported once.
    mutexUnlock(&g_slotsMutex);
}

void os::switch_cpu_profiler::Start(bool sampler)
{
    g_usageWindowStart = armGetSystemTick();
    FindModule();

    if (!sampler || g_samplerStarted)
        return;

    // Above every thread it samples (the audio pump is 0x20), so a sample is never late behind them.
    if (R_SUCCEEDED(threadCreate(&g_samplerThread, SamplerThread, nullptr, nullptr, 0x10000, 0x1E, -2)))
    {
        svcSetThreadCoreMask(g_samplerThread.handle, -1, 0x7);
        if (R_SUCCEEDED(threadStart(&g_samplerThread)))
        {
            g_samplerStarted = true;
            fprintf(stderr, "CPU profiler: sampling registered threads every %llu us, report every %llu s\n",
                (unsigned long long)(SAMPLE_INTERVAL_NS / 1000), (unsigned long long)(REPORT_INTERVAL_NS / 1000000000ull));
        }
    }
}

void os::switch_cpu_profiler::AppendThreadCpuUsage(std::string& out)
{
    // Called from the game's main thread: never wait for the sampler.
    if (!mutexTryLock(&g_slotsMutex))
        return;

    const uint64_t now = armGetSystemTick();
    const uint64_t window = now - g_usageWindowStart;
    g_usageWindowStart = now;
    if (window == 0)
    {
        mutexUnlock(&g_slotsMutex);
        return;
    }

    char entry[96];
    for (ThreadSlot& thread : g_slots)
    {
        if (!thread.active)
            continue;

        const uint64_t ticks = ThreadTicks(thread.handle);
        if (ticks < thread.usageTicks)
        {
            // [Lost Odyssey] The thread exited without unregistering (a named
            // std::thread) and its handle no longer answers: drop it.
            thread.active = false;
            continue;
        }
        const uint64_t used = ticks - thread.usageTicks;
        thread.usageTicks = ticks;

        // Only threads that did something worth a look.
        const double percent = 100.0 * double(used) / double(window);
        if (percent < 0.5)
            continue;

        snprintf(entry, sizeof(entry), "%s%s %.1f%%", out.empty() || out.back() == ' ' ? "" : ", ", thread.name, percent);
        out += entry;
    }
    mutexUnlock(&g_slotsMutex);
}

size_t os::switch_cpu_profiler::CopyThreads(ThreadInfo* out, size_t capacity)
{
    size_t count = 0;
    mutexLock(&g_slotsMutex);
    for (const ThreadSlot& thread : g_slots)
    {
        if (!thread.active || count == capacity)
            continue;

        out[count].handle = thread.handle;
        memcpy(out[count].name, thread.name, sizeof(out[count].name));
        count++;
    }
    mutexUnlock(&g_slotsMutex);
    return count;
}

bool os::switch_cpu_profiler::TryGetCurrentThreadName(char* out, size_t size)
{
    if (size == 0 || !mutexTryLock(&g_slotsMutex))
        return false;

    const Handle handle = threadGetCurHandle();
    bool found = false;
    for (const ThreadSlot& thread : g_slots)
    {
        if (thread.active && thread.handle == handle)
        {
            snprintf(out, size, "%s", thread.name);
            found = true;
            break;
        }
    }

    mutexUnlock(&g_slotsMutex);
    return found;
}

uint64_t os::switch_cpu_profiler::ModuleBase()
{
    return g_moduleBase;
}

uint64_t os::switch_cpu_profiler::ModuleSize()
{
    return g_moduleSize;
}

void os::switch_cpu_profiler::WriteLog(const char* text, size_t size)
{
    // What the line buffer holds goes first; the lock keeps other threads' lines out of the middle.
    flockfile(stderr);
    fflush(stderr);
    const int descriptor = fileno(stderr);
    while (size > 0)
    {
        const ssize_t written = write(descriptor, text, size);
        if (written <= 0)
            break;
        text += written;
        size -= size_t(written);
    }
    funlockfile(stderr);
}
