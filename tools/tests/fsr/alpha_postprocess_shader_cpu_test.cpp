#include "gpu/fsr_alpha_propagation_policy.h"
#include "gpu/shader/fsr_alpha_postprocess_hlsl.h"
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

int main(int argc, char** argv) {
    if (argc != 3) return 1;
    const std::filesystem::path sourceDir = argv[1], outputDir = argv[2];
    std::filesystem::create_directories(outputDir);
    constexpr std::array<uint64_t, 4> hashes = {
        0x7c260eacff1d681dull, 0x53dd5d081c7945cfull,
        0xee90000c755c0472ull, 0xb4b4d54a7a2d6b96ull};
    for (const uint64_t hash : hashes) {
        char name[17];
        std::snprintf(name, sizeof(name), "%016llx", static_cast<unsigned long long>(hash));
        std::ifstream input(sourceDir / (std::string(name) + ".hlsl"));
        if (!input) return 2;
        xenos::TranslatedShader shader{};
        shader.isPixelShader = true;
        shader.colorTargetsWritten = 1;
        shader.hlsl.assign(std::istreambuf_iterator<char>(input), {});
        if (shader.hlsl.empty()) return 3;
        const auto generated = xenos::fsr_alpha_postprocess::Pixel(shader, hash, 0);
        if (generated.empty() || generated.find("XeFsrMaskTap") == std::string::npos) return 4;
        std::ofstream output(outputDir / (std::string(name) + ".hlsl"));
        output << generated;
        if (!output) return 5;
    }
    std::ofstream area(outputDir / "host-area-maximum.hlsl");
    area << xenos::fsr_alpha_postprocess::AreaMaximum;
    if (!area) return 7;
    using gpu::fsr_alpha::LOD0ClampFootprint;
    using gpu::fsr_alpha::SamplerFootprint;
    if (LOD0ClampFootprint(0x485u, true) != SamplerFootprint::Linear ||
        LOD0ClampFootprint(0x000u, true) != SamplerFootprint::Unsupported ||
        LOD0ClampFootprint(0x485u, false) != SamplerFootprint::Unsupported) return 6;
    return 0;
}
