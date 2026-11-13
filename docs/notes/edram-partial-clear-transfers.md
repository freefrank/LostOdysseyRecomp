# Transfers from uniformly cleared EDRAM regions

`TransferRegion` already replaces a transfer draw with a deferred destination
clear when its source owes one whole-target color clear. A rectangular clear
can also determine the result: a transfer may read only a smaller prefix of a
larger source allocation. Previously this case replayed the source clear and
submitted the conversion draw anyway.

The new case requires one pure-color pending operation, RGBA16F source and
destination, identical resolution scales, a nonempty in-bounds footprint, and
one rectangle containing the entire sampled prefix. With scale 1 the existing
shader loads `int2(pos.xy)`, so that prefix is exactly `[0,w) x [0,h)`. It does
not infer coverage from a bounding box or a union of rectangles. Lists above
32 rectangles retain the native draw to bound CPU work. Partial clears with
different scales and ambiguous FP16 conversions also retain the native path.

The existing `ConstantTransfer` conversion and destination `QueueClear` path
are reused. Source clears stay pending. Motion replay is flushed before the
decision. SDR/HDR, resolve invalidation, FSR alpha and transfer accounting
remain in their original locations. Native occlusion queries surround the
guest draw itself, after target acquisition, rather than these internal
conversion draws. No shader, format, precision, resource lifetime or image
quality change is introduced.

This extends the whole-clear transfer path from #495. It does not duplicate
#733's removal of covered rectangles inside the pending-clear queue. The patch
applies to the combined #731/#732/#733 branch, and the new CPU fixture passes
there too.

## Validation

Baseline: v1.1.0, `f9da0ae9bdd481a5ea4fe6431f3a4724d8261147`.

The exact production method is compiled against a graphics boundary model and
compared with an eager FP16 pixel oracle. GCC 13 Release and ASan/UBSan run
3,864 scenarios and 389,438 checks. Coverage includes all nine class pairs,
native/1080p/2403-wide/3204-wide/4K scales, each uncovered edge, disjoint and
adjacent rectangles, merged lists, the 32-rectangle limit, replay writes,
intermediate reads, destination preservation, signed zero, conversion
ambiguity and format/aspect fallbacks. The unchanged upstream method fails the
targeted partial-clear assertion.

On the same synthetic inputs:

| Recorded operation | Upstream | Candidate |
| --- | ---: | ---: |
| Deferred clear transfers | 90 | 1,177 |
| Native transfer draws | 3,774 | 2,687 |

That is **1,087 additional native draws omitted in the synthetic fixture**,
not a per-frame or game result. No render-pass reduction is inferred from
these counts.

The expanded `LoLazyClearGpuTest` reads back the real transfer shader from a
uniform prefix surrounded by different source texels and compares it with the
CPU result cleared into the destination. Mesa 25.2.8 lavapipe (CPU Vulkan):
200 existing deferred-clear scripts are byte-identical (37,986,304 bytes per
mode in total), and 26,527 accepted transfer texels match across all class
pairs. The validation-layer run reports no Vulkan errors. Deliberately
leaving one sampled column uncleared (`--mutate-partial`) fails, as does the
existing one-FP16-step mutation.

The complete production renderer translation unit compiles with GCC 13
against the pinned graphics interfaces and the existing generated-guest/PCH
boundary declarations. This is not a complete runtime or APK link. Physical
Adreno/Mali/desktop rendering, candidate frequency, GPU/CPU time, FPS, energy
and thermal gains remain unmeasured.

## Reproduction

Host model, without game assets:

```sh
python3 tools/tests/edram_partial_clear_test.py --out out/edram-release
python3 tools/tests/edram_partial_clear_test.py --out out/edram-sanitize --sanitize
git show f9da0ae9:LostOdysseyRecomp/gpu/renderer.cpp > out/edram-baseline.cpp
python3 tools/tests/edram_partial_clear_test.py --renderer out/edram-baseline.cpp --out out/edram-before --baseline
```

Without `--baseline`, the upstream invocation must fail the new targeted
assertion. `--baseline` keeps the pixel oracle and fallback checks, allowing
the native draw/clear counts to be compared without requiring the new path.
In environments that cannot run LeakSanitizer, set
`ASAN_OPTIONS=detect_leaks=0` for the sanitizer invocation. ASan/UBSan remain
enabled.

After initializing and patching the pinned public dependencies as in the
FG game integration workflow, the asset-free graphics fixture is available:

```sh
cmake -S tools/tests/motion_replay -B out/edram-gpu -DCMAKE_BUILD_TYPE=Release -DLO_ENABLE_DLSS=OFF -DLO_ENABLE_FSR=OFF
cmake --build out/edram-gpu --target LoLazyClearGpuTest motion_renderer_compile --parallel 4
out/edram-gpu/LoLazyClearGpuTest --vulkan 200
out/edram-gpu/LoLazyClearGpuTest --vulkan 0 --mutate-partial
```

The last command must return nonzero for a `partial source clear` mismatch.
For hardware A/B, build baseline and candidate with optimized native code and
use the same save, driver, actual render dimensions, effects, FPS cap, warmed
shader caches and starting thermal conditions. Capture stable gameplay and
CGI/scene transitions. A reduction in this fixture's native commands is not a
measurement of game performance or proof of a CGI/audio fix.
