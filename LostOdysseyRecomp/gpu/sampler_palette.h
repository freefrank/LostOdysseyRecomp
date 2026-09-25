#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace gpu::sampling {
inline constexpr uint64_t DefaultKey = 0x2 | (0x2 << 2) | (0x1 << 4);
// Host-only recipe metadata; never passed to the sampler backend or shader.
inline constexpr uint64_t EligibleBit = uint64_t(1) << 32;
inline constexpr uint32_t NormalizeLevel(uint32_t level) {
    return level == 2 || level == 4 || level == 8 || level == 16 ? level : 0;
}
inline constexpr uint32_t ClampLevel(uint32_t requested, uint32_t maximum) {
    requested = NormalizeLevel(requested);
    while (requested > maximum) requested >>= 1;
    return requested >= 2 ? requested : 0;
}
inline constexpr uint64_t Recipe(uint64_t key, bool eligible) {
    return (key & 0x7FFFu) | (eligible ? EligibleBit : 0);
}
inline constexpr uint64_t EffectiveKey(uint64_t recipe, uint32_t level) {
    const uint64_t key = recipe & 0x7FFFu;
    if (!(recipe & EligibleBit) || !NormalizeLevel(level)) return key;
    const uint64_t code = level == 16 ? 4 : level == 8 ? 3 : level == 4 ? 2 : 1;
    return key | (uint64_t(1) << 15) | (code << 16);
}
// Conservative override: only filtered, depth-tested guest 2D geometry.
// Render-target/depth fetches, screen-space UI, point sampling, explicit base
// mip selection, and host/temporal substitutions retain their original state.
inline constexpr bool Eligible(uint64_t key, bool guestUpload, uint32_t dimension,
                               bool sceneGeometry, bool substituted) {
    const auto mag = key & 3, min = (key >> 2) & 3, mip = (key >> 4) & 3;
    return guestUpload && dimension == 1 && sceneGeometry && !substituted &&
        mag == 1 && min == 1 && mip <= 1;
}

// A descriptor table is immutable from its first bind until its last GPU use.
// Callers retain Current() in *each* batch that binds it, including replay.
// This class has no GPU waits and no settings reads. Factories return null on
// failure. Publication changes nothing until all resources have been created.
template<class Sampler, class DescriptorSet, uint32_t Capacity = 64>
class Palette {
    static_assert(Capacity > 1 && Capacity <= 64);
public:
    struct Version {
        uint32_t level = 0;
        std::map<uint64_t, uint32_t> slots;
        std::array<uint64_t, Capacity> recipes{};
        std::array<std::shared_ptr<Sampler>, Capacity> samplers{};
        // Destroy descriptors before their samplers (reverse member order).
        std::shared_ptr<DescriptorSet> descriptors;
        uint64_t Key(uint32_t index) const {
            return index < Capacity ? EffectiveKey(recipes[index], level) : DefaultKey;
        }
    };
    using Lease = std::shared_ptr<const Version>;
    Lease Current() const { return current_; }
    uint32_t Level() const { return current_ ? current_->level : 0; }
    void BeginDraw() { used_ = 1; } // slot zero is an immutable dummy fallback

    template<class MakeSampler, class MakeSet>
    bool Initialize(MakeSampler&& makeSampler, MakeSet&& makeSet) {
        auto next = std::make_shared<Version>();
        auto fallback = makeSampler(DefaultKey);
        if (!fallback) return false;
        next->recipes.fill(DefaultKey);
        next->samplers.fill(fallback);
        next->slots.emplace(DefaultKey, 0);
        return Publish(std::move(next), makeSet);
    }

    template<class MakeSampler, class MakeSet>
    bool Reconfigure(uint32_t level, MakeSampler&& makeSampler, MakeSet&& makeSet) {
        level = NormalizeLevel(level);
        if (!current_) return false;
        if (level == current_->level) return true;
        auto next = std::make_shared<Version>(*current_);
        next->level = level;
        for (const auto& [recipe, index] : next->slots) {
            const auto key = next->Key(index);
            if (key == current_->Key(index)) continue;
            auto sampler = makeSampler(key);
            if (!sampler) return false;
            next->samplers[index] = std::move(sampler);
        }
        return Publish(std::move(next), makeSet);
    }

    template<class MakeSampler, class MakeSet>
    std::optional<uint32_t> Select(uint64_t recipe, MakeSampler&& makeSampler, MakeSet&& makeSet) {
        if (!current_) return std::nullopt;
        if (const auto found = current_->slots.find(recipe); found != current_->slots.end()) {
            used_ |= uint64_t(1) << found->second;
            return found->second;
        }
        // Reuse a slot NOT referenced by the current draw. Older draws keep
        // their own immutable versions; new scenes cannot exhaust a global
        // palette forever or silently sample an unrelated slot zero.
        uint32_t index = uint32_t(current_->slots.size());
        if (index == Capacity) {
            for (index = 1; index < Capacity && (used_ & (uint64_t(1) << index)); ++index) {}
            if (index == Capacity) return std::nullopt;
        }
        auto sampler = makeSampler(EffectiveKey(recipe, current_->level));
        if (!sampler) return std::nullopt;
        auto next = std::make_shared<Version>(*current_);
        if (next->slots.size() == Capacity) next->slots.erase(next->recipes[index]);
        next->recipes[index] = recipe;
        next->samplers[index] = std::move(sampler);
        next->slots.emplace(recipe, index);
        if (!Publish(std::move(next), makeSet)) return std::nullopt;
        used_ |= uint64_t(1) << index;
        return index;
    }
private:
    template<class MakeSet>
    bool Publish(std::shared_ptr<Version> next, MakeSet&& makeSet) {
        // Do not write the old table, even on a failed candidate construction.
        auto descriptors = makeSet(next->samplers);
        if (!descriptors) return false;
        next->descriptors = std::move(descriptors);
        current_ = std::move(next);
        return true;
    }
    Lease current_;
    uint64_t used_ = 1;
};

// GPU-slot ownership: clearing one completed slot must not release a version
// still referenced by another in-flight slot or the palette's current table.
template<class Lease>
void Retain(std::vector<Lease>& uses, const Lease& version) {
    if (version && (uses.empty() || uses.back() != version)) uses.push_back(version);
}
} // namespace gpu::sampling
