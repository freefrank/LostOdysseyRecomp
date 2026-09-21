#pragma once

#include <algorithm>
#include <bit>
#include <climits>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>

namespace gpu::color_qualification {

inline constexpr uint64_t kTonemapVS = 0x9b81c55ca39bb529ull;
inline constexpr uint64_t kTonemapPS = 0xb4b4d54a7a2d6b96ull;
inline constexpr uint32_t kTonemapC10XBits = 0x3ee8ba2eu;

enum class ProducerRejectReason : uint32_t {
    None = 0,
    ShaderMismatch = 1,
    C10Mismatch = 2,
    ColorMaskMismatch = 3,
    BlendMismatch = 4,
    DepthTestEnabled = 5,
    StencilTestEnabled = 6,
    CullingEnabled = 7,
    AlphaTestEnabled = 8,
    DebugOverrides = 9,
    TargetFormatMismatch = 10,
    TargetExpBiasMismatch = 11,
    VtxFmtMismatch = 12,
    SharedFlagsRejected = 13,
    PrimitiveMismatch = 14,
    ScissorMismatch = 15,
    Slot95Unbound = 16,
    IndexRangeOverflow = 17,
    StreamBytesOverflow = 18,
    NonFiniteCoordinate = 19,
    ZOutOfRange = 20,
    WNotOne = 21,
    CoverageNotFull = 22,
    TopologyInvalid = 23,
};

struct ProducerPipelineCheck {
    uint64_t vs = 0;
    uint64_t ps = 0;
    uint32_t c10xBits = 0;
    uint32_t colorMask = 0;
    uint32_t blend = 0;
    uint32_t depthControl = 0;
    uint32_t modeCull = 0;
    uint32_t colorControl = 0;
    uint32_t guestTargetFormat = 0; // Must be 0 (maps to host FP16)
    uint32_t targetExpBias = 0;     // Must be 0
    uint32_t vtxFmt = 0;            // Must be 4
    uint32_t sharedFlags = 0;       // Must NOT have bits 2|4|8|16
    bool debugOverrides = false;
};

inline ProducerRejectReason CheckProducerPipeline(const ProducerPipelineCheck& c) noexcept {
    if (c.vs != kTonemapVS || c.ps != kTonemapPS) return ProducerRejectReason::ShaderMismatch;
    if (c.c10xBits != kTonemapC10XBits) return ProducerRejectReason::C10Mismatch;
    if ((c.colorMask & 7) != 7) return ProducerRejectReason::ColorMaskMismatch;
    // Copy blend: ONE (1) / ZERO (0) / ADD (0)
    const uint32_t srcFactor = c.blend & 31;
    const uint32_t blendOp = (c.blend >> 5) & 7;
    const uint32_t dstFactor = (c.blend >> 8) & 31;
    if (srcFactor != 1 || dstFactor != 0 || blendOp != 0) return ProducerRejectReason::BlendMismatch;

    // depth test disabled or pass-all without rejection
    if ((c.depthControl & 2) != 0 && ((c.depthControl >> 4) & 7) != 7) return ProducerRejectReason::DepthTestEnabled;
    // stencil test disabled: (depthControl & 1) == 0
    if ((c.depthControl & 1) != 0) return ProducerRejectReason::StencilTestEnabled;
    // triangle culling disabled: (modeCull & 3) == 0
    if ((c.modeCull & 3) != 0) return ProducerRejectReason::CullingEnabled;
    // alpha test disabled: (colorControl & 8) == 0
    if ((c.colorControl & 8) != 0) return ProducerRejectReason::AlphaTestEnabled;
    if (c.debugOverrides) return ProducerRejectReason::DebugOverrides;
    // shared.flags bits 2 (ps debug), 4 (vs debug), 8 (vs raw/no VTE), 16 (tex debug) rejected
    if ((c.sharedFlags & (2 | 4 | 8 | 16)) != 0) return ProducerRejectReason::SharedFlagsRejected;
    if (c.guestTargetFormat != 0) return ProducerRejectReason::TargetFormatMismatch;
    if (c.targetExpBias != 0) return ProducerRejectReason::TargetExpBiasMismatch;
    if (c.vtxFmt != 4) return ProducerRejectReason::VtxFmtMismatch;
    return ProducerRejectReason::None;
}

struct QuadGeometryCheckResult {
    bool ok = false;
    float bounds[4] = {0.0f, 0.0f, 0.0f, 0.0f}; // xmin, ymin, xmax, ymax
    ProducerRejectReason rejectReason = ProducerRejectReason::None;
};

inline QuadGeometryCheckResult CheckQuadCoverage(
    std::span<const float, 24> positions, // 6 vertices * float4 (x,y,z,w) from arena
    float physicalVpX, float physicalVpY, float physicalVpW, float physicalVpH,
    int32_t scissorL, int32_t scissorT, int32_t scissorR, int32_t scissorB,
    const float ndcScale[3], const float ndcOffset[3], const float halfPixel[2]) noexcept {

    QuadGeometryCheckResult res{};
    float xy[6][2];
    float xmin = INFINITY, ymin = INFINITY, xmax = -INFINITY, ymax = -INFINITY;

    if (!std::isfinite(physicalVpX) || !std::isfinite(physicalVpY) ||
        !std::isfinite(physicalVpW) || !std::isfinite(physicalVpH)) {
        res.rejectReason = ProducerRejectReason::NonFiniteCoordinate;
        return res;
    }
    // Origin-0 coverage token: only width/height are declared.
    if (physicalVpX != 0.0f || physicalVpY != 0.0f) {
        res.rejectReason = ProducerRejectReason::CoverageNotFull;
        return res;
    }
    // Scissor compares as int32, so extents must fit INT32_MAX exactly.
    // float(UINT32_MAX) rounds to 2^32 and must not be accepted then cast.
    const auto exactInt32Extent = [](float value) {
        if (!(value >= 1.0f) || double(value) > double(INT32_MAX)) return false;
        const auto rounded = int32_t(value);
        return float(rounded) == value;
    };
    if (!exactInt32Extent(physicalVpW) || !exactInt32Extent(physicalVpH)) {
        res.rejectReason = ProducerRejectReason::CoverageNotFull;
        return res;
    }
    const uint32_t vpW = uint32_t(physicalVpW);
    const uint32_t vpH = uint32_t(physicalVpH);

    // Scissor must cover the origin-0 physical viewport.
    if (scissorL > 0 || scissorT > 0 || scissorR < int32_t(vpW) || scissorB < int32_t(vpH)) {
        res.rejectReason = ProducerRejectReason::ScissorMismatch;
        return res;
    }

    for (unsigned i = 0; i < 6; ++i) {
        const float px = positions[i * 4 + 0];
        const float py = positions[i * 4 + 1];
        const float pz = positions[i * 4 + 2];
        const float pw = positions[i * 4 + 3];

        if (!std::isfinite(px) || !std::isfinite(py) || !std::isfinite(pz) || !std::isfinite(pw)) {
            res.rejectReason = ProducerRejectReason::NonFiniteCoordinate;
            return res;
        }
        if (pw != 1.0f) {
            res.rejectReason = ProducerRejectReason::WNotOne;
            return res;
        }
        // W=1 contract: transformed Z is strictly [0, 1].
        const float transformedZ = pz * ndcScale[2] + ndcOffset[2];
        if (!std::isfinite(transformedZ) || transformedZ < 0.0f || transformedZ > 1.0f) {
            res.rejectReason = ProducerRejectReason::ZOutOfRange;
            return res;
        }
        xy[i][0] = physicalVpX + (px * ndcScale[0] + ndcOffset[0] + halfPixel[0] + 1.0f) * physicalVpW * 0.5f;
        xy[i][1] = physicalVpY + (1.0f - py * ndcScale[1] - ndcOffset[1] - halfPixel[1]) * physicalVpH * 0.5f;
        xmin = std::min(xmin, xy[i][0]);
        xmax = std::max(xmax, xy[i][0]);
        ymin = std::min(ymin, xy[i][1]);
        ymax = std::max(ymax, xy[i][1]);
    }

    res.bounds[0] = xmin; res.bounds[1] = ymin; res.bounds[2] = xmax; res.bounds[3] = ymax;

    // Cover every pixel center. Equality with the last center is the exclude edge.
    const float firstCenterX = 0.5f;
    const float firstCenterY = 0.5f;
    const float lastCenterX = physicalVpW - 0.5f;
    const float lastCenterY = physicalVpH - 0.5f;
    if (xmin >= firstCenterX || ymin >= firstCenterY || xmax <= lastCenterX || ymax <= lastCenterY) {
        res.rejectReason = ProducerRejectReason::CoverageNotFull;
        return res;
    }

    // Projection epsilon only. Separated corners must not collapse into one quad vertex.
    constexpr float kCornerEps = 1e-3f;
    unsigned masks[2] = {};
    float cornerXY[4][2]{};
    bool cornerSeen[4]{};
    for (unsigned i = 0; i < 6; ++i) {
        const bool right = std::abs(xy[i][0] - xmax) <= kCornerEps;
        const bool left = std::abs(xy[i][0] - xmin) <= kCornerEps;
        const bool bottom = std::abs(xy[i][1] - ymax) <= kCornerEps;
        const bool top = std::abs(xy[i][1] - ymin) <= kCornerEps;
        if (right == left || bottom == top) {
            res.rejectReason = ProducerRejectReason::TopologyInvalid;
            return res;
        }
        const unsigned corner = (right ? 1u : 0u) + (bottom ? 2u : 0u);
        if (!cornerSeen[corner]) {
            cornerXY[corner][0] = xy[i][0];
            cornerXY[corner][1] = xy[i][1];
            cornerSeen[corner] = true;
        } else if (std::abs(xy[i][0] - cornerXY[corner][0]) > kCornerEps ||
                   std::abs(xy[i][1] - cornerXY[corner][1]) > kCornerEps) {
            res.rejectReason = ProducerRejectReason::TopologyInvalid;
            return res;
        }
        masks[i / 3] |= 1u << corner;
    }

    const unsigned common = masks[0] & masks[1];
    const bool validTopology = (std::popcount(masks[0]) == 3) && (std::popcount(masks[1]) == 3) &&
        ((masks[0] | masks[1]) == 15) && (common == 9 || common == 6);
    if (!validTopology) {
        res.rejectReason = ProducerRejectReason::TopologyInvalid;
        return res;
    }

    res.ok = true;
    return res;
}

inline bool CheckNetIdentityRBSwap(uint32_t fetchWord0, uint32_t fetchWord3, bool resolveSwapRedBlue) noexcept {
    const uint32_t sign = (fetchWord0 >> 2) & 0xFF;
    const uint32_t swizzle = (fetchWord3 >> 1) & 0xFFF;
    if (sign != 0) return false;
    const uint32_t sR = swizzle & 7;
    const uint32_t sG = (swizzle >> 3) & 7;
    const uint32_t sB = (swizzle >> 6) & 7;

    if (resolveSwapRedBlue) {
        return (sR == 2 && sG == 1 && sB == 0);
    } else {
        return (sR == 0 && sG == 1 && sB == 2);
    }
}

// Inline helper functions operating on texture/surface qualification fields directly
inline void MarkHostTextureProducer(uint64_t& producerFrame, uint32_t& qualifiedWidth, uint32_t& qualifiedHeight,
    uint64_t frame, uint32_t width, uint32_t height) noexcept {
    producerFrame = frame;
    qualifiedWidth = width;
    qualifiedHeight = height;
}

inline void InvalidateHostTextureProducer(uint64_t& producerFrame, uint32_t& qualifiedWidth, uint32_t& qualifiedHeight) noexcept {
    producerFrame = ~0ull;
    qualifiedWidth = 0;
    qualifiedHeight = 0;
}

inline void OnDrawWriteHostTexture(uint64_t& producerFrame, uint32_t& qualifiedWidth, uint32_t& qualifiedHeight, uint32_t colorMask) noexcept {
    if ((colorMask & 7) != 0) {
        InvalidateHostTextureProducer(producerFrame, qualifiedWidth, qualifiedHeight);
    }
}

inline bool IsHostTextureQualified(uint64_t producerFrame, uint64_t frame) noexcept {
    return producerFrame != ~0ull && producerFrame == frame;
}

inline void MarkSurfaceResolved(uint64_t& sdrWriteOrdinal, uint64_t ordinal) noexcept {
    sdrWriteOrdinal = ordinal;
}

inline void InvalidateSurfaceResolved(uint64_t& sdrWriteOrdinal) noexcept {
    sdrWriteOrdinal = 0;
}

} // namespace gpu::color_qualification
