# Particle rendering audit and rect-list corner reuse (2026-10-28)

Base reviewed: upstream `main` **f9da0ae9bdd481a5ea4fe6431f3a4724d8261147**
(v1.1.0). The fork's `main` was fast-forwarded from `2128383494a53071db658cbf550778beedc6240d`
to this commit. This is an implementation and game-free validation report,
**not a Flame Thrower hardware profile or a demonstrated fix for 22 FPS**.

## What is established

The current rect-list vertex path emits six different encoded vertex IDs per
rectangle. `rect_list_hlsl.h::Vertex` evaluates the translated guest program
for all three source vertices in every host invocation. However,
`XeRectListPick` returns exactly the same position and every interpolator for
corner 2/3 and for corner 1/4; PointSize always comes from `s0`.

Change only the index sequence:

```
Original: 0, 1, 2, 3, 4, 5
Reused:   0, 1, 2, 2, 1, 5
```

The guest triple encoded in the upper index bits, base offset, triangle count,
winding, provoking-vertex values and primitive order stay identical. No shader
arithmetic, resource bindings, precision, particle count, resolution, lights,
blending or depth settings change. Index count and upload size remain six
32-bit indices per rectangle. The existing GS diagnostic path is unaffected.

This exposes four unique IDs to the post-transform cache rather than six.
[Vulkan explicitly permits vertex result reuse for repeated indices](https://docs.vulkan.org/spec/latest/chapters/shaders.html#shaders-vertex-execution),
but invocation reuse is implementation-dependent. Four is **not** a portable
promise of actual invocations. The Vulkan software fixture below measures it.
The wrapper still evaluates three guest vertices per actual host invocation;
removing that entire triple evaluation would require a different design and
has not been implemented.

`LO_RECT_LIST_REUSE_CORNERS=0` restores the exact original indices. The default
is reuse. The shader source and cache contracts do not change. Twelve files
(four HLSL variants and their eight SPIR-V/DXIL binaries) generated with the
same pinned compiler are byte-identical between the original header and the
modified header. No shader-pack invalidation or pipeline-key change is needed.

`LO_RENDER_TIMING=1` adds a prepared rect-list draw/rectangle count per renderer
frame and reports the selected encoding. These count index-expansion candidates
on the CPU, **not native emissions or vertex invocations**: a later scissor,
upload check or independent no-op optimization can still omit the draw. Rect
lists are not automatically classified as particles. Their absence during
Flame Thrower would rule out this optimization for that scene; their presence
alone does not establish a GPU cost.

## Current rendering path and remaining hypotheses

The checked path is `Draw/DrawImpl` -> shader translation/preparation -> target
and texture acquisition -> descriptor/constant/geometry preparation -> primitive
conversion -> scissor/framebuffer/pipeline binding -> native draw -> optional
HDR/alpha/motion replay -> EDRAM ownership/resolve -> presentation. The Plume
code was inspected at its pinned `d890ac8` revision with the current
`plume-lostodyssey.patch` and `plume-sdl3.patch`, rather than relying on old
notes about what might already be optimized.

| Area | Evidence in the current source | Conclusion / next measurement |
| --- | --- | --- |
| Guest particle generation | The runtime submits guest draw packets; generated PPC/game assets and a running guest are absent here. | Cannot identify Flame Thrower's current emitter workload, shaders or primitive types from a live scene. No guest simulation changes. |
| Rect-list VS | Six encoded IDs, three guest evaluations per invocation; two pairs have identical outputs. | Verified redundant work opportunity; this patch enables index reuse. |
| Transparent overdraw | Native draws preserve guest blend/depth/order. The user's resolution-sensitive slowdown is consistent with fragment/bandwidth pressure. | A hypothesis, not a measured attribution. This patch does not reduce fragment work. Need per-pass counters/timestamps on Adreno. No batching, sorting or alpha/particle reduction. |
| Pixel shading | Current texture-info specialization already folds decode state. Pinned DXC `-O3` removes the texture-dimension calculation from a normalized zero-offset 2D fetch in an isolated SPIR-V inspection. FP16 vertex unpacking and EDRAM format/range conversions carry guest semantics. | No evidence-backed additional PS rewrite found. Do not infer a costly fetch helper from its source alone, or replace exact conversions with native relaxed FP16. Actual flame PS microcode/counters remain needed. |
| Draw/state preparation | Index/vertex preparation caches, constant upload reuse, immutable texture-set caches, last/front pipeline caches and sampler palette reuse already exist. Quad lists already reuse shared indices. | CPU work and stalls must be measured separately; no claim that eliminating CPU hash/descriptor work fixes GPU saturation. |
| Plume Vulkan | Current patch elides identical graphics pipeline binds inside a pass, descriptor rebinds, unchanged viewport/scissor, and covered read-only barriers; same-framebuffer binding keeps the pass. | Reimplementing those would duplicate existing work. Writable/ownership transitions cannot simply be removed. |
| EDRAM | Current code has deferred clears, tile ownership conversion and constant whole-clear transfer elimination; resolves already reuse consecutive identical copies. | Further opportunities are substantially covered by the open PRs below. No speculative removal of visibility/format handoffs. |
| Dynamic lights | Current host shadow settings/hooks are separate from the guest light lifetime/submission. | Need synchronized current light draw/PS-constant evidence. No light suppression, fade removal or altered shadow policy. |
| Pipeline creation | Prebuilt/background/linked pipeline paths and record diagnostics exist. | Cold compilation may hitch; warmed sustained effect cost is a separate question. No cache-hit/compile or CPU/GPU conflation. |

`docs/notes/fire-hit-rendering.md` is explicitly a dated investigation record.
It reports brightness/color-lifecycle observations and warns that full captures
stall frames by roughly 0.7 seconds. It does **not** establish the performance
cause on today's code or prove that an authored brightness fade is wrong.
There is no exposure/light-intensity change in this contribution.

## Open work checked before publication

GitHub state and branch diffs were checked on 2026-10-28. An open PR is not an
integrated fix. All PRs in this table were open, unmerged Drafts at that check.

| PR | Scope; relationship to this patch |
| --- | --- |
| [#718](https://github.com/freefrank/LostOdysseyRecomp/pull/718) | Active-slot descriptor miss scans; CPU preparation, independent. |
| [#722](https://github.com/freefrank/LostOdysseyRecomp/pull/722) | In-memory pipeline hashing; CPU lookup, independent. |
| [#723](https://github.com/freefrank/LostOdysseyRecomp/pull/723) | Low shadow caster budget/cache; no reuse here of that policy or its quality tradeoffs. |
| [#731](https://github.com/freefrank/LostOdysseyRecomp/pull/731) | Avoid transitions on reused color resolves, independent native copy path. |
| [#732](https://github.com/freefrank/LostOdysseyRecomp/pull/732) | Omit mode-5 read-only depth draws while retaining preparation; independent emission predicate. |
| [#733](https://github.com/freefrank/LostOdysseyRecomp/pull/733) | Prune covered partial clear rectangles, independent clear queue. |
| #734 | PR and issue lookups returned 404. No accessible proposal could be established. |
| [#735](https://github.com/freefrank/LostOdysseyRecomp/issues/735) | This number is a Treasure Trove feature **issue**, not a particle/Vulkan PR. |
| [#736](https://github.com/freefrank/LostOdysseyRecomp/pull/736) | Reuse footprint-covering partial clears for constant EDRAM transfers, independent. |
| [#745](https://github.com/freefrank/LostOdysseyRecomp/pull/745) | Experimental Tester diagnostics/export, not a published complete APK or a GPU optimization. |

## Validation and limits

- `LoRectListTest`: **2,643,008 checks** in GCC 13.3 Release and ASan/UBSan.
  Original encoding oracle, indexed/non-indexed input, base offsets, range
  boundaries, malformed/nonconsecutive triples, tails, repeated rectangles,
  order, scratch reuse, default and opt-out; 5,000 seeded randomized inputs.
  The test preserves the upstream rejection/overflow behavior rather than
  silently changing which rectangles it accepts.
- Negative controls reject restoring the six unique IDs and reject sharing
  the wrong corner. A corrupted GPU index stream also fails the FP16/depth
  comparison, so an unchanged/blank readback cannot silently pass.
- Real translator plus production rect wrapper: eight SPIR-V/DXIL compile
  variants covering pointer/UBO constants and static/relative addressing.
  Four additional saved SPIR-V modules validated for Vulkan 1.2.
  Existing render-resolution shader fixture: **22 checks passed**.
- Vulkan readback: **96 matched pairs**, 64 rectangles each, 128x96 FP16 color
  plus D32 depth; **14,155,776 compared bytes**, all identical. Covers rotated
  source corner order, mirrored winding, overlap, grid edges/clipping,
  perspective W, opaque depth writing, ordered alpha blending and additive
  blending, including fragment derivatives. All 96 changed-scene witnesses
  differ. Khronos validation reports **zero errors**.
- Vulkan implementation: **Mesa 25.2.8 lavapipe/llvmpipe, CPU device**, LLVM
  20.1.2 on AMD EPYC 9V74. It is not a physical desktop GPU, Adreno or Mali.
- Real pipeline-statistics queries: aggregate VS invocations **36,864 ->
  24,576** across those 96 comparisons. Per draw: 384 -> 256 VS invocations,
  384 assembled vertices and 128 triangles in both arms. For the first scene,
  FS invocations remain 13,542 (opaque) and 357,930 (alpha/additive) in both arms.
  This establishes a 33.3% VS invocation reduction in this fixture only.
- Actual production `renderer.cpp` compiles with GCC 13 and with Android NDK
  r28c/Clang 19 for ARM64/API 26, against pinned graphics headers. The existing
  generated-guest/PCH boundary stand-ins and `LO_HAS_FSR=0` /
  `LO_HAS_FSR_D3D12=0` were used. This is not full-game linking or an APK.
  The CPU index fixture also compiles and links as an Android ARM64 executable;
  it was not executed on Android.
- Broader local review suite: **47 tests passed** with SDL fixtures disabled.
  The Linux namespace fixture is unavailable in this container and was excluded
  after its namespace failure/timeout. An allocator retry uses
  `ASAN_OPTIONS=detect_leaks=0` because LeakSanitizer cannot inspect `/proc`;
  ASan/UBSan remain enabled for the targeted index test. An executable-bit
  staging issue on one unrelated binary was corrected before the passing run.
  The Windows-only shader-loop fixture could not compile here (`windows.h`).
- Compatibility integration with #718, #722, #723, #731, #732, #733, #736 and
  #745: patches apply together without conflicts, **53 review tests pass**
  (same namespace exclusion/SDL-off/LeakSanitizer environment limits), and the
  complete combined renderer translation unit compiles for both host and
  Android ARM64. This is compile/regression compatibility, not a linked game
  or acceptance of those independent Drafts. Prepared-rect counters remain
  outside the isolated native-emission block tested by #732.

### Synthetic timing, not a speedup claim

Fifteen warmed ABBA rounds, 30 samples per arm/mode, same 64 overlapping
perspective rectangles, shader/state/resources, validation enabled in both arms.
The measured interval spans clear/render work, excludes readback, and uses
explicit fixture-only fence waits. Percentiles use nearest rank. These are
software Vulkan queue intervals; neither Android GPU time nor game frametime.

| Mode | Original median/P95/P99 ms | Reuse median/P95/P99 ms |
| --- | --- | --- |
| Opaque/depth-write | 1.140598 / 1.342960 / 1.350560 | 1.196767 / 1.406984 / 1.466737 |
| Alpha blend | 1.568366 / 2.516900 / 2.788284 | 1.603868 / 1.933648 / 2.240645 |
| Additive blend | 1.514617 / 1.868186 / 2.773582 | 1.511029 / 1.764013 / 1.871932 |

**These timings do not demonstrate a stable elapsed-time improvement.** Do not
convert this table into a game FPS gain. Real Flame Thrower FPS, frame P95/P99,
per-pass GPU time, bandwidth, power, frequencies and thermal results are all
**unmeasured**. No complete game, physical device or game capture was available.
The reported 30 -> ~22 FPS at 1080p remains the user's observation.

## Reproduction

The CPU test is standalone:

```sh
cmake -S tools/tests/rect_list -B out/rect-list-cpu -DCMAKE_BUILD_TYPE=Release
cmake --build out/rect-list-cpu
ctest --test-dir out/rect-list-cpu --output-on-failure
```

For GPU tests initialize the pinned `tools/XenosRecomp` submodule and its
`thirdparty/dxc-bin` and `thirdparty/fmt` submodules. Install Vulkan development
headers/loader and an ICD (plus validation layers for `--validation`).

```sh
cmake -S tools/tests/rect_list -B out/rect-list-gpu \
  -DCMAKE_BUILD_TYPE=Release -DLO_RECT_LIST_GPU_TESTS=ON
cmake --build out/rect-list-gpu --parallel 4
export LO_DXC_PATH="$PWD/tools/XenosRecomp/thirdparty/dxc-bin/lib/x64/libdxcompiler.so"
out/rect-list-gpu/LoRectListGpuTest --validation --benchmark
```

The fixture uses the first enumerated Vulkan device; select the ICD explicitly
when needed and retain the printed device/driver type with results. Do not
present lavapipe output as physical GPU validation. The test's Vulkan waits,
pipeline statistics and timestamps are not added to the game.

## Xiaomi 14 acceptance and Tester integration

A maintainer must first build a complete **debuggable** Android APK with the
normal authorized generated game/native inputs. This report supplies no APK.
The current `RuntimeActivity.getArguments()` already forwards `LO_*` string
intent extras on debuggable builds, before game initialization. A fresh process
is required for each A/B arm. This works without adding a graphics setting:

```sh
adb shell am force-stop io.github.freefrank.lostodyssey
adb shell am start -n io.github.freefrank.lostodyssey/.RuntimeActivity \
  --es LO_RECT_LIST_REUSE_CORNERS 0 --es LO_RENDER_TIMING 1 \
  --es LO_RECORD_TIMING 1 --es LO_FRAME_TIMING 1
```

Repeat in a fresh process with `LO_RECT_LIST_REUSE_CORNERS 1`. Confirm the
startup line reports the intended encoding. Release APKs do not forward those
debug extras. Keep the same debug-log setting and diagnostics in both arms.
Use diagnostic-off runs as a separate overhead control.

1. Xiaomi 14, Adreno 750, Android 16: same driver/version, 1920x1080, all
   graphics/light/particle settings, 30 FPS cap, warmed shader cache, battery,
   charging state and starting thermal state. Keep FG off for correlation.
   Repeat A-B-B-A with matched gameplay inputs, at least five Flame Throwers
   per arm. **Magma Blast does not substitute for Flame Thrower.**
2. Identify windows before, during the actual hit effect, and after it. Record
   timestamps/renderer frame IDs and keep the three phases separate. Compare
   accepted-present interval mean/P95/P99 and its reciprocal rate; the renderer
   batch GPU intervals, CPU preparation wall time, record thread on-CPU share,
   fence waits, prepared rect candidates, resolves and pipeline creation separately.
   Do not add overlapping CPU timers or interpret queue batches as isolated PS
   time. Use AGI/appropriate supported tooling for actual per-pass GPU counters,
   bandwidth and VS/FS invocations when available.
3. Use `LO_DRAW_TRACE`/shader-source capture only in a separate targeted
   identification run to associate actual particle primitive types and VS/PS
   hashes. Full GPU readback captures stall the scene and are unsuitable for
   FPS comparisons. Keep raw counters and missing/unsupported values explicit.
4. Inspect alpha edges, overlapping flames, depth occlusion, clipping, light
   flicker, explosions and UI. Compare matched deterministic frames when
   possible; authored random brightness/animation differences require a
   synchronized replay, not merely adjacent screenshots. Repeat on physical
   Mali and desktop Vulkan/D3D12; Metal also needs runtime acceptance.
5. After cold matched trials, compare sustained repeated attacks with matched
   cooling/charging conditions; retain frame distributions and any exposed
   frequencies, OS thermal status/headroom and battery temperature. A battery
   sensor is not GPU temperature. No frequency/utilization values are inferred.

#745 can be combined later as a separate diagnostic patch. Its Advanced profile
already uses completed renderer/presentation query batches and exports P95/P99,
CPU/thread and optional thermal data. It does **not** provide per-draw shader
identity, hardware GPU utilization/frequency, bandwidth or isolated pass times.
Its GPU fields are cumulative completed queue intervals: compare appropriate
counter deltas/windows, not a cumulative value to a single frame. It starts off
and has no complete APK/device validation yet. Retain the same Tester profile
in both arms and separately assess its overhead. The new rect encoding is
printed in the runtime log and is not automatically a new Tester CSV column.

Keep this contribution Draft until complete-runtime and physical-device
acceptance. There is no promise that vertex work dominates the opening boss,
or that this change alone restores a sustained 30 FPS. Further fragment,
lighting and EDRAM changes need matched current captures and measured costs.
