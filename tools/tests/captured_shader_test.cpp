#include "../../LostOdysseyRecomp/gpu/shader_identity.h"
#include <cstdio>
#include <cstdlib>

static unsigned checks;
static void Check(bool ok) { ++checks; if (!ok) std::abort(); }
static uint64_t ByteReference(const std::vector<uint32_t>& words)
{
    uint64_t hash = 14695981039346656037ull;
    for (auto word : words)
        for (unsigned shift = 0; shift < 32; shift += 8)
            hash = (hash ^ ((word >> shift) & 255)) * 1099511628211ull;
    return hash;
}
static uint64_t WordReference(const std::vector<uint32_t>& words)
{
    uint64_t lane[4];
    for (uint64_t k = 0; k < 4; ++k) lane[k] = 14695981039346656037ull ^ k;
    for (size_t i = 0; i < words.size(); ++i) lane[i % 4] = (lane[i % 4] ^ words[i]) * 1099511628211ull;
    uint64_t hash = lane[0];
    for (int k = 1; k < 4; ++k) hash = (hash ^ lane[k]) * 1099511628211ull;
    return hash;
}
struct CountingCache
{
    gpu::shader_identity::Cache cache{1024};
    unsigned calls = 0;
    uint64_t Get(uint64_t, const uint32_t* words, uint32_t count)
    {
        ++calls;
        // Deliberately force all snapshots into the same command-hash bucket.
        return cache.Get(42, words, count);
    }
};
int main()
{
    using gpu::shader_identity::CapturedShader;
    CapturedShader vertex, pixel;
    CountingCache cache;
    Check(vertex.Words() == nullptr && vertex.Count() == 0);
    Check(!vertex.Load(nullptr, 0));
    Check(!vertex.Load(nullptr, 0x10001));
    std::vector<uint32_t> guest{0x01020304, 0x12345678, 0, 0xFFFFFFFF};
    Check(vertex.Load(guest.data(), uint32_t(guest.size())));
    Check(vertex.CommandHash() == WordReference(guest));
    Check(vertex.RendererHash(cache) == ByteReference(guest));
    Check(cache.calls == 1);
    auto relocated = guest;
    Check(!vertex.Load(relocated.data(), uint32_t(relocated.size())));
    for (unsigned i = 0; i < 2000; ++i) Check(vertex.RendererHash(cache) == ByteReference(guest));
    Check(cache.calls == 1);
    // Guest writes are invisible to the captured microcode until IM_LOAD.
    guest[1] ^= 0x80000001;
    Check(vertex.RendererHash(cache) == ByteReference(relocated));
    Check(vertex.Words()[1] == relocated[1]);
    Check(vertex.Load(guest.data(), uint32_t(guest.size())));
    Check(vertex.RendererHash(cache) == ByteReference(guest));
    Check(cache.calls == 2);
    Check(pixel.Load(relocated.data(), uint32_t(relocated.size())));
    Check(pixel.RendererHash(cache) == ByteReference(relocated));
    Check(vertex.RendererHash(cache) == ByteReference(guest));
    Check(!vertex.Load(nullptr, 0) && !vertex.Load(nullptr, 0x10001));
    Check(vertex.RendererHash(cache) == ByteReference(guest));
    // Every word and both boundary lengths must invalidate the identity.
    for (unsigned n : {1u, 31u, 128u, 2048u, 65536u}) {
        guest.resize(n);
        for (unsigned i = 0; i < n; ++i) guest[i] = i * 2654435761u + n;
        Check(vertex.Load(guest.data(), n));
        Check(vertex.CommandHash() == WordReference(guest));
        Check(vertex.RendererHash(cache) == ByteReference(guest));
        const auto calls = cache.calls;
        Check(!vertex.Load(guest.data(), n));
        Check(vertex.RendererHash(cache) == ByteReference(guest));
        Check(cache.calls == calls);
        for (unsigned i : {0u, n / 2, n - 1}) {
            guest[i] ^= 0x01010101;
            Check(vertex.Load(guest.data(), n));
            Check(vertex.RendererHash(cache) == ByteReference(guest));
        }
    }
    // Evict the backing identity cache without invalidating another owned stage.
    Check(pixel.RendererHash(cache) == ByteReference(relocated));
    const auto calls = cache.calls;
    Check(pixel.RendererHash(cache) == ByteReference(relocated));
    Check(cache.calls == calls);
    // A shader loaded again from its source reuses its snapshot and identity after
    // a full compare; changed bytes at that source are captured again.
    {
        CapturedShader stage;
        CountingCache ids;
        Check(stage.RendererHash(ids) == ByteReference({}) && ids.calls == 1);
        std::vector<uint32_t> a{1, 2, 3, 4, 5}, b{9, 8, 7};
        Check(stage.Load(a.data(), 5) && stage.RendererHash(ids) == ByteReference(a));
        Check(stage.Load(b.data(), 3) && stage.RendererHash(ids) == ByteReference(b));
        Check(ids.calls == 3);
        Check(stage.Load(a.data(), 5));
        Check(stage.CommandHash() == WordReference(a) && stage.RendererHash(ids) == ByteReference(a));
        Check(stage.Count() == 5 && stage.Words()[4] == 5 && ids.calls == 3);
        Check(!stage.Load(a.data(), 5));
        a[4] ^= 1;
        Check(stage.Load(b.data(), 3) && stage.RendererHash(ids) == ByteReference(b) && ids.calls == 3);
        Check(stage.Load(a.data(), 5));
        Check(stage.CommandHash() == WordReference(a) && stage.RendererHash(ids) == ByteReference(a));
        Check(stage.Words()[4] == a[4] && ids.calls == 4);
        // Same source, other length: a new snapshot, not the stored one.
        Check(stage.Load(a.data(), 4));
        Check(stage.CommandHash() == WordReference({a[0], a[1], a[2], a[3]}));
        // The stored microcode stays bounded; evicted sources are captured again.
        std::vector<std::vector<uint32_t>> big(40, std::vector<uint32_t>(65536));
        for (size_t s = 0; s < big.size(); ++s) {
            for (size_t i = 0; i < big[s].size(); ++i) big[s][i] = uint32_t(i * 2654435761u + s);
            Check(stage.Load(big[s].data(), 65536));
            Check(stage.CommandHash() == WordReference(big[s]));
        }
        for (size_t s = 0; s < big.size(); s += 13) {
            Check(stage.Load(big[s].data(), 65536));
            Check(stage.CommandHash() == WordReference(big[s]) && stage.RendererHash(ids) == ByteReference(big[s]));
        }
    }
    std::printf("captured shader: %u checks passed\n", checks);
}
