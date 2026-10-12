#pragma once
#include <algorithm>
#include <cstdint>
#include <tuple>
#include <vector>

namespace gpu::renderer {
struct DepthClearRect { int32_t left, top, right, bottom; };

// Preserve the exact covered area while reducing EDRAM tile clear batches.
// Only equal-height horizontal spans and equal-width adjacent vertical spans
// can become one rectangle; a bounding box would incorrectly clear holes.
inline void CoalesceDepthClearRects(std::vector<DepthClearRect>& rects)
{
    rects.erase(std::remove_if(rects.begin(), rects.end(), [](const auto& r) {
        return r.left >= r.right || r.top >= r.bottom;
    }), rects.end());
    std::sort(rects.begin(), rects.end(), [](const auto& a, const auto& b) {
        return std::tie(a.top, a.bottom, a.left, a.right) < std::tie(b.top, b.bottom, b.left, b.right);
    });
    std::size_t count = 0;
    for (const auto r : rects) {
        if (count && rects[count - 1].top == r.top && rects[count - 1].bottom == r.bottom &&
            r.left <= rects[count - 1].right) {
            rects[count - 1].right = std::max(rects[count - 1].right, r.right);
        } else rects[count++] = r;
    }
    rects.resize(count);
    std::sort(rects.begin(), rects.end(), [](const auto& a, const auto& b) {
        return std::tie(a.left, a.right, a.top, a.bottom) < std::tie(b.left, b.right, b.top, b.bottom);
    });
    count = 0;
    for (const auto r : rects) {
        if (count && rects[count - 1].left == r.left && rects[count - 1].right == r.right &&
            rects[count - 1].bottom == r.top) {
            rects[count - 1].bottom = r.bottom;
        } else rects[count++] = r;
    }
    rects.resize(count);
}

// Map a rectangle between 32-bit depth views of the same EDRAM base.
// EDRAM tiles contain 80 x 16 samples; MSAA changes the pixel-to-sample
// scale, while a different pitch changes where each tile appears on screen.
// Replaces the contents of `result`, keeping its storage.
inline void MapDepthClear(std::vector<DepthClearRect>& result,
    uint32_t srcPitch, uint32_t srcMsaa, DepthClearRect area,
    uint32_t dstPitch, uint32_t dstHeight, uint32_t dstMsaa)
{
    result.clear();
    if (!srcPitch || !dstPitch || srcMsaa > 2 || dstMsaa > 2) return;
    const uint32_t sx = srcMsaa == 2 ? 2 : 1, sy = srcMsaa ? 2 : 1;
    const uint32_t dx = dstMsaa == 2 ? 2 : 1, dy = dstMsaa ? 2 : 1;
    const uint32_t srcTiles = (srcPitch * sx + 79) / 80;
    const uint32_t dstTiles = (dstPitch * dx + 79) / 80;
    const uint32_t x0 = uint32_t(std::clamp(area.left, 0, int32_t(srcPitch))) * sx;
    const uint32_t x1 = uint32_t(std::clamp(area.right, 0, int32_t(srcPitch))) * sx;
    const uint32_t y0 = uint32_t(std::max(0, area.top)) * sy;
    const uint32_t y1 = uint32_t(std::max(0, area.bottom)) * sy;
    for (uint32_t y = y0; y < y1; ) {
        const uint32_t endY = std::min(y1, (y / 16 + 1) * 16);
        for (uint32_t x = x0; x < x1; ) {
            const uint32_t endX = std::min(x1, (x / 80 + 1) * 80);
            const uint32_t tile = (y / 16) * srcTiles + x / 80;
            const uint32_t ox = (tile % dstTiles) * 80, oy = (tile / dstTiles) * 16;
            DepthClearRect r{
                int32_t((ox + x % 80) / dx), int32_t((oy + y % 16) / dy),
                int32_t((ox + x % 80 + endX - x + dx - 1) / dx),
                int32_t((oy + y % 16 + endY - y + dy - 1) / dy)};
            r.right = std::min(r.right, int32_t(dstPitch));
            r.bottom = std::min(r.bottom, int32_t(dstHeight));
            if (r.left < r.right && r.top < r.bottom) result.push_back(r);
            x = endX;
        }
        y = endY;
    }
}

inline std::vector<DepthClearRect> MapDepthClear(
    uint32_t srcPitch, uint32_t srcMsaa, DepthClearRect area,
    uint32_t dstPitch, uint32_t dstHeight, uint32_t dstMsaa)
{
    std::vector<DepthClearRect> result;
    MapDepthClear(result, srcPitch, srcMsaa, area, dstPitch, dstHeight, dstMsaa);
    return result;
}
}
