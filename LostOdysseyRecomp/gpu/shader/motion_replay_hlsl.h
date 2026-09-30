#pragma once
#include "xenos_translator.h"
#include <cctype>
#include <string>
#include <string_view>

namespace xenos::motion_replay {
// This adapter replays the translated program itself. It does not guess a world
// matrix, bone palette, VP slot or vertex input format from a shader hash.
inline std::string ReplaceToken(std::string text, std::string_view token, std::string_view replacement) {
    size_t p = 0;
    auto word = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; };
    while ((p = text.find(token, p)) != std::string::npos) {
        if ((p && word(text[p - 1])) || (p + token.size() < text.size() && word(text[p + token.size()]))) { p += token.size(); continue; }
        text.replace(p, token.size(), replacement); p += replacement.size();
    }
    return text;
}
inline std::string ExtendPushConstants(std::string source) {
    const std::string needle = "    uint64_t PixelShaderConstants;";
    const auto at = source.find(needle);
    if (at == std::string::npos) return {};
    source.insert(at + needle.size(), "\n    uint64_t MotionHistory;");
    return source;
}
inline constexpr const char* kPreviousBindings = R"HLSL(
#ifndef __spirv__
cbuffer XeMotionHistory : register(b3, space0) {
    float4 xeMvConstants[256];
    uint4 xeMvShared[13];
    float4 xeMvDimensions;
    uint4 xeMvMetadata;
};
#endif
float4 XeMvConst(int i) {
#ifdef __spirv__
    return vk::RawBufferLoad<float4>(xePush.MotionHistory + uint64_t(clamp(i, 0, 255)) * 16);
#else
    return xeMvConstants[clamp(i, 0, 255)];
#endif
}
uint XeMvSharedWord(uint i) {
#ifdef __spirv__
    return vk::RawBufferLoad<uint>(xePush.MotionHistory + 4096 + uint64_t(i) * 4);
#else
    return xeMvShared[i >> 2][i & 3];
#endif
}
float4 XeMvSharedVector(uint i) {
    return asfloat(uint4(XeMvSharedWord(i), XeMvSharedWord(i+1), XeMvSharedWord(i+2), XeMvSharedWord(i+3)));
}
bool XeMvBool(uint i) { return ((XeMvSharedWord((i >> 5) & 7) >> (i & 31)) & 1) != 0; }
uint XeMvLoopConst(uint i) { return XeMvSharedWord(8 + (i & 31)); }
float4 XeMvSize() {
#ifdef __spirv__
    return vk::RawBufferLoad<float4>(xePush.MotionHistory + 4304);
#else
    return xeMvDimensions;
#endif
}
uint4 XeMvMeta() {
#ifdef __spirv__
    return vk::RawBufferLoad<uint4>(xePush.MotionHistory + 4320);
#else
    return xeMvMetadata;
#endif
}
#define xeMvNdcScale (XeMvSharedVector(40))
#define xeMvNdcOffset (XeMvSharedVector(44))
#define xeMvHalfPixelOffset (asfloat(uint2(XeMvSharedWord(48), XeMvSharedWord(49))))
#define xeMvVtxFmt (XeMvSharedWord(50))
#define xeMvFlags (XeMvSharedWord(51))
)HLSL";

inline std::string Vertex(const TranslatedShader& vs) {
    if (vs.isPixelShader || !vs.errors.empty()) return {};
    const auto entry = vs.hlsl.find("void main(\n");
    if (entry == std::string::npos) return {};
    auto prefix = ExtendPushConstants(vs.hlsl.substr(0, entry));
    if (prefix.empty()) return {};
    auto current = ReplaceToken(vs.hlsl.substr(entry), "main", "XeMvCurrent");
    auto previous = ReplaceToken(vs.hlsl.substr(entry), "main", "XeMvPrevious");
    for (const char* s : {"XeConst", "XeBool", "XeLoopConst"}) previous = ReplaceToken(previous, s, std::string("XeMv") + (s + 2));
    for (const char* s : {"xeNdcScale", "xeNdcOffset", "xeHalfPixelOffset", "xeVtxFmt", "xeFlags"}) previous = ReplaceToken(previous, s, std::string("xeMv") + (s + 2));
    std::string wrapper = "\nvoid main(in uint id : SV_VertexID, out precise float4 position : SV_Position";
    for (unsigned i = 0; i < 16; ++i) wrapper += ", out float4 o" + std::to_string(i) + " : TEXCOORD" + std::to_string(i);
    wrapper += ", out float4 previousClip : TEXCOORD16, out float4 currentClip : TEXCOORD17, out XePointSizeOutput xePointSizeOut) {\n";
    wrapper += "    XeMvCurrent(id, position";
    for (unsigned i = 0; i < 16; ++i) wrapper += ", o" + std::to_string(i);
    wrapper += ", xePointSizeOut);\n    currentClip = position; previousClip = 0;\n    if (XeMvMeta().x != 0) {\n";
    for (unsigned i = 0; i < 16; ++i) wrapper += "        float4 ignored" + std::to_string(i) + ";\n";
    wrapper += "        XePointSizeOutput ignoredPointSize;\n        XeMvPrevious(id, previousClip";
    for (unsigned i = 0; i < 16; ++i) wrapper += ", ignored" + std::to_string(i);
    wrapper += ", ignoredPointSize);\n    }\n}\n";
    return prefix + kPreviousBindings + current + previous + wrapper;
}

