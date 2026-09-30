#pragma once

#include <cstdint>
#include <cmath>

namespace gpu::movie_clear
{
// Host-private Type-0 register payload emitted by the movie helper.  These
// indices intentionally sit outside the guest register file and are consumed
// by CommandProcessor before its normal range rejection.
constexpr uint32_t RegisterBase = 0x7F00;
constexpr uint32_t Magic = 0x4C4F4D42; // "LOMB"
enum Register : uint32_t
{
    Begin = RegisterBase,
    SurfaceInfo,
    ColorInfo,
    OriginalX,
    OriginalY,
    OriginalWidth,
    OriginalHeight,
    SafeLeft,
    SafeRight,
    Commit,
};
constexpr uint32_t WordCount = Commit - RegisterBase + 1;
inline uint32_t ScaleBoundary(float guest, uint32_t physicalExtent, uint32_t guestExtent)
{
    if (!guestExtent) return 0;
    return uint32_t(std::lround(double(guest) * physicalExtent / guestExtent));
}
}
