#include "gpu/vertex_cache.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
using gpu::geometry_prepare::VertexCache;
using gpu::geometry_prepare::VertexEntry;

uint64_t checks = 0;

void Check(bool condition, const char* name, uint64_t key = 0)
{
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s (key=%llu, check=%llu)\n", name,
            static_cast<unsigned long long>(key), static_cast<unsigned long long>(checks));
        std::exit(1);
    }
}

auto Bytes(uint64_t key)
{
    std::array<uint8_t, 24> bytes{};
    for (size_t i = 0; i < bytes.size(); ++i)
        bytes[i] = uint8_t((key >> ((i % 8) * 8)) ^ (i * 29 + 71));
    return bytes;
}

uint64_t Offset(uint64_t key) { return 0x100000000ull + key * 64; }

VertexEntry Entry(uint64_t key)
{
    VertexEntry entry{Offset(key), {}, key + 123, uint8_t(key & 1)};
    const auto source = Bytes(key);
    entry.content.Capture(source.data(), source.size());
    // The source dies here. Surviving entries must own their captured samples.
    return entry;
}

void CheckEntry(uint64_t key, const VertexEntry& entry, bool checkFrame = true)
{
    auto bytes = Bytes(key);
    Check(entry.offset == Offset(key), "offset still belongs to key", key);
    Check(entry.slot == uint8_t(key & 1), "slot still belongs to key", key);
    if (checkFrame) Check(entry.lastFrame == key + 123, "frame still belongs to key", key);
    Check(entry.content.Matches(bytes.data(), bytes.size()), "captured samples still belong to key", key);
    bytes[0] ^= 0x80;
    Check(!entry.content.Matches(bytes.data(), bytes.size()), "different source is not a cache hit", key);
}

void CheckAll(VertexCache& cache)
{
    size_t visited = 0;
    for (auto& [key, entry] : cache) {
        auto found = cache.find(key);
        Check(found != cache.end(), "iterated key can be found", key);
        Check(&found->second == &entry, "lookup and iteration identify same entry", key);
        CheckEntry(key, entry);
        ++visited;
    }
    Check(visited == cache.size(), "iteration covers current size");
}

void TestChurn(size_t requestedCapacity)
{
    VertexCache cache(requestedCapacity);
    const size_t capacity = std::max(size_t(1), requestedCapacity);
    const size_t buckets = cache.bucket_count();
    Check(buckets != 0, "buckets reserved before first insertion");
    uintptr_t initialStorage = 0;
    const size_t inserts = capacity * 3 + 37;
    for (size_t i = 0; i < inserts; ++i) {
        const uint64_t key = i + 1;
        Check(cache.find(key) == cache.end(), "caller inserts only absent keys", key);
        cache.emplace(key, Entry(key));
        Check(cache.size() == std::min(i + 1, capacity), "churn respects capacity", key);
        Check(cache.bucket_count() == buckets, "churn never grows buckets", key);
        Check(cache.Evictions() == (i + 1 > capacity ? i + 1 - capacity : 0), "one eviction per full-cache insertion", key);
        const auto storage = reinterpret_cast<uintptr_t>(&*cache.begin());
        if (i == 0) initialStorage = storage;
        Check(storage == initialStorage, "reserved value storage does not relocate", key);
        auto found = cache.find(key);
        Check(found != cache.end(), "new key admitted at capacity", key);
        CheckEntry(key, found->second);
        if (i + 1 == capacity || i + 1 == capacity * 2) CheckAll(cache);
    }
    CheckAll(cache);
    std::printf("churn capacity=%zu inserts=%zu final_size=%zu buckets=%zu evictions=%llu\n",
        requestedCapacity, inserts, cache.size(), buckets,
        static_cast<unsigned long long>(cache.Evictions()));
}

void TestRecentUse(size_t capacity)
{
    VertexCache cache(capacity);
    for (uint64_t key = 1; key <= capacity; ++key) cache.emplace(key, Entry(key));
    // Simulate GetVertexBuffer's hit update. The oldest inserted key is now the
    // most recently used candidate and must survive replacement by colder keys.
    cache.find(1)->second.lastFrame = 1000000;
    cache.emplace(capacity + 1, Entry(capacity + 1));
    Check(cache.find(2) == cache.end(), "colder candidate evicted instead of touched key");
    for (uint64_t key = capacity + 2; key < capacity * 10 + 2; ++key) {
        auto hot = cache.find(1);
        Check(hot != cache.end(), "hot key survives prior candidate windows");
        hot->second.lastFrame = 1000000 + key;
        cache.emplace(key, Entry(key));
        hot = cache.find(1);
        Check(hot != cache.end(), "hot candidate retained after insertion");
        CheckEntry(1, hot->second, false);
        Check(hot->second.lastFrame == 1000000 + key, "lastFrame hit update survives relocation");
    }
}