inline std::string Pixel(const TranslatedShader* ps) {
    if (ps && (!ps->isPixelShader || ps->writesDepth || !ps->errors.empty())) return {};
    std::string prefix, coverage;
    if (ps) {
        const auto entry = ps->hlsl.find("void main(\n");
        if (entry == std::string::npos) return {};
        prefix = ExtendPushConstants(ps->hlsl.substr(0, entry));
        coverage = ReplaceToken(ps->hlsl.substr(entry), "main", "XeMvCoverage");
    } else prefix = ExtendPushConstants(std::string(
        "#define XE_PIXEL_SHADER 1\n"
        "#define XE_SAMPLE(t, s, uv) t.Sample(s, uv)\n") + GetShaderCommonHlsl());
    if (prefix.empty()) return {};
    std::string wrapper = "\nstruct MvOutput { float2 velocity : SV_Target0; float2 depths : SV_Target1; uint tag : SV_Target2; };\n";
    wrapper += "MvOutput main(in float4 p : SV_Position";
    for (unsigned i = 0; i < 16; ++i) wrapper += ", in float4 i" + std::to_string(i) + " : TEXCOORD" + std::to_string(i);
    // D3D12 links interpolants by packed signature register. Keep the two
    // appended clips adjacent to TEXCOORD0..15; SV_IsFrontFace consumes its own
    // input register and would otherwise shift TEXCOORD16/17 past the VS slots.
    wrapper += ", in float4 previousClip : TEXCOORD16, in float4 currentClip : TEXCOORD17, in bool face : SV_IsFrontFace) {\n";
    if (ps) {
        const auto targets = ps->colorTargetsWritten ? ps->colorTargetsWritten : 1;
        for (unsigned i = 0; i < 4; ++i) if (targets & (1u << i)) wrapper += "    float4 ignored" + std::to_string(i) + ";\n";
        wrapper += "    XeMvCoverage(p";
        for (unsigned i = 0; i < 16; ++i) wrapper += ", i" + std::to_string(i);
        wrapper += ", face";
        for (unsigned i = 0; i < 4; ++i) if (targets & (1u << i)) wrapper += ", ignored" + std::to_string(i);
        wrapper += ");\n";
    }
    // Keep the original pixel shader's discard/alpha test; DXC eliminates unused shading.
    wrapper += R"HLSL(
    MvOutput o = (MvOutput)0;
    o.depths.x = p.z;
    uint4 meta = XeMvMeta();
    if (meta.x == 0 || !all(isfinite(previousClip)) || !all(isfinite(currentClip)) ||
        previousClip.w <= 1e-6 || currentClip.w <= 1e-6) return o;
    float2 extent = XeMvSize().xy;
    float2 previousPixel = (previousClip.xy / previousClip.w * float2(.5, -.5) + .5) * extent;
    float2 currentPixel = (currentClip.xy / currentClip.w * float2(.5, -.5) + .5) * extent;
    // Current program was rasterized with jitter; PREVIOUS constants were saved
    // before jitter. Removing current jitter exactly once gives geometric motion.
    o.velocity = previousPixel - currentPixel + asfloat(meta.yz);
    o.depths.y = previousClip.z / previousClip.w; // host reverse depth, already converted by original epilogue
    if (!all(isfinite(o.velocity)) || !isfinite(o.depths.y) || o.depths.y <= 0 || o.depths.y > 1) {
        o.velocity = 0; o.depths.y = 0; return o;
    }
    // Identical unjittered inputs prove zero geometric displacement. Avoid the
    // rounding residue of interpolated clip subtraction plus jitter cancellation.
    // Coverage and finite/depth checks above still control validity.
    if ((meta.w & 1u) != 0) o.velocity = float2(0.0, 0.0);
    o.tag = meta.x;
    return o;
}
)HLSL";
    return prefix + kPreviousBindings + coverage + wrapper;
}
}
