#pragma once
#include "xenos_translator.h"
#include "motion_replay_hlsl.h"
#include <string>

namespace xenos::fsr_alpha_replay {

// The original translated PS executes unchanged, including alpha-test and
// discard. Its oC0 output is routed to a separate single-channel attachment.
inline std::string Pixel(const TranslatedShader& ps) {
    if (!ps.isPixelShader || ps.writesDepth || !ps.errors.empty() ||
        (ps.colorTargetsWritten & 1u) == 0) return {};
    const auto entry = ps.hlsl.find("void main(\n");
    if (entry == std::string::npos) return {};
    std::string source = ps.hlsl.substr(0, entry);
    source += motion_replay::ReplaceToken(ps.hlsl.substr(entry), "main", "XeFsrAlphaOriginal");
    source += "\nfloat main(in float4 p : SV_Position";
    for (unsigned i = 0; i < 16; ++i)
        source += ", in float4 i" + std::to_string(i) + " : TEXCOORD" + std::to_string(i);
    source += ", in bool face : SV_IsFrontFace) : SV_Target0 {\n    float4 color0 = 0;\n";
    for (unsigned i = 1; i < 4; ++i)
        if (ps.colorTargetsWritten & (1u << i))
            source += "    float4 ignored" + std::to_string(i) + " = 0;\n";
    source += "    XeFsrAlphaOriginal(p";
    for (unsigned i = 0; i < 16; ++i) source += ", i" + std::to_string(i);
    source += ", face, color0";
    for (unsigned i = 1; i < 4; ++i)
        if (ps.colorTargetsWritten & (1u << i)) source += ", ignored" + std::to_string(i);
    source += ");\n    return saturate(color0.a);\n}\n";
    return source;
}

} // namespace xenos::fsr_alpha_replay
