// LostOdysseyRecomp (2026-10-27): the PC side of the Switch's deko3d shaders.
//
//   deko3d_shader_tool translate <shader.spv> [out.glsl]
//       the GLSL the console makes from one of the game's SPIR-V shaders (vs_* / ps_* by file name)
//   deko3d_shader_tool records <out.dkcache> < list-of-spv-files
//       only the translation records (what the console needs to skip SPIRV-Cross), no compiles
//   deko3d_shader_tool variants <out.dkcache> < list of "<shader.spv> <id>=<value> ..." / "vsout <vs.spv> <ps.spv|->"
//       compiles the variants the renderer and plume_deko3d ask for (tools/deko3d/recipe_variants.py lists them from
//       the pipeline recipe files): texture-info specializations, and vertex shaders that store only the outputs
//       their pipeline's pixel shader reads (constant 905); each under plume::Deko3DVariantKey, as loadVariant looks
//       it up
//   deko3d_shader_tool cache <out.dkcache> < list-of-spv-files
//       translates and compiles (uam) every listed shader and appends its DKSH to the cache file the console reads
//       (/switch/LostOdysseyRecomp/cache/deko3d_shaders.dkcache): records {'DKS1', size, key, DKSH}, key = FNV-1a 64
//       of the GLSL text, as plume_deko3d's loadVariant hashes it. Cache files can be concatenated.
//
// The SPIR-V files come from the Vulkan shader pack (tools/deko3d/extract_spirv.py portable_vk.lospv <dir>). The
// translation uses the game's pipeline layout as renderer.cpp builds it for the Switch (deko3d_game_layout.h).
#include "../../thirdparty/plume-deko3d/plume_deko3d_spirv.h"
#include "../../LostOdysseyRecomp/gpu/deko3d_game_layout.h"
#include <uam_api.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

static uint64_t Fnv1a64(const void *data, size_t size) {
    const uint8_t *p = static_cast<const uint8_t *>(data);
    uint64_t h = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < size; i++) {
        h ^= p[i];
        h *= 0x100000001b3ull;
    }
    return h;
}

static bool ReadFile(const std::string &path, std::vector<uint32_t> &words) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f)
        return false;
    const std::streamsize size = f.tellg();
    if (size <= 0 || size % 4)
        return false;
    words.resize(size_t(size) / 4);
    f.seekg(0);
    return bool(f.read(reinterpret_cast<char *>(words.data()), size));
}

static void WriteRecord(std::ofstream &out, uint64_t key, const std::vector<uint8_t> &data) {
    const uint32_t magic = 0x31534B44; // "DKS1"
    const uint32_t size = uint32_t(data.size());
    out.write(reinterpret_cast<const char *>(&magic), 4);
    out.write(reinterpret_cast<const char *>(&size), 4);
    out.write(reinterpret_cast<const char *>(&key), 8);
    out.write(reinterpret_cast<const char *>(data.data()), size);
}

