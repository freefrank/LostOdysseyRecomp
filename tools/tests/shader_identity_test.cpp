#include "../../LostOdysseyRecomp/gpu/shader_identity.h"
#include <cstdio>
#include <cstdlib>

static unsigned checks;
static void Check(bool ok) { ++checks; if (!ok) std::abort(); }
// Independent reference walks each word's bytes, preserving the old byte FNV.
static uint64_t Reference(const std::vector<uint32_t>& words)
{
    uint64_t h = 14695981039346656037ull;
    for (auto word : words)
        for (unsigned shift = 0; shift != 32; shift += 8)
            h = (h ^ ((word >> shift) & 255)) * 1099511628211ull;
    return h;
}
int main()
{
    gpu::shader_identity::Cache normal, bounded(1024), disabled(0);
    std::vector<uint32_t> words;
    Check(normal.Get(0, nullptr, 0) == Reference(words));
    uint32_t random = 1234567;
    for (unsigned n : {1u, 4u, 31u, 128u, 2048u, 65536u})
    {
        words.resize(n);
        for (auto& word : words) { random ^= random << 13; random ^= random >> 17; random ^= random << 5; word = random; }
        for (auto* cache : {&normal, &bounded, &disabled})
        {
            const auto expected = Reference(words);
            // Forced index collisions, duplicate contents at a different address,
            // mutation at the same address/count, and replacement of prior entries.
            Check(cache->Get(42, words.data(), n) == expected);
            auto copy = words;
            Check(cache->Get(42, copy.data(), n) == expected);
            copy[n / 2] ^= 0x80000001;
            Check(cache->Get(42, copy.data(), n) == Reference(copy));
            Check(cache->Get(42, words.data(), n) == expected);
        }
    }
    Check(normal.Get(42, nullptr, 0) == Reference({}));
    std::printf("shader identity: %u checks passed\n", checks);
}
