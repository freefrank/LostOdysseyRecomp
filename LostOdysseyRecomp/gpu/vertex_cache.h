#pragma once

#include "geometry_prepare.h"
#include "guest_writes.h"
#include <ankerl/unordered_dense.h>
#include <algorithm>

namespace gpu::geometry_prepare
{
    // Guest write state of a cached source when it was taken: the newest noted
    // write in its range and the published note count read before that.
    struct SourceStamp
    {
        uint32_t generation = 0;
        uint32_t checked = 0;
    };

    // Call before reading the source bytes, so a write noted while they are
    // read makes the next lookup take them again.
    inline SourceStamp TakeStamp(uint32_t physical, size_t bytes)
    {
        SourceStamp stamp;
        stamp.checked = guest_writes::Published();
        stamp.generation = guest_writes::Newest(physical & 0x1FFFFFFFu, uint32_t(bytes));
        return stamp;
    }

    // True while no guest write was noted in the source's range since its
    // stamp was taken. Without any new note anywhere, one load decides.
    inline bool Unchanged(SourceStamp& stamp, uint32_t physical, size_t bytes)
    {
        const uint32_t published = guest_writes::Published();
        if (stamp.checked == published) return true;
        if (guest_writes::Newest(physical & 0x1FFFFFFFu, uint32_t(bytes)) > stamp.generation) return false;
        stamp.checked = published;
        return true;
    }

    struct VertexEntry
    {
        uint64_t offset;
        uint64_t lastFrame;
        uint8_t slot = 0;
        SourceStamp stamp;
    };

    // This is lookup metadata, not ownership of arena bytes. Discarding an
    // entry makes a future fetch upload again; recorded GPU offsets stay valid.
    // Reserve a bounded working set once, so city exploration cannot trigger a
    // whole-table rehash (262K -> 524K buckets cost 42 ms in the city capture).
    class VertexCache
    {
        using Map = ankerl::unordered_dense::map<uint64_t, VertexEntry>;
        Map entries;
        size_t capacity;
        size_t evictionCursor = 0;
        uint64_t evictions = 0;
        // Changes whenever an entry is added, removed or refreshed: a pointer
        // taken at an unchanged version still names the same, unchanged entry.
        uint64_t version = 1;

    public:
        static constexpr size_t kCapacity = 65536;
        static constexpr size_t kEvictionCandidates = 16;
        explicit VertexCache(size_t limit = kCapacity)
            : capacity(std::max(size_t(1), limit))
        {
            entries.reserve(capacity);
        }
        auto begin() { return entries.begin(); }
        auto end() { return entries.end(); }
        auto find(uint64_t key) { return entries.find(key); }
        auto erase(Map::iterator it) {
            ++version;
            return entries.erase(it);
        }
        // The caller gave an entry that stays cached a new arena copy and stamp.
        void Refreshed() { ++version; }
        uint64_t Version() const { return version; }
        size_t size() const { return entries.size(); }
        size_t bucket_count() const { return entries.bucket_count(); }
        uint64_t Evictions() const { return evictions; }

        void emplace(uint64_t key, VertexEntry&& entry)
        {
            // Caller already checked/removed this key; no iterators or entry
            // references survive insertion. Sample a rotating bounded window,
            // keeping recently used buffers without a per-hit LRU list update.
            while (entries.size() == capacity)
            {
                size_t victim = evictionCursor % entries.size();
                const size_t count = std::min(kEvictionCandidates, entries.size());
                for (size_t i = 1; i < count; ++i)
                {
                    const size_t candidate = (evictionCursor + i) % entries.size();
                    if ((entries.begin() + candidate)->second.lastFrame <
                        (entries.begin() + victim)->second.lastFrame)
                        victim = candidate;
                }
                evictionCursor = (evictionCursor + count) % entries.size();
                erase(entries.begin() + victim);
                ++evictions;
            }
            entries.emplace(key, std::move(entry));
            ++version;
        }
    };

