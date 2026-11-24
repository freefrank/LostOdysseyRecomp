# Sparse descriptor rewrite scan

On a texture descriptor cache miss, `RewriteBindings` used to compare all
32 texture pointers to the dummy to recover a mask the draw already bounds
with `activeSlots`. Pass that existing mask and inspect only its set bits.
Active bindings that currently hold a dummy are still excluded. The writes
remain exactly `previous | next`; inactive stale bindings are still reset.
Keys, immutable descriptor sets, hashing, pool ownership and completed-fence
lifetimes are unchanged. Full/unknown masks retain the original scan. Debug
builds assert that slots outside the supplied mask really hold the dummy.
The alpha-mask replay explicitly includes its substituted slots in the mask.

## Validation

- The production cache fixture passes 67,656 checks, including 65,536 exhaustive
  old/active eight-slot combinations, active dummy bindings, bit 31, growth,
  fence resets, repeated hits and the original swapped-view cases.
- The fixture passes GCC 13 C++20 Release and ASan/UBSan. LeakSanitizer cannot
  inspect `/proc` in this runner, so only leak detection was disabled.
- The existing 30/60/90/120 frame-pacer/guest interval fixture still passes.
- No full renderer/NDK build, Android run or screenshot comparison was possible.

Synthetic benchmark, Xeon Platinum 8573C x86-64, GCC 13 `-O3 -DNDEBUG`, five
alternating rounds, 2.3 million miss-helper invocations per round; median ns/call:

| Active slots | Full scan | Sparse scan |
| --- | ---: | ---: |
| 1 | 64.92 | 7.39 |
| 4 | 72.28 | 14.25 |
| 8 | 82.21 | 24.83 |
| 16 | 85.57 | 52.79 |
| 32 | 73.38 | 75.43 |

This is the isolated rewrite helper on synthetic keys, **not game FPS, frame
time saved or an Android result**. The dense case has no useful benefit and
shows a small host timing difference; the full scan remains its algorithm.
Reproduce with `tools/tests/texture_descriptor_rewrite_bench.cpp` and the
command at its top. The next gate is matched-scene Android `LO_RECORD_TIMING`
Sets-phase and `LO_RENDER_TIMING` CPU measurements, plus identical captures.
Actual benefit scales with descriptor misses and sparsity, not total draw count.
