// LostOdysseyRecomp (2026-10-27): the guest shaders' pipeline layout as plume_deko3d numbers it, for the PC tool
// that fills the deko3d shader cache (tools/deko3d/deko3d_shader_tool.cpp). It must match what renderer.cpp builds
// with the Vulkan descriptor layout and uniform-buffer constants (set 0: the vertex arena; sets 1-3: 2D, 3D and cube
// textures, kTextureSlots each; set 4: the sampler palette; set 5: the vertex, shared and pixel constant banks), or
// the console's GLSL differs from the tool's and every cache lookup misses.
#pragma once

#include "../../thirdparty/plume-deko3d/plume_deko3d_spirv.h"

namespace lo::deko3d {
    inline constexpr uint32_t kGameTextureSlots = 32;
    inline constexpr uint32_t kGameSamplerPalette = 64;

    inline plume::Deko3DSpirvLayout GameLayout() {
        using Kind = plume::Deko3DSpirvLayout::Kind;
        plume::Deko3DSpirvLayout layout;
        auto add = [&](uint32_t set, uint32_t binding, uint32_t count, uint32_t index, uint32_t slot, Kind kind) {
            plume::Deko3DSpirvLayout::Range r;
            r.set = set;
            r.binding = binding;
            r.count = count;
            r.descriptorIndex = index;
            r.slot = slot;
            r.kind = kind;
            layout.ranges.push_back(r);
        };
        add(0, 0, 1, 0, 0, Kind::Storage);
        for (uint32_t set = 1; set <= 3; set++)
            for (uint32_t slot = 0; slot < kGameTextureSlots; slot++)
                add(set, slot, 1, slot, 0, Kind::Texture);
        add(4, 0, kGameSamplerPalette, 0, 0, Kind::Sampler);
        for (uint32_t bank = 0; bank < 3; bank++)
            add(5, bank, 1, bank, bank, Kind::Uniform);
        return layout;
    }
}
