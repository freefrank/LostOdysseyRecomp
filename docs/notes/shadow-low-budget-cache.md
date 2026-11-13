# Dynamic shadows Low: unique caster budget and initialization cache

Baseline: upstream `5fd18ccca9c98a5f8a3fda15e4af6eeac592c45a` (2026-10-25), including #318, #548 and #716. This work changes only Low's guest-side selection. It does not introduce a renderer, shader, Vulkan barrier or shadow-resolution change. Physical Android and game-image validation are still required.

## Findings and scope

`patches/graphics_effects.cpp` delegates shadow-depth rendering to `823D5E50` in On and Low. Off alone skips that function. Low's `823BE570` / `823BE9D0` hooks reject eligible light/primitive interactions before the original creates their shadows. One admitted interaction is **not** one GPU pass: the guest can fade it out, create additional shadows, and submit multiple depth/projection draws.

The old Low policy added a size for every light interaction, although its history was keyed by primitive. An eight-light caster could dominate the previous frame's eighth-size threshold. A size threshold also did not enforce an eight-object budget: ties and hysteresis admitted more. `lastOnScreen = 0` incorrectly gave never-visible objects grace during the first 15 initialization calls.

Existing GPU work was checked before making changes:

- #716 already resets adaptive occlusion to Fast on map load. Its per-frame synchronization problem must not be reimplemented as a shadow fix.
- The renderer already reuses render targets by EDRAM identity/extent, preallocates the shadow atlas's tile extent, and scales only classified shadow targets at 1x/2x/4x.
- `Transition` suppresses identical layouts. Pending clears already feed Vulkan load-op clears; the Plume patch avoids ending a pass for an empty barrier operation. Resolve-copy reuse already has invalidation rules.
- The depth-only attachment policy already differs between Metal and other platforms; Android does not need another unmeasured attachment heuristic.
- Shadow filter loop exits and the shadow-projection coordinate repair already exist. No speculative shader rewrite is added.

An off-screen caster can still cast onto a visible receiver. Consequently this Low-only visibility heuristic is not applied to On. No Android GPU bottleneck or new GPU-time saving has been established here.

## Implementation

`patches/shadow_budget.cpp` now measures a tracked caster once per synchronous `InitDynamicShadows`, shares its decision across lights, and ranks unique candidates from the preceding pass. It keeps eight identities, prefers incumbents using the existing 0.8 factor, and resolves ties deterministically. An observed caster retains the 15-pass off-screen grace; a never-observed one has none.

The measurement stamp expires on every initialization. Renderer, view-buffer/count, address-space and setting changes reset history. Changed bounds, positions, attached geometry and view matrices are sampled on the next pass. An index change resets a reused primitive identity. Missing-pass histories are pruned; history and candidate storage are bounded to 4096 entries. Capacity overflow delegates untracked interactions to the original, favoring correctness over the budget, and is reported separately. No old guest pointer is dereferenced from history.

Caching assumes visibility/bounds/views stay immutable during the original's synchronous enumeration. `LO_SHADOW_CACHE_VERIFY=1` recomputes repeated measurements to test this assumption; disagreement delegates the remainder and discards history. Mid-pass identity changes also fail open. This verifier is a diagnostic, not a default mutation detector for arbitrary concurrent guest writes.

**Selection boundary:** the first pass has no global rank, so its eight admissions follow enumeration order. With a full prior budget, a newly encountered identity waits until the next ranking; an absent prior winner can leave a slot unused for one pass. The renderer/view resets reduce scene-transition carryover, but camera cuts, replacement casters and shadow pop-in still require visual acceptance. The 0.8 coefficient is retained; this is a ranked budget, not pixel-identical reproduction of the old threshold policy.

## Validation completed

The game-free fixture compiles the **complete production** `shadow_budget.cpp` and `graphics_effects.cpp` against stand-ins for guest memory/context, settings, logging and original guest calls. It does not compile/link the full game runtime or emulate GPU rendering.

