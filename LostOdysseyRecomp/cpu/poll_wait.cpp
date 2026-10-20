#include <stdafx.h>
#include "poll_wait.h"
#include "render_fence.h"
#include <atomic>
#include <chrono>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>
#include <os/logger.h>
#include <kernel/memory.h>
#include <patches/occlusion_history.h>

extern "C" PPC_FUNC(__imp__sub_823CF390);
extern "C" PPC_FUNC(__imp__sub_82322478);

// Retain the generated function, including register saves, return values and
// query ordering. Only its repeated not-ready probes can pause on the host.
PPC_FUNC(sub_823CF390)
{
    // Policy::History answers the game's deferred reads without the loop.
    if (occlusion_history::Read(ctx, base)) return;
    poll_wait::RunScoped(poll_wait::Kind::Query, [&] { __imp__sub_823CF390(ctx, base); });
}

// Loading screens. While the game thread waits on a render command fence
// (FlushRenderingCommands during a map or battle load), the rendering thread
// (RenderingThreadMain 0x824856A0) runs one queued command, then ticks the
// loading-screen movie (0x823B63E0) whenever its tick interval has passed.
// Each tick presents a frame and waits for the GPU to be at most 3 frames
// behind; with presentation paced at 30 fps that wait alone outlasts the
// interval, so the queue drains at about one command per frame and a map load
// takes up to a minute. Measured 2026-10-14: the commands take 3-23 ms in
// total per wait; the ticks take the rest (one tick per command, ~33 ms each,
// tick interval 1/30 s), and the tick's frame is held by the guest's two-vblank
// present interval. render_fence::LoadBoost() makes vblanks faster during such
// a wait (gpu/command_processor.cpp). Pacing at 60 fps alone changed nothing.
// Tested 2026-10-14 17:07: ticks went from ~33 to ~6.4 ms and each batch
// drained ~3x faster, but the game then queued ~3x more commands; the share of
// play time spent in these waits stayed at 23% and the first map load stayed
// at 41 s. The loading wheel flickered, so the boost is opt-in (LO_LOAD_BOOST=1).
// Skipping ticks instead (LO_LOAD_TICK_THROTTLE=1, at most one tick every
// 100 ms between commands) left battle transitions on the loading screen for
// minutes on 2026-10-14, so it is off by default.
namespace
{
    std::atomic<int> g_fenceWaiters{0};
    std::atomic<uint64_t> g_fenceWaitSinceMs{0};
    std::atomic<uint64_t> g_lastMovieTickMs{0};
    std::atomic<uint32_t> g_ticksRun{0}, g_ticksDeferred{0};

    // What the rendering thread does during a fence wait: queued commands
    // (by their Execute function) and loading-screen ticks, in microseconds.
    struct CommandStat { uint32_t execute; uint32_t count; uint64_t us; };
    std::mutex g_statsMutex;
    std::vector<CommandStat> g_commandStats;
    uint64_t g_tickUs = 0, g_tickCount = 0, g_tickMaxUs = 0;
    uint64_t g_commandUs = 0, g_commandCount = 0;
    thread_local std::chrono::steady_clock::time_point t_commandStart;
    thread_local uint32_t t_commandExecute = 0;

    uint64_t ElapsedUs(std::chrono::steady_clock::time_point since)
    {
        return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - since).count());
    }

    std::string TakeRenderThreadStats()
    {
        std::lock_guard lock(g_statsMutex);
        std::sort(g_commandStats.begin(), g_commandStats.end(),
            [](const CommandStat& a, const CommandStat& b) { return a.us > b.us; });
        std::string out = fmt::format("{} commands {:.0f} ms, {} ticks {:.0f} ms (longest {:.1f} ms)",
            g_commandCount, g_commandUs / 1e3, g_tickCount, g_tickUs / 1e3, g_tickMaxUs / 1e3);
        for (size_t i = 0; i < g_commandStats.size() && i < 6; ++i)
            out += fmt::format("; exec {:#x} x{} {:.0f} ms", g_commandStats[i].execute, g_commandStats[i].count,
                g_commandStats[i].us / 1e3);
        g_commandStats.clear();
        g_tickUs = g_tickCount = g_tickMaxUs = g_commandUs = g_commandCount = 0;
        return out;
    }

    uint64_t NowMs()
    {
        return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    bool TickThrottleEnabled()
    {
        static const bool on = [] {
            const char* v = getenv("LO_LOAD_TICK_THROTTLE");
            return v && v[0] == '1';
        }();
        return on;
    }
}

bool render_fence::LoadBoost()
{
    static const bool on = [] {
        const char* v = getenv("LO_LOAD_BOOST");
        return v && v[0] == '1';
    }();
    return on && LongWait();
}

