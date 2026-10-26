#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace gpu::texture_descriptors {
// A pooled set keeps its views across completed-fence intervals. The mask names
// the bindings that may hold something other than the bank's dummy texture;
// a set whose other bindings are unknown uses kUnknownBindings.
constexpr uint32_t kUnknownBindings = ~uint32_t(0);

template<class Texture, size_t Slots>
uint32_t NonDummyBindings(const std::array<Texture*, Slots>& key, const Texture* dummy,
    uint32_t candidates = kUnknownBindings)
{
    static_assert(Slots <= 32);
    uint32_t mask = 0;
    if (candidates != kUnknownBindings) {
        // The draw already knows which bindings its shaders can populate.
        // Inactive slots must still name the dummy, as in the non-cache path.
#ifndef NDEBUG
        for (size_t slot = 0; slot < Slots; ++slot)
            assert(((candidates >> slot) & 1) || key[slot] == dummy);
#endif
        if constexpr (Slots < 32) candidates &= (uint32_t(1) << Slots) - 1;
        for (; candidates; candidates &= candidates - 1) {
            const uint32_t slot = uint32_t(std::countr_zero(candidates));
            if (key[slot] != dummy) mask |= uint32_t(1) << slot;
        }
        return mask;
    }
    for (size_t slot = 0; slot < Slots; ++slot)
        if (key[slot] != dummy) mask |= uint32_t(1) << slot;
    return mask;
}

// Writes the bindings that make a pooled set equal to `key`: those it may still
// hold from earlier use plus the key's own non-dummy bindings. Every other
// binding already holds the dummy the key names. Returns the set's new mask.
template<class Texture, size_t Slots, class Write>
uint32_t RewriteBindings(uint32_t previous, const std::array<Texture*, Slots>& key,
    const Texture* dummy, Write&& write, uint32_t candidates = kUnknownBindings)
{
    const uint32_t next = NonDummyBindings(key, dummy, candidates);
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
//
// Open addressing over a power-of-two table whose entries carry the generation
// they were added in, so Clear() is a counter bump and the table and the key
// storage keep their memory: a node map allocated every set of every frame and
// freed it all again at each fence. Keys are compared slot by slot with 8-byte
// loads; the caller has just stored the slots one by one, and a wider load across
// several of those stores cannot be store-forwarded.
template<class Texture, class Set, size_t Slots>
class BatchCache {
public:
    using Key = std::array<Texture*, Slots>;
private:
    struct Entry { uint64_t hash = 0; uint32_t generation = 0; uint32_t key = 0; Set* set = nullptr; };
    std::vector<Entry> table = std::vector<Entry>(256);
    std::vector<Key> keys; // this generation's keys, by Entry::key
    uint32_t generation = 1;
    const Entry* last = nullptr;

    // Pointer identity only, in four independent lanes (registers, not an array)
    // instead of one 32-step multiply chain.
    static uint64_t HashOf(const Key& key) {
        static_assert(Slots % 4 == 0);
        constexpr uint64_t kPrime = 1099511628211ull;
        uint64_t lane0 = 14695981039346656037ull, lane1 = 0x9e3779b97f4a7c15ull;
        uint64_t lane2 = 0xc2b2ae3d27d4eb4full, lane3 = 0x165667b19e3779f9ull;
        const auto word = [&](size_t i) { return uint64_t(reinterpret_cast<uintptr_t>(key[i])); };
        for (size_t i = 0; i < Slots; i += 4) {
            lane0 = (lane0 ^ word(i)) * kPrime;
            lane1 = (lane1 ^ word(i + 1)) * kPrime;
            lane2 = (lane2 ^ word(i + 2)) * kPrime;
            lane3 = (lane3 ^ word(i + 3)) * kPrime;
        }
        const uint64_t h = (((lane0 ^ lane1) * kPrime ^ lane2) * kPrime) ^ lane3;
        return h ^ (h >> 29);
    }
    static bool Equal(const Key& a, const Key& b) {
        for (size_t i = 0; i < Slots; ++i)
            if (a[i] != b[i]) return false;
        return true;
    }
    Entry& Insert(uint64_t hash, uint32_t keyIndex, Set* set) {
        const size_t mask = table.size() - 1;
        size_t i = hash & mask;
        while (table[i].generation == generation) i = (i + 1) & mask;
        table[i] = {hash, generation, keyIndex, set};
        return table[i];
    }
public:
    template<class Create>
    Set* Acquire(const Key& key, Create&& create, bool& reused) {
        if (last && Equal(keys[last->key], key)) {
            reused = true;
            return last->set;
        }
        const uint64_t hash = HashOf(key);
        const size_t mask = table.size() - 1;
        for (size_t i = hash & mask; table[i].generation == generation; i = (i + 1) & mask) {
            const Entry& entry = table[i];
            if (entry.hash == hash && Equal(keys[entry.key], key)) {
                reused = true;
                last = &entry;
                return entry.set;
            }
        }
        reused = false;
        Set* set = create();
        if ((keys.size() + 1) * 2 > table.size()) {
            // Keep the load at most one half: rebuild this generation's entries.
            std::vector<Entry> old(table.size() * 2);
            old.swap(table);
            for (const Entry& entry : old)
                if (entry.generation == generation) Insert(entry.hash, entry.key, entry.set);
        }
        keys.push_back(key);
        last = &Insert(hash, uint32_t(keys.size() - 1), set);
        return set;
    }
    void Clear() {
        if (++generation == 0) {
            // Wrapped: entries of generation 0 would read as current.
            std::fill(table.begin(), table.end(), Entry{});
            generation = 1;
        }
        keys.clear();
        last = nullptr;
    }
};
}
