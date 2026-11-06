#pragma once

// From ChanseyIsTheBest/UnleashedRecomp-NX (GPL-3.0, as this project).
// Lost Odyssey: the sampler starts when the file cpu-profile exists in the
// app folder on the SD card (sdmc:/switch/LostOdysseyRecomp/cpu-profile);
// tools/switch/switch-cpu-profile.py names the offsets it reports.
#if defined(__SWITCH__)

#include <cstddef>
#include <cstdint>
#include <string>

// [Switch] Per-thread CPU accounting and a sampling CPU profiler.
//
// Threads register themselves with a name. The kernel counts the CPU time of every thread; the GPU
// pass profiler report prints each registered thread's share of the last report window
// (AppendThreadCpuUsage). With the cpu-profile file present, a sampler thread also pauses each
// registered thread every 2 ms, reads where it is (program counter) and resumes it; every 30 seconds
// it writes the hottest code addresses of each thread to stderr.log, as offsets into the executable
// that tools/switch-cpu-profile.py turns into function names (recompiled game functions included).
//
// Lost Odyssey: LO_CPU_PROFILE_SAMPLES=1 (or a comma-separated list of thread names, e.g.
// "Guest Main,GPU CmdProc") also keeps every sample of those threads, with the host LR and, for guest
// threads, the guest call chain (return addresses from the PowerPC back chain), and appends them to
// cpu-samples.bin in the app folder at each report. tools/switch/switch-cpu-profile.py --samples reads it.
namespace os::switch_cpu_profiler
{
    // The name is copied (up to 47 characters).
    void RegisterCurrentThread(const char* name);
    void RegisterCurrentThreadWithAddress(const char* prefix, uint32_t guestAddress);
    void UnregisterCurrentThread();

    // The calling (registered) thread runs guest code: where its PPCContext pointer lives (the thread_local
    // g_ppcContext, which GuestToHostFunction swaps), the offsets of r1 and lr in a PPCContext (lrOffset
    // UINT32_MAX: no lr), guest memory's base and the guest stack's [low, high) guest addresses. With
    // LO_CPU_PROFILE_SAMPLES the sampler then walks the guest stack's back chain at each sample (below).
    void SetCurrentThreadGuestStack(void* const* contextSlot, uint32_t r1Offset, uint32_t lrOffset,
        const uint8_t* memoryBase, uint32_t stackLow, uint32_t stackHigh);
    void ClearCurrentThreadGuestStack();

    // Starts the per-thread accounting, and the sampler when asked ([Switch] SwitchCpuProfiler).
    void Start(bool sampler);

    // CPU time of every registered thread since the previous call, as "name 12.3%" entries. Never waits
    // for the sampler: while it holds the thread list, the entries are left out of this report.
    void AppendThreadCpuUsage(std::string& out);

    // LO_THREAD_PLACEMENT=1: where threads run. NoteCore (called from each thread's wait paths, at most every 2 ms, one
    // relaxed test when off) counts the core it finds the thread on; AppendThreadPlacement gives, since the previous
    // call, each core's idle share and, for threads above 1% CPU, priority, ideal core, allowed mask, those counts
    // and how often the core changed between two of them. Leaves AppendThreadCpuUsage's window alone.
    void NoteCore();
    void AppendThreadPlacement(std::string& out);

    // The registered threads (kernel handle and name), for the stall watchdog. Returns how many were copied.
    struct ThreadInfo
    {
        uint32_t handle;
        char name[48];
    };
    size_t CopyThreads(ThreadInfo* out, size_t capacity);

    // Every registered thread's CPU time so far (kernel ticks) and the system tick it was read at, for callers that
    // keep their own windows (debug/perf_stats.cpp). Leaves AppendThreadCpuUsage's window alone. wait=false gives up
    // (returns 0) while the sampler holds the thread list.
    struct ThreadTime
    {
        uint32_t handle;
        uint64_t ticks;
        char name[48];
    };
    size_t SampleThreadTicks(ThreadTime* out, size_t capacity, uint64_t& systemTick, bool wait);
    // System ticks per second (the kernel's thread tick counts use the same clock).
    uint64_t TickFrequency();

    // The calling thread's registered name, for a crash report: false (and `out` untouched) when it has none or the
    // thread list is locked at the moment (the crash handler cannot wait for it).
    bool TryGetCurrentThreadName(char* out, size_t size);

    // Called once per presented frame. With the sampler on, a watchdog writes every registered
    // thread's position to hang.log in the app folder after 4 s without a frame (then 15 s and 40 s).
    void Heartbeat();

    // Every registered thread's position except the caller's (pc, lr, wait, frames, return addresses on
    // the stack) and the malloc lock owner, as text in a static buffer. Never allocates and never waits
    // long for a lock. For the crash handler and the hang watchdog; not reentrant. With a sink (a file
    // name for os::switch_platform::RawAppend), each thread is appended there as it is dumped and the
    // returned text is only what is left.
    const char* DumpThreads(const char* title, size_t& size, const char* sink = nullptr);

    // Start and size of the executable's code: the offsets every report prints are relative to it.
    uint64_t ModuleBase();
    uint64_t ModuleSize();

    // Appends text to stderr.log in one write. stderr is line-buffered (so that a crash loses nothing), which
    // made every fwrite of a report one SD card write per line: seconds for a CPU profile, during which the
    // writer held stderr's lock (and the profiler its thread list).
    void WriteLog(const char* text, size_t size);
    inline void WriteLog(const std::string& text) { WriteLog(text.data(), text.size()); }
}

#endif
