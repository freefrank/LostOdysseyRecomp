#include "../../LostOdysseyRecomp/gpu/texture_descriptor_cache.h"
#include <cstdio>
#include <cstdlib>

namespace {
int checks = 0;
void Check(bool condition) {
    ++checks;
    if (!condition) { std::fprintf(stderr, "FAILED check %d\n", checks); std::exit(1); }
}
}
int main() {
    using Cache = gpu::texture_descriptors::BatchCache<int, int, 32>;
    Cache cache, otherBank;
    int textures[4]{}, sets[16]{};
    unsigned allocations = 0;
    auto create = [&]() { return &sets[allocations++]; };
    Cache::Key key{};
    key.fill(&textures[0]);
    bool reused = true;
    auto* first = cache.Acquire(key, create, reused);
    Check(!reused && allocations == 1);
    Check(cache.Acquire(key, create, reused) == first && reused && allocations == 1);
    auto secondKey = key;
    secondKey[31] = &textures[1];
    auto* second = cache.Acquire(secondKey, create, reused);
    Check(!reused && second != first && allocations == 2);
    Check(cache.Acquire(key, create, reused) == first && reused);
    auto permuted = key;
    permuted[0] = &textures[1];
    Check(cache.Acquire(permuted, create, reused) != second && !reused);
    Check(otherBank.Acquire(key, create, reused) != first && !reused);
    // Guest content changes do not change descriptor identity; texture barriers
    // and uploads remain the caller's responsibility, not cache side effects.
    textures[0] = 123;
    Check(cache.Acquire(key, create, reused) == first && reused);
    // A completed fence clears identity before objects or pool slots are reused.
    cache.Clear();
    Check(cache.Acquire(key, create, reused) != first && !reused);
    auto* current = cache.Acquire(key, create, reused);
    Check(reused);
    for (int i = 0; i < 2000; ++i)
        if (cache.Acquire(key, create, reused) != current || !reused) return 1;
    Check(allocations == 5);
    cache.Clear();
    cache.Clear();
    Check(cache.Acquire(key, create, reused) != current && !reused && allocations == 6);
    std::printf("texture descriptor cache: %d checks passed (including 2000 repeated hits)\n", checks);
}
