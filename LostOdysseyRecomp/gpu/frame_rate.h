#pragma once
#include <array>
#include <cstdint>

namespace gpu::frame_rate
{
// Real game-frame targets, not generated/presented display-frame multipliers.
// Keep settings validation, menu ordering and the command processor in sync.
inline constexpr std::array<uint32_t, 4> kNativeRates{30, 60, 90, 120};
inline constexpr uint32_t kDefault = kNativeRates.front();
inline constexpr uint32_t kCount = uint32_t(kNativeRates.size());
inline constexpr uint32_t kGuestRefreshHz = 60;

// VBlank remains the console's 60 Hz clock. A host deadline supplies any
// higher native cadence; zero is reserved for the LO_FPS diagnostic override.
constexpr bool NeedsImmediate(uint32_t fps)
{
    return fps == 0 || fps > kGuestRefreshHz;
}
constexpr bool HostVsyncEnabled(uint32_t fps, bool baselineVsync, bool forceImmediate = false)
{
    return baselineVsync && !forceImmediate && !NeedsImmediate(fps);
}

constexpr bool Supported(uint32_t fps)
{
    for (const auto rate : kNativeRates)
        if (rate == fps) return true;
    return false;
}
constexpr uint32_t Normalize(uint32_t fps)
{
    return Supported(fps) ? fps : kDefault;
}
constexpr uint32_t MenuIndex(uint32_t fps)
{
    for (uint32_t i = 0; i < kCount; ++i)
        if (kNativeRates[i] == fps) return i;
    return 0;
}
constexpr uint32_t FromMenuIndex(uint32_t index)
{
    return index < kCount ? kNativeRates[index] : kDefault;
}
}
