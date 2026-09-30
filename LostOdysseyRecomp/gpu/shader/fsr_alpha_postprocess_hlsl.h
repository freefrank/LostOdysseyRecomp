#pragma once

#include "xenos_translator.h"
#include <array>
#include <string>
#include <string_view>

namespace xenos::fsr_alpha_postprocess {

inline std::string Pixel(const TranslatedShader& original, uint64_t psHash, uint32_t pointSlots) {
    if (!original.isPixelShader || original.writesDepth || !original.errors.empty()) return {};
    const auto entry = original.hlsl.find("void main(\n");
    if (entry == std::string::npos) return {};
    std::string source = original.hlsl.substr(0, entry);
    source += R"hlsl(
float XeFsrMaskTap(Texture2D<float4> mask, uint slot, float2 uv, bool pointSample) {
    if (pointSample) {
        uint width, height;
        mask.GetDimensions(width, height);
        int2 pixel = clamp(int2(floor(uv * float2(width, height))), int2(0, 0), int2(width, height) - 1);
        return mask.Load(int3(pixel, 0)).r;
    }
    uint width, height;
    mask.GetDimensions(width, height);
    float2 texel = uv * float2(width, height) - 0.5;
    int2 base = int2(floor(texel));
    float2 blend = frac(texel);
    float maximum = 0.0;
    [unroll] for (int y = 0; y < 2; ++y) {
        [unroll] for (int x = 0; x < 2; ++x) {
            float weight = (x == 0 ? 1.0 - blend.x : blend.x) *
                (y == 0 ? 1.0 - blend.y : blend.y);
            if (weight > 0.0) {
                int2 pixel = clamp(base + int2(x, y), int2(0, 0), int2(width, height) - 1);
                maximum = max(maximum, mask.Load(int3(pixel, 0)).r);
            }
        }
    }
    return maximum;
}
)hlsl";
    // The original translated VS exports TEXCOORD0..15. Keep the same input
    // interface so interpolation, clip positions and constant-derived UV taps
    // are identical to the color draw.
    source += "float main(float4 p : SV_Position";
    for (unsigned i = 0; i < 16; ++i)
        source += ", float4 i" + std::to_string(i) + " : TEXCOORD" + std::to_string(i);
    source += ") : SV_Target0 {\n    float m = 0.0;\n";
    const auto tap = [&](unsigned slot, std::string_view uv) {
        source += "    m = max(m, XeFsrMaskTap(tex2D_" + std::to_string(slot) + ", " +
            std::to_string(slot) + "u, " + std::string(uv) + ", " +
            ((pointSlots & (1u << slot)) ? "true" : "false") + "));\n";
    };
    static constexpr std::array<std::string_view, 9> nine = {
        "i3.wz", "i0.xy", "i0.wz", "i1.xy", "i1.wz",
        "i2.xy", "i2.wz", "i3.xy", "i4.xy"};
    if (psHash == 0x7c260eacff1d681dull) {
        for (const auto uv : nine) tap(0, uv);
    } else if (psHash == 0x53dd5d081c7945cfull) {
        for (const auto uv : nine)
            tap(0, "clamp(" + std::string(uv) + ", XeConst(10).xy, XeConst(10).zw)");
    } else if (psHash == 0xee90000c755c0472ull) {
        for (unsigned i = 0; i < 8; ++i) {
            tap(0, "i" + std::to_string(i) + ".xy");
            tap(0, "i" + std::to_string(i) + ".wz");
        }
    } else if (psHash == 0xb4b4d54a7a2d6b96ull) {
        source += R"hlsl(
    float depthValue = XeTextureResult(XeTex2D(tex2D_1, XeSampler(1u), i1.xy,
        float2(0, 0), 1u, false), 1u).x;
    float z = clamp(rcp(depthValue * XeConst(0).z - XeConst(0).w), FLT_MIN, FLT_MAX);
    float distance = -XeConst(1).x + z;
    bool nearSide = -distance > 0.0;
    float exponent = nearSide ? XeConst(4).x : XeConst(4).y;
    float scale = nearSide ? XeConst(5).x : XeConst(5).y;
    float2 interval = saturate(float2(-distance - XeConst(2).x, distance - XeConst(2).y) *
        float2(clamp(rcp(XeConst(3).x), FLT_MIN, FLT_MAX),
               clamp(rcp(XeConst(3).y), FLT_MIN, FLT_MAX)));
    float curve = nearSide ? interval.x : interval.y;
    float w = exp2(clamp(log2(abs(curve)), FLT_MIN, FLT_MAX) * exponent) * scale;
    bool dof = w > XeConst(255).x;
)hlsl";
        source += "    if (!dof || XeConst(255).y - w != 0.0) m = max(m, XeFsrMaskTap(tex2D_0, 0u, i1.xy, " +
            std::string((pointSlots & 1u) ? "true" : "false") + "));\n";
        source += "    if (dof && any(w * XeConst(6).xyz != 0.0)) m = max(m, XeFsrMaskTap(tex2D_2, 2u, i0.xy, " +
            std::string((pointSlots & 4u) ? "true" : "false") + "));\n";
        source += "    if (XeConst(7).x != 0.0) m = max(m, XeFsrMaskTap(tex2D_3, 3u, i0.xy, " +
            std::string((pointSlots & 8u) ? "true" : "false") + "));\n";
    } else return {};
    source += "    return saturate(m);\n}\n";
    return source;
}

inline constexpr const char* AreaMaximum = R"hlsl(
Texture2D<float4> sourceMask : register(t0, space1);
float main(float4 position : SV_Position) : SV_Target0 {
    uint width, height;
    sourceMask.GetDimensions(width, height);
    float2 scale = float2(width, height) / float2(1280.0, 720.0);
    float2 lower = floor(position.xy) * scale;
    float2 upper = lower + scale;
    int2 first = int2(floor(lower));
    int2 last = int2(ceil(upper));
    float maximum = 0.0;
    [loop] for (int y = first.y; y < last.y; ++y) {
        float wy = max(0.0, min(upper.y, float(y + 1)) - max(lower.y, float(y)));
        [loop] for (int x = first.x; x < last.x; ++x) {
            float wx = max(0.0, min(upper.x, float(x + 1)) - max(lower.x, float(x)));
            if (wx * wy > 0.0)
                maximum = max(maximum, sourceMask.Load(int3(clamp(int2(x, y),
                    int2(0, 0), int2(width, height) - 1), 0)).r);
        }
    }
    return maximum;
}
)hlsl";

} // namespace xenos::fsr_alpha_postprocess
