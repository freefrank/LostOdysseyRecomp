// Synthetic CPU lookup benchmark, not Android frame time or game FPS.
// g++ -std=c++20 -O3 -DNDEBUG -I LostOdysseyRecomp tools/tests/pipeline_key_hash_bench.cpp -o /tmp/pipeline-hash-bench
#include "gpu/pipeline_cache.h"
#include <atomic>
#include <cstdio>
#include <numeric>
#include <random>

namespace cache = gpu::pipeline_cache;
namespace {
volatile uint64_t checksum;
struct PreviousHash {
    size_t operator()(const cache::Key& key) const noexcept {
        return size_t(cache::detail::Hash(cache::detail::Encode(key)));
    }
};
constexpr size_t kDraws = 2300, kRepeats = 400, kRounds = 5;
#if defined(_MSC_VER)
#define LO_BENCH_NOINLINE __declspec(noinline)
#else
#define LO_BENCH_NOINLINE __attribute__((noinline))
#endif
template<class Hash> LO_BENCH_NOINLINE size_t HashOne(const cache::Key& key) {
    return Hash{}(key);
}

std::vector<cache::Key> Inputs(size_t count) {
    std::vector<cache::Key> keys(count);
    std::mt19937_64 random(0x4C4F50534F);
    for (auto& k : keys) {
        k.vs = random(); k.ps = random(); k.blend = 0x00010001;
        k.depthControl = 6 | (uint32_t(random() % 8) << 4);
        k.modeCull = uint32_t(random() % 8); k.colorMask = 15;
        k.prim = 4; k.rtFormat = 7; k.depthFormat = 9;
    }
    return keys;
}

// Reproduce GetPipeline's raw-key front index; both arms use the same front
// cache, normalization, last hit and equality. Only the map's hasher changes.
size_t FrontIndex(const cache::Key& k) {
    uint64_t h = k.vs * 0x9E3779B97F4A7C15ull ^ k.ps * 0xC2B2AE3D27D4EB4Full ^
        ((uint64_t(k.blend) << 32 | k.depthControl) * 0x165667B19E3779F9ull) ^
        (uint64_t(k.colorMask) << 32 | k.rtFormat);
    h = (h ^ (h >> 31)) * 0x94D049BB133111EBull;
    return size_t(h >> 56);
}

template<class Hash> struct Lookup {
    std::unordered_map<cache::Key, uint64_t, Hash> map;
    struct Recent { cache::Key raw{}, key{}; const uint64_t* value = nullptr; };
    std::array<Recent, 256> recent{};
    cache::Key lastKey{};
    const uint64_t* last = nullptr;
    uint64_t mapLookups = 0;
    explicit Lookup(const std::vector<cache::Key>& keys) {
        map.reserve(keys.size());
        for (size_t i = 0; i < keys.size(); ++i) map.emplace(cache::Normalize(keys[i]), i + 1);
    }
    uint64_t Find(const cache::Key& raw) {
        auto& r = recent[FrontIndex(raw)];
        if (r.value && r.raw == raw) { lastKey = r.key; last = r.value; return *last; }
        const auto key = cache::Normalize(raw);
        if (!last || key != lastKey) { ++mapLookups; last = &map.at(key); }
        lastKey = key; r = {raw, key, last};
        return *last;
    }
};

template<class Work> double Time(Work& work) {
    uint64_t sum = 0;
    const auto start = std::chrono::steady_clock::now();
    for (size_t repeat = 0; repeat < kRepeats; ++repeat) {
        sum += work();
        std::atomic_signal_fence(std::memory_order_acq_rel);
    }
    const auto end = std::chrono::steady_clock::now();
    checksum = sum;
    return std::chrono::duration<double, std::nano>(end - start).count() / (kRepeats * kDraws);
}

template<class Before, class After> void Abba(const char* label, Before& before, After& after) {
    std::vector<double> a, b;
    before(); after(); // warm both input sets and maps before timing
    for (size_t round = 0; round < kRounds; ++round) {
        const double a1 = Time(before), b1 = Time(after), b2 = Time(after), a2 = Time(before);
        a.push_back((a1 + a2) / 2); b.push_back((b1 + b2) / 2);
        std::printf("%s round=%zu before_ns=%.3f after_ns=%.3f\n", label, round, a.back(), b.back());
    }
    std::sort(a.begin(), a.end()); std::sort(b.begin(), b.end());
    std::printf("%s median_before_ns=%.3f median_after_ns=%.3f change_percent=%.2f\n",
        label, a[kRounds / 2], b[kRounds / 2], 100 * (b[kRounds / 2] / a[kRounds / 2] - 1));
}
}

int main() {
    const auto keys = Inputs(kDraws);
    auto beforeHash = [&] { uint64_t sum = 0; for (const auto& k : keys) sum += HashOne<PreviousHash>(k); return sum; };
    auto afterHash = [&] { uint64_t sum = 0; for (const auto& k : keys) sum += HashOne<cache::KeyHash>(k); return sum; };
    Abba("hash", beforeHash, afterHash);
    for (size_t count : {size_t(1), size_t(8), size_t(128), size_t(512), kDraws}) {
        const auto workingSet = Inputs(count);
        Lookup<PreviousHash> before(workingSet);
        Lookup<cache::KeyHash> after(workingSet);
        std::vector<size_t> order(kDraws);
        for (size_t i = 0; i < order.size(); ++i) order[i] = i % count;
        std::mt19937 random(0x4C4F); std::shuffle(order.begin(), order.end(), random);
        const auto expected = std::accumulate(order.begin(), order.end(), uint64_t(0)) + kDraws;
        auto oldLookup = [&] { uint64_t sum = 0; for (auto i : order) sum += before.Find(workingSet[i]); return sum; };
        auto newLookup = [&] { uint64_t sum = 0; for (auto i : order) sum += after.Find(workingSet[i]); return sum; };
        if (oldLookup() != expected || newLookup() != expected) return 1;
        const std::string label = "front_lookup_" + std::to_string(count);
        Abba(label.c_str(), oldLookup, newLookup);
        const auto total = uint64_t(2 + 2 * kRounds * kRepeats) * kDraws;
        std::printf("%s map_lookup_percent=%.2f\n", label.c_str(), 100.0 * before.mapLookups / total);
        if (before.mapLookups != after.mapLookups) return 1;
    }
}
