#pragma once

#include "geometry_prepare.h"
#include <ankerl/unordered_dense.h>
#include <algorithm>

namespace gpu::geometry_prepare
{
    struct VertexEntry
    {
        uint64_t offset;
        VertexSampledContent content;
        uint64_t lastFrame;
        uint8_t slot = 0;
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
        size_t byteCapacity;
        size_t capturedBytes = 0;
        size_t evictionCursor = 0;
        uint64_t evictions = 0;

    public:
        static constexpr size_t kCapacity = 65536;
        static constexpr size_t kEvictionCandidates = 16;
        static constexpr size_t kByteCapacity = 256ull << 20;
        explicit VertexCache(size_t limit = kCapacity, size_t bytes = kByteCapacity)
            : capacity(std::max(size_t(1), limit)), byteCapacity(bytes)
        {
            entries.reserve(capacity);
        }
        auto begin() { return entries.begin(); }
        auto end() { return entries.end(); }
        auto find(uint64_t key) { return entries.find(key); }
        auto erase(Map::iterator it) {
            capturedBytes -= it->second.content.AllocatedBytes();
            return entries.erase(it);
        }
        size_t CapturedBytes() const { return capturedBytes; }
        size_t size() const { return entries.size(); }
        size_t bucket_count() const { return entries.bucket_count(); }
        uint64_t Evictions() const { return evictions; }

        void emplace(uint64_t key, VertexEntry&& entry)
        {
            // Caller already checked/removed this key; no iterators or entry
            // references survive insertion. Sample a rotating bounded window,
            // keeping recently used buffers without a per-hit LRU list update.
            const size_t bytes = entry.content.AllocatedBytes();
            // Oversized buffers remain usable for this draw, but are not cached.
            if (bytes > byteCapacity) return;
            while (!entries.empty() &&
                (entries.size() == capacity || capturedBytes > byteCapacity - bytes))
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
            if (inserted) capturedBytes += bytes;
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
        // Source guest bytes the conversion was built from.
        SampledContent content;
        // Lazily computed on a motion draw; exact source validation above
        // ensures this fingerprint still describes the cached output.
        uint64_t motionIndexHash = 0;
        bool motionIndexHashReady = false;
        uint64_t lastFrame = 0;
        size_t AllocatedBytes() const { return content.AllocatedBytes() + data.capacity() * sizeof(uint32_t); }
    };

    // Converted index buffers are host-side scratch copies, so unlike the
    // vertex arena no slot purge is needed: entries stay valid across wraps.
    // Only large buffers are cached: below kMinCount a direct conversion is
    // cheaper than the lookup plus content validation.
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
