#pragma once
#include "xenos_translator.h"
#include <cstdint>
#include <string>
#include <vector>

namespace xenos::rect_list {
// Xenos rect lists draw a rectangle per three vertices; the hardware derives
// the fourth corner. Instead of a geometry shader (Metal has none, and it is
// slow on many desktop GPUs), each rectangle becomes six host indices. Each
// index encodes the rectangle's first guest vertex and the corner:
// firstVertex << kCornerBits | corner. Every invocation runs the translated
// program for all three guest vertices. Reuse the two shared corners so the
// post-transform cache can shade four vertices rather than six, without
// changing either triangle or adding resources/bindings. Reuse is a driver
// optimization, not a guarantee of four invocations.
inline constexpr uint32_t kCornerBits = 3;
inline constexpr uint32_t kVerticesPerRect = 6;
inline constexpr uint32_t kTriangleCorners[kVerticesPerRect] = {0, 1, 2, 2, 1, 5};
inline constexpr uint32_t kMaxFirstVertex = (UINT32_MAX >> kCornerBits) - 2;

inline constexpr const char* kCornerHelper = R"HLSL(
// The right-angle corner is the vertex opposite the longest edge. It is
// rotated to the front (keeps the winding), and the fourth vertex is the sum
// of its neighbours minus the corner. Corners 0-5 form the triangles
// (a, b, d) and (d, b, a'), the same as a strip a, b, d, a'.
uint XeRectListCorner(float4 v0, float4 v1, float4 v2)
{
    float2 p0 = v0.xy / v0.w, p1 = v1.xy / v1.w, p2 = v2.xy / v2.w;
    float e0 = dot(p1 - p2, p1 - p2);
    float e1 = dot(p2 - p0, p2 - p0);
    float e2 = dot(p0 - p1, p0 - p1);
    return (e0 >= e1 && e0 >= e2) ? 0u : (e1 >= e2 ? 1u : 2u);
}
float4 XeRectListPick(float4 v0, float4 v1, float4 v2, uint c, uint corner)
{
    float4 a = c == 0u ? v0 : (c == 1u ? v1 : v2);
    float4 b = c == 0u ? v1 : (c == 1u ? v2 : v0);
    float4 d = c == 0u ? v2 : (c == 1u ? v0 : v1);
    if (corner == 0u) return a;
    if (corner == 1u || corner == 4u) return b;
    if (corner == 2u || corner == 3u) return d;
    return b + d - a;
}
)HLSL";

// Builds the six encoded indices per rectangle. baseVertex is folded into the
// encoding, so the draw must use a base vertex of 0. Indexed rectangles are
// supported when their three indices are consecutive; any other rectangle,
// or one past the encodable range, is skipped and counted.
inline uint32_t ExpandIndices(std::vector<uint32_t>& indices, std::vector<uint32_t>& scratch,
    bool& useIndices, uint32_t indexCount, uint32_t baseVertex, bool reuseCorners = true) {
    auto& out = scratch;
    out.clear();
    const uint32_t rects = (useIndices ? uint32_t(indices.size()) : indexCount) / 3;
    out.reserve(size_t(rects) * kVerticesPerRect);
    uint32_t skipped = 0;
    for (uint32_t r = 0; r < rects; r++) {
        const uint32_t v0 = useIndices ? indices[r * 3] : r * 3;
        if (useIndices && (indices[r * 3 + 1] != v0 + 1 || indices[r * 3 + 2] != v0 + 2)) { skipped++; continue; }
        const uint32_t first = v0 + baseVertex;
        if (first > kMaxFirstVertex) { skipped++; continue; }
        for (uint32_t corner = 0; corner < kVerticesPerRect; corner++)
            out.push_back((first << kCornerBits) | (reuseCorners ? kTriangleCorners[corner] : corner));
    }
    indices.swap(out);
    useIndices = true;
    return skipped;
}

inline std::string Vertex(const TranslatedShader& vs) {
    if (vs.isPixelShader) return {};
    const auto entry = vs.hlsl.find("void main(\n");
    if (entry == std::string::npos) return {};
    std::string source = vs.hlsl;
    source.replace(entry, 10, "void XeRectListVertex(");
    source += kCornerHelper;
    std::string wrapper = "\nvoid main(in uint xeRectVertexId : SV_VertexID, out precise float4 oPos : SV_Position";
    for (unsigned i = 0; i < 16; ++i) wrapper += ", out float4 o" + std::to_string(i) + " : TEXCOORD" + std::to_string(i);
    wrapper += ", out XePointSizeOutput xePointSizeOut) {\n";
    wrapper += "    uint first = xeRectVertexId >> " + std::to_string(kCornerBits) + "u;\n";
    wrapper += "    uint corner = xeRectVertexId & " + std::to_string((1u << kCornerBits) - 1) + "u;\n";
    for (unsigned v = 0; v < 3; ++v) {
        const auto n = std::to_string(v);
        wrapper += "    float4 p" + n + ";\n";
        for (unsigned i = 0; i < 16; ++i) wrapper += "    float4 t" + n + "_" + std::to_string(i) + ";\n";
        wrapper += "    XePointSizeOutput s" + n + ";\n";
        wrapper += "    XeRectListVertex(first + " + n + "u, p" + n;
        for (unsigned i = 0; i < 16; ++i) wrapper += ", t" + n + "_" + std::to_string(i);
        wrapper += ", s" + n + ");\n";
    }
    wrapper += "    uint c = XeRectListCorner(p0, p1, p2);\n";
    wrapper += "    oPos = XeRectListPick(p0, p1, p2, c, corner);\n";
    for (unsigned i = 0; i < 16; ++i) {
        const auto n = std::to_string(i);
        wrapper += "    o" + n + " = XeRectListPick(t0_" + n + ", t1_" + n + ", t2_" + n + ", c, corner);\n";
    }
    wrapper += "    xePointSizeOut = s0;\n}\n";
    return source + wrapper;
}
}
