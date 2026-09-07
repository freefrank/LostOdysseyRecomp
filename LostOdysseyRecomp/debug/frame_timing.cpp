#include <stdafx.h>
#include "frame_timing.h"
#include <gpu/command_processor.h>
#include <gpu/frame_pacer.h>
#include <os/logger.h>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <atomic>

namespace frame_timing
{
bool Enabled() { static const bool enabled = getenv("LO_FRAME_TIMING") != nullptr; return enabled; }
namespace
{
std::mutex mutex;
std::atomic<uint64_t> inputTick{0};
uint64_t ticks = 0, intervals = 0;
double deltaSum = 0;
uint32_t requested = 0, encoded = 0;
double cpIdleMs = 0;
}
void CpIdle(double milliseconds) { if (Enabled()) cpIdleMs += milliseconds; }
void EngineTick(double delta)
{
    static const bool inputTicks = [] { const char* value = getenv("LO_TEST_INPUT_TICKS"); return value && strcmp(value, "1") == 0; }();
    if (inputTicks) inputTick.fetch_add(1, std::memory_order_relaxed);
    if (!Enabled()) return;
    std::lock_guard lock(mutex);
    ++ticks;
    if (std::isfinite(delta)) deltaSum += delta;
}
uint64_t InputTick() { return inputTick.load(std::memory_order_relaxed); }
void GuestInterval(uint32_t request, uint32_t encode)
{
    if (!Enabled()) return;
    std::lock_guard lock(mutex);
    ++intervals; requested = request; encoded = encode;
}
void Present(uint32_t swap, uint32_t fps, double flushMs, double waitMs, double presentMs,
    const PacingSample& pacing)
{
    if (!Enabled()) return;
    using Clock = std::chrono::steady_clock;
    static auto start = Clock::now();
    static uint32_t count = 0;
    static double flush = 0, wait = 0, present = 0;
    static double sleepRequest = 0, sleepActual = 0, wakeOver = 0, wakeMax = 0, between = 0;
    static uint32_t slept = 0, over1ms = 0, paired = 0;
    ++count; flush += flushMs; wait += waitMs; present += presentMs;
    sleepRequest += pacing.requestedMs; sleepActual += pacing.actualMs;
    wakeOver += pacing.overshootMs;
    wakeMax = std::max(wakeMax, pacing.overshootMs);
    slept += pacing.requestedMs > 0;
    over1ms += pacing.overshootMs > 1.0;
    if (pacing.hasPrevious) { between += pacing.betweenMs; ++paired; }
    const auto now = Clock::now();
    const double seconds = std::chrono::duration<double>(now - start).count();
    if (seconds < 1.0) return;
    std::lock_guard lock(mutex);
    LOG_INFO("frame timing completed={} target={} window={:.6f}s presents={} rate={:.3f} flush={:.3f}ms pace={:.3f}ms present={:.3f}ms engine_ticks={} delta_sum={:.6f} intervals={} last_interval={:#x}->{:#x}",
        swap, fps, seconds, count, count / seconds, flush / count, wait / count, present / count,
        ticks, deltaSum, intervals, requested, encoded);
    LOG_INFO("frame pacing completed={} samples={} sleep_requested={:.3f}ms sleep_actual={:.3f}ms wake_over={:.3f}ms wake_max={:.3f}ms slept={} over_1ms={} between={:.3f}ms paired={} cp_idle={:.3f}ms",
        swap, count, sleepRequest / count, sleepActual / count, wakeOver / count, wakeMax,
        slept, over1ms, paired ? between / paired : 0.0, paired, cpIdleMs / count);
    start = now; count = 0; flush = wait = present = deltaSum = 0; ticks = intervals = 0;
    sleepRequest = sleepActual = wakeOver = wakeMax = between = cpIdleMs = 0;
    slept = over1ms = paired = 0;
}
}

extern "C" PPC_FUNC(__imp__sub_827B6AD8);
PPC_FUNC(sub_827B6AD8)
{
    const auto requested = ctx.r7.u32;
    // Controlled A/B: disable only the guest interval conversion while keeping
    // the exact same host cap, video configuration and instrumentation.
    static const bool mapInterval = [] {
        const char* value = getenv("LO_GUEST_INTERVAL");
        return !value || strcmp(value, "0") != 0;
    }();
    static const bool immediate120 = [] {
        const char* value = getenv("LO_EXPERIMENTAL_120");
        return value && strcmp(value, "1") == 0;
    }();
    const auto mapped = mapInterval ? gpu::MapPresentInterval(requested, uint32_t(ctx.lr), gpu::GetFrameRateTarget(), immediate120) : requested;
    if (mapped != requested) ctx.r7.u64 = (ctx.r7.u64 & 0xFFFFFFFF00000000ull) | mapped;
    if (uint32_t(ctx.lr) == 0x827B4A4C) frame_timing::GuestInterval(requested, ctx.r7.u32);
    __imp__sub_827B6AD8(ctx, base);
}
