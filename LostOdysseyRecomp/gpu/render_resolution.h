#pragma once
#include <algorithm>
#include <cstdint>

namespace gpu::resolution {
struct Size {
    uint32_t width = 1280, height = 720;
    bool operator==(const Size&) const = default;
};
// Auto follows the largest integral 16:9 rectangle inside the output. A
// minimized/uninitialized output uses the native size, never a zero allocation.
inline constexpr Size ResolveInternalSize(uint32_t mode, uint32_t outputWidth, uint32_t outputHeight) {
    switch (mode) {
    case 720: return {1280, 720};
    case 1080: return {1920, 1080};
    case 1440: return {2560, 1440};
    case 2160: return {3840, 2160};
    default:
        if (!outputWidth || !outputHeight) return {};
        const uint32_t units = std::clamp(std::min(outputWidth / 16, outputHeight / 9), 1u, 240u);
        return {units * 16, units * 9};
    }
}
// Map boundaries, rather than independently rounding an origin and a width:
// adjacent rectangles then share precisely the same physical pixel boundary.
inline constexpr uint32_t Scale(uint32_t guest, uint32_t internalHeight) {
    return uint32_t((uint64_t(guest) * internalHeight + 360) / 720);
}
inline constexpr uint32_t TargetHeight(uint32_t pitch, uint32_t height, uint32_t internalHeight) {
    // Square EDRAM surfaces include shadow maps and luminance reductions. Their
    // texture resolution is independent of the scene's output resolution.
    if (pitch == height || Scale(pitch, internalHeight) > 16384 || Scale(height, internalHeight) > 16384)
        return 720;
    return internalHeight;
}
}
