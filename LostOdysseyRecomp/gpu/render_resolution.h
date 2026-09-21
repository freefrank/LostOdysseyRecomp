#pragma once
#include <algorithm>
#include <cstdint>

namespace gpu::resolution {
struct Size {
    uint32_t width = 1280, height = 720;
    bool operator==(const Size&) const = default;
};
enum class TargetRole : uint8_t { Unknown, Scene, Fixed };
// The guest remains 1280x720.  Host scene targets follow a wider output
// horizontally (Hor+) while 16:9 and narrower outputs retain the old fit.
// A minimized/uninitialized output uses the native size, never a zero allocation.
inline constexpr Size ResolveInternalSize(uint32_t mode, uint32_t outputWidth, uint32_t outputHeight) {
    const bool wide = outputWidth && outputHeight && uint64_t(outputWidth) * 9 > uint64_t(outputHeight) * 16;
    const auto widthForHeight = [&](uint32_t height) {
        return wide ? uint32_t((uint64_t(height) * outputWidth + outputHeight / 2) / outputHeight)
                    : uint32_t((uint64_t(height) * 16) / 9);
    };
    switch (mode) {
    case 720: return {widthForHeight(720), 720};
    case 1080: return {widthForHeight(1080), 1080};
    case 1440: return {widthForHeight(1440), 1440};
    case 2160: return {widthForHeight(2160), 2160};
    default:
        if (!outputWidth || !outputHeight) return {};
        if (wide) {
            const uint32_t height = (std::clamp)(outputHeight, 1u, 2160u);
            return {widthForHeight(height), height};
        }
        const uint32_t units = (std::clamp)((std::min)(outputWidth / 16, outputHeight / 9), 1u, 240u);
        return {units * 16, units * 9};
    }
}
// Map boundaries, rather than independently rounding an origin and a width:
// adjacent rectangles then share precisely the same physical pixel boundary.
inline constexpr uint32_t Scale(uint32_t guest, uint32_t internalHeight) {
    return uint32_t((uint64_t(guest) * internalHeight + 360) / 720);
}
inline constexpr uint32_t ScaleX(uint32_t guest, uint32_t internalWidth) {
    return uint32_t((uint64_t(guest) * internalWidth + 640) / 1280);
}
inline constexpr Size TargetSize(uint32_t pitch, uint32_t height, Size internal) {
    // Square EDRAM surfaces include shadow maps and luminance reductions. Their
    // texture resolution is independent of the scene's output resolution.
    if (pitch == height || ScaleX(pitch, internal.width) > 16384 || Scale(height, internal.height) > 16384)
        return {};
    return internal;
}
// Catalogued scene surfaces use both dimensions of the CPU frame plan. Fixed
// surfaces retain their guest texels. Until the catalog proves a role, retain
// the historical isotropic height scale rather than widening by output aspect.
inline constexpr Size TargetSizeForRole(TargetRole role, uint32_t pitch, uint32_t height, Size plan) {
    switch (role) {
    case TargetRole::Scene: return plan;
    case TargetRole::Fixed: return {};
    case TargetRole::Unknown:
        return TargetSize(pitch, height, { uint32_t((uint64_t(plan.height) * 16) / 9), plan.height });
    }
    return {};
}
// A plan may carry a sub-720 official DLSS input. Only catalogued Scene targets
// receive that size; Fixed and Unknown retain their legacy mappings and never
// invent an aspect ratio from a rounded recommended input.
inline constexpr Size TargetSizeForPlan(TargetRole role, uint32_t pitch, uint32_t height, Size input, Size legacy) {
    return role == TargetRole::Scene ? input : TargetSizeForRole(role, pitch, height, legacy);
}
inline constexpr uint32_t TargetHeight(uint32_t pitch, uint32_t height, uint32_t internalHeight) {
    return TargetSize(pitch, height, {uint32_t((uint64_t(internalHeight) * 16) / 9), internalHeight}).height;
}
}
