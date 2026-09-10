#pragma once
#include "shader/position_evidence.h"
#include <array>
#include <cstdint>

namespace gpu::taa_collection::binding {
// Wire values are diagnostic categories, never host pointers or device IDs.
enum class TextureKind : uint32_t { Unknown, Resolved, CroppedResolve, GuestUpload, Dummy, TemporalDisplay, SpatialAA };
enum class ProducerState : uint32_t { Unknown, UniformUnjittered, UniformJittered, Mixed };
struct Transform {
    int32_t slot = -1;
    uint32_t phase = 0;
    bool applied = false;
    std::array<uint32_t, 16> guestVP{}, uploadedVP{};
    std::array<uint32_t, 4> viewport{}; // IEEE float bits: x, y, width, height.
    std::array<uint32_t, 2> jitterNdc{}; // Actual applied XY; zero if unapplied.
    bool operator==(const Transform&) const = default;
};
struct Texture {
    uint32_t slot = 0, bank = 0; // Descriptor bank: 0 is 2D, 1 is 3D, 2 is cube.
    TextureKind kind = TextureKind::Unknown;
    uint32_t guestFormat = 0, hostFormat = 0, dimension = 0, swizzle = 0;
    uint32_t sourceMip = 0, sign = 0;
    bool swapRedBlue = false;
    std::array<uint32_t, 6> sampler{}; // Decoded clamp U/V/W and min/mag/mip filters.
    std::array<uint32_t, 2> guestExtent{}, hostExtent{}, parentExtent{};
    std::array<uint32_t, 4> resolveRect{}; // Pixel rectangle x, y, width, height.
    // -1 means unknown or older than 255 renderer frames; never an absolute ID.
    int32_t producerFrameAge = -1;
    int32_t resolveFrameAge = -1, resolveGap = -1;
    ProducerState producerState = ProducerState::Unknown;
    uint32_t producerDraws = 0;
    Transform producer{};
    bool operator==(const Texture&) const = default;
};
struct Record {
    uint64_t vs = 0, ps = 0;
    uint32_t width = 0, height = 0;
    int32_t slot = -1;
    uint32_t candidates = 0, flags = 0, rejection = 0;
    position_evidence::Summary position{};
    uint32_t guards = 0;
    Transform consumer{};
    std::array<uint32_t, 4> psC0{}; // Actual uploaded screen UV scale/offset bits.
    Texture texture{};
    bool operator==(const Record&) const = default;
};
}
