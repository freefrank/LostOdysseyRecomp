#include <stdafx.h>
#include "frame_timing.h"
#include <gpu/command_processor.h>
#include <gpu/frame_pacer.h>
#include <settings/config.h>
#include <gpu/render_timing.h>
#include <os/logger.h>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <mutex>
#include <atomic>

namespace frame_timing
{
bool Enabled() { static const bool enabled = getenv("LO_FRAME_TIMING") != nullptr; return enabled || gpu::render_timing::Enabled(); }
namespace
{
std::mutex mutex;
std::atomic<uint64_t> inputTick{0};
uint64_t ticks = 0, intervals = 0;
double deltaSum = 0;
uint32_t requested = 0, encoded = 0;
double cpIdleMs = 0;
double cpPresentIdleMs = 0;
}
void CpIdle(double milliseconds) { if (Enabled()) { cpIdleMs += milliseconds; cpPresentIdleMs += milliseconds; } }
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
    const double presentIdleMs = cpPresentIdleMs;
    cpPresentIdleMs = 0;
    if (gpu::render_timing::Enabled())
    {
        // These adjacent intervals cover previous completed-present end through
        // this completed-present end. The first call has no preceding boundary.
        // PresentFrontbuffer plus the event pump comprise presentMs; betweenMs
        // includes command processing, intermediate flushes, waits and previous
        // post-present diagnostics. These are wall intervals, not CPU usage.
        const bool valid = pacing.presentAccepted && pacing.hasPrevious && std::isfinite(pacing.betweenMs) && pacing.betweenMs >= 0 &&
            std::isfinite(flushMs) && flushMs >= 0 && std::isfinite(waitMs) && waitMs >= 0 &&
            std::isfinite(presentMs) && presentMs >= 0;
        LOG_INFO("present timing completed={} target={} present_accepted={} has_previous={} sample_valid={} frame_ms={} "
            "between_ms={} flush_ms={:.6f} pace_ms={:.6f} present_and_events_ms={:.6f} "
            "sleep_requested_ms={:.6f} sleep_actual_ms={:.6f} wake_overshoot_ms={:.6f} previous_swap_log_ms={:.6f} previous_swap_post_ms={:.6f} cp_idle_ms={:.6f} scope=accepted_present_api_wall_intervals_not_display_latency",
            swap, fps, pacing.presentAccepted, pacing.hasPrevious, valid,
            valid ? fmt::format("{:.6f}", pacing.betweenMs + flushMs + waitMs + presentMs) : std::string("unknown"),
            pacing.hasPrevious ? fmt::format("{:.6f}", pacing.betweenMs) : std::string("unknown"),
            flushMs, waitMs, presentMs, pacing.requestedMs, pacing.actualMs, pacing.overshootMs,
            pacing.previousSwapLogMs, pacing.previousSwapPostMs, presentIdleMs);
    }
    // Keep the existing one-second report opt-in separately. LO_RENDER_TIMING
    // records every completed sample instead of estimating FPS from heartbeats.
    static const bool aggregate = getenv("LO_FRAME_TIMING") != nullptr;
    if (!aggregate) return;
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
    uint64_t snapTicks = 0, snapIntervals = 0;
    double snapDelta = 0, snapIdle = 0;
    uint32_t snapRequested = 0, snapEncoded = 0;
    {
        std::lock_guard lock(mutex);
        snapTicks = ticks;
        snapDelta = deltaSum;
        snapIntervals = intervals;
        snapRequested = requested;
        snapEncoded = encoded;
        snapIdle = cpIdleMs;
        ticks = intervals = 0;
        deltaSum = cpIdleMs = 0;
    }
    const uint32_t snapCount = count;
    const double snapFlush = flush, snapWait = wait, snapPresent = present;
    const double snapSleepRequest = sleepRequest, snapSleepActual = sleepActual;
    const double snapWakeOver = wakeOver, snapWakeMax = wakeMax, snapBetween = between;
    const uint32_t snapSlept = slept, snapOver1ms = over1ms, snapPaired = paired;
    start = now;
    count = 0;
    flush = wait = present = 0;
    sleepRequest = sleepActual = wakeOver = wakeMax = between = 0;
    slept = over1ms = paired = 0;
    // Every-frame present timing already records this window. The 1s summary
    // is extra file I/O on the swap thread and has stalled city presents 200ms+.
    if (gpu::render_timing::Enabled()) return;
    LOG_INFO("frame timing completed={} target={} window={:.6f}s presents={} rate={:.3f} flush={:.3f}ms pace={:.3f}ms present={:.3f}ms engine_ticks={} delta_sum={:.6f} intervals={} last_interval={:#x}->{:#x} source=guest_swap engine_rate={:.3f} game_time_ratio={:.6f}",
        swap, fps, seconds, snapCount, snapCount / seconds, snapFlush / snapCount, snapWait / snapCount, snapPresent / snapCount,
        snapTicks, snapDelta, snapIntervals, snapRequested, snapEncoded, snapTicks / seconds, snapDelta / seconds);
    LOG_INFO("frame pacing completed={} samples={} sleep_requested={:.3f}ms sleep_actual={:.3f}ms wake_over={:.3f}ms wake_max={:.3f}ms slept={} over_1ms={} between={:.3f}ms paired={} cp_idle={:.3f}ms",
        swap, snapCount, snapSleepRequest / snapCount, snapSleepActual / snapCount, snapWakeOver / snapCount, snapWakeMax,
        snapSlept, snapOver1ms, snapPaired ? snapBetween / snapPaired : 0.0, snapPaired, snapIdle / snapCount);
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
    // Native 90/120 use the same real-time engine path as 30/60. Only the
    // identified game's present interval changes; no clock or delta scaling.
    const auto mapped = mapInterval ? gpu::MapPresentInterval(requested, uint32_t(ctx.lr), gpu::GetFrameRateTarget(), settings::GetConfig().variableRefreshRate) : requested;
    if (mapped != requested) ctx.r7.u64 = (ctx.r7.u64 & 0xFFFFFFFF00000000ull) | mapped;
    if (uint32_t(ctx.lr) == 0x827B4A4C) frame_timing::GuestInterval(requested, ctx.r7.u32);
    __imp__sub_827B6AD8(ctx, base);
}