void ResetSlot(VertexCache& cache, uint8_t slot)
{
    // This is the renderer's erase-while-iterating pattern. erase moves the
    // last dense value into this position; the returned iterator must be revisited.
    for (auto it = cache.begin(); it != cache.end();) {
        if (it->second.slot == slot) it = cache.erase(it);
        else ++it;
    }
}

void TestSlotReset(size_t capacity)
{
    VertexCache cache(capacity);
    const size_t buckets = cache.bucket_count();
    for (uint64_t key = 1; key <= capacity * 3 + 5; ++key) cache.emplace(key, Entry(key));
    std::vector<uint64_t> before;
    size_t survivors = 0;
    for (auto& [key, entry] : cache) {
        before.push_back(key);
        survivors += entry.slot == 1;
    }
    ResetSlot(cache, 0);
    Check(cache.size() == survivors, "reset removes every matching slot including swapped-back values");
    for (auto key : before) {
        auto found = cache.find(key);
        Check((found != cache.end()) == bool(key & 1), "reset preserves exactly the other slot", key);
        if (found != cache.end()) CheckEntry(key, found->second);
    }
    CheckAll(cache);
    ResetSlot(cache, 1);
    Check(cache.size() == 0 && cache.begin() == cache.end(), "second slot reset empties cache");
    Check(cache.bucket_count() == buckets, "slot reset preserves preallocated buckets");
    const uint64_t evictionsBeforeRefill = cache.Evictions();
    // All entries have slot 0, forcing repeated deletion at the same dense
    // position. Refill also exercises a nonzero eviction cursor after a reset.
    for (uint64_t i = 0; i < capacity; ++i) {
        const uint64_t key = 1000000 + i * 2;
        cache.emplace(key, Entry(key));
    }
    Check(cache.Evictions() == evictionsBeforeRefill, "refill below capacity does not evict");
    CheckAll(cache);
    ResetSlot(cache, 0);
    Check(cache.size() == 0, "same-slot swapped-back entries are not skipped");
    ResetSlot(cache, 1);
    for (uint64_t i = 0; i < capacity + 3; ++i) {
        const uint64_t key = 2000000 + i;
        cache.emplace(key, Entry(key));
    }
    Check(cache.size() == capacity && cache.bucket_count() == buckets, "churn remains bounded after repeated resets");
    CheckAll(cache);
}

void TestContentReplacement(size_t bytes)
{
    constexpr uint64_t key = 90000;
    VertexCache cache(4);
    auto source = std::vector<uint8_t>(bytes);
    for (size_t i = 0; i < bytes; ++i) source[i] = uint8_t(i * 31 + 17);
    VertexEntry original{123456, {}, 1000000, 0};
    original.content.Capture(source.data(), source.size());
    cache.emplace(key, std::move(original));
    for (uint64_t other = 1; other <= 3; ++other) cache.emplace(other, Entry(other));
    const auto oldSource = source;
    source.front() ^= 0x55;
    auto found = cache.find(key);
    Check(!found->second.content.Matches(source.data(), source.size()), "changed content forces replacement");
    cache.erase(found);
    // Same key, new source, arena offset and slot: no iterator survives erase or
    // emplace, matching the runtime's fresh-upload path.
    constexpr uint64_t replacementOffset = 0x7abc000040ull;
    VertexEntry replacement{replacementOffset, {}, 2000000, 1};
    replacement.content.Capture(source.data(), source.size());
    cache.emplace(key, std::move(replacement));
    const auto captured = source;
    std::fill(source.begin(), source.end(), 0);
    for (uint64_t other = 4; other < 100; ++other) {
        cache.emplace(other, Entry(other));
        found = cache.find(key);
        Check(found != cache.end(), "recent replacement survives churn");
        Check(found->second.offset == replacementOffset, "replacement offset survives dense movement");
        Check(found->second.slot == 1 && found->second.lastFrame == 2000000, "replacement slot and frame survive dense movement");
        Check(found->second.content.Matches(captured.data(), captured.size()), "replacement retains owned samples after source mutation");
        Check(!found->second.content.Matches(oldSource.data(), oldSource.size()), "replacement does not retain old samples");
    }
}

