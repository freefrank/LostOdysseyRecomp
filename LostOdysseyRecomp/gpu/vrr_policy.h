#pragma once
#include "frame_rate.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace gpu::vrr
{
// Application-side policy only. Neither an asynchronous Present nor the
// desktop refresh rate proves that the monitor/driver is using adaptive sync.
inline constexpr uint32_t kRefreshHeadroom = 3;
constexpr uint32_t OutputLimit(uint32_t refreshHz)
{
    // SDL2 reports integer Hz; leave room for fractional modes and pacing error.
    return refreshHz >= 24 && refreshHz <= 1000 ? refreshHz - kRefreshHeadroom : 0;
}
constexpr bool HostVsyncEnabled(uint32_t nativeFps, bool baselineVsync,
    bool forceImmediate, bool requested)
{
    return frame_rate::HostVsyncEnabled(nativeFps, baselineVsync, forceImmediate || requested);
}
constexpr uint32_t PacingTarget(uint32_t nativeFps, bool requested,
    uint32_t refreshHz, uint32_t fixedMultiplier = 1)
{
    const auto outputLimit = OutputLimit(refreshHz);
    // Keep diagnostic uncapped mode and unknown-display fallback unchanged.
    if (!requested || !nativeFps || !outputLimit) return nativeFps;
    const auto divisor = std::clamp(fixedMultiplier, 1u, 16u);
    return std::min(nativeFps, std::max(1u, outputLimit / divisor));
}
inline float DynamicTarget(float requestedTarget, bool requested, uint32_t refreshHz)
{
    const auto limit = OutputLimit(refreshHz);
    // Do not sanitize invalid FG input here: selection must still reject it.
    if (!requested || !limit || !std::isfinite(requestedTarget) || requestedTarget < 0)
        return requestedTarget;
    return requestedTarget == 0 ? float(limit) : std::min(requestedTarget, float(limit));
}
inline uint32_t DynamicPacingTarget(uint32_t nativeFps, bool requested,
    uint32_t refreshHz, float requestedTarget)
{
    const auto paced = PacingTarget(nativeFps, requested, refreshHz);
    const auto target = DynamicTarget(requestedTarget, requested, refreshHz);
    if (!requested || !nativeFps || !std::isfinite(target) || target <= 0) return paced;
    return std::min(paced, uint32_t(std::clamp(target, 1.0f, 1000.0f)));
}
}
