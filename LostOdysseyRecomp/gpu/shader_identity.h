#pragma once

#include <array>
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

    // In-memory index of a microcode snapshot: FNV-1a over the words in four
    // interleaved lanes, folded in lane order (shorter multiply chains). The
    // lanes live in registers; an array indexed by i & 3 went through memory.
    inline uint64_t CommandWordHash(const uint32_t* words, uint32_t count)
    {
        constexpr uint64_t kPrime = 0x100000001b3ull, kBasis = 0xcbf29ce484222325ull;
        uint64_t lane0 = kBasis, lane1 = kBasis ^ 1, lane2 = kBasis ^ 2, lane3 = kBasis ^ 3;
        uint32_t i = 0;
        for (; i + 4 <= count; i += 4) {
            lane0 = (lane0 ^ words[i]) * kPrime;
            lane1 = (lane1 ^ words[i + 1]) * kPrime;
            lane2 = (lane2 ^ words[i + 2]) * kPrime;
            lane3 = (lane3 ^ words[i + 3]) * kPrime;
        }
        if (i < count) lane0 = (lane0 ^ words[i]) * kPrime;
        if (i + 1 < count) lane1 = (lane1 ^ words[i + 1]) * kPrime;
        if (i + 2 < count) lane2 = (lane2 ^ words[i + 2]) * kPrime;
        uint64_t hash = lane0;
        hash = (hash ^ lane1) * kPrime;
        hash = (hash ^ lane2) * kPrime;
        return (hash ^ lane3) * kPrime;
    }

    // IM_LOAD captures microcode on the command-processor thread. Guest memory
    // may change at any time, but draws consume this owned snapshot until the
    // next load. Incoming bytes are revalidated at a load (see Load for when a
    // compare is skipped), then the renderer identity is resolved only once
    // for that snapshot, even across cache eviction. Snapshots are kept per
    // source address: a shader loaded again from the same place reuses its copy
    // and both hashes once all its bytes compare equal. Words and both hashes
    // depend only on the bytes, never on which snapshot holds them.
    class CapturedShader
    {
        struct Snapshot
        {
            std::vector<uint32_t> words;
            uint64_t commandHash = 0;
            uint64_t rendererHash = 0;
            bool rendererHashValid = false;
            const uint32_t* source = nullptr; // IM_LOAD address; null for inline words
            uint64_t validatedEpoch = 0;      // sync epoch in which `source` last matched `words`
        };
        // Node-based: `current` stays valid while other sources are added.
        std::unordered_map<const uint32_t*, Snapshot> snapshots;
        // Sources validated recently, by address: a repeat in the same sync
        // epoch skips the map. Valid while `generation` is unchanged.
        struct Recent { const uint32_t* source = nullptr; Snapshot* snapshot = nullptr; uint64_t generation = 0; };
        std::array<Recent, 256> recent{};
        uint64_t generation = 1; // advances when snapshots are erased
        // IM_LOAD_IMMEDIATE words sit in the command stream, which is reused for
        // other packets, so their address names nothing: kept by content.
        std::array<Snapshot, 8> immediates;
        size_t immediateNext = 0;
        Snapshot none; // before the first load
        Snapshot* current = &none;
        size_t storedWords = 0;
        static constexpr size_t kStoredWordLimit = 2u << 20; // 8 MiB of microcode

        Recent& RecentFor(const uint32_t* source)
        {
            return recent[(uint64_t(reinterpret_cast<uintptr_t>(source)) * 0x9E3779B97F4A7C15ull) >> 56];
        }
        void Validated(Snapshot& snapshot, uint64_t epoch)
        {
            snapshot.validatedEpoch = epoch;
            if (epoch) RecentFor(snapshot.source) = {snapshot.source, &snapshot, generation};
        }

    public:
        CapturedShader() = default;
        CapturedShader(const CapturedShader&) = delete;
        CapturedShader& operator=(const CapturedShader&) = delete;

        // IM_LOAD of `count` words at guest address `source`. `epoch` is
        // CommandProcessor::SyncEpoch(), or 0 to compare on every load.
        //
        // A source whose bytes matched its snapshot earlier in the same sync
        // epoch is taken as unchanged without another compare. An IM_LOAD names
        // microcode the guest keeps in memory (a D3D shader object), and the
        // guest rewrites memory the GPU reads only after the GPU has signalled
        // it is done with it: a fence or memory write, an interrupt, a swap or
        // the read-pointer writeback, each of which ends the epoch. Only a guest
        // write racing the GPU could change the bytes within an epoch, and it
        // is seen at the next one: the premise of the vertex and index source
        // checks (Renderer::SourceMatches). LoadImmediate keeps comparing.
        bool Load(const uint32_t* source, uint32_t count, uint64_t epoch = 0)
        {
            if (!count || count > 0x10000) return false;
            const size_t size = size_t(count) * sizeof(uint32_t);
            if (epoch) {
                const Recent& known = RecentFor(source);
                if (known.source == source && known.generation == generation &&
                    known.snapshot->validatedEpoch == epoch && known.snapshot->words.size() == count) {
                    if (known.snapshot == current) return false;
                    current = known.snapshot;
                    return true;
                }
            }
            if (current->words.size() == count && !std::memcmp(current->words.data(), source, size)) {
                if (current->source == source) Validated(*current, epoch);
                return false;
            }
            auto [it, inserted] = snapshots.try_emplace(source);
            Snapshot& snapshot = it->second;
            if (!inserted && snapshot.words.size() == count && !std::memcmp(snapshot.words.data(), source, size)) {
                Validated(snapshot, epoch);
                current = &snapshot;
                return true;
            }
            if (storedWords - snapshot.words.size() + count > kStoredWordLimit) {
                // Bounded: drop every other source (the one being loaded stays).
                for (auto other = snapshots.begin(); other != snapshots.end();)
                    other = other == it ? std::next(other) : snapshots.erase(other);
                storedWords = snapshot.words.size();
                ++generation;
            }
            storedWords = storedWords - snapshot.words.size() + count;
            snapshot.words.assign(source, source + count);
            snapshot.commandHash = CommandWordHash(snapshot.words.data(), count);
            snapshot.rendererHashValid = false;
            snapshot.source = source;
            Validated(snapshot, epoch);
            current = &snapshot;
            return true;
        }

        // IM_LOAD_IMMEDIATE: `words` are the packet's inline payload.
        bool LoadImmediate(const uint32_t* words, uint32_t count)
        {
            if (!count || count > 0x10000) return false;
            const size_t size = size_t(count) * sizeof(uint32_t);
            if (current->words.size() == count && !std::memcmp(current->words.data(), words, size))
                return false;
            for (auto& snapshot : immediates)
                if (snapshot.words.size() == count && !std::memcmp(snapshot.words.data(), words, size)) {
                    current = &snapshot;
                    return true;
                }
            Snapshot& snapshot = immediates[immediateNext];
            immediateNext = (immediateNext + 1) % immediates.size();
            snapshot.words.assign(words, words + count);
            snapshot.commandHash = CommandWordHash(snapshot.words.data(), count);
            snapshot.rendererHashValid = false;
            current = &snapshot;
            return true;
        }

        const uint32_t* Words() const { return current->words.empty() ? nullptr : current->words.data(); }
        uint32_t Count() const { return uint32_t(current->words.size()); }
        uint64_t CommandHash() const { return current->commandHash; }

        template<class IdentityCache>
        uint64_t RendererHash(IdentityCache& cache)
        {
            if (!current->rendererHashValid) {
                current->rendererHash = cache.Get(current->commandHash, Words(), Count());
                current->rendererHashValid = true;
            }
            return current->rendererHash;
        }
    };
}
