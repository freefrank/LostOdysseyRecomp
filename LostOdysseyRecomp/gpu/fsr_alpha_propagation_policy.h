#pragma once
#include <cstdint>

namespace gpu::fsr_alpha {
struct MaskRect {
    uint32_t x = 0, y = 0, width = 0, height = 0;
    bool operator==(const MaskRect&) const = default;
};

inline bool Contains(MaskRect outer, MaskRect inner) {
    return inner.width && inner.height && inner.x >= outer.x && inner.y >= outer.y &&
        uint64_t(inner.x) + inner.width <= uint64_t(outer.x) + outer.width &&
        uint64_t(inner.y) + inner.height <= uint64_t(outer.y) + outer.height;
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