static plume::Deko3DSpirvStage StageOf(const std::string &path) {
    const size_t slash = path.find_last_of("/\\");
    const std::string name = path.substr(slash == std::string::npos ? 0 : slash + 1);
    return name.rfind("vs_", 0) == 0 ? plume::Deko3DSpirvStage::Vertex : plume::Deko3DSpirvStage::Fragment;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s translate <shader.spv> [out.glsl] | cache <out.dkcache> < spv-list\n", argv[0]);
        return 2;
    }
    const plume::Deko3DSpirvLayout layout = lo::deko3d::GameLayout();
    const plume::Deko3DSpirvOptions options;
    const std::string mode = argv[1];
    if (mode == "translate") {
        std::vector<uint32_t> words;
        if (!ReadFile(argv[2], words)) {
            std::fprintf(stderr, "cannot read %s\n", argv[2]);
            return 1;
        }
        const auto r = plume::Deko3DTranslateSpirv(words.data(), words.size(), StageOf(argv[2]), layout, options);
        if (!r.ok) {
            std::fprintf(stderr, "%s: %s\n", argv[2], r.error.c_str());
            if (!r.glsl.empty() && argc > 3)
                std::ofstream(argv[3], std::ios::binary) << r.glsl;
            return 1;
        }
        if (argc > 3)
            std::ofstream(argv[3], std::ios::binary) << r.glsl;
        else
            std::cout << r.glsl;
        for (size_t k = 0; k < r.textures.size(); k++) {
            const auto &t = r.textures[k];
            std::fprintf(stderr, "binding %zu: image set %u index %u, sampler %s set %u index %u (uniform %u + %u)\n", k,
                         t.imageSet, t.imageIndex,
                         t.samplerFrom == plume::Deko3DSpirvTexture::SamplerFrom::None ? "none" :
                         t.samplerFrom == plume::Deko3DSpirvTexture::SamplerFrom::Fixed ? "fixed" : "from a uniform word",
                         t.samplerSet, t.samplerIndex, t.uniformSlot, t.uniformOffset);
        }
        for (const auto &n : r.notes)
            std::fprintf(stderr, "note: %s\n", n.c_str());
        return 0;
    }
    if (mode == "cache") {
        std::ofstream out(argv[2], std::ios::binary | std::ios::app);
        if (!out) {
            std::fprintf(stderr, "cannot write %s\n", argv[2]);
            return 1;
        }
        uam::setBanjoFlags(447); // as plume_deko3d (Deko3DOptions::uamFlags), so both make the same code
        uam::init(true);
        std::string path;
        size_t ok = 0, translateFailed = 0, compileFailed = 0;
        const auto started = std::chrono::steady_clock::now();
        while (std::getline(std::cin, path)) {
            if (path.empty())
                continue;
            std::vector<uint32_t> words;
            if (!ReadFile(path, words)) {
                std::fprintf(stderr, "cannot read %s\n", path.c_str());
                ++translateFailed;
                continue;
            }
            const auto stage = StageOf(path);
            const auto r = plume::Deko3DTranslateSpirv(words.data(), words.size(), stage, layout, options);
            if (!r.ok) {
                std::printf("TRANSLATE-FAIL %s: %s\n", path.c_str(), r.error.c_str());
                ++translateFailed;
                continue;
            }
            const uam::Result c = uam::compile(stage == plume::Deko3DSpirvStage::Vertex ? uam::Stage::Vertex : uam::Stage::Fragment, r.glsl.c_str());
            if (!c.ok) {
                std::printf("COMPILE-FAIL %s: %s\n", path.c_str(), c.log.substr(0, 300).c_str());
                ++compileFailed;
                continue;
            }
            WriteRecord(out, Fnv1a64(r.glsl.data(), r.glsl.size()), c.dksh);
            // The translation itself, so the console skips SPIRV-Cross for this shader.
            WriteRecord(out, plume::Deko3DTranslationKey(words.data(), words.size(), layout, options), plume::Deko3DWriteTranslation(r));
            out.flush();
            std::printf("OK %s\n", path.c_str());
            std::fflush(stdout);
            ++ok;
            if (ok % 500 == 0)
                std::fprintf(stderr, "%zu compiled (%.0f s)\n", ok,
                             std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count());
        }
        std::printf("done: %zu compiled, %zu not translated, %zu not compiled\n", ok, translateFailed, compileFailed);
        return translateFailed || compileFailed ? 1 : 0;
    }
    if (mode == "variants") {
        std::ofstream out(argv[2], std::ios::binary | std::ios::app);
        if (!out) {
            std::fprintf(stderr, "cannot write %s\n", argv[2]);
            return 1;
        }
        uam::setBanjoFlags(447);
        uam::init(true);
        struct Translated {
            bool ok = false;
            std::string glsl;
            std::vector<std::pair<uint32_t, uint32_t>> defaults; // by id, as plume_deko3d orders them
            uint32_t inputMask = 0, outputMask = 0;
        };
        std::map<std::string, Translated> translations;
        const auto get = [&](const std::string &path) -> const Translated & {
            auto it = translations.find(path);
            if (it != translations.end())
                return it->second;
            Translated &t = translations[path];
            std::vector<uint32_t> words;
            if (!ReadFile(path, words)) {
                std::printf("READ-FAIL %s\n", path.c_str());
                return t;
            }
            const auto r = plume::Deko3DTranslateSpirv(words.data(), words.size(), StageOf(path), layout, options);
            if (!r.ok) {
                std::printf("TRANSLATE-FAIL %s: %s\n", path.c_str(), r.error.c_str());
                return t;
            }
            t.ok = true;
            t.glsl = r.glsl;
            const std::string define = "#define SPIRV_CROSS_CONSTANT_ID_";
            for (size_t p = t.glsl.find(define); p != std::string::npos; p = t.glsl.find(define, p + 1)) {
                char *end = nullptr;
                const uint32_t id = uint32_t(std::strtoul(t.glsl.c_str() + p + define.size(), &end, 10));
                t.defaults.push_back({id, uint32_t(std::strtoul(end, nullptr, 0))});
            }
            std::sort(t.defaults.begin(), t.defaults.end());
            plume::Deko3DVaryingMasks(t.glsl, t.inputMask, t.outputMask);
            return t;
        };
        std::string line;
        size_t ok = 0, same = 0, failed = 0;
        const auto started = std::chrono::steady_clock::now();
        while (std::getline(std::cin, line)) {
            std::istringstream in(line);
            std::string path;
            if (!(in >> path))
                continue;
            std::map<uint32_t, uint32_t> values;
            if (path == "vsout") {
                // "vsout <vs.spv> <ps.spv | ->": the vertex shader for a pipeline with that pixel shader (or none):
                // constant 905 = the outputs it writes that the pixel shader reads (plume_deko3d's vsOutputs).
                std::string ps;
                in >> path >> ps;
                const Translated &v = get(path);
                if (!v.ok) {
                    ++failed;
                    continue;
                }
                uint32_t reads = 0;
                if (ps != "-") {
                    const Translated &f = get(ps);
                    if (!f.ok) {
                        ++failed;
                        continue;
                    }
                    reads = f.inputMask;
                }
                const bool has905 = std::any_of(v.defaults.begin(), v.defaults.end(), [](const auto &d) { return d.first == 905u; });
                if (!has905 || (reads & v.outputMask) == v.outputMask) {
                    ++same;
                    continue;
                }
                values[905] = reads & v.outputMask;
            } else {
                for (std::string item; in >> item;) {
                    const size_t eq = item.find('=');
                    if (eq != std::string::npos)
                        values[uint32_t(std::stoul(item.substr(0, eq)))] = uint32_t(std::stoul(item.substr(eq + 1), nullptr, 0));
                }
            }
            const Translated &t = get(path);
            if (!t.ok) {
                ++failed;
                continue;
            }
            // loadVariant's defines: the constants that differ from their defaults, by id.
            std::string defines;
            for (const auto &[id, value] : t.defaults) {
                const auto it = values.find(id);
                if (it == values.end() || it->second == value)
                    continue;
                char text[64];
                std::snprintf(text, sizeof(text), "#define SPIRV_CROSS_CONSTANT_ID_%u %uu\n", id, it->second);
                defines += text;
            }
            if (defines.empty()) {
                ++same; // the default variant, already in the pack
                continue;
            }
            const size_t nl = t.glsl.find('\n');
            const std::string source = t.glsl.substr(0, nl + 1) + defines + t.glsl.substr(nl + 1);
            const auto stage = StageOf(path);
            const uam::Result c = uam::compile(stage == plume::Deko3DSpirvStage::Vertex ? uam::Stage::Vertex : uam::Stage::Fragment, source.c_str());
            if (!c.ok) {
                std::printf("COMPILE-FAIL %s: %s\n", line.c_str(), c.log.substr(0, 300).c_str());
                ++failed;
                continue;
            }
            WriteRecord(out, plume::Deko3DVariantKey(Fnv1a64(t.glsl.data(), t.glsl.size()), defines), c.dksh);
            out.flush();
            std::printf("OK %s %zu\n", line.c_str(), c.dksh.size());
            ++ok;
            if (ok % 200 == 0)
                std::fprintf(stderr, "%zu variants (%.0f s)\n", ok,
                             std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count());
        }
        std::printf("done: %zu variants, %zu default only, %zu failed\n", ok, same, failed);
        return failed ? 1 : 0;
    }
    if (mode == "records") {
        // Translation records only (no uam): for a cache made before records existed.
        std::ofstream out(argv[2], std::ios::binary | std::ios::app);
        std::string path;
        size_t ok = 0, failed = 0;
        while (std::getline(std::cin, path)) {
            if (path.empty())
                continue;
            std::vector<uint32_t> words;
            if (!ReadFile(path, words)) {
                ++failed;
                continue;
            }
            const auto r = plume::Deko3DTranslateSpirv(words.data(), words.size(), StageOf(path), layout, options);
            if (!r.ok) {
                std::printf("TRANSLATE-FAIL %s: %s\n", path.c_str(), r.error.c_str());
                ++failed;
                continue;
            }
            WriteRecord(out, plume::Deko3DTranslationKey(words.data(), words.size(), layout, options), plume::Deko3DWriteTranslation(r));
            ++ok;
        }
        std::printf("done: %zu records, %zu failed\n", ok, failed);
        return failed ? 1 : 0;
    }
    std::fprintf(stderr, "unknown mode %s\n", mode.c_str());
    return 2;
}
