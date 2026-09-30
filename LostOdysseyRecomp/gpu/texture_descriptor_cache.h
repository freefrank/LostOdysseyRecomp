#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <unordered_map>

namespace gpu::texture_descriptors {
// A pooled set keeps its views across completed-fence intervals. The mask names
// the bindings that may hold something other than the bank's dummy texture;
// a set whose other bindings are unknown uses kUnknownBindings.
constexpr uint32_t kUnknownBindings = ~uint32_t(0);

template<class Texture, size_t Slots>
uint32_t NonDummyBindings(const std::array<Texture*, Slots>& key, const Texture* dummy)
{
    static_assert(Slots <= 32);
    uint32_t mask = 0;
    for (size_t slot = 0; slot < Slots; ++slot)
        if (key[slot] != dummy) mask |= uint32_t(1) << slot;
    return mask;
}

// Writes the bindings that make a pooled set equal to `key`: those it may still
// hold from earlier use plus the key's own non-dummy bindings. Every other
// binding already holds the dummy the key names. Returns the set's new mask.
template<class Texture, size_t Slots, class Write>
uint32_t RewriteBindings(uint32_t previous, const std::array<Texture*, Slots>& key,
    const Texture* dummy, Write&& write)
{
    const uint32_t next = NonDummyBindings(key, dummy);
    uint32_t pending = previous | next;
    if constexpr (Slots < 32) pending &= (uint32_t(1) << Slots) - 1;
    for (; pending; pending &= pending - 1) {
        const auto slot = uint32_t(std::countr_zero(pending));
        write(slot, key[slot]);
    }
    return next;
}

// One cache per descriptor bank and completed-fence interval. A hit never
// updates descriptors: their full contents remain immutable until Clear().
template<class Texture, class Set, size_t Slots>
class BatchCache {
public:
    using Key = std::array<Texture*, Slots>;
private:
    struct Hash {
        size_t operator()(const Key& key) const {
            // Pointer identity only. FNV-1a over the slot array beats 32 separate
            // std::hash mixes on the per-draw lookup (city: ~1800 draws × 3 banks).
            size_t h = size_t(14695981039346656037ull);
            for (auto* texture : key) {
                auto p = reinterpret_cast<uintptr_t>(texture);
                h ^= static_cast<size_t>(p);
                h *= size_t(1099511628211ull);
            }
            return h;
        }
    };
    std::unordered_map<Key, Set*, Hash> entries;
    Key lastKey{};
    Set* lastSet = nullptr;
    bool hasLast = false;
public:
    template<class Create>
    Set* Acquire(const Key& key, Create&& create, bool& reused) {
        if (hasLast && lastKey == key) {
            reused = true;
            return lastSet;
        }
        const auto found = entries.find(key);
        reused = found != entries.end();
        Set* set = reused ? found->second : create();
        if (!reused) entries.emplace(key, set);
        lastKey = key;
        lastSet = set;
        hasLast = true;
        return set;
    }
    void Clear() {
        entries.clear();
        hasLast = false;
        lastSet = nullptr;
    }
};
}
