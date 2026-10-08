#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace gpu::aspect_layout
{
inline constexpr float NativeWidth = 1280.0f;
inline constexpr float NativeHeight = 720.0f;
inline constexpr float NativeAspect = NativeWidth / NativeHeight;

struct Scale
{
    float x = 1.0f, y = 1.0f;
    bool IsIdentity() const { return x == 1.0f && y == 1.0f; }
};

// Widen the scene horizontally on wide displays and vertically on tall ones.
// Applying the same scale to a 2D canvas instead preserves its native shape
// inside a centred 16:9 safe area, without cropping its content.
inline Scale ForAspect(float aspect)
{
    if (!std::isfinite(aspect) || aspect <= 0.0f) return {};
    return aspect > NativeAspect ? Scale{NativeAspect / aspect, 1.0f}
                                 : Scale{1.0f, aspect / NativeAspect};
}

// The 3D view. narrowTall keeps the 16:9 height on a tall output and narrows
// the horizontal view instead, like a console's 4:3 output; UI and movies still
// use ForAspect.
inline Scale ForScene(float aspect, bool narrowTall)
{
    if (narrowTall && std::isfinite(aspect) && aspect > 0.0f && aspect < NativeAspect)
        return {NativeAspect / aspect, 1.0f};
    return ForAspect(aspect);
}

inline float FitBoundary(float value, float extent, float scale, float anchor = 0.5f)
{
    return (1.0f - scale) * extent * anchor + value * scale;
}

inline std::array<uint32_t, 4> FitScissor(std::array<uint32_t, 4> edges, Scale scale, float verticalAnchor = 0.5f)
{
    // FCanvas uses an empty rectangle to disable clipping, rather than to
    // discard all pixels. Preserve that sentinel, including nonzero edges.
    if (edges[0] == edges[2] || edges[1] == edges[3]) return edges;
    for (uint32_t edge = 0; edge < edges.size(); ++edge)
    {
        const bool vertical = (edge & 1) != 0;
        edges[edge] = uint32_t(std::lround(FitBoundary(float(edges[edge]),
            vertical ? NativeHeight : NativeWidth, vertical ? scale.y : scale.x, vertical ? verticalAnchor : 0.5f)));
    }
    // A clip narrower than a pixel after fitting must not become that sentinel.
    for (uint32_t axis = 0; axis < 2; ++axis)
        if (edges[axis] == edges[axis + 2])
        {
            if (edges[axis]) --edges[axis];
            else ++edges[axis + 2];
        }
    return edges;
}

struct Rect { float x, y, width, height; };

// Native Canvas quads outside the 16:9 content area. The ordinary centred fit
// maps their outer edges to the physical output edges, on either aspect axis.
inline std::array<Rect, 4> MenuBars(Scale scale)
{
    const float x = NativeWidth * (1.0f - scale.x) / (2.0f * scale.x);
    const float y = NativeHeight * (1.0f - scale.y) / (2.0f * scale.y);
    return {{{-x, 0, x, NativeHeight}, {NativeWidth, 0, x, NativeHeight},
             {0, -y, NativeWidth, y}, {0, NativeHeight, NativeWidth, y}}};
}

// The renderer retains the guest half-pixel offset (+1/1280, -1/720).
// Compose after the existing canvas transform, including its homogeneous
// column, so translated/rotated HUD stacks receive the same screen-space fit.
inline float CanvasComponent(float value, float homogeneous, float scale, float halfPixel, float ndcOffset = 0.0f)
{
    return scale * value + homogeneous * (ndcOffset - (1.0f - scale) * halfPixel);
}
}