- GCC 13.3, Linux x86-64: Release and ASan/UBSan pass; 12,364 normal checks and 12,369 verifier checks. Local LeakSanitizer is disabled because this runner cannot inspect `/proc`; ASan/UBSan remain enabled.
- Six related local CTest entries pass: both shadow fixtures, shadow resolution/atlas clears, pending clears, occlusion queries and temporal jitter.
- [Runtime review CI](https://github.com/Steprant/LostOdysseyRecomp/actions/runs/40224072925) passes all 51 tests, including the new fixtures.
- [Focused CI](https://github.com/Steprant/LostOdysseyRecomp/actions/runs/40224299561) passes Linux GCC + ASan/UBSan and Windows clang-cl tests, and cross-compiles/links the fixture with NDK r28c, arm64-v8a, API 26. The Android native executable is a CI artifact; it is not a game APK or a device result.

Tests cover multi-light decisions, a hard cap/ties, stable traversal, the 0.8 preference, exact grace expiry, attached/cyclic bounds, changed bounds/transforms/views, reused indices, overflow, mode switches, unscoped delegation, depth-hook registers and 300 animated multi-light iterations. Verifier cases deliberately mutate visibility, bounds and camera inputs mid-enumeration.

## Synthetic comparison

Same fixture and compiler flags (`g++ 13.3 -std=c++20 -O3 -DNDEBUG`), baseline source above versus the candidate. These numbers are host CPU/constructor-call evidence, **not game FPS, GPU passes or Android gains**.

| Fixture | Original | Candidate |
| --- | ---: | ---: |
| Never-visible startup, 60 casters: forwarded interactions | 60 | 0 |
| Twelve equal-sized visible casters: admitted objects | 12 | 8 |
| Ten descending-size casters, largest with eight lights: admitted objects | 3 | 8 |
| Eight casters with four lights: forwarded interactions | 32 | 32 |
| Same simple four-light fixture: guest word reads per initialization | 736 | 290 |
| Four views, eight attached bounds/caster: guest word reads | 2784 | 802 |
| Same attached-bound fixture: median CPU ns/initialization | 5319.38 | 2234.40 |

The CPU timing uses nine alternating pairs of 20,000 initializations, each after 100 warm-ups, on an AMD EPYC 9V74 host with Linux/glibc 2.39. Per-round baseline ns: 4568.13, 5308.07, 5530.89, 5146.95, 5352.47, 5378.98, 5319.38, 5327.08, 5129.92. Candidate: 2292.13, 2102.85, 2478.89, 2157.62, 1888.69, 2389.44, 2369.03, 1785.24, 2234.40. Median time falls about 58%, roughly **3.1 microseconds** in this synthetic workload. Memory reads are instrumented; host clocks are not fixed. This does not imply a significant whole-frame CPU saving.

Correcting the multi-light ranking restores shadows that the original wrongly excluded, so some scenes may do **more** GPU work. The measured benefit is avoiding repeated CPU measurements and enforcing the intended object selection, not a blanket Low FPS improvement.

Reproduce without private game inputs:

```sh
cmake -S tools/tests/shadow_budget -B out/shadow-test -DCMAKE_BUILD_TYPE=Release
cmake --build out/shadow-test
ctest --test-dir out/shadow-test --output-on-failure

git show 5fd18ccca9c98a5f8a3fda15e4af6eeac592c45a:LostOdysseyRecomp/patches/shadow_budget.cpp > /tmp/shadow-baseline.cpp
cmake -S tools/tests/shadow_budget -B out/shadow-baseline -DCMAKE_BUILD_TYPE=Release \
  -DLO_SHADOW_SOURCE=/tmp/shadow-baseline.cpp
cmake --build out/shadow-baseline
# The old implementation is intentionally observed/benchmarked, not expected
# to pass the new regression assertions.
python tools/tests/shadow_budget/compare.py \
  out/shadow-baseline/LoShadowBudgetTest out/shadow-test/LoShadowBudgetTest --rounds 9
```

## Diagnostics and remaining acceptance

`LO_SHADOW_STATS=1` logs totals every 120 Low initialization passes: eligible interactions, measurements, cache hits, selection rejections, kept decisions, overflow/mutation fallback, history size, average gate wall time and total initialization wall time. Disabled: no clock reads or statistics increments. Enabled timings include diagnostic overhead and are CPU wall intervals, not GPU time or CPU utilization. The default cache remains active without diagnostics.

Reuse existing `LO_RENDER_TIMING` for overlapping CPU components, accepted-present intervals and valid renderer-queue GPU batches. Accept GPU samples only when `gpu_complete=true`; these batches exclude presentation/compositor work. `LO_RECORD_TIMING` and a bounded `LO_GPU_DRAW_TIMING_FRAME` can localize draws, with their documented overhead. Do not equate the new interaction counters with depth/projection pass counts; confirm those in a GPU capture. Keep diagnostic settings matched in A/B runs; use the verifier in correctness runs separately from timings.

Pending game/device matrix:

1. Build baseline and candidate with identical compiler, game inputs, assets, shader cache state and settings. The complete runtime cannot be built here without the private/generated game inputs.
2. Start with Uhra Main Street and a multi-character/light battle at 1080p, 60 FPS cap and 1x shadows. Repeat fixed-save views and the same camera route in alternating order after warm-up; include indoor/outdoor views and real-time cinematics.
3. Check On/Low/Off, 720p/1080p, and shadow 1x/2x/4x. Preserve TAA/upscaler/occlusion settings between paired runs, then exercise combinations for correctness. A 2x shadow multiplier has four times the texels; 4x has sixteen times.
4. Collect accepted-present mean FPS and nearest-rank P95/P99 frame time, renderer CPU wall components, complete GPU-batch timings, actual shadow draw/pass counts and memory. Record thermal state/clocks where exposed, repeat after sustained play, and reject thermally incomparable pairs. No such device measurements are supplied by this contribution.
5. On Adreno and Mali, inspect off-screen receivers, camera cuts, NPC arrivals, attachments, ties, grace expiry and full-budget newcomers for pop-in/flicker. Compare frozen frames where possible and repeated-baseline animation noise otherwise. Repeat game acceptance on Windows/Linux and the other supported backends.

No 1080p60, frame-time percentile, GPU-time, memory, energy or game-image-parity result is claimed. Keep the upstream PR draft until physical-device and full-runtime/visual evidence is available.
