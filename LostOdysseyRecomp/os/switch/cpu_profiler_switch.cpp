#include <os/platform.h>
#if LO_PLATFORM_SWITCH
#include <os/switch_cpu_profiler.h>
#include <os/switch_platform.h>

#include <switch.h>

#include <unistd.h>

#include <cstdarg>

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
extern "C" void __malloc_lock(struct _reent*);

namespace
{
    constexpr int MAX_THREADS = 256;   // the movie player starts threads all game long
    constexpr uint32_t SLOT_SHIFT = 23;   // 256 slots; code offsets >> 4 below 1 << 23 (128 MiB)
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

    // LO_CPU_PROFILE_SAMPLES: every sample kept (cpu-samples.bin). A record is 16 bytes plus 4 per guest
    // return address; the buffer holds a report window of the busy threads (about 10 MiB in a battle).
    constexpr uint32_t RAW_BUFFER_BYTES = 24u << 20;
    constexpr uint32_t RAW_GUEST_DEPTH = 32;
    constexpr uint32_t RAW_MAGIC = 0x53434F4C; // "LOCS"
    constexpr uint32_t RAW_VERSION = 1;

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
        bool raw = false;           // its samples go to cpu-samples.bin
        // Guest threads (SetCurrentThreadGuestStack): where the PPCContext pointer is, and the guest stack.
        void* const volatile* guestContext = nullptr;
        uint32_t stackLow = 0;
        uint32_t stackHigh = 0;
        // LO_THREAD_PLACEMENT: what the thread saw at NoteCore, and the counts at the previous placement report.
        uint32_t coreHits[4]{};
        uint32_t coreMoves = 0;
        int8_t lastCore = -1;
        uint32_t reportedHits[4]{};
        uint32_t reportedMoves = 0;
        uint64_t placementTicks = 0;
        bool coreRuleApplied = false;
    };

    // cpu-samples.bin: one chunk per report window, a RawChunk, then RawName per sampled thread, then the
    // records (RawRecord + depth guest return addresses, innermost first). Little-endian.
    struct RawChunk
    {
        uint32_t magic;
        uint32_t version;
        uint64_t moduleBase;
        uint64_t tickFrequency;
        uint64_t startTick;         // the sampler's start: record times are relative to it
        uint64_t endTick;
        uint32_t nameCount;
        uint32_t recordBytes;
        uint32_t dropped;           // records that did not fit
        uint32_t intervalUs;
    };
    struct RawName
    {
        uint32_t slot;
        char name[48];
    };
    struct RawRecord
    {
        uint32_t timeUs;            // since startTick (wraps after 71 minutes)
        uint8_t slot;
        uint8_t depth;
        uint8_t flags;              // 1: in a system call; 2: guest chain read
        uint8_t reserved;
        uint32_t pc;                // offsets into the executable; UINT32_MAX outside it
        uint32_t lr;
    };
    static_assert(sizeof(RawRecord) == 16);

    bool g_rawEnabled = false;
    char g_rawFilter[256]{};        // empty: every thread
    uint8_t g_rawBuffer[RAW_BUFFER_BYTES];
    uint32_t g_rawBytes = 0;
    uint32_t g_rawDropped = 0;
    uint64_t g_rawStartTick = 0;
    const uint8_t* g_guestBase = nullptr;
    uint32_t g_guestR1Offset = 0;
    uint32_t g_guestLrOffset = UINT32_MAX;

    bool RawWanted(const char* name)
    {
        if (!g_rawEnabled)
            return false;
        if (!g_rawFilter[0])
            return true;
        const size_t length = strlen(name);
        for (const char* item = g_rawFilter; *item;)
        {
            const char* end = strchr(item, ',');
            const size_t itemLength = end ? size_t(end - item) : strlen(item);
            if (itemLength == length && memcmp(item, name, length) == 0)
                return true;
            if (!end)
                break;
            item = end + 1;
        }
        return false;
    }

    struct Bucket
    {
        uint32_t key; // 0 = empty; otherwise ((slot << SLOT_SHIFT) | (offset >> 4)) + 1
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

    // Hang dump: when no frame has been presented for a while, a watchdog thread pauses every
    // registered thread and writes where each one is to hang.log. A freeze entering Uhra stopped
    // every file write of the process (the log writer, stderr, the crash handler and the first
    // version of this dump, which wrote from the sampler through newlib). So the watchdog is its
    // own thread, never allocates or takes a library lock, and writes through a file system
    // session of its own (os::switch_platform::RawAppend), outside libnx's shared ones.
    std::atomic<uint64_t> g_heartbeat{0};
    Thread g_watchdogThread;
    constexpr uint64_t HANG_DUMP_AFTER_NS[] = { 4'000'000'000ull, 15'000'000'000ull, 40'000'000'000ull };
    constexpr uint32_t HANG_FRAMES = 24;
    constexpr uint32_t HANG_SCAN_BYTES = 8192;
    constexpr uint32_t HANG_SCAN_ENTRIES = 32;
    char g_hangText[256 * 1024];
    size_t g_hangLength = 0;

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

    bool Readable(uint64_t address, uint64_t size)
    {
        MemoryInfo info{};
        u32 pageInfo = 0;
        return R_SUCCEEDED(svcQueryMemory(&info, &pageInfo, address)) && info.type != MemType_Unmapped &&
            (info.perm & Perm_R) != 0 && address + size <= info.addr + info.size;
    }

    __attribute__((format(printf, 1, 2))) void HangAppend(const char* format, ...)
    {
        if (g_hangLength + 1 >= sizeof(g_hangText))
            return;
        va_list args;
        va_start(args, format);
        const int length = vsnprintf(g_hangText + g_hangLength, sizeof(g_hangText) - g_hangLength, format, args);
        va_end(args);
        if (length > 0)
            g_hangLength = std::min(sizeof(g_hangText) - 1, g_hangLength + size_t(length));
    }

    void HangAddress(const char* label, uint64_t address)
    {
        if (InModule(address))
            HangAppend(" %s elf+0x%llx", label, (unsigned long long)(address - g_moduleBase));
        else
            HangAppend(" %s 0x%llx", label, (unsigned long long)address);
    }

    const char* SlotName(Handle handle)
    {
        for (const ThreadSlot& thread : g_slots)
        {
            if (thread.active && thread.handle == handle)
                return thread.name;
        }
        return "(unregistered)";
    }

    struct HangThread
    {
        Result pauseResult;
        Result contextResult;
        uint64_t pc, lr, sp, fp;
        uint64_t x[4];
        uint32_t frameCount;
        uint64_t frames[HANG_FRAMES];
        uint32_t scanCount;
        uint64_t scan[HANG_SCAN_ENTRIES];
    };

    void HangWrite(const char* text, size_t size)
    {
        os::switch_platform::RawAppend("hang.log", text, size);
    }

    // Where DumpThreadsBody writes after each thread, so a dump that stops partway still shows how far
    // it got (nullptr: kept in g_hangText).
    const char* g_dumpSink = nullptr;
    void FlushDump()
    {
        if (g_dumpSink && g_hangLength)
        {
            os::switch_platform::RawAppend(g_dumpSink, g_hangText, g_hangLength);
            g_hangLength = 0;
        }
    }

    void HangLine(const char* format, ...) __attribute__((format(printf, 1, 2)));
    void HangLine(const char* format, ...)
    {
        char line[256];
        va_list args;
        va_start(args, format);
        const int length = vsnprintf(line, sizeof(line), format, args);
        va_end(args);
        if (length > 0)
            HangWrite(line, std::min(size_t(length), sizeof(line) - 1));
    }

    // A libnx recursive lock: owner handle (bit 30: waiters), owner tag, depth.
    void HangLock(const char* name, const volatile u32* lock)
    {
        HangAppend("  %s lock 0x%x owner 0x%x depth %u", name, lock[0], lock[1], lock[2]);
        if (lock[0] != 0)
            HangAppend(" (%s)", SlotName(Handle(lock[0] & ~0x40000000u)));
        HangAppend("\n");
    }

    // newlib's malloc lock is static: find it in __malloc_lock's ADRP/ADD pair.
    const volatile u32* MallocLock()
    {
        const uint64_t function = reinterpret_cast<uint64_t>(&__malloc_lock);
        const uint32_t adrp = ReadCode(function);
        const uint32_t add = ReadCode(function + 4);
        if ((adrp & 0x9F000000u) != 0x90000000u || (add & 0xFFC00000u) != 0x91000000u)
            return nullptr;
        int64_t page = int64_t(((adrp >> 29) & 3) | (((adrp >> 5) & 0x7FFFF) << 2));
        if (page & (1ll << 20))
            page -= 1ll << 21;
        const uint64_t address = (function & ~0xFFFull) + uint64_t(page << 12) + ((add >> 10) & 0xFFF);
        return reinterpret_cast<const volatile u32*>(address);
    }

    void DumpThread(const char* name, Handle handle);

    // Reads the thread list without waiting: a stuck thread may hold its lock.
    void DumpThreadsBody(Handle self, bool listLocked)
    {
        HangAppend("locks:\n");
        if (const volatile u32* lock = MallocLock())
            HangLock("malloc", lock);
        HangAppend("  profiler thread list 0x%x (%s)%s\n", g_slotsMutex, SlotName(Handle(g_slotsMutex & ~0x40000000u)),
            listLocked ? "" : ", read without it");
        FlushDump();
        if (g_samplerStarted)
        {
            DumpThread("CPU profiler sampler", g_samplerThread.handle);
            FlushDump();
        }
        for (const ThreadSlot& slot : g_slots)
        {
            if (slot.active && slot.handle != self)
            {
                DumpThread(slot.name, slot.handle);
                FlushDump();
            }
        }
    }

    void DumpThread(const char* name, Handle handle)
    {
        {
            HangThread thread{};
            thread.pauseResult = svcSetThreadActivity(handle, ThreadActivity_Paused);
            if (R_SUCCEEDED(thread.pauseResult))
            {
                // Only copies while it is paused (see the sampler): no allocation, lock or log.
                ThreadContext context;
                thread.contextResult = svcGetThreadContext3(&context, handle);
                if (R_SUCCEEDED(thread.contextResult))
                {
                    thread.pc = context.pc.x;
                    thread.lr = context.lr;
                    thread.sp = context.sp;
                    thread.fp = context.fp;
                    for (int i = 0; i < 4; ++i)
                        thread.x[i] = context.cpu_gprs[i].x;
                    for (uint64_t frame = thread.fp; thread.frameCount < HANG_FRAMES && frame && (frame & 7) == 0 &&
                        Readable(frame, 16);)
                    {
                        uint64_t record[2];
                        memcpy(record, reinterpret_cast<const void*>(frame), sizeof(record));
                        thread.frames[thread.frameCount++] = record[1];
                        if (record[0] <= frame)
                            break;
                        frame = record[0];
                    }
                    MemoryInfo info{};
                    u32 pageInfo = 0;
                    if ((thread.sp & 7) == 0 && R_SUCCEEDED(svcQueryMemory(&info, &pageInfo, thread.sp)) &&
                        info.type != MemType_Unmapped && (info.perm & Perm_R) != 0)
                    {
                        const uint64_t end = std::min<uint64_t>(info.addr + info.size, thread.sp + HANG_SCAN_BYTES);
                        for (uint64_t address = thread.sp; address + 8 <= end && thread.scanCount < HANG_SCAN_ENTRIES; address += 8)
                        {
                            uint64_t value;
                            memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
                            if (IsReturnAddress(value))
                                thread.scan[thread.scanCount++] = value;
                        }
                    }
                }
                svcSetThreadActivity(handle, ThreadActivity_Runnable);
            }

            HangAppend("thread %s (handle 0x%x) cpu ticks %llu\n", name, handle,
                (unsigned long long)ThreadTicks(handle));
            if (R_FAILED(thread.pauseResult) || R_FAILED(thread.contextResult))
            {
                HangAppend("  no context: pause 0x%x context 0x%x\n", thread.pauseResult, thread.contextResult);
                return;
            }
            HangAppend(" ");
            HangAddress("pc", thread.pc);
            HangAddress("lr", thread.lr);
            HangAppend(" sp 0x%llx x0 0x%llx x1 0x%llx x2 0x%llx x3 0x%llx\n", (unsigned long long)thread.sp,
                (unsigned long long)thread.x[0], (unsigned long long)thread.x[1], (unsigned long long)thread.x[2],
                (unsigned long long)thread.x[3]);
            if (InSystemCall(thread.pc))
            {
                const uint32_t svc = (ReadCode(thread.pc) >> 5) & 0xFFFF;
                HangAppend("  in svc 0x%x", svc);
                const uint64_t mallocLock = reinterpret_cast<uint64_t>(MallocLock());
                if (svc == 0x1A) // ArbitrateLock(owner handle, mutex address, tag): waiting for a mutex
                    HangAppend(": waiting for mutex 0x%llx%s held by %s (handle 0x%x)", (unsigned long long)thread.x[1],
                        thread.x[1] == mallocLock ? " (malloc)" : "", SlotName(Handle(thread.x[0])), uint32_t(thread.x[0]));
                else if (svc == 0x1C) // WaitProcessWideKeyAtomic(mutex address, condition address, tag, timeout)
                    HangAppend(": condition variable wait, mutex 0x%llx, condition 0x%llx, timeout %lld ns",
                        (unsigned long long)thread.x[0], (unsigned long long)thread.x[1], (long long)thread.x[3]);
                else if (svc == 0x18) // WaitSynchronization(index out, handles, count, timeout)
                    HangAppend(": waiting on %llu handle(s), timeout %lld ns", (unsigned long long)thread.x[2], (long long)thread.x[3]);
                else if (svc == 0x34) // WaitForAddress(address, type, value, timeout)
                    HangAppend(": waiting for address 0x%llx, timeout %lld ns", (unsigned long long)thread.x[0], (long long)thread.x[3]);
                else if (svc == 0x21 || svc == 0x22 || svc == 0x23)
                    HangAppend(": IPC request");
                HangAppend("\n");
            }
            if (thread.frameCount)
            {
                HangAppend("  frames:");
                for (uint32_t i = 0; i < thread.frameCount; ++i)
                    HangAddress("", thread.frames[i]);
                HangAppend("\n");
            }
            if (thread.scanCount)
            {
                HangAppend("  stack:");
                for (uint32_t i = 0; i < thread.scanCount; ++i)
                    HangAddress("", thread.scan[i]);
                HangAppend("\n");
            }
        }
    }

    void WatchdogThread(void*)
    {
        const Handle self = threadGetCurHandle();
        HangLine("\n=== hang watchdog armed: dumps after %llu, %llu and %llu s without a frame ===\n",
            (unsigned long long)(HANG_DUMP_AFTER_NS[0] / 1000000000ull), (unsigned long long)(HANG_DUMP_AFTER_NS[1] / 1000000000ull),
            (unsigned long long)(HANG_DUMP_AFTER_NS[2] / 1000000000ull));
        uint64_t lastBeat = 0;
        uint64_t lastBeatTick = armGetSystemTick();
        uint64_t lastLoopTick = lastBeatTick;
        uint32_t dumps = 0;
        for (;;)
        {
            svcSleepThread(250'000'000);
            const uint64_t beat = g_heartbeat.load(std::memory_order_relaxed);
            const uint64_t tick = armGetSystemTick();
            // A 250 ms sleep that took over 2 s: the whole process was suspended (HOME menu, sleep mode), the frames
            // too. The wait for a frame starts again from now.
            const bool suspended = tick - lastLoopTick > armNsToTicks(2'000'000'000ull);
            lastLoopTick = tick;
            if (suspended)
            {
                lastBeatTick = tick;
                dumps = 0;
            }
            if (beat != lastBeat)
            {
                lastBeat = beat;
                lastBeatTick = tick;
                dumps = 0;
                continue;
            }
            if (beat == 0 || dumps >= std::size(HANG_DUMP_AFTER_NS) ||
                tick - lastBeatTick < armNsToTicks(HANG_DUMP_AFTER_NS[dumps]))
                continue;
            // Written first and on its own: if the dump never follows, the dump itself got stuck.
            HangLine("hang %u: no frame for %llu ms, dumping threads\n", dumps + 1,
                (unsigned long long)armTicksToNs(tick - lastBeatTick) / 1000000ull);
            bool locked = false;
            for (int attempt = 0; attempt < 50 && !locked; ++attempt)
            {
                locked = mutexTryLock(&g_slotsMutex);
                if (!locked)
                    svcSleepThread(1'000'000);
            }
            g_hangLength = 0;
            HangAppend("\n=== hang dump %u: no frame presented for %llu ms; offsets are into LostOdysseyRecomp.elf ===\n",
                ++dumps, (unsigned long long)armTicksToNs(tick - lastBeatTick) / 1000000ull);
            g_dumpSink = "hang.log";
            DumpThreadsBody(self, locked);
            g_dumpSink = nullptr;
            if (locked)
                mutexUnlock(&g_slotsMutex);
            HangWrite(g_hangText, g_hangLength);
        }
    }

    void AddSample(uint32_t slot, uint64_t pc)
    {
        const uint64_t base = g_moduleBase;
        if (pc < base || pc - base >= (uint64_t(1) << (SLOT_SHIFT + 4)))
            return;

        const uint32_t key = ((slot << SLOT_SHIFT) | uint32_t((pc - base) >> 4)) + 1;
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
                const uint32_t slotA = (a.key - 1) >> SLOT_SHIFT;
                const uint32_t slotB = (b.key - 1) >> SLOT_SHIFT;
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
                    if (((bucket.key - 1) >> SLOT_SHIFT) != thread.slot || shown++ == REPORT_LINES_PER_THREAD)
                        continue;

                    snprintf(line, sizeof(line), "    %5.1f%%  +0x%07x\n", 100.0 * bucket.count / thread.samples,
                        ((bucket.key - 1) & ((1u << SLOT_SHIFT) - 1)) << 4);
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

                // The raw record, still while it is paused: the guest stack must not change under the walk.
                if (thread.raw && R_SUCCEEDED(result))
                {
                    if (g_rawBytes + sizeof(RawRecord) + RAW_GUEST_DEPTH * 4 <= RAW_BUFFER_BYTES)
                    {
                        RawRecord* record = reinterpret_cast<RawRecord*>(g_rawBuffer + g_rawBytes);
                        record->timeUs = uint32_t(armTicksToNs(armGetSystemTick() - g_rawStartTick) / 1000);
                        record->slot = uint8_t(slot);
                        record->depth = 0;
                        record->flags = inSystemCall ? 1 : 0;
                        record->reserved = 0;
                        record->pc = InModule(context.pc.x) ? uint32_t(context.pc.x - g_moduleBase) : UINT32_MAX;
                        record->lr = InModule(context.lr) ? uint32_t(context.lr - g_moduleBase) : UINT32_MAX;
                        uint32_t* chain = reinterpret_cast<uint32_t*>(record + 1);
                        const uint8_t* ctx = thread.guestContext ? static_cast<const uint8_t*>(*thread.guestContext) : nullptr;
                        if (ctx && g_guestBase)
                        {
                            // The guest LR first (the caller of a function that has not called anything yet, else a
                            // return address inside it), then the back chain: each frame's word 0 is the caller's
                            // r1, and the prologue saved the caller's LR 8 bytes below that.
                            auto load = [](uint32_t address)
                            {
                                uint32_t value;
                                memcpy(&value, g_guestBase + address, 4);
                                return __builtin_bswap32(value);
                            };
                            uint32_t depth = 0;
                            if (g_guestLrOffset != UINT32_MAX)
                            {
                                uint64_t lr;
                                memcpy(&lr, ctx + g_guestLrOffset, 8);
                                chain[depth++] = uint32_t(lr);
                            }
                            uint32_t r1;
                            memcpy(&r1, ctx + g_guestR1Offset, 4);
                            for (uint32_t sp = r1; depth < RAW_GUEST_DEPTH;)
                            {
                                if ((sp & 3) != 0 || sp < thread.stackLow || sp + 4 > thread.stackHigh)
                                    break;
                                const uint32_t back = load(sp);
                                if (back <= sp || (back & 3) != 0 || back + 4 > thread.stackHigh)
                                    break;
                                chain[depth++] = load(back - 8);
                                sp = back;
                            }
                            record->depth = uint8_t(depth);
                            record->flags |= 2;
                        }
                        g_rawBytes += sizeof(RawRecord) + record->depth * 4;
                    }
                    else
                    {
                        g_rawDropped++;
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

                std::vector<RawName> rawNames;
                if (g_rawEnabled)
                {
                    for (uint32_t slot = 0; slot < MAX_THREADS; slot++)
                    {
                        if (g_slots[slot].raw)
                        {
                            RawName name{};
                            name.slot = slot;
                            memcpy(name.name, g_slots[slot].name, sizeof(name.name));
                            rawNames.push_back(name);
                        }
                    }
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

                if (g_rawEnabled)
                {
                    // Only this thread fills the buffer: nothing is sampled while it is written.
                    RawChunk chunk{};
                    chunk.magic = RAW_MAGIC;
                    chunk.version = RAW_VERSION;
                    chunk.moduleBase = g_moduleBase;
                    chunk.tickFrequency = armGetSystemTickFreq();
                    chunk.startTick = g_rawStartTick;
                    chunk.endTick = armGetSystemTick();
                    chunk.nameCount = uint32_t(rawNames.size());
                    chunk.recordBytes = g_rawBytes;
                    chunk.dropped = g_rawDropped;
                    chunk.intervalUs = uint32_t(SAMPLE_INTERVAL_NS / 1000);
                    std::string head(reinterpret_cast<const char*>(&chunk), sizeof(chunk));
                    head.append(reinterpret_cast<const char*>(rawNames.data()), rawNames.size() * sizeof(RawName));
                    const uint64_t writeStart = armGetSystemTick();
                    const bool written = os::switch_platform::RawAppend("cpu-samples.bin", head.data(), head.size()) &&
                        (g_rawBytes == 0 ||
                            os::switch_platform::RawAppend("cpu-samples.bin", reinterpret_cast<const char*>(g_rawBuffer), g_rawBytes));
                    char line[200];
                    snprintf(line, sizeof(line), "[cpu samples] %u bytes, %u dropped, %u threads -> cpu-samples.bin%s, %llu ms\n",
                        g_rawBytes, g_rawDropped, chunk.nameCount, written ? "" : " (write failed)",
                        (unsigned long long)(armTicksToNs(armGetSystemTick() - writeStart) / 1000000));
                    os::switch_cpu_profiler::WriteLog(line, strlen(line));
                    g_rawBytes = 0;
                    g_rawDropped = 0;
                }

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
        thread.raw = RawWanted(thread.name);
        thread.guestContext = nullptr;
        memset(thread.coreHits, 0, sizeof(thread.coreHits));
        memset(thread.reportedHits, 0, sizeof(thread.reportedHits));
        thread.coreMoves = thread.reportedMoves = 0;
        thread.lastCore = -1;
        thread.coreRuleApplied = false;
        thread.placementTicks = thread.usageTicks;
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
    {
        g_slots[slot].active = false; // Its samples, if any, are still reported once.
        g_slots[slot].guestContext = nullptr;
    }
    mutexUnlock(&g_slotsMutex);
}

void os::switch_cpu_profiler::SetCurrentThreadGuestStack(void* const* contextSlot, uint32_t r1Offset, uint32_t lrOffset,
    const uint8_t* memoryBase, uint32_t stackLow, uint32_t stackHigh)
{
    const Handle handle = threadGetCurHandle();
    mutexLock(&g_slotsMutex);
    g_guestBase = memoryBase;
    g_guestR1Offset = r1Offset;
    g_guestLrOffset = lrOffset;
    const int slot = FindSlot(handle);
    if (slot >= 0)
    {
        g_slots[slot].stackLow = stackLow;
        g_slots[slot].stackHigh = stackHigh;
        g_slots[slot].guestContext = contextSlot;
    }
    mutexUnlock(&g_slotsMutex);
}

void os::switch_cpu_profiler::ClearCurrentThreadGuestStack()
{
    const Handle handle = threadGetCurHandle();
    mutexLock(&g_slotsMutex);
    const int slot = FindSlot(handle);
    if (slot >= 0)
        g_slots[slot].guestContext = nullptr;
    mutexUnlock(&g_slotsMutex);
}

namespace
{
    // LO_THREAD_PLACEMENT. The kernel gives a core's idle time only to a thread running on that core, so
    // one small thread is pinned to each.
    constexpr uint64_t NOTE_CORE_INTERVAL_TICKS = 19'200'000 / 500;   // 2 ms

    bool g_placementEnabled = false;
    Thread g_coreIdleThreads[4];
    std::atomic<uint64_t> g_coreIdleTicks[4]{};
    std::atomic<uint64_t> g_coreIdleReadAt[4]{};
    uint64_t g_reportedIdleTicks[4]{};
    uint64_t g_reportedIdleReadAt[4]{};
    uint64_t g_placementWindowStart = 0;

    // LO_THREAD_CORES=name=core:mask[:percent],...: experiments with thread placement, needing LO_THREAD_PLACEMENT.
    // Applied at a report to a thread of that name above percent CPU (default 50, which picks the render thread among
    // its namesakes).
    constexpr double CORE_RULE_MIN_PERCENT = 50.0;

    void ApplyCoreRule(ThreadSlot& thread, double percent)
    {
        const char* rules = getenv("LO_THREAD_CORES");
        if (!rules || thread.coreRuleApplied)
            return;

        const size_t length = strlen(thread.name);
        for (const char* rule = rules; *rule;)
        {
            const char* end = strchr(rule, ',');
            int core = -1;
            unsigned mask = 0;
            int minPercent = int(CORE_RULE_MIN_PERCENT);
            if (strncmp(rule, thread.name, length) == 0 && rule[length] == '=' &&
                sscanf(rule + length + 1, "%d:%x:%d", &core, &mask, &minPercent) >= 2 && percent >= minPercent)
            {
                const Result rc = svcSetThreadCoreMask(thread.handle, core, mask);
                thread.coreRuleApplied = true;
                fprintf(stderr, "thread placement: %s -> core %d mask 0x%x (0x%x)\n", thread.name, core, mask, rc);
                return;
            }
            if (!end)
                break;
            rule = end + 1;
        }
    }

    void CoreIdleThread(void* argument)
    {
        const size_t core = reinterpret_cast<size_t>(argument);
        for (;;)
        {
            uint64_t ticks = 0;
            if (R_SUCCEEDED(svcGetInfo(&ticks, InfoType_IdleTickCount, INVALID_HANDLE, UINT64_MAX)))
            {
                g_coreIdleTicks[core].store(ticks, std::memory_order_relaxed);
                g_coreIdleReadAt[core].store(armGetSystemTick(), std::memory_order_relaxed);
            }
            svcSleepThread(1'000'000'000);
        }
    }

    void StartPlacementLog()
    {
        g_placementEnabled = true;
        g_placementWindowStart = armGetSystemTick();
        for (size_t core = 0; core < 4; core++)
        {
            // Above the audio pump, like the sampler: a read takes microseconds, once a second.
            const Result rc = threadCreate(&g_coreIdleThreads[core], CoreIdleThread, reinterpret_cast<void*>(core),
                nullptr, 0x2000, 0x1D, int(core));
            if (R_FAILED(rc) || R_FAILED(threadStart(&g_coreIdleThreads[core])))
                fprintf(stderr, "thread placement: no idle reader on core %zu (0x%x)\n", core, rc);
        }
    }
}

void os::switch_cpu_profiler::Start(bool sampler)
{
    g_usageWindowStart = armGetSystemTick();
    FindModule();

    {
        u64 coreMask = 0;
        svcGetInfo(&coreMask, InfoType_CoreMask, CUR_PROCESS_HANDLE, 0);
        fprintf(stderr, "thread placement: the process may use cores 0x%llx\n", (unsigned long long)coreMask);
    }
    if (const char* placement = getenv("LO_THREAD_PLACEMENT"); placement && placement[0] == '1')
        StartPlacementLog();

    if (!sampler || g_samplerStarted)
        return;

    if (const char* samples = getenv("LO_CPU_PROFILE_SAMPLES"); samples && *samples && strcmp(samples, "0") != 0)
    {
        mutexLock(&g_slotsMutex);
        g_rawEnabled = true;
        snprintf(g_rawFilter, sizeof(g_rawFilter), "%s", strcmp(samples, "1") == 0 ? "" : samples);
        g_rawStartTick = armGetSystemTick();
        for (ThreadSlot& thread : g_slots)
            thread.raw = thread.active && RawWanted(thread.name);
        mutexUnlock(&g_slotsMutex);
        fprintf(stderr, "CPU profiler: every sample of %s to cpu-samples.bin\n", g_rawFilter[0] ? g_rawFilter : "every thread");
    }

    // The hang watchdog: above the sampler and every game thread.
    if (R_SUCCEEDED(threadCreate(&g_watchdogThread, WatchdogThread, nullptr, nullptr, 0x10000, 0x1C, -2)))
    {
        svcSetThreadCoreMask(g_watchdogThread.handle, -1, 0x7);
        threadStart(&g_watchdogThread);
    }
    else
    {
        fprintf(stderr, "CPU profiler: no hang watchdog\n");
    }

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

void os::switch_cpu_profiler::NoteCore()
{
    if (!g_placementEnabled)
        return;

    // Per thread: the slot stays this thread's until it exits, so it needs looking up once.
    static thread_local int slot = -1;
    static thread_local uint64_t nextTick = 0;
    const uint64_t now = armGetSystemTick();
    if (now < nextTick)
        return;
    nextTick = now + NOTE_CORE_INTERVAL_TICKS;

    if (slot < 0)
    {
        if (!mutexTryLock(&g_slotsMutex))
            return;
        slot = FindSlot(threadGetCurHandle());
        mutexUnlock(&g_slotsMutex);
        if (slot < 0)
            return;
    }

    ThreadSlot& thread = g_slots[slot];
    const int core = int(svcGetCurrentProcessorNumber()) & 3;
    thread.coreHits[core]++;
    if (thread.lastCore >= 0 && thread.lastCore != core)
        thread.coreMoves++;
    thread.lastCore = int8_t(core);
}

void os::switch_cpu_profiler::AppendThreadPlacement(std::string& out)
{
    if (!g_placementEnabled || !mutexTryLock(&g_slotsMutex))
        return;

    const uint64_t now = armGetSystemTick();
    const uint64_t window = now - g_placementWindowStart;
    g_placementWindowStart = now;

    char entry[160];
    out += "idle";
    for (size_t core = 0; core < 4; core++)
    {
        const uint64_t idle = g_coreIdleTicks[core].load(std::memory_order_relaxed);
        const uint64_t readAt = g_coreIdleReadAt[core].load(std::memory_order_relaxed);
        if (readAt == 0)
            snprintf(entry, sizeof(entry), " c%zu n/a", core);
        else if (g_reportedIdleReadAt[core] == 0 || readAt == g_reportedIdleReadAt[core])
            snprintf(entry, sizeof(entry), " c%zu ?", core);
        else
            snprintf(entry, sizeof(entry), " c%zu %.1f%%", core,
                100.0 * double(idle - g_reportedIdleTicks[core]) / double(readAt - g_reportedIdleReadAt[core]));
        out += entry;
        g_reportedIdleTicks[core] = idle;
        g_reportedIdleReadAt[core] = readAt;
    }

    for (size_t i = 0; i < MAX_THREADS; i++)
    {
        ThreadSlot& thread = g_slots[i];
        if (!thread.active)
            continue;

        const uint64_t ticks = ThreadTicks(thread.handle);
        const uint64_t used = ticks >= thread.placementTicks ? ticks - thread.placementTicks : 0;
        thread.placementTicks = ticks;
        uint32_t hits[4];
        uint32_t total = 0;
        for (size_t core = 0; core < 4; core++)
        {
            hits[core] = thread.coreHits[core] - thread.reportedHits[core];
            thread.reportedHits[core] = thread.coreHits[core];
            total += hits[core];
        }
        const uint32_t moves = thread.coreMoves - thread.reportedMoves;
        thread.reportedMoves = thread.coreMoves;

        const double percent = window ? 100.0 * double(used) / double(window) : 0.0;
        if (percent < 1.0)
            continue;

        s32 ideal = -1;
        u64 mask = 0;
        s32 priority = -1;
        svcGetThreadCoreMask(&ideal, &mask, thread.handle);
        svcGetThreadPriority(&priority, thread.handle);
        ApplyCoreRule(thread, percent);
        snprintf(entry, sizeof(entry), " | #%zu %s cpu %.1f%% prio 0x%x ideal %d mask 0x%llx seen c0/1/2/3 %u/%u/%u/%u moves %u",
            i, thread.name, percent, unsigned(priority), int(ideal), (unsigned long long)mask, hits[0], hits[1], hits[2], hits[3], moves);
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

size_t os::switch_cpu_profiler::SampleThreadTicks(ThreadTime* out, size_t capacity, uint64_t& systemTick, bool wait)
{
    if (wait)
        mutexLock(&g_slotsMutex);
    else if (!mutexTryLock(&g_slotsMutex))
        return 0;
    size_t count = 0;
    systemTick = armGetSystemTick();
    for (const ThreadSlot& thread : g_slots)
    {
        if (!thread.active || count == capacity)
            continue;
        out[count].handle = thread.handle;
        out[count].ticks = ThreadTicks(thread.handle);
        memcpy(out[count].name, thread.name, sizeof(out[count].name));
        count++;
    }
    mutexUnlock(&g_slotsMutex);
    return count;
}

uint64_t os::switch_cpu_profiler::TickFrequency()
{
    return armGetSystemTickFreq();
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
void os::switch_cpu_profiler::Heartbeat()
{
    g_heartbeat.fetch_add(1, std::memory_order_relaxed);
}
const char* os::switch_cpu_profiler::DumpThreads(const char* title, size_t& size, const char* sink)
{
    g_dumpSink = sink;
    bool locked = false;
    for (int attempt = 0; attempt < 20 && !(locked = mutexTryLock(&g_slotsMutex)); ++attempt)
        svcSleepThread(1'000'000);
    g_hangLength = 0;
    HangAppend("%s", title);
    DumpThreadsBody(threadGetCurHandle(), locked);
    g_dumpSink = nullptr;
    if (locked)
        mutexUnlock(&g_slotsMutex);
    size = g_hangLength;
    return g_hangText;
}
#endif
