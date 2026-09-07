// Compile real translated 2D texture instructions; no GPU or game data required.
#include <gpu/shader/xenos_translator.h>
#include <gpu/shader/xenos_shader_code.h>
#include <gpu/shader/dxc_compiler.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

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
        std::printf("render resolution shader: %u translated fetch/weights programs compiled\n", checks);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
