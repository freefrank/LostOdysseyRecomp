#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <unordered_map>

namespace gpu::texture_descriptors {
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
