#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace gpu::shader_identity
{
    // The command-word hash is only an index. Compare all copied microcode bytes
    // before reusing the renderer-byte hash, including on command-hash collisions.
    class Cache
    {
        struct Entry { std::vector<uint32_t> words; uint64_t hash; };
        std::unordered_map<uint64_t, std::vector<Entry>> entries;
        size_t bytes = 0;
        size_t limit;

    public:
        explicit Cache(size_t byteLimit = 16 * 1024 * 1024) : limit(byteLimit) {}

        uint64_t Get(uint64_t commandHash, const uint32_t* words, uint32_t count)
        {
            const size_t size = size_t(count) * sizeof(uint32_t);
            if (auto it = entries.find(commandHash); it != entries.end())
                for (const auto& entry : it->second)
                    if (entry.words.size() == count && (!size || !std::memcmp(entry.words.data(), words, size)))
                        return entry.hash;

            uint64_t hash = 0xcbf29ce484222325ull;
            const auto* data = reinterpret_cast<const uint8_t*>(words);
            for (size_t i = 0; i < size; ++i) { hash ^= data[i]; hash *= 0x100000001b3ull; }
            // Bound this optional CPU cache independently of GPU shader lifetime.
            const size_t cost = size + sizeof(Entry) + 64;
            if (cost > limit) return hash;
            if (cost > limit - bytes) { entries.clear(); bytes = 0; }
            Entry entry{{}, hash};
            if (count) entry.words.assign(words, words + count);
            entries[commandHash].push_back(std::move(entry));
            bytes += cost;
            return hash;
        }
    };
}