uint32_t render_fence::LoadBoostVblankHz()
{
    static const uint32_t hz = [] {
        const char* v = getenv("LO_LOAD_VBLANK_HZ");
        const unsigned long n = v ? strtoul(v, nullptr, 10) : 0;
        return n >= 60 && n <= 2000 ? uint32_t(n) : 480u;
    }();
    return hz;
}

void render_fence::CommandBegin(uint32_t command)
{
    t_commandExecute = 0;
    if (!g_fenceWaitSinceMs.load(std::memory_order_relaxed) || command < 0x10000)
        return;
    uint8_t* const base = g_memory.base;
    const uint32_t vtable = PPC_LOAD_U32(command);
    if (vtable < 0x82000000 || vtable >= 0x84000000)
        return;
    t_commandExecute = PPC_LOAD_U32(vtable + 4);
    t_commandStart = std::chrono::steady_clock::now();
}

void render_fence::CommandEnd()
{
    if (!t_commandExecute)
        return;
    const uint64_t us = ElapsedUs(t_commandStart);
    std::lock_guard lock(g_statsMutex);
    ++g_commandCount;
    g_commandUs += us;
    for (auto& c : g_commandStats)
        if (c.execute == t_commandExecute) { ++c.count; c.us += us; t_commandExecute = 0; return; }
    if (g_commandStats.size() < 256)
        g_commandStats.push_back({ t_commandExecute, 1, us });
    t_commandExecute = 0;
}

bool render_fence::LongWait()
{
    const uint64_t since = g_fenceWaitSinceMs.load(std::memory_order_acquire);
    return since && NowMs() - since >= 250;
}

// FRenderCommandFence::Wait: this guest loop repeatedly invokes Sleep(0) while
// a shared value exceeds its threshold. Keep its original reads and comparison;
// opt in only this call tree. Guest thread exit uses longjmp: no objects with
// destructors here.
PPC_FUNC(sub_82322478)
{
    if (g_fenceWaiters.fetch_add(1, std::memory_order_acq_rel) == 0)
        g_fenceWaitSinceMs.store(NowMs(), std::memory_order_release);
    poll_wait::RunScoped(poll_wait::Kind::SharedValue, [&] { __imp__sub_82322478(ctx, base); });
    if (g_fenceWaiters.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
        const uint64_t since = g_fenceWaitSinceMs.exchange(0, std::memory_order_acq_rel);
        const uint32_t run = g_ticksRun.exchange(0), deferred = g_ticksDeferred.exchange(0);
        const std::string stats = TakeRenderThreadStats();
        if (since && NowMs() - since >= 1000)
        {
            static bool loggedIntervals = false;
            if (!loggedIntervals)
            {
                // RenderingThreadMain's tick intervals (seconds): idle, between commands.
                loggedIntervals = true;
                uint8_t* const base = g_memory.base;
                const uint32_t idle = PPC_LOAD_U32(0x822185B8), busy = PPC_LOAD_U32(0x822185B4);
                float fIdle, fBusy;
                std::memcpy(&fIdle, &idle, 4);
                std::memcpy(&fBusy, &busy, 4);
                LOG_INFO("rendering thread tick interval: idle {} s, between commands {} s", fIdle, fBusy);
            }
            LOG_INFO("render fence wait {:.1f} s: loading-screen ticks run {}, deferred {}; rendering thread: {}",
                (NowMs() - since) / 1000.0, run, deferred, stats);
        }
    }
}

extern "C" PPC_FUNC(__imp__sub_823B63E0);
PPC_FUNC(sub_823B63E0)
{
    // Only the call between two queued commands (not the idle-loop tick).
    if (uint32_t(ctx.lr) == 0x824857A4)
    {
        const uint64_t since = g_fenceWaitSinceMs.load(std::memory_order_acquire);
        const uint64_t now = NowMs();
        if (TickThrottleEnabled() && since && now - since >= 250 &&
            now - g_lastMovieTickMs.load(std::memory_order_relaxed) < 100)
        {
            g_ticksDeferred.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        g_ticksRun.fetch_add(1, std::memory_order_relaxed);
    }
    const bool measure = g_fenceWaitSinceMs.load(std::memory_order_relaxed) != 0;
    const auto start = std::chrono::steady_clock::now();
    __imp__sub_823B63E0(ctx, base);
    g_lastMovieTickMs.store(NowMs(), std::memory_order_relaxed);
    if (measure)
    {
        const uint64_t us = ElapsedUs(start);
        std::lock_guard lock(g_statsMutex);
        ++g_tickCount;
        g_tickUs += us;
        if (us > g_tickMaxUs) g_tickMaxUs = us;
    }
}

// GpuPoll for the title GPU timestamp wait lives in debug/gpu_wait_trace.cpp.
// That file already owns PPC_FUNC(sub_823B62A0) and PPC_FUNC(sub_827B6278).
