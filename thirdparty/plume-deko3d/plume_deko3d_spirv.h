//
// LostOdysseyRecomp (2026-10-27): SPIR-V to the GLSL that uam (deko3d's shader compiler) takes, for plume's deko3d
// back end. Shared by the console (shaders a DKSH cache lacks) and the PC tool that fills that cache
// (tools/deko3d/deko3d_shader_tool.cpp), so both make the same text and the cache keys (a hash of the text) match.
//
// deko3d has no descriptor sets, no 64-bit pointers and binds a texture with its sampler as one handle. The SPIR-V
// is the Vulkan back end's; the translation:
//   * uniform and storage buffers get the deko3d binding plume_deko3d gives their (set, binding) in the pipeline
//     layout (Deko3DSpirvLayout); push constants become the uniform buffer at binding 5;
//   * each (texture, sampler) pair a shader samples with becomes one combined sampler at binding k (0-31). Where the
//     handle's two halves come from is returned (Deko3DSpirvTexture): the texture's descriptor, and the sampler's,
//     fixed or chosen by a word of a uniform buffer (Xenos shaders: xeSamplers[XeShared.xeSamplerIndex[i].x]);
//   * reads through buffer_reference pointers whose base is the address window's uniform (LostOdysseyRecomp: the
//     vertex arena, XeShared.xeVertexArenaAddress) become reads of the storage buffer that window is bound as;
//   * specialization constants become SPIRV_CROSS_CONSTANT_ID_<id> macros (plume_deko3d's variants define them);
//   * the vertex stage's y is flipped back (the SPIR-V is made with -fvk-invert-y; deko3d's device uses D3D's y).
//
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace plume {
    // The pipeline layout as plume_deko3d numbers it (createPipelineLayout): the k-th constant buffer of the layout
    // (set order, binding order) is uniform binding k, storage buffers likewise.
    struct Deko3DSpirvLayout {
        enum class Kind : uint8_t { Other, Uniform, Storage, Texture, Sampler };
        struct Range {
            uint32_t set = 0;
            uint32_t binding = 0;
            uint32_t count = 1;
            uint32_t descriptorIndex = 0;  // first descriptor of the range in its set
            uint32_t slot = 0;             // deko3d uniform / storage binding of the first descriptor
            Kind kind = Kind::Other;
        };
        std::vector<Range> ranges;
        const Range *find(uint32_t set, uint32_t binding) const {
            for (const Range &r : ranges)
                if (r.set == set && r.binding == binding)
                    return &r;
            return nullptr;
        }
    };

    // Where the two halves of combined binding k's handle come from.
    struct Deko3DSpirvTexture {
        enum class SamplerFrom : uint8_t { None, Fixed, UniformWord };
        uint32_t imageSet = 0;
        uint32_t imageIndex = 0;     // descriptor index in imageSet
        SamplerFrom samplerFrom = SamplerFrom::None;
        uint32_t samplerSet = 0;
        uint32_t samplerIndex = 0;   // Fixed: its descriptor index; UniformWord: index of the array's element 0
        uint32_t uniformSlot = 0;    // UniformWord: the uniform binding and byte offset of the word (the element)
        uint32_t uniformOffset = 0;
    };

    // The address window: buffer_reference loads whose base is uint64(u.x) | uint64(u.y) << 32 of this uvec2 member
    // read the storage buffer bound at (set, binding) from its start (base 0).
    struct Deko3DSpirvOptions {
        std::string addressWindowMember = "xeVertexArenaAddress";
        uint32_t addressWindowSet = 0;
        uint32_t addressWindowBinding = 0;
    };

    enum class Deko3DSpirvStage : uint8_t { Vertex, Geometry, Fragment, Compute };

    struct Deko3DSpirvResult {
        bool ok = false;
        std::string glsl;
        std::string error;
        std::vector<Deko3DSpirvTexture> textures;  // combined binding k = textures[k]
        uint32_t uniformMask = 0;
        uint32_t storageMask = 0;
        std::vector<uint32_t> vertexInputs;        // vertex stage: the location of each attribute (identity)
        // Storage windows at a pushed address (the renderer's own shaders read their constants through push constant
        // addresses): storage binding 'slot' starts at the 64-bit address at byte 'pushOffset' of the push constants.
        struct PushWindow {
            uint32_t slot = 0;
            uint32_t pushOffset = 0;
        };
        std::vector<PushWindow> pushWindows;
        std::vector<std::string> notes;            // what was approximated (logged by the caller)
    };

    // The uniform binding plume_deko3d gives push constants (Deko3DCommandList::setGraphicsPushConstants).
    constexpr uint32_t Deko3DPushUniformSlot = 5;
    // The storage bindings of push constant address windows (Deko3DSpirvResult::pushWindows).
    constexpr uint32_t Deko3DPushWindowFirstSlot = 12;
    constexpr uint32_t Deko3DPushWindowCount = 3;

    Deko3DSpirvResult Deko3DTranslateSpirv(const uint32_t *words, size_t wordCount, Deko3DSpirvStage stage,
                                           const Deko3DSpirvLayout &layout, const Deko3DSpirvOptions &options);

    // ---- translation records (2026-10-27): what a translation gives the back end besides the GLSL, kept in the
    // DKSH cache under Deko3DTranslationKey so the console skips SPIRV-Cross for shaders the PC already translated.
    // Changing the translation's output means changing Deko3DTranslatorVersion.
    constexpr uint32_t Deko3DTranslatorVersion = 3;  // 3 (2026-10-27): native min/max, constant 905, varying masks
    // FNV-1a 64 of the GLSL text: the DKSH cache key of a variant (plume_deko3d's loadVariant).
    uint64_t Deko3DGlslKey(const std::string &glsl);
    // The cache key of a translation: the SPIR-V, the layout's numbering, the options and the translator version.
    uint64_t Deko3DTranslationKey(const uint32_t *words, size_t wordCount, const Deko3DSpirvLayout &layout,
                                  const Deko3DSpirvOptions &options);
    struct Deko3DTranslationRecord {
        uint64_t glslKey = 0;
        uint32_t uniformMask = 0, storageMask = 0;
        std::vector<Deko3DSpirvTexture> textures;
        std::vector<Deko3DSpirvResult::PushWindow> pushWindows;
        std::vector<uint32_t> vertexInputs;
        std::vector<std::pair<uint32_t, uint32_t>> specDefaults;  // id, default value (SPIRV_CROSS_CONSTANT_ID_n)
        // The varying locations it declares ("layout(location = N) in / out" lines), as plume_deko3d reads them
        // from the text (Deko3DShaderUsage::inputMask / outputMask).
        uint32_t inputMask = 0, outputMask = 0;
    };
    // The varying locations a translation's GLSL declares, as plume_deko3d's makeGlslShader reads them.
    void Deko3DVaryingMasks(const std::string &glsl, uint32_t &inputMask, uint32_t &outputMask);
    std::vector<uint8_t> Deko3DWriteTranslation(const Deko3DSpirvResult &result);
    bool Deko3DReadTranslation(const std::vector<uint8_t> &bytes, Deko3DTranslationRecord &record);

    // LostOdysseyRecomp (2026-10-27): the cache key of a specialized variant: the FNV-1a 64 key of the shader's
    // default GLSL, then the variant's defines ("#define SPIRV_CROSS_CONSTANT_ID_<id> <value>u\n" for each constant
    // that differs from its default, by id) hashed on. The PC tool writes pre-built variants under it.
    inline uint64_t Deko3DVariantKey(uint64_t baseKey, const std::string &defines) {
        uint64_t h = 0xcbf29ce484222325ull ^ baseKey;
        for (unsigned i = 0; i < 8; i++) {
            h ^= uint8_t(baseKey >> (8 * i));
            h *= 0x100000001b3ull;
        }
        for (const char c : defines) {
            h ^= uint8_t(c);
            h *= 0x100000001b3ull;
        }
        return h ^ 0x564152494e543031ull;  // "VARINT01"
    }
}