void TestIndexCache()
{
    using gpu::geometry_prepare::ConvertIndices;
    using gpu::geometry_prepare::IndexCache;
    using gpu::geometry_prepare::IndexEntry;
    using gpu::geometry_prepare::IndexKey;
    IndexCache cache(4);
    std::vector<uint8_t> src(64);
    for (size_t i = 0; i < src.size(); ++i) src[i] = uint8_t(i * 17 + 3);
    constexpr uint32_t count = 16;
    const size_t srcBytes = size_t(count) * 2;
    const IndexKey key{0x12000000u, count, 4u, 0u, 2u};
    Check(cache.find(key) == cache.end(), "absent index key misses");
    std::vector<uint32_t> converted(count);
    ConvertIndices(src.data(), converted.data(), count, false, 2u);
    IndexEntry entry;
    entry.data = converted;
    entry.content.Capture(src.data(), srcBytes);
    entry.lastFrame = 7;
    cache.emplace(key, std::move(entry));
    auto found = cache.find(key);
    Check(found != cache.end(), "stored index key hits");
    Check(found->second.data == converted, "cached output matches conversion");
    Check(found->second.content.Matches(src.data(), srcBytes), "unchanged index source matches");
    src[0] ^= 0x40;
    Check(!found->second.content.Matches(src.data(), srcBytes), "mutated index source misses");
    src[0] ^= 0x40;
    Check(cache.find(IndexKey{0x12000000u, count, 13u, 0u, 2u}) == cache.end(), "primitive type participates in index key");
    Check(cache.find(IndexKey{0x12000000u, count, 4u, 1u, 2u}) == cache.end(), "index width participates in index key");
    Check(cache.find(IndexKey{0x12000000u, count, 4u, 0u, 1u}) == cache.end(), "endian mode participates in index key");
    for (uint32_t i = 0; i < 100; ++i) {
        IndexEntry filler;
        filler.data = converted;
        filler.content.Capture(src.data(), srcBytes);
        filler.lastFrame = i;
        cache.emplace(IndexKey{0x13000000u + i, count, 4u, 0u, 2u}, std::move(filler));
    }
    Check(cache.size() == 4, "index churn respects capacity");
    Check(cache.bucket_count() != 0, "index buckets reserved before first insertion");
}

void TestIndexContentAndBudget()
{
    using namespace gpu::geometry_prepare;
    // This is the source extent used by a 16-bit indexed draw. The old
    // sampling scheme missed byte 601, within index 300.
    std::vector<uint8_t> source(16384, 0);
    const IndexKey key{0x12000000u, 8192u, 4u, 0u, 1u};
    IndexCache cache(4, 50000);
    IndexEntry initial;
    initial.data.resize(key.count);
    ConvertIndices(source.data(), initial.data.data(), key.count, false, key.endian);
    initial.content.Capture(source.data(), source.size());
    initial.lastFrame = 1;
    cache.emplace(key, std::move(initial));
    Check(cache.AllocatedBytes() == source.size() + key.count * sizeof(uint32_t), "index payload accounting");
    source[601] = 1;
    auto found = cache.find(key);
    Check(found != cache.end() && !found->second.content.Matches(source.data(), source.size()),
        "same-frame middle index mutation invalidates cached output");
    found->second.motionIndexHash = 0x1234;
    found->second.motionIndexHashReady = true;
    IndexEntry replacement;
    replacement.data.resize(key.count);
    ConvertIndices(source.data(), replacement.data.data(), key.count, false, key.endian);
    replacement.content.Capture(source.data(), source.size());
    replacement.lastFrame = 1;
    cache.emplace(key, std::move(replacement));
    found = cache.find(key);
    Check(found != cache.end() && found->second.content.Matches(source.data(), source.size()), "changed indices recaptured");
    Check(found->second.data[300] == 1, "changed index converted instead of reusing old output");
    Check(!found->second.motionIndexHashReady, "changed indices discard the cached motion signature");
    Check(cache.AllocatedBytes() == 49152 && cache.PeakBytes() == 49152, "replacement does not double count bytes");

    // A larger replacement for the same key cannot leave stale data behind.
    IndexEntry oversized;
    oversized.data.resize(10000);
    oversized.content.Capture(source.data(), source.size());
    cache.emplace(key, std::move(oversized));
    Check(cache.find(key) == cache.end() && cache.AllocatedBytes() == 0, "oversized replacement bypasses cache");

    IndexCache bounded(4, 1000);
    for (uint32_t i = 0; i < 10; ++i)
    {
        IndexEntry entry;
        entry.data.resize(128);
        entry.content.Capture(source.data(), 256);
        bounded.emplace(IndexKey{0x20000000u + i, 128u, 4u, 0u, 2u}, std::move(entry));
        Check(bounded.AllocatedBytes() <= 1000, "index cache stays within byte budget");
    }
    Check(bounded.size() == 1 && bounded.Evictions() == 9 && bounded.PeakBytes() <= 1000,
        "byte pressure evicts old index payloads");
}

