#pragma once
#include <algorithm>
#include <cstdint>

namespace gpu::fsr_alpha {
struct MaskRect {
    uint32_t x = 0, y = 0, width = 0, height = 0;
    bool operator==(const MaskRect&) const = default;
};

enum class SamplerFootprint : uint8_t { Unsupported, Point, Linear };
inline SamplerFootprint LOD0ClampFootprint(uint64_t samplerKey, bool singleMip) {
    const uint32_t mag = samplerKey & 3u, min = (samplerKey >> 2) & 3u;
    const uint32_t addressU = (samplerKey >> 6) & 7u;
    const uint32_t addressV = (samplerKey >> 9) & 7u;
    const auto clamp = [](uint32_t mode) { return mode == 2u || mode == 4u; };
    if (!singleMip || !clamp(addressU) || !clamp(addressV) ||
        (samplerKey & (1ull << 15)) || (mag == 0) != (min == 0))
        return SamplerFootprint::Unsupported;
    return mag == 0 ? SamplerFootprint::Point : SamplerFootprint::Linear;
}

inline bool Contains(MaskRect outer, MaskRect inner) {
    return inner.width && inner.height && inner.x >= outer.x && inner.y >= outer.y &&
        uint64_t(inner.x) + inner.width <= uint64_t(outer.x) + outer.width &&
        uint64_t(inner.y) + inner.height <= uint64_t(outer.y) + outer.height;
}

// A whole-attachment color clear is recorded only when its GPU command is
// emitted. The ordinal is the renderer draw count at that command; the next
// draw sees that same count before incrementing it.
struct ClearBackground {
    uint64_t frame = 0, epoch = 0, colorAllocation = 0, ordinal = 0;
    uint32_t width = 0, height = 0;
    MaskRect affectedRect{};
    const char* kind = nullptr;
    const char* invalidatedBy = nullptr;
};

inline bool QualifiesInsetReplacement(const ClearBackground* clear,
    uint64_t frame, uint64_t epoch, uint64_t allocation, uint32_t width, uint32_t height,
    uint64_t drawOrdinal, MaskRect publishedRect, MaskRect drawWrittenRect) {
    return clear && clear->kind && !clear->invalidatedBy &&
        clear->frame == frame && clear->epoch == epoch &&
        clear->colorAllocation == allocation && clear->width == width && clear->height == height &&
        clear->ordinal <= drawOrdinal &&
        clear->affectedRect == MaskRect{0, 0, width, height} &&
        Contains(clear->affectedRect, publishedRect) && Contains(publishedRect, drawWrittenRect);
}

// BlitRegion and the mask copy keep source/destination pixel coordinates equal.
// Only pixels both copied and already proven by the producer are valid.
inline MaskRect IntersectCopyValidRect(MaskRect proven, MaskRect actualCopy) {
    const uint64_t left = std::max(proven.x, actualCopy.x);
    const uint64_t top = std::max(proven.y, actualCopy.y);
    const uint64_t right = std::min(uint64_t(proven.x) + proven.width,
        uint64_t(actualCopy.x) + actualCopy.width);
    const uint64_t bottom = std::min(uint64_t(proven.y) + proven.height,
        uint64_t(actualCopy.y) + actualCopy.height);
    if (right <= left || bottom <= top) return {};
    return {uint32_t(left), uint32_t(top), uint32_t(right - left), uint32_t(bottom - top)};
}

inline const char* PostprocessGuardReason(bool completeInputs, bool provenGeometry,
    bool samplerSupported, bool depthEnabled, bool stencilEnabled,
    bool geometryShader, bool cullEnabled, bool blendEnabled,
    bool insetGeometry = false, bool clearBackgroundAvailable = false) {
    if (!completeInputs) return "input_unavailable";
    if (!provenGeometry && insetGeometry && !clearBackgroundAvailable) return "clear_background_unavailable";
    if (!provenGeometry) return "quad_unavailable";
    if (!samplerSupported) return "sampler_unavailable";
    if (depthEnabled) return "depth_enabled";
    if (stencilEnabled) return "stencil_enabled";
    if (geometryShader) return "geometry_shader";
    if (cullEnabled) return "cull_enabled";
    if (blendEnabled) return "blend_enabled";
    return "available";
}

// In the captured cfmt12 FP16 pass, these exact blends leave destination
// alpha intact. Additive RGB has a destination coefficient of one. The three
// observed source-over PS use clamped source alpha, so their destination RGB
// coefficient is in [0,1]; keeping the old MAX mask is only a conservative
// PartialCoverage bound on the earlier transparency, not new PS coverage.
inline bool RetainsRawAfterAuditedLocalBlend(uint64_t vs, uint64_t ps,
    uint32_t blend, uint32_t colorMask, uint32_t guestColorFormat,
    bool fp16Target, uint32_t sharedFlags, bool forceNoBlend) {
    if (guestColorFormat != 12 || !fp16Target || !(colorMask & 7u) ||
        (sharedFlags & (2u | 4u | 8u | 16u | 32u)) || forceNoBlend) return false;
    if (blend == 0x01000106u) return true;
    return blend == 0x01000706u && vs == 0x4c87bb5b986defc8ull &&
        (ps == 0x03b446965d7d52b3ull || ps == 0x342877796a1673e6ull ||
            ps == 0x9791230246c5bc3cull);
}

struct CopyReuseSignature {
    uint64_t frame = 0, epoch = 0, sourceAllocation = 0, destinationAllocation = 0;
    uint32_t width = 0, height = 0;
    MaskRect rect{};
    uint64_t rawColorAllocation = 0, rawDepthAllocation = 0;
    uint32_t rawDraws = 0;
    bool operator==(const CopyReuseSignature&) const = default;
};
}
