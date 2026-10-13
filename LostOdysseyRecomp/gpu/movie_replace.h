#pragma once

#include <cstdint>

namespace gpu::movie_replace
{
// Host-private Type-0 register payload, like movie_clear.h: draw the
// replacement frame of Session at TimeLow/TimeHigh (movie microseconds) over
// the guest movie quad that was just drawn into this rectangle.
constexpr uint32_t RegisterBase = 0x7F10;
constexpr uint32_t Magic = 0x4C4F4D56; // "LOMV"
enum Register : uint32_t
{
    Begin = RegisterBase,
    SurfaceInfo,
    ColorInfo,
    X,
    Y,
    Width,
    Height,
    Session,
    TimeLow,
    TimeHigh,
    Commit,
};
constexpr uint32_t WordCount = Commit - RegisterBase + 1;
static_assert(Commit < 0x7F20, "the frame plan registers start at 0x7F20");

// Guest render thread, around the FLO_MOVIE proxy render (gpu/hor_plus.cpp).
void ProxyBegin(uint32_t guestMovie);
void ProxyEnd();
// After the guest quad of the movie in the current proxy; emits the packet
// when that movie has a replacement.
void EmitDraw(uint32_t device, uint32_t surfaceInfo, uint32_t colorInfo, float x, float y, float width, float height);
}
