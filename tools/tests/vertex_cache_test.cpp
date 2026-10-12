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

uint64_t Offset(uint64_t key) { return 0x100000000ull + key * 64; }
uint32_t Generation(uint64_t key) { return uint32_t(key * 7 + 3); }

VertexEntry Entry(uint64_t key)
{
    VertexEntry entry{Offset(key), key + 123, uint8_t(key & 1), {}};
    entry.stamp.generation = Generation(key);
    return entry;
}

void CheckEntry(uint64_t key, const VertexEntry& entry, bool checkFrame = true)
{
    Check(entry.offset == Offset(key), "offset still belongs to key", key);
    Check(entry.slot == uint8_t(key & 1), "slot still belongs to key", key);
    Check(entry.stamp.generation == Generation(key), "write stamp still belongs to key", key);
    if (checkFrame) Check(entry.lastFrame == key + 123, "frame still belongs to key", key);
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

// A cached source is taken again once a write lands anywhere in its range,
// and only then. Each case uses its own physical range.
void TestWriteStamps()
{
    using namespace gpu::geometry_prepare;
    namespace writes = gpu::guest_writes;
    constexpr uint32_t base = 0x01000000u;                 // physical
    constexpr uint32_t bytes = 3u << writes::kBlockShift;   // three blocks
    SourceStamp stamp = TakeStamp(base, bytes);
    Check(Unchanged(stamp, base, bytes), "fresh stamp is unchanged");
    writes::NotePhysical(base + bytes + 1, 4);              // next block, outside
    Check(Unchanged(stamp, base, bytes), "write after the range keeps the source");
    Check(stamp.checked == writes::Published(), "a clean scan records the published count");
    writes::Note(0xA0000000u + base + (2u << writes::kBlockShift), 4); // A view, last block
    Check(!Unchanged(stamp, base, bytes), "write in the last block changes the source");
    Check(!Unchanged(stamp, base, bytes), "a changed source stays changed until taken again");
    stamp = TakeStamp(base, bytes);
    Check(Unchanged(stamp, base, bytes), "retaken source is unchanged");

    // C and E alias A; E sits one 4 KiB page later (MmGetPhysicalAddress).
    Check(writes::PhysicalOf(0xC0123000u) == 0x00123000u, "C view maps onto physical");
    Check(writes::PhysicalOf(0xE0123000u) == 0x00124000u, "E view is one page later");
    Check(writes::PhysicalOf(0x00123000u) == 0x00123000u, "physical stays physical");
    constexpr uint32_t other = 0x02000000u;
    stamp = TakeStamp(other, 64);
    writes::Note(0xE0000000u + other - 0x1000u, 4);
    Check(!Unchanged(stamp, other, 64), "E view write reaches the physical block");
    stamp = TakeStamp(other, 64);
    writes::Note(0x80000000u + other, 4);                    // virtual heap, not a physical view
    Check(Unchanged(stamp, other, 64), "virtual heap writes are ignored");

    // Inline draw data in the command buffer is never cached.
    writes::SetCommandBuffer(0x00314000u, 0x00400000u);
    Check(writes::InCommandBuffer(0x00314000u, 16), "start of the command buffer");
    Check(writes::InCommandBuffer(0x00713FF0u, 16), "end of the command buffer");
    Check(writes::InCommandBuffer(0x00313FF0u, 32), "source overlapping the start");
    Check(!writes::InCommandBuffer(0x00714000u, 16), "after the command buffer");
    Check(!writes::InCommandBuffer(0x00313FF0u, 16), "before the command buffer");
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
    const IndexKey key{0x12000000u, count, 4u, 0u, 2u};
    Check(cache.find(key) == cache.end(), "absent index key misses");
    std::vector<uint32_t> converted(count);
    ConvertIndices(src.data(), converted.data(), count, false, 2u);
    IndexEntry entry;
    entry.data = converted;
    entry.lastFrame = 7;
    cache.emplace(key, std::move(entry));
    auto found = cache.find(key);
    Check(found != cache.end(), "stored index key hits");
    Check(found->second.data == converted, "cached output matches conversion");
    Check(cache.find(IndexKey{0x12000000u, count, 13u, 0u, 2u}) == cache.end(), "primitive type participates in index key");
    Check(cache.find(IndexKey{0x12000000u, count, 4u, 1u, 2u}) == cache.end(), "index width participates in index key");
    Check(cache.find(IndexKey{0x12000000u, count, 4u, 0u, 1u}) == cache.end(), "endian mode participates in index key");
    for (uint32_t i = 0; i < 100; ++i) {
        IndexEntry filler;
        filler.data = converted;
        filler.lastFrame = i;
        cache.emplace(IndexKey{0x13000000u + i, count, 4u, 0u, 2u}, std::move(filler));
    }
    Check(cache.size() == 4, "index churn respects capacity");
    Check(cache.bucket_count() != 0, "index buckets reserved before first insertion");
}

void TestIndexReplacementAndBudget()
{
    using namespace gpu::geometry_prepare;
    std::vector<uint8_t> source(16384, 0);
    const IndexKey key{0x12000000u, 8192u, 4u, 0u, 1u};
    IndexCache cache(4, 50000);
    IndexEntry initial;
    initial.data.resize(key.count);
    ConvertIndices(source.data(), initial.data.data(), key.count, false, key.endian);
    initial.lastFrame = 1;
    cache.emplace(key, std::move(initial));
    Check(cache.AllocatedBytes() == key.count * sizeof(uint32_t), "index payload accounting");
    auto found = cache.find(key);
    found->second.motionIndexHash = 0x1234;
    found->second.motionIndexHashReady = true;
    source[601] = 1;
    IndexEntry replacement;
    replacement.data.resize(key.count);
    ConvertIndices(source.data(), replacement.data.data(), key.count, false, key.endian);
    replacement.lastFrame = 1;
    cache.emplace(key, std::move(replacement));
    found = cache.find(key);
    Check(found != cache.end() && found->second.data[300] == 1, "changed index converted instead of reusing old output");
    Check(!found->second.motionIndexHashReady, "changed indices discard the cached motion signature");
    Check(cache.AllocatedBytes() == 32768 && cache.PeakBytes() == 32768, "replacement does not double count bytes");

    // A larger replacement for the same key cannot leave stale data behind.
    IndexEntry oversized;
    oversized.data.resize(20000);
    cache.emplace(key, std::move(oversized));
    Check(cache.find(key) == cache.end() && cache.AllocatedBytes() == 0, "oversized replacement bypasses cache");

    IndexCache bounded(4, 1000);
    for (uint32_t i = 0; i < 10; ++i)
    {
        IndexEntry entry;
        entry.data.resize(128);
        bounded.emplace(IndexKey{0x20000000u + i, 128u, 4u, 0u, 2u}, std::move(entry));
        Check(bounded.AllocatedBytes() <= 1000, "index cache stays within byte budget");
    }
    Check(bounded.size() == 1 && bounded.Evictions() == 9 && bounded.PeakBytes() <= 1000,
        "byte pressure evicts old index payloads");
}

// The renderer's lookups trust an entry pointer while the version is unchanged.
void TestVersion()
{
    VertexCache cache(4);
    const uint64_t v0 = cache.Version();
    cache.emplace(1, Entry(1));
    Check(cache.Version() != v0, "insert changes the version");
    const uint64_t v1 = cache.Version();
    auto it = cache.find(1);
    Check(cache.Version() == v1, "lookup keeps the version");
    it->second.offset = Offset(9);
    cache.Refreshed();
    Check(cache.Version() != v1, "refresh changes the version");
    const uint64_t v2 = cache.Version();
    cache.erase(cache.find(1));
    Check(cache.Version() != v2, "erase changes the version");
    for (uint64_t key = 10; key < 20; ++key) cache.emplace(key, Entry(key));
    Check(cache.size() == 4, "evictions keep the capacity");
}
}

int main()
{
    for (size_t capacity : {size_t(0), size_t(1), size_t(2), size_t(15), size_t(16), size_t(17), size_t(257), VertexCache::kCapacity})
        TestChurn(capacity);
    for (size_t capacity : {size_t(2), size_t(16), size_t(17)}) TestRecentUse(capacity);
    for (size_t capacity : {size_t(1), size_t(2), size_t(15), size_t(16), size_t(17), size_t(257)}) TestSlotReset(capacity);
    TestWriteStamps();
    TestIndexCache();
    TestIndexReplacementAndBudget();
    TestVersion();
    std::printf("vertex cache: %llu checks passed (bounded metadata fixture; no GPU or game)\n",
        static_cast<unsigned long long>(checks));
}
