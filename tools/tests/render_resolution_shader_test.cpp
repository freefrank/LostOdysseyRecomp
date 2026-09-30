// Compile real translated 2D texture instructions; no GPU or game data required.
#include <gpu/shader/xenos_translator.h>
#include <gpu/shader/xenos_shader_code.h>
#include <gpu/shader/dxc_compiler.h>
#include <gpu/shader/motion_replay_hlsl.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

static bool WritesSpirvPointSize(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < 20 || bytes.size() % 4) return false;
    std::vector<uint32_t> words(bytes.size() / 4);
    std::memcpy(words.data(), bytes.data(), bytes.size());
    if (words[0] != 0x07230203) return false;
    uint32_t pointSizeId = 0;
    for (size_t i = 5; i < words.size();) {
        const uint32_t count = words[i] >> 16, opcode = words[i] & 0xffff;
        if (!count || i + count > words.size()) return false;
        if (opcode == 71 && count >= 4 && words[i + 2] == 11 && words[i + 3] == 1)
            pointSizeId = words[i + 1]; // OpDecorate BuiltIn PointSize
        i += count;
    }
    if (!pointSizeId) return false;
    for (size_t i = 5; i < words.size();) {
        const uint32_t count = words[i] >> 16, opcode = words[i] & 0xffff;
        if (opcode == 62 && count >= 3 && words[i + 1] == pointSizeId) return true; // OpStore
        i += count;
    }
    return false;
}

int main() {
    try {
        using namespace xenos;
        unsigned checks = 0;
        for (bool pixel : {false, true}) for (bool denormalized : {false, true})
        for (bool weights : {false, true}) for (bool implicitLod : {false, true}) {
            std::array<uint32_t, 6> code{};
            ControlFlowExecInstruction cf{};
            cf.address = 1; cf.count = 1; cf.sequence = 1; cf.opcode = ControlFlowOpcode::ExecEnd;
            std::memcpy(code.data(), &cf, 6);
            TextureFetchInstruction fetch{};
            fetch.opcode = weights ? FetchOpcode::GetTextureWeights : FetchOpcode::TextureFetch;
            fetch.dimension = TextureDimension::Texture2D;
            fetch.constIndex = 31; fetch.dstSwizzle = 0x688; fetch.srcSwizzle = 4;
            fetch.offsetX = 3; fetch.offsetY = -1; fetch.texCoordDenorm = denormalized;
            fetch.useCompLod = implicitLod;
            std::memcpy(code.data() + 3, &fetch, 12);
            auto translated = TranslateShader(code.data(), uint32_t(code.size()), pixel);
            if (!translated.errors.empty()) throw std::runtime_error(translated.errors);
            const std::string callEnd = std::string("float2(1.5, -0.5), 31u, ") + (denormalized ? "true)" : "false)");
            if (translated.hlsl.find(callEnd) == std::string::npos)
                throw std::runtime_error("missing slot, signed guest texel offset or coordinate mode in translated fetch");
            // Keep the sample live so DXC must validate its resource binding and
            // helper contract, not only a dead instruction in an unused register.
            if (pixel) {
                const auto at = translated.hlsl.find("oC0 = clamp(");
                if (at == std::string::npos) throw std::runtime_error("missing pixel epilogue");
                translated.hlsl.insert(at, "oC0 = r0;\n\t");
            }
            auto compiled = CompileHlsl(translated.hlsl, "main", pixel ? "ps_6_0" : "vs_6_0");
            if (!compiled.ok) throw std::runtime_error(compiled.errors);
            ++checks;
        }
        for (bool guestExportsPointSize : {false, true}) {
            std::array<uint32_t, 6> code{};
            ControlFlowExecInstruction cf{};
            cf.address = 1; cf.count = 1; cf.opcode = ControlFlowOpcode::ExecEnd;
            std::memcpy(code.data(), &cf, 6);
            AluInstruction alu{};
            alu.exportData = 1;
            alu.vectorDest = uint32_t(guestExportsPointSize ? ExportRegister::VSPointSizeEdgeFlagKillVertex : ExportRegister::VSPosition);
            alu.vectorWriteMask = 1;
            alu.vectorOpcode = AluVectorOpcode::Add;
            alu.scalarOpcode = AluScalarOpcode::RetainPrev;
            std::memcpy(code.data() + 3, &alu, 12);
            auto translated = TranslateShader(code.data(), uint32_t(code.size()), false);
            if (!translated.errors.empty() || translated.usesPointSize != guestExportsPointSize)
                throw std::runtime_error("point size export classification failed");
            for (auto format : {ShaderBinaryFormat::Spirv, ShaderBinaryFormat::Dxil}) {
                auto compiled = CompileHlsl(translated.hlsl, "main", "vs_6_0", format);
                if (!compiled.ok) throw std::runtime_error(compiled.errors);
                if (format == ShaderBinaryFormat::Spirv && !WritesSpirvPointSize(compiled.bytecode))
                    throw std::runtime_error("vertex shader does not write SPIR-V PointSize");
                ++checks;
                if (!guestExportsPointSize) {
                    const auto replay = motion_replay::Vertex(translated);
                    if (replay.empty()) throw std::runtime_error("motion replay vertex wrapper missing");
                    auto replayCompiled = CompileHlsl(replay, "main", "vs_6_0", format);
                    if (!replayCompiled.ok) throw std::runtime_error(replayCompiled.errors);
                    if (format == ShaderBinaryFormat::Spirv && !WritesSpirvPointSize(replayCompiled.bytecode))
                        throw std::runtime_error("motion replay vertex does not write SPIR-V PointSize");
                    ++checks;
                }
            }
        }
        std::printf("render resolution shader: %u translation and compilation checks passed\n", checks);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
