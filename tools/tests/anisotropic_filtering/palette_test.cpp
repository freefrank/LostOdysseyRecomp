#include <gpu/sampler_description.h>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
using namespace gpu::sampling;
unsigned checks = 0;
void Check(bool ok, const char* text) {
    ++checks;
    if (!ok) throw std::runtime_error(text);
}
struct Sampler { uint64_t key; };
struct Set {
    unsigned id;
    std::vector<std::weak_ptr<Sampler>> handles;
    std::vector<uint64_t> keys;
    bool Intact() const {
        for (size_t i = 0; i < handles.size(); ++i) {
            auto p = handles[i].lock();
            if (!p || p->key != keys[i]) return false;
        }
        return true;
    }
};
struct Factory {
    unsigned samplerCalls = 0, setCalls = 0;
    int failSamplerAt = -1;
    bool failSet = false;
    auto Samplers() { return [this](uint64_t key) -> std::shared_ptr<Sampler> {
        ++samplerCalls;
        Check(!(key & EligibleBit), "recipe metadata must never reach the backend");
        if (failSamplerAt == int(samplerCalls)) return {};
        return std::make_shared<Sampler>(Sampler{key});
    }; }
    auto Sets() { return [this](const auto& handles) -> std::shared_ptr<Set> {
        ++setCalls;
        if (failSet) return {};
        auto result = std::make_shared<Set>(); result->id = setCalls;
        for (auto& p : handles) {
            Check(bool(p), "every descriptor initialized");
            result->handles.emplace_back(p); result->keys.push_back(p->key);
        }
        return result;
    }; }
};
using Cache = Palette<Sampler, Set>;
void Policy() {
    constexpr uint64_t linear = 0x15;
    for (uint32_t n : {0u,2u,4u,8u,16u}) Check(NormalizeLevel(n) == n, "valid level");
    for (uint32_t n : {1u,3u,7u,17u,~0u}) Check(NormalizeLevel(n) == 0, "invalid level");
    Check(ClampLevel(16, 8) == 8 && ClampLevel(16, 6) == 4, "round down capability");
    Check(ClampLevel(16, 0) == 0 && ClampLevel(16, 1) == 0, "unsupported capability");
    Check(ClampLevel(2, 16) == 2 && ClampLevel(0, 16) == 0, "do not raise request");
    Check(Eligible(linear, true, 1, true, false), "material eligible");
    Check(!Eligible(0, true, 1, true, false), "point protected");
    Check(!Eligible(linear, false, 1, true, false), "resolved/depth protected");
    Check(!Eligible(linear, true, 1, false, false), "UI/vertex sampling protected");
    Check(!Eligible(linear, true, 1, true, true), "host temporal substitution protected");
    for (uint32_t dim : {0u,2u,3u}) Check(!Eligible(linear,true,dim,true,false), "non-2D protected");
    for (uint64_t key : {0x25ull,0x35ull,0x14ull,0x11ull,0x1Full})
        Check(!Eligible(key,true,1,true,false), "base mip/reserved/mixed filters protected");
    for (uint32_t level : {0u,2u,4u,8u,16u}) {
        const auto key = EffectiveKey(Recipe(linear, true), level);
        const auto desc = Describe(key);
        Check(desc.anisotropyEnabled == bool(level), "enable bit");
        Check(desc.maxAnisotropy == (level ? level : 1), "level decode");
        Check(desc.mipLODBias == 0.0f && desc.minLOD == 0.0f, "unchanged LOD policy");
        Check(desc.minFilter == plume::RenderFilter::LINEAR && desc.magFilter == plume::RenderFilter::LINEAR, "linear policy");
        Check(EffectiveKey(Recipe(linear, false), level) == linear, "protected recipe never overridden");
    }
    const auto point = Describe(0);
    Check(point.minFilter == plume::RenderFilter::NEAREST && point.magFilter == plume::RenderFilter::NEAREST &&
          point.mipmapMode == plume::RenderMipmapMode::NEAREST && !point.anisotropyEnabled, "Off point stays point");
    for (uint64_t address = 0; address < 8; ++address) {
        const uint64_t raw = linear | (address << 6) | (address << 9) | (address << 12);
        const auto off = Describe(raw), af = Describe(EffectiveKey(Recipe(raw, true), 16));
        Check(off.addressU == af.addressU && off.addressV == af.addressV && off.addressW == af.addressW, "addresses unchanged");
    }
}
void SwitchingAndFailure() {
    Factory f; Cache c;
    Check(c.Initialize(f.Samplers(), f.Sets()), "initialize");
    for (unsigned n = 0; n < 13; ++n) {
        c.BeginDraw(); Check(c.Select(Recipe(0x15 | (uint64_t(n) << 6), true), f.Samplers(), f.Sets()).has_value(), "insert material");
    }
    c.BeginDraw(); auto protectedSlot = c.Select(Recipe(0x480, false), f.Samplers(), f.Sets());
    Check(bool(protectedSlot), "protected slot");
    const auto size = c.Current()->slots.size();
    auto original = c.Current();
    const auto originalId = original->descriptors->id;
    for (unsigned loop = 0; loop < 200; ++loop) {
        for (uint32_t level : {0u,2u,4u,8u,16u}) {
            Check(c.Reconfigure(level, f.Samplers(), f.Sets()), "runtime switch");
            Check(c.Level() == level && c.Current()->slots.size() == size, "switch does not consume slots");
            Check(original->descriptors->Intact() && original->descriptors->id == originalId, "bound original never mutated");
            Check(c.Current()->Key(*protectedSlot) == 0x480, "protected sampling unchanged");
        }
    }
    // Failure at each AF sampler creation, including after earlier successes.
    for (unsigned n = 1; n <= 13; ++n) {
        auto before = c.Current(); f.failSamplerAt = int(f.samplerCalls + n);
        Check(!c.Reconfigure(8, f.Samplers(), f.Sets()), "injected sampler failure");
        Check(c.Current() == before && c.Level() == 16 && before->descriptors->Intact(), "sampler failure atomic");
        f.failSamplerAt = -1;
    }
    auto before = c.Current(); f.failSet = true;
    Check(!c.Reconfigure(8, f.Samplers(), f.Sets()), "injected descriptor failure");
    Check(c.Current() == before && before->descriptors->Intact(), "descriptor failure atomic");
    c.BeginDraw();
    Check(!c.Select(Recipe(0x15 | (42ull<<6), true), f.Samplers(), f.Sets()), "new key failure");
    Check(c.Current() == before, "failed new key not published");
    f.failSet = false;
    c.BeginDraw();
    auto slot = c.Select(Recipe(0x15,true), f.Samplers(), f.Sets());
    Check(bool(slot), "cache lookup");
    const auto calls = std::pair{f.samplerCalls, f.setCalls};
    for (unsigned i=0; i<1000; ++i) Check(bool(c.Select(Recipe(0x15,true), f.Samplers(), f.Sets())), "cache hit");
    Check(calls == std::pair{f.samplerCalls, f.setCalls}, "steady-state lookup allocates nothing");
    // Hold the same version in two independent GPU slots (and a replay use).
    std::array<std::vector<Cache::Lease>,2> slots;
    Retain(slots[0], c.Current()); Retain(slots[0], c.Current()); Retain(slots[1], c.Current());
    Check(slots[0].size() == 1, "replay retention deduplicated");
    std::weak_ptr<const Cache::Version> old = c.Current(); before.reset();
    Check(c.Reconfigure(4, f.Samplers(), f.Sets()), "replacement while pending");
    slots[0].clear(); Check(!old.expired(), "other pending batch still owns old descriptors");
    slots[1].clear(); Check(old.expired(), "last completion releases old descriptors and samplers");
}
void CapacityAndInitialization() {
    Factory f; Palette<Sampler,Set,4> c;
    f.failSamplerAt = 1;
    Check(!c.Initialize(f.Samplers(), f.Sets()) && !c.Current(), "failed initial sampler");
    f.failSamplerAt = -1; f.failSet = true;
    Check(!c.Initialize(f.Samplers(), f.Sets()) && !c.Current(), "failed initial descriptor");
    f.failSet = false; Check(c.Initialize(f.Samplers(), f.Sets()), "retry initialization");
    for (unsigned n=0; n<500; ++n) {
        auto previous = c.Current();
        c.BeginDraw();
        const auto a = c.Select(Recipe((n*3)%32768,true), f.Samplers(), f.Sets());
        const auto b = c.Select(Recipe((n*3+1)%32768,true), f.Samplers(), f.Sets());
        const auto d = c.Select(Recipe((n*3+2)%32768,true), f.Samplers(), f.Sets());
        Check(a && b && d, "capacity reused across scenes");
        Check(*a != *b && *a != *d && *b != *d, "do not evict current draw slots");
        auto before = c.Current();
        Check(!c.Select(Recipe(32767,false), f.Samplers(), f.Sets()), "full current draw fails explicitly");
        Check(c.Current() == before && before->Key(0) == DefaultKey, "overflow does not return unrelated zero");
        Check(previous->descriptors->Intact(), "eviction keeps old GPU snapshot intact");
    }
}
}
int main() {
    try { Policy(); SwitchingAndFailure(); CapacityAndInitialization(); }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
    std::cout << "AF palette: " << checks << " checks passed\n";
}
