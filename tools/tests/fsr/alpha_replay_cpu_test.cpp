#include "gpu/fsr_alpha_mask_policy.h"
#include "gpu/shader/fsr_alpha_replay_hlsl.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

int main(int argc, char** argv) {
    using gpu::fsr_alpha::AuditedPair;
    using gpu::fsr_alpha::AuditedState;
    constexpr std::array<std::pair<uint64_t, uint64_t>, 6> pairs = {{
        {0x22557143e0f243ddull, 0x549c25501ca08530ull},
        {0x22557143e0f243ddull, 0x62674247668b2cfbull},
        {0x22557143e0f243ddull, 0x69865680ed1eff11ull},
        {0x4c87bb5b986defc8ull, 0x819898a6ae24db7bull},
        {0x4c87bb5b986defc8ull, 0x8ba62e23bbf0febeull},
        {0x4c87bb5b986defc8ull, 0x670b45e31df82273ull},
    }};
    for (auto [vs, ps] : pairs) if (!AuditedPair(vs, ps)) return 1;
    if (AuditedPair(0x0eb223d33f8e8e0cull, 0x463f83252b104180ull) ||
        AuditedPair(pairs[0].second, pairs[0].first) ||
        !AuditedState(0x01000106u, 0x00700762u, 0x00010000u, 0xFu, 13u) ||
        !AuditedState(0x01000106u, 0x00700762u, 0x000105a5u, 0xFu, 13u) ||
        AuditedState(0x01000106u, 0x00700762u, 0x000005a5u, 0xFu, 13u) ||
        AuditedState(0x01000106u, 0x00700766u, 0x00010000u, 0xFu, 13u)) return 2;

    xenos::TranslatedShader ps{};
    ps.isPixelShader = true;
    ps.colorTargetsWritten = 1;
    ps.hlsl = "float4 Toy(float4 c) { if (c.x < 0) discard; return c; }\n"
        "void main(\n in float4 p : SV_Position, out float4 oC0 : SV_Target0) {\n"
        " oC0 = Toy(p);\n}\n";
    const std::string wrapped = xenos::fsr_alpha_replay::Pixel(ps);
    if (wrapped.find("XeFsrAlphaOriginal(p") == std::string::npos ||
        wrapped.find("return saturate(color0.a)") == std::string::npos ||
        wrapped.find("discard") == std::string::npos ||
        wrapped.find("float main(") == std::string::npos ||
        wrapped.find("void XeFsrAlphaOriginal(") == std::string::npos) return 3;
    ps.colorTargetsWritten = 2; // no oC0: cannot infer opacity from another MRT
    if (!xenos::fsr_alpha_replay::Pixel(ps).empty()) return 4;
    ps.colorTargetsWritten = 1; ps.writesDepth = true;
    if (!xenos::fsr_alpha_replay::Pixel(ps).empty()) return 5;
    if (argc == 3) {
        const std::filesystem::path input = argv[1], output = argv[2];
        std::filesystem::create_directories(output);
        for (auto [vs, hash] : pairs) {
            (void)vs;
            const std::string name = [&] {
                constexpr char hex[] = "0123456789abcdef";
                std::string text(16, '0');
                for (unsigned i = 0; i < 16; ++i)
                    text[i] = hex[(hash >> ((15 - i) * 4)) & 15];
                return text;
            }();
            std::ifstream file(input / (name + ".hlsl"));
            if (!file) return 6;
            ps = {}; ps.isPixelShader = true; ps.colorTargetsWritten = 1;
            ps.hlsl.assign(std::istreambuf_iterator<char>(file), {});
            const auto source = xenos::fsr_alpha_replay::Pixel(ps);
            if (source.empty()) return 7;
            std::ofstream generated(output / (name + ".hlsl"));
            generated << source;
            if (!generated) return 8;
        }
    } else if (argc != 1) return 9;
    return 0;
}
