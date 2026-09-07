# AMD resolve initialization

Date: **2026-09-06**

## Delivery status

**User accepted on 2026-09-06; merged into local `main`; unpublished.** The fix was committed as `43ce0e53` on `amd-fix`. After the NVIDIA manual check, the user reported no glitches and authorized merging if the logs showed no blocker. The log review below found none. The fix is merged into local `main` without conflicts in an independent worktree. No push or release has occurred, and this fix is **not included in v0.2.1**. This note updates the [historical investigation](amd-8060s-dark-render-handoff.md), without rewriting its earlier conclusions.

## Cause and implementation

A newly allocated placed render-target resolve destination received a partial copy before full-subresource initialization: 432×242 pixels copied into a 448×242 FP16 allocation. This violates the applicable [D3D12 placed-texture metadata initialization requirement](https://microsoft.github.io/DirectX-Specs/d3d/ResourceHeaps.html#metadata-initialization-and-resource-invalidation-for-placed-textures). A state transition alone does not initialize metadata; the failure is not merely unfilled right-edge padding.

In [renderer.cpp](../../LostOdysseyRecomp/gpu/renderer.cpp):

- Immediately after allocating `rs.tex`, `ResolveOnGpu` transitions it to `COLOR_WRITE`, binds `GetFramebuffer`, and performs a full `clearColor(0, transparent black)`. Initialization occurs once per allocation, preserving untouched pixels during subsequent partial updates.
- After `Flush` waits for GPU completion, framebuffer cache entries referencing retiring textures are erased before `retiredTextures.clear()`. This prevents initialization RTVs from surviving texture retirement and matching reused pointers.
- The diagnostic `existed` lookup now uses `ColorClassOf`, matching render-target keys; this corrects logging, not rendering.

Temporary diagnostic additions were removed from delivery source and retained in `out/amd-runtime-diagnostics.patch`; existing diagnostic facilities are not claimed removed.

## Evidence

Runtime06 title draw28 clouds and draw31 first-downsample output were nonzero, but the immediately following resolve seq4 was zero. Capturing draws did **not** fix the resolve. Draw38 still copied a nonzero scene; draw39 composited black blur over it. Its sampled blur RGB and source-alpha blend explain the black scene without requiring a title-shader/NaN defect.

The independent probe used identical initialized sources, 480×480 FP16 source, 448×242 destination, and 432×242 copies:

| Adapter / destination initialization | f1991 / f2163 result | Validation per case |
| --- | --- | --- |
| AMD / none | Both black | Error 1422 |
| AMD / full clear | Exact source | 0 errors, 1 warning |
| WARP / none | Exact source despite invalid use | Error 1422 |
| WARP / full clear | Exact source | 0 errors, 1 warning |

Warning 820 concerns the missing optimized clear value, a performance suggestion. JSON comparisons cover 428×242; the probe report additionally verifies the entire copied rectangle byte-exact after initialization. Artifacts: `out/amd-binding-probe/README.md` and `out/amd-binding-probe/resolve-runs/20260906-194005-025/{results.json,run.log}`.

## Build and regression

The new [LoResolveCopyGpuTest](../../tools/tests/resolve_copy_gpu_test.cpp), registered in [CMake](../../LostOdysseyRecomp/CMakeLists.txt), passed on AMD: three allocations × two passes, each with zero mismatches and 418176 nonzero **components**. It checks padded resolves, partial-update preservation and reallocation. The deliberate `--uninitialized` negative control exited 1: 104544 mismatched pixels, all-zero output. Texture-layout and depth-clear-layout tests passed.

Logs:

- `out/build/windows-clang/resolve-clean-build.log`: clean build succeeded.
- `out/build/windows-clang/resolve-final-build.log`: final build succeeded, including the lifetime fix.
- `out/resolve-copy-final-test.log`
- `out/resolve-copy-negative-test.log`
- `out/texture-layout-final-test.log`
- `out/depth-layout-final-test.log`

Final EXE SHA256: `9396D624EB77B023E84F52AFF17B5263B002DEB91691262F5B527DBDBBFC8794`.

Recorded isolated gameplay validation:

- Runtime07: initialization experiment restored resolve seq4–12 and title clouds; `out/amd-runtime-07/title.png`.
- Runtime08: normal title at `out/amd-runtime-08/screen_300.ppm`, without per-draw/per-resolve dumping.
- Runtime10: clean diagnostic-free build showed nonblack opening-battle sky, characters, enemies and background. Fixed captures: `out/amd-runtime-10/title-fixed.png` (f240), `out/amd-runtime-10/battle-fixed.png` (f2880). This preceded the final lifetime addition.
- Runtime11: final EXE loaded an independently copied save; forward movement triggered scene2 random battle. Characters, background, sparks and depth of field were visible at f2100. Fixed captures: `out/amd-runtime-11/title-fixed.png` (f300), `out/amd-runtime-11/battle-fixed.png` (f2100). Exit was requested through the isolated stop file; main user save/profile/settings/cache remained unchanged.

## NVIDIA follow-up — 2026-09-06

At the user's request, the committed fix was tested on a local NVIDIA GeForce RTX 5080 with driver `32.0.16.1656`. Independent fixed and parent-renderer control builds succeeded. The comparison kept other build objects and the machine's pre-existing plume changes the same; it is a controlled renderer comparison, not a claim that both builds came from pristine checkouts. Runtime logs explicitly identify `D3D12 on NVIDIA GeForce RTX 5080`.

`out/nvidia-amd-fix-regression/unit-results.json` records:

- FP16 resolve: three allocations × two passes, zero mismatched pixels on every pass, including partial-update preservation and reallocation.
- Depth clear: 0, 1, 16, 17, 720 and 721 rectangles, zero mismatches across 942080 depth pixels in each case.
- Texture-layout and depth-clear-layout tests passed.
- The deliberately uninitialized resolve control also produced matching pixels on this NVIDIA machine. This means the AMD black output was not reproduced by that control here; it does not make the uninitialized API usage valid.

Both `baseline-opening` and `fixed-opening` reached 2400 swaps and remained alive for approximately 83 seconds. Despite the directory names, their captured content was the title and settings screen, **not opening-battle gameplay**. Settings captures at swaps 600, 1200, 1800 and 2400 were pixel-identical between builds. The animated title capture differed, so it is not a same-frame equality result. Evidence: `out/nvidia-amd-fix-regression/opening-comparison.json`, `baseline-opening/runtime.log` and `fixed-opening/runtime.log` under the same evidence root.

Both `baseline-map12` and `fixed-map12` reached 2400 swaps, remaining alive for 88.4 and 88.6 seconds respectively without early exit. Runtime logs identify Map 12, **Monorail - The Great Gate Station**. The harness then deliberately terminated each process; the recorded exit code 1 is not evidence of a crash. Captures at swaps 1200, 1800 and 2400 had baseline/fixed RGB mean absolute differences of approximately 0.867, 0.796 and 0.795 in 0–255 units, with mean RGB levels around 106. Inspection of `out/nvidia-amd-fix-regression/map12-comparison.png` found no newly introduced darkening in these samples. These separate runs are not exact same-frame alignment tests. Swap 600 was the same black loading frame in both builds and is not counted as a gameplay-image pass.

At fixed Map 12 swap 1800, the five 448×242 blur resolves contained 102133, 102223, 102228, 102240 and 102240 nonzero RGB pixels respectively. The blur chain was therefore not empty in the tested NVIDIA scene. The per-scene comparisons, texture counts and preservation results are recorded in `out/nvidia-amd-fix-regression/visual-results.json`; run duration and termination details are in each Map 12 directory's `result.json` and `runtime.log`.

The automated follow-up tested the existing fix without publishing a build; visual user acceptance was recorded subsequently below. All 15 original files in the preservation manifest, including the main EXE/PDB and save/profile files, retained their hashes. Private captures and test artifacts remain local.

No NVIDIA battle regression or performance benchmark was completed in this follow-up. Its positive result is limited to the GPU/layout tests and sampled title/settings/Map 12 behavior; it is not a full-playthrough compatibility result.

## Manual NVIDIA acceptance and merge review — 2026-09-06

The user tested the fixed EXE with SHA256 `e9ab2eda9301aed2877888ad285c357a92d265faceecb6a4da6f2529c6b74857` and reported “没发现有glitch” (no glitches observed). This accepts the tested visual behavior; broader scene coverage remains regression work rather than a pending acceptance blocker.

The original workspace's local evidence directory is `out/nvidia-amd-fix-regression/manual-20260906-202710`. Its runtime log identifies the NVIDIA RTX 5080, records the last heartbeat at swap 1681 near 30 fps, and ends with the window closing and normal exit at 61.283 seconds. Audio queue drops/errors remained zero. There were no error/fatal-level records or device-removed reports.

The log was not warning-free: 241 warnings comprised 238 vertex-format-0 notes, two known shader-preparation failures (`ps78af7d75d932c582` and `vs291187f5ef8ba74a`), and one unknown `ShaderDumpxe` root entry also present in the parent baseline. The shader-preparation failures were already documented in `docs/notes/shader-preparation.md`; this run does not establish that every shader compiled successfully. These records supplied no new blocker for the authorized local merge.

## Limits

The original left/right images were **scene seq10 versus composite seq16 from one AMD capture**, not NVIDIA/AMD machines. NVIDIA was not retested during the initial AMD validation; the later NVIDIA follow-up and user acceptance are scoped above. The user's earlier NVIDIA title failure remains compatible with an application-side, cross-vendor API violation. Gameplay comparisons are not same-frame pixel-alignment tests. User acceptance does not establish complete-playthrough compatibility or coverage of every shader and scene. Private captures remain local; mutable `latest.png` is not an evidence reference.
