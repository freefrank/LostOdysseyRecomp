// Offline check for the Xenos shader translator: translates dumped microcode
// (vs_*.bin / ps_*.bin written by LO_SHADER_DUMP_DIR, big-endian dwords) to
// HLSL and compiles it with DXC.
//
// Usage: LoShaderTool <shader.bin | directory> [--print] [--out <dir>]

#include <gpu/shader/xenos_translator.h>
#include <gpu/shader/dxc_compiler.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static uint32_t ByteSwap32(uint32_t v)
{
    return (v >> 24) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) | (v << 24);
}

static bool ProcessFile(const fs::path& path, bool print, const fs::path& outDir, int& failures)
{
    std::ifstream in(path, std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (bytes.size() < 12 || (bytes.size() % 4) != 0)
    {
        printf("%s: bad size %zu\n", path.string().c_str(), bytes.size());
        return false;
    }

    std::vector<uint32_t> dwords(bytes.size() / 4);
    for (size_t i = 0; i < dwords.size(); i++)
    {
        uint32_t v;
        memcpy(&v, &bytes[i * 4], 4);
        dwords[i] = ByteSwap32(v);
    }

    std::string name = path.filename().string();
    bool isPixel = name.rfind("ps_", 0) == 0;
    xenos::TranslatedShader translated = xenos::TranslateShader(dwords.data(), uint32_t(dwords.size()), isPixel);

    if (print)
        printf("---- %s ----\n%s\n", name.c_str(), translated.hlsl.c_str());
    if (!outDir.empty())
    {
        fs::create_directories(outDir);
        std::ofstream(outDir / (path.stem().string() + ".hlsl")) << translated.hlsl;
    }

    xenos::CompiledShader compiled = xenos::CompileHlsl(translated.hlsl, "main", isPixel ? "ps_6_0" : "vs_6_0");
    printf("%-28s %4zu dwords  %s  dxil=%zu bytes  vfetch=%016llx tex=%08x%s\n", name.c_str(), dwords.size(),
        compiled.ok ? "OK  " : "FAIL", compiled.dxil.size(),
        (unsigned long long)translated.vertexFetchSlotMask[0], translated.textureSlotMask,
        translated.errors.empty() ? "" : "  notes!");
    if (!translated.errors.empty())
        printf("    notes: %s", translated.errors.c_str());
    if (!compiled.ok)
    {
        failures++;
        printf("%s\n", compiled.errors.c_str());
        if (!print)
        {
            // Print the source with line numbers for the failing shader.
            size_t line = 1, pos = 0;
            while (pos < translated.hlsl.size())
            {
                size_t end = translated.hlsl.find('\n', pos);
                if (end == std::string::npos) end = translated.hlsl.size();
                printf("%4zu  %s\n", line++, translated.hlsl.substr(pos, end - pos).c_str());
                pos = end + 1;
            }
        }
    }
    return compiled.ok;
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        printf("Usage: LoShaderTool <shader.bin | directory> [--print] [--out <dir>]\n");
        return 1;
    }
    bool print = false;
    fs::path outDir;
    for (int i = 2; i < argc; i++)
    {
        if (strcmp(argv[i], "--print") == 0) print = true;
        else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) outDir = argv[++i];
    }

    if (!xenos::DxcAvailable())
        printf("warning: dxcompiler.dll not found, only translating\n");

    int failures = 0, total = 0;
    fs::path input = argv[1];
    if (fs::is_directory(input))
    {
        for (auto& entry : fs::directory_iterator(input))
        {
            if (entry.path().extension() == ".bin")
            {
                total++;
                ProcessFile(entry.path(), print, outDir, failures);
            }
        }
    }
    else
    {
        total++;
        ProcessFile(input, print, outDir, failures);
    }
    printf("\n%d shaders, %d failures\n", total, failures);
    return failures ? 2 : 0;
}
