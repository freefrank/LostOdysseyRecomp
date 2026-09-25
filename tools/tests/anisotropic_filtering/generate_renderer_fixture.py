"""Compile the production AF methods with synthetic device/settings services.

No PPC executable or vendor GPU is needed. The separate Vulkan fixture exercises
native Plume resources; this fixture verifies the renderer's actual wiring.
"""
import argparse
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--root', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
source = (args.root / 'LostOdysseyRecomp/gpu/renderer.cpp').read_text(encoding='utf-8')
start = source.index('            std::shared_ptr<RenderSampler> CreateGuestSampler(')
end = source.index('            // ---- shaders ', start)
methods = source[start:end]
assert methods.count('settings::GetConfig()') == 1
bind_start = source.index('                auto bindTextures = ')
bind_end = source.index('                const auto samplerVersion = ', bind_start)
assert 'settings::GetConfig().anisotropicFiltering' not in source[bind_start:bind_end]
assert 'RefreshAnisotropicFiltering();' not in source[bind_start:bind_end]
assert '!(shared.vtxFmt & 1)' in source[bind_start:bind_end]
assert '!((vs->info.textureSlotMask >> slot) & 1)' in source[bind_start:bind_end]
recycle = source[source.index('            bool RecycleSlot('):source.index('            bool WaitForGpu(')]
assert recycle.index('if (!video::WaitForGpuFence(s.fence.get())) return false;') < recycle.index('s.samplerVersions.clear();')
assert source.count('s.samplerVersions.clear();') == 1
assert 'Gpu().samplerVersions.size() >= kSamplerVersionsPerBatch' in source
assert source.count('samplerState.BeginDraw();') == 1
assert source.count('RefreshAnisotropicFiltering();') == 1
end_draw = source.index('            void Resolve(', bind_end) if '            void Resolve(' in source[bind_end:] else len(source)
draw_tail = source[bind_end:end_draw]
assert draw_tail.index('sampling::Retain(Gpu().samplerVersions, samplerVersion)') < draw_tail.index('commandList->setGraphicsDescriptorSet(set0, 0)')
assert 'set0 = vulkan ? staticSet0.get() : samplerVersion->descriptors.get();' in draw_tail
assert 'RenderDescriptorSet* set4 = vulkan ? samplerVersion->descriptors.get() : nullptr;' in draw_tail
# All guest and replay bindings use the pinned generation, never the host table.
assert 'staticSamplerSet.get()' not in draw_tail
assert 'ApplyAnisotropicOverride' not in source
prefix = r'''
#include <gpu/sampler_description.h>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#define LOG_WARNING(...) ((void)0)
#define LOG_INFO(...) ((void)0)
namespace sampling = gpu::sampling;
namespace gpu::render_arena { constexpr unsigned kVertexArenaSize = 4096; }
namespace settings {
struct Config { uint32_t anisotropicFiltering = 0; } config;
unsigned reads = 0;
Config GetConfig() { ++reads; return config; }
}
void Check(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
struct RenderSampler { plume::RenderSamplerDesc description; };
struct RenderDescriptorSet {
    unsigned bank = 0, buffers = 0, writes = 0;
    std::array<const RenderSampler*,64> samplers{};
    void setBuffer(unsigned index, int* buffer, unsigned bytes) {
        Check(bank == 0 && index < 96 && buffer && bytes == 4096, "vertex bank wiring"); ++buffers;
    }
    void setSampler(unsigned index, RenderSampler* sampler) {
        const auto base = bank == 0 ? 96u : 0u;
        Check(index >= base && index < base + 64 && sampler, "sampler bank wiring");
        samplers[index-base] = sampler; ++writes;
    }
};
struct Device {
    bool failSampler = false, failSet = false;
    unsigned creations = 0, sets = 0;
    std::unique_ptr<RenderSampler> createSampler(const plume::RenderSamplerDesc& desc) {
        ++creations;
        return failSampler ? nullptr : std::make_unique<RenderSampler>(RenderSampler{desc});
    }
};
struct Builder {
    unsigned bank = 0;
    std::unique_ptr<RenderDescriptorSet> create(Device* d) {
        ++d->sets;
        if (d->failSet) return {};
        auto set = std::make_unique<RenderDescriptorSet>(); set->bank = bank; return set;
    }
};
struct Renderer {
    static constexpr unsigned kSamplerPalette = 64, kVertexFetchSlots = 96;
    using SamplerPalette = sampling::Palette<RenderSampler,RenderDescriptorSet>;
    Device ownedDevice; Device* device = &ownedDevice;
    bool vulkan = false;
    Builder setBuilders[5]{};
    std::unique_ptr<int> vertexArena = std::make_unique<int>(1);
    unsigned vfetchDescriptorBase = 0, samplerDescriptorBase = 96;
    SamplerPalette samplerState;
    std::shared_ptr<RenderSampler> defaultSampler;
    uint32_t maximumAnisotropy = 16, lastAnisotropyRequest = UINT32_MAX, frame = 0;
    uint64_t anisotropyConfigFrame = ~0ull, samplerFailureFrame = ~0ull;
    Renderer(bool vk) : vulkan(vk) {
        samplerDescriptorBase = vk ? 0 : 96;
        setBuilders[4].bank = 4;
        defaultSampler = device->createSampler(sampling::Describe(sampling::DefaultKey));
        Check(samplerState.Initialize([&](uint64_t k) { return CreateGuestSampler(k); },
            [&](const auto& handles) { return CreateSamplerTable(handles); }), "initialization");
    }
'''
tail = r'''
};
void Test(bool vulkan) {
    settings::reads = 0; settings::config.anisotropicFiltering = 0;
    Renderer r(vulkan);
    const auto initial = r.samplerState.Current();
    Check(initial->descriptors->buffers == (vulkan ? 0u : 96u), "D3D12 vertices / Vulkan separate bank");
    Check(initial->descriptors->writes == 64, "complete table");
    r.RefreshAnisotropicFiltering();
    r.samplerState.BeginDraw();
    const auto slot = r.GetSamplerIndex(0x15,true); Check(bool(slot), "material selection");
    const auto old = r.samplerState.Current();
    const auto oldHandles = old->descriptors->samplers;
    const auto createCount = r.device->creations, setCount = r.device->sets;
    for (unsigned i = 0; i < 1000; ++i) {
        r.RefreshAnisotropicFiltering(); r.samplerState.BeginDraw();
        Check(r.GetSamplerIndex(0x15,true) == slot, "hot cache");
    }
    Check(settings::reads == 1 && r.device->creations == createCount && r.device->sets == setCount, "no per-texture lock or allocation");
    settings::config.anisotropicFiltering = 16;
    r.RefreshAnisotropicFiltering(); Check(r.samplerState.Level() == 0, "coherent frame snapshot");
    ++r.frame; r.RefreshAnisotropicFiltering(); Check(r.samplerState.Level() == 16, "live next frame");
    Check(r.samplerState.Current()->slots.size() == old->slots.size(), "no mode-key growth");
    Check(old->descriptors->samplers == oldHandles && !old->samplers[*slot]->description.anisotropyEnabled, "old generation immutable");
    Check(r.ActualSamplerKey(*slot) == sampling::EffectiveKey(sampling::Recipe(0x15,true),16), "diagnostics use applied level");
    auto active = r.samplerState.Current();
    r.device->failSet = true; settings::config.anisotropicFiltering = 8;
    ++r.frame; r.RefreshAnisotropicFiltering();
    Check(r.samplerState.Current() == active && r.samplerState.Level() == 16, "failed table transaction retains active");
    auto failures = r.device->sets;
    ++r.frame; r.RefreshAnisotropicFiltering();
    Check(r.device->sets == failures, "failed request not retried every frame");
    r.device->failSet = false; settings::config.anisotropicFiltering = 4;
    ++r.frame; r.RefreshAnisotropicFiltering(); Check(r.samplerState.Level() == 4, "changed request can retry");
    r.device->failSampler = true; settings::config.anisotropicFiltering = 2;
    active = r.samplerState.Current(); ++r.frame; r.RefreshAnisotropicFiltering();
    Check(r.samplerState.Current() == active, "sampler creation failure retains table");
    r.device->failSampler = false; r.maximumAnisotropy = 8; settings::config.anisotropicFiltering = 16;
    ++r.frame; r.RefreshAnisotropicFiltering(); Check(r.samplerState.Level() == 8, "device limit respected");
    settings::config.anisotropicFiltering = 0; ++r.frame; r.RefreshAnisotropicFiltering();
    Check(r.samplerState.Level() == 0 && r.GetSamplerIndex(0x15,true) == slot, "Off restores original with stable indices");
}
int main() {
    try { Test(false); Test(true); }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
    std::cout << "Production renderer AF methods and source wiring passed (synthetic services)\n";
}
'''
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(prefix + methods + tail, encoding='utf-8')
