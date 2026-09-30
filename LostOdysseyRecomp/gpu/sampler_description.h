#pragma once
#include "sampler_palette.h"
#include <plume_render_interface_types.h>

namespace gpu::sampling {
inline plume::RenderSamplerDesc Describe(uint64_t key) {
    using namespace plume;
    RenderSamplerDesc desc;
    auto filter = [](uint32_t f) { return f == 0 ? RenderFilter::NEAREST : RenderFilter::LINEAR; };
    desc.magFilter = filter(key & 3);
    desc.minFilter = filter((key >> 2) & 3);
    desc.mipmapMode = ((key >> 4) & 3) == 0 ? RenderMipmapMode::NEAREST : RenderMipmapMode::LINEAR;
    auto clamp = [](uint32_t c) {
        switch (c) {
        case 0: return RenderTextureAddressMode::WRAP;
        case 1: return RenderTextureAddressMode::MIRROR;
        case 3: case 5: return RenderTextureAddressMode::MIRROR_ONCE;
        case 6: case 7: return RenderTextureAddressMode::BORDER;
        default: return RenderTextureAddressMode::CLAMP;
        }
    };
    desc.addressU = clamp((key >> 6) & 7);
    desc.addressV = clamp((key >> 9) & 7);
    desc.addressW = clamp((key >> 12) & 7);
    desc.anisotropyEnabled = ((key >> 15) & 1) != 0;
    const uint32_t code = (key >> 16) & 7;
    desc.maxAnisotropy = desc.anisotropyEnabled && code >= 1 && code <= 4 ? 1u << code : 1u;
    // Keep the existing LOD policy; no sharpening bias or invented mip chain.
    return desc;
}
} // namespace gpu::sampling