    // Key for the index conversion cache: the guest source extent plus the
    // conversion parameters. The primitive type is part of the key because
    // quad/fan expansion changes the cached output.
    struct IndexKey
    {
        uint32_t base = 0;
        uint32_t count = 0;
        uint32_t prim = 0;
        uint8_t wide = 0;
        uint8_t endian = 0;
        bool operator==(const IndexKey& other) const = default;
    };
    struct IndexKeyHash
    {
        uint64_t operator()(const IndexKey& key) const noexcept
        {
            uint64_t hash = key.base;
            hash ^= uint64_t(key.count) + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
            hash ^= uint64_t(key.prim) + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
            hash ^= uint64_t(key.wide | (key.endian << 1)) + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
            return hash;
        }
    };
    struct IndexEntry
    {
        // Final post-expansion indices, ready to upload.
        std::vector<uint32_t> data;
        // Lazily computed on a motion draw; an entry is replaced when its
        // source changes, so this fingerprint describes the cached output.
        uint64_t motionIndexHash = 0;
        bool motionIndexHashReady = false;
        uint64_t lastFrame = 0;
        // Last upload of data: its upload-ring offset, valid while the
        // renderer's ring generation still equals ringGeneration (0: none).
        uint64_t ringOffset = 0;
        uint64_t ringGeneration = 0;
        // Copy of data in the vertex arena, valid while that arena half's reset
        // generation still equals arenaGeneration (UINT64_MAX: none).
        uint64_t arenaOffset = UINT64_MAX;
        uint64_t arenaGeneration = 0;
        uint8_t arenaSlot = 0;
        SourceStamp stamp;
        size_t AllocatedBytes() const { return data.capacity() * sizeof(uint32_t); }
    };

    // Converted index buffers are host-side scratch copies, so unlike the
    // vertex arena no slot purge is needed: entries stay valid across wraps.
    // Only large buffers are cached: below kMinCount a direct conversion is
    // cheaper than the lookup.
    class IndexCache
    {
        using Map = ankerl::unordered_dense::map<IndexKey, IndexEntry, IndexKeyHash>;
        Map entries;
        size_t capacity;
        size_t byteCapacity;
        size_t allocatedBytes = 0;
        size_t peakBytes = 0;
        size_t evictionCursor = 0;
        uint64_t evictions = 0;

    public:
        static constexpr size_t kCapacity = 4096;
        static constexpr size_t kByteCapacity = 64ull << 20;
        static constexpr uint32_t kMinCount = 256;
        static constexpr size_t kEvictionCandidates = 16;
        explicit IndexCache(size_t limit = kCapacity, size_t bytes = kByteCapacity)
            : capacity(std::max(size_t(1), limit)), byteCapacity(bytes)
        {
            entries.reserve(capacity);
        }
        auto begin() { return entries.begin(); }
        auto end() { return entries.end(); }
        auto find(const IndexKey& key) { return entries.find(key); }
        size_t size() const { return entries.size(); }
        size_t bucket_count() const { return entries.bucket_count(); }
        uint64_t Evictions() const { return evictions; }
        size_t AllocatedBytes() const { return allocatedBytes; }
        size_t PeakBytes() const { return peakBytes; }

        auto erase(Map::iterator it)
        {
            allocatedBytes -= it->second.AllocatedBytes();
            return entries.erase(it);
        }

        void emplace(const IndexKey& key, IndexEntry&& entry)
        {
            // Replacements must update accounting too. No iterator or entry
            // reference may survive this call. Oversized entries bypass cache.
            if (auto old = entries.find(key); old != entries.end()) erase(old);
            const size_t bytes = entry.AllocatedBytes();
            if (bytes > byteCapacity) return;
            while (!entries.empty() &&
                (entries.size() == capacity || allocatedBytes > byteCapacity - bytes))
            {
                size_t victim = evictionCursor % entries.size();
                const size_t count = std::min(kEvictionCandidates, entries.size());
                for (size_t i = 1; i < count; ++i)
                {
                    const size_t candidate = (evictionCursor + i) % entries.size();
                    if ((entries.begin() + candidate)->second.lastFrame <
                        (entries.begin() + victim)->second.lastFrame)
                        victim = candidate;
                }
                evictionCursor = (evictionCursor + count) % entries.size();
                erase(entries.begin() + victim);
                ++evictions;
            }
            const auto [it, inserted] = entries.emplace(key, std::move(entry));
            if (inserted)
            {
                allocatedBytes += bytes;
                peakBytes = std::max(peakBytes, allocatedBytes);
            }
        }
    };
}
