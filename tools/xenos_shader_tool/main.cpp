// Offline check for the Xenos shader translator: translates dumped microcode
// (vs_*.bin / ps_*.bin written by LO_SHADER_DUMP_DIR, big-endian dwords) to
// HLSL and compiles it with DXC.
//
// Usage: LoShaderTool <shader.bin | directory> [--print] [--out <dir>] [--vulkan] [--jobs N]

#include <gpu/shader/xenos_translator.h>
#include <gpu/shader/dxc_compiler.h>
#include <gpu/shader/cache.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <algorithm>

namespace fs = std::filesystem;
static bool spirv=false;

static uint32_t ByteSwap32(uint32_t v)
{
    return (v >> 24) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) | (v << 24);
}

static bool ProcessFile(const fs::path& path, bool print, const fs::path& outDir, const fs::path& cacheDir, int& failures)
{
    std::ifstream in(path, std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (bytes.size() < 12 || (bytes.size() % 4) != 0)
    {
        printf("%s: bad size %zu\n", path.string().c_str(), bytes.size());
        failures++;
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
    if (!isPixel && name.rfind("vs_", 0) != 0) {
        printf("%s: expected vs_ or ps_ microcode filename\n", name.c_str());
        failures++;
        return false;
    }
    uint64_t hash = 0xcbf29ce484222325ull;
    for (uint8_t b : bytes) { hash ^= b; hash *= 0x100000001b3ull; }
    fs::path cachePath;
    if (!cacheDir.empty()) {
        cachePath = cacheDir / xenos::cache::FileName(isPixel, hash, spirv);
        std::error_code ec;
        const auto sourceDir = cacheDir / "source";
        fs::create_directories(sourceDir, ec);
        if (ec) { printf("Cannot create source cache: %s\n", ec.message().c_str()); failures++; return false; }
        auto sourceName = xenos::cache::FileName(isPixel, hash, spirv);
        sourceName = sourceName.substr(0, 19) + ".bin";
        const auto sourcePath = sourceDir / sourceName;
        if (!fs::exists(sourcePath)) {
            std::ofstream source(sourcePath, std::ios::binary);
            source.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
            source.close();
            if (!source) { printf("Cannot write source cache\n"); failures++; return false; }
        }
        std::ifstream cached(cachePath, std::ios::binary);
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(cached)), {});
        if (!print && outDir.empty() && xenos::cache::CompleteBinary(data, spirv)) {
            printf("%s: cached\n", name.c_str());
            return true;
        }
    }
    xenos::TranslatedShader translated = xenos::TranslateShader(dwords.data(), uint32_t(dwords.size()), isPixel);

    if (print)
        printf("---- %s ----\n%s\n", name.c_str(), translated.hlsl.c_str());
    if (!outDir.empty())
    {
        fs::create_directories(outDir);
        std::ofstream(outDir / (path.stem().string() + ".hlsl")) << translated.hlsl;
    }

    xenos::CompiledShader compiled = xenos::CompileHlsl(translated.hlsl, "main", isPixel ? "ps_6_0" : "vs_6_0", spirv?xenos::ShaderBinaryFormat::Spirv:xenos::ShaderBinaryFormat::Dxil);
    printf("%-28s %4zu dwords  %s  bytecode=%zu bytes  vfetch=%016llx tex=%08x%s\n", name.c_str(), dwords.size(),
        compiled.ok ? "OK  " : "FAIL", compiled.bytecode.size(),
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
    else if (!cachePath.empty()) {
        std::ofstream cached(cachePath, std::ios::binary);
        cached.write(reinterpret_cast<const char*>(compiled.bytecode.data()), compiled.bytecode.size());
        cached.close();
        if (!cached) { printf("Cannot write %s\n", cachePath.string().c_str()); failures++; return false; }
    }
    return compiled.ok;
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        printf("Usage: LoShaderTool <shader.bin | directory> [--print] [--out <dir>] [--cache <runtime-cache-dir>] [--vulkan] [--jobs N]\n");
        return 1;
    }
    bool print = false;
    unsigned jobs = 1;
    fs::path outDir;
    fs::path cacheDir;
    for (int i = 2; i < argc; i++)
    {
        if (strcmp(argv[i], "--vulkan") == 0) spirv = true;
        else if (strcmp(argv[i], "--jobs") == 0 && i + 1 < argc) jobs = std::clamp(std::stoul(argv[++i]),1ul,64ul);
        else if (strcmp(argv[i], "--print") == 0) print = true;
        else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) outDir = argv[++i];
        else if (strcmp(argv[i], "--cache") == 0 && i + 1 < argc) cacheDir = argv[++i];
        else { printf("Unknown or incomplete argument: %s\n", argv[i]); return 1; }
    }

    if (!xenos::DxcAvailable())
        printf("warning: dxcompiler.dll not found, only translating\n");

    int failures = 0, total = 0;
    if (!cacheDir.empty()) {
        std::error_code ec;
        fs::create_directories(cacheDir, ec);
        if (ec) { printf("Cannot create cache: %s\n", ec.message().c_str()); return 1; }
    }
    fs::path input = argv[1];
    if (fs::is_directory(input))
    {
        std::vector<fs::path> sources;
        for (auto& entry : fs::directory_iterator(input))
        {
            if (entry.path().extension() == ".bin")
                sources.push_back(entry.path());
        }
        std::sort(sources.begin(),sources.end());
        total=int(sources.size());
        std::atomic<size_t> next{0};
        std::atomic<int> failed{0};
        auto worker=[&] {
            for (;;) {
                const size_t index=next.fetch_add(1);
                if(index>=sources.size())return;
                int localFailures=0;
                ProcessFile(sources[index],print,outDir,cacheDir,localFailures);
                failed.fetch_add(localFailures);
            }
        };
        { std::vector<std::jthread> workers;for(unsigned i=0;i<(print?1:jobs);++i)workers.emplace_back(worker); }
        failures=failed;
    }
    else
    {
        total++;
        ProcessFile(input, print, outDir, cacheDir, failures);
    }
    printf("\n%d shaders, %d failures\n", total, failures);
    return failures ? 2 : 0;
}
