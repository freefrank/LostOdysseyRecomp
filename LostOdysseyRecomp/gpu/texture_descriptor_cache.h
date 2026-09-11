#pragma once

#include <array>
#include <cstddef>
#include <functional>
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
            size_t h = 0;
            for (auto* texture : key)
                h ^= std::hash<Texture*>{}(texture) + size_t(0x9e3779b9) + (h << 6) + (h >> 2);
            return h;
        }
    };
    std::unordered_map<Key, Set*, Hash> entries;
public:
    template<class Create>
    Set* Acquire(const Key& key, Create&& create, bool& reused) {
        const auto found = entries.find(key);
        reused = found != entries.end();
        if (reused) return found->second;
        Set* set = create();
        entries.emplace(key, set);
        return set;
    }
    void Clear() { entries.clear(); }
};
}