void TestVertexSamplingPolicy()
{
    using namespace gpu::geometry_prepare;
    std::vector<uint8_t> source(16384, 0);
    VertexEntry vertex{};
    IndexEntry index{};
    vertex.content.Capture(source.data(), source.size());
    index.content.Capture(source.data(), source.size());
    source[601] = 1;
    Check(vertex.content.Matches(source.data(), source.size()), "vertex sampling accepts known blind spot by policy");
    Check(!index.content.Matches(source.data(), source.size()), "index validation remains exact despite vertex policy");
    source[601] = 0;
    for (size_t offset : {size_t(0), size_t(511), size_t(512), size_t(575), size_t(15872), size_t(16383)}) {
        source[offset] = 1;
        Check(!vertex.content.Matches(source.data(), source.size()), "vertex detects sampled changes");
        source[offset] = 0;
    }
    source.resize(8192);
    vertex.content.Capture(source.data(), source.size());
    source[601] = 1;
    Check(!vertex.content.Matches(source.data(), source.size()), "small vertex buffers remain exact");
}

void TestExactContent()
{
    using gpu::geometry_prepare::ExactContent;
    for (size_t bytes : {size_t(0), size_t(1), size_t(511), size_t(512), size_t(8192), size_t(8193), size_t(16384), size_t(65536)})
    {
        // Every source byte participates, including the former sample gaps.
        std::vector<uint8_t> source(bytes);
        for (size_t i = 0; i < bytes; ++i) source[i] = uint8_t(i * 31 + 17);
        ExactContent content;
        Check(!content.Matches(source.data(), source.size()), "empty content never matches");
        content.Capture(source.data(), source.size());
        Check(content.Size() == bytes, "snapshot size tracks source");
        Check(content.Matches(source.data(), source.size()), "identical small buffer matches");
        Check(!content.Matches(source.data(), bytes + 1), "size change misses");
        for (size_t i = 0; i < bytes; ++i)
        {
            source[i] ^= 0x55;
            Check(!content.Matches(source.data(), source.size()), "any small-buffer mutation misses");
            source[i] ^= 0x55;
        }
        Check(content.Matches(source.data(), source.size()), "small buffer matches after restore");
    }
}
}

int main()
{
    {
        VertexCache bytes(100, 48);
        bytes.emplace(1, Entry(1)); bytes.emplace(2, Entry(2)); bytes.emplace(3, Entry(3));
        Check(bytes.size()==2 && bytes.CapturedBytes()==48, "snapshot byte budget");
        auto huge=Entry(4); std::vector<uint8_t> payload(49); huge.content.Capture(payload.data(), payload.size());
        bytes.emplace(4, std::move(huge));
        Check(bytes.find(4)==bytes.end() && bytes.CapturedBytes()==48, "oversized entry bypass");
        while (bytes.size()) bytes.erase(bytes.begin());
        Check(bytes.CapturedBytes()==0, "snapshot accounting after slot erase");
    }
    for (size_t capacity : {size_t(0), size_t(1), size_t(2), size_t(15), size_t(16), size_t(17), size_t(257), VertexCache::kCapacity})
        TestChurn(capacity);
    for (size_t capacity : {size_t(2), size_t(16), size_t(17)}) TestRecentUse(capacity);
    for (size_t capacity : {size_t(1), size_t(2), size_t(15), size_t(16), size_t(17), size_t(257)}) TestSlotReset(capacity);
    for (size_t bytes : {size_t(1), size_t(8192), size_t(8196), size_t(16384)}) TestContentReplacement(bytes);
    TestIndexCache();
    TestIndexContentAndBudget();
    TestExactContent();
    TestVertexSamplingPolicy();
    std::printf("vertex cache: %llu checks passed (bounded metadata fixture; no GPU or game)\n",
        static_cast<unsigned long long>(checks));
}
