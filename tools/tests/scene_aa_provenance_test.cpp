#include "../../LostOdysseyRecomp/gpu/scene_aa_provenance.h"
#include <cstdio>
#include <initializer_list>

int main()
{
    using namespace gpu::scene_aa;
    unsigned checks = 0, failures = 0;
    auto check = [&](bool ok, const char* label) {
        ++checks;
        if (!ok) { ++failures; std::printf("FAIL: %s\n", label); }
    };
    Provenance source, destination;
    check(source.Get(1, 10) == Coverage::None, "empty source");
    source.MarkFull(1, 10);
    check(source.Get(1, 10) == Coverage::Full, "full scene copy");
    // Ordinary UI overlays and later candidate-recognition failures do not
    // mutate the fact of an already completed scene copy.
    destination.Resolve(1, 20, source.Get(1, 10), true);
    destination.Resolve(1, 20, source.Get(1, 10), false);
    check(destination.Get(1, 20) == Coverage::Full, "partial resolve retains full treated scene");
    check(SkipFinalAA(destination.Get(1, 20)), "UI composition avoids double AA");

    // Review counterexample: partial unprocessed writes into a treated target.
    destination.Resolve(1, 20, Coverage::None, false);
    check(destination.Get(1, 20) == Coverage::Mixed, "partial overwrite is mixed");
    check(SkipFinalAA(destination.Get(1, 20)), "mixed skips destructive second full-frame AA");
    destination.Resolve(1, 20, Coverage::Full, false);
    check(destination.Get(1, 20) == Coverage::Mixed, "partial treated write cannot prove full coverage");

    // Review counterexample: clear or full opaque copy into the same allocation.
    source.Invalidate(1, 10, true);
    check(source.Get(1, 10) == Coverage::None, "full clear invalidates same allocation");
    destination.Resolve(1, 20, source.Get(1, 10), true);
    check(!SkipFinalAA(destination.Get(1, 20)), "full unprocessed replacement restores final AA");
    source.MarkFull(1, 10);
    source.Invalidate(1, 10, false);
    check(source.Get(1, 10) == Coverage::Mixed, "partial clear retains treatment elsewhere");

    check(source.Get(2, 10) == Coverage::None, "stale source frame rejected");
    check(source.Get(1, 11) == Coverage::None, "recycled allocation rejected");
    source.Resolve(2, 10, Coverage::None, false);
    check(source.Get(2, 10) == Coverage::Mixed, "partial new-frame write retains old treated pixel risk");
    source.Resolve(3, 11, Coverage::None, false);
    check(source.Get(3, 11) == Coverage::None, "new allocation has no retained treatment");

    // Exercise the entire partial-copy table independently of enum ordinals.
    for (auto old : {Coverage::None, Coverage::Full, Coverage::Mixed})
        for (auto incoming : {Coverage::None, Coverage::Full, Coverage::Mixed})
        {
            destination.Resolve(4, 20, old, true);
            destination.Resolve(4, 20, incoming, false);
            const auto expected = old == incoming ? old : Coverage::Mixed;
            check(destination.Get(4, 20) == expected, "partial coverage table");
            destination.Resolve(4, 20, incoming, true);
            check(destination.Get(4, 20) == incoming, "full replacement table");
        }
    std::printf("Scene AA provenance: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
