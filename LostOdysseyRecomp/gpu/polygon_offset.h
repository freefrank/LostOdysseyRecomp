#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace gpu {
struct PolygonOffset {
    int32_t constant = 0;
    float slope = 0;
};

// Xenos face selection and D3D floating-depth approximation, as documented by
// Xenia draw_util::GetPreferredFacePolygonOffset/GetD3D10IntegerPolygonOffset.
// Plume exposes one bias for both faces. Prefer a visible front face, then back.
inline PolygonOffset GetPolygonOffset(uint32_t mode, bool polygonal, bool float24,
                                     float frontScale, float frontOffset,
                                     float backScale, float backOffset) {
    float scale = 0, offset = 0;
    if (polygonal) {
        if ((mode & (1u << 11)) && !(mode & 1)) {
            scale = frontScale;
            offset = frontOffset;
        }
        if ((mode & (1u << 12)) && !(mode & 2) && scale == 0 && offset == 0) {
            scale = backScale;
            offset = backOffset;
        }
    } else if (mode & (1u << 13)) {
        scale = frontScale;
        offset = frontOffset;
    }
    if (!std::isfinite(scale) || !std::isfinite(offset)) return {};

    // Scale registers use 1/16 subpixels. D24FS8 has three fewer mantissa
    // bits than host D32, so bias is rounded away from zero in groups of 8 ULPs.
    // UNORM24 on D32 uses the [0.5, 1) worst-case exponent approximation.
    const double unit = float24 ? 8.0 : 1.0;
    const double magnitude = std::ceil(double(std::abs(offset)) *
                                      (float24 ? 2097152.0 : 16777215.0)) * unit;
    const auto constant = int32_t(std::min(magnitude, double(std::numeric_limits<int32_t>::max())));
    return {offset < 0 ? -constant : constant, scale * (1.0f / 16.0f)};
}
} // namespace gpu
