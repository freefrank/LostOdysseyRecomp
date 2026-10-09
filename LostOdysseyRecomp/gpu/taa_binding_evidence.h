#pragma once
#include <array>
#include <cstdint>

// F1 capture: the texture actually bound to slot 0 of a draw ("texture_binding"
// lines in the capture trace). Categories, extents and ages only, never host pointers.
namespace gpu::taa_collection::binding {
enum class TextureKind : uint32_t { Unknown, Resolved, CroppedResolve, GuestUpload, Dummy, TemporalDisplay, SpatialAA };
struct Texture {
    uint32_t bank = 0; // Descriptor bank: 0 is 2D, 1 is 3D, 2 is cube.
    TextureKind kind = TextureKind::Unknown;
    uint32_t guestFormat = 0;
    std::array<uint32_t, 2> guestExtent{}, hostExtent{}, parentExtent{};
    // -1 means unknown or older than 255 renderer frames; never an absolute ID.
    int32_t resolveFrameAge = -1, resolveGap = -1;
};
inline int32_t RelativeAge(uint64_t now, uint64_t before) noexcept {
    return before <= now && now - before <= 255 ? int32_t(now - before) : -1;
}
}
