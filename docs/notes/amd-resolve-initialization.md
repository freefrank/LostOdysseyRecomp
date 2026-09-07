# AMD resolve initialization

Date: **2026-09-06**

## Delivery status

**`amd-fix` development branch; user requested commit on 2026-09-06; unpublished; visual user acceptance pending.** Technical review passed; implementation and recorded validation are complete, and runtime code is unchanged from that validation. No push or release is authorized; this fix is **not included in v0.2.1**. This note updates the [historical investigation](amd-8060s-dark-render-handoff.md), without rewriting its earlier conclusions.

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

## Limits

The original left/right images were **scene seq10 versus composite seq16 from one AMD capture**, not NVIDIA/AMD machines. NVIDIA was not retested; the user's earlier NVIDIA title failure remains compatible with an application-side, cross-vendor API violation. Gameplay comparisons are not same-frame pixel-alignment tests. These results establish neither complete playthrough compatibility nor final user acceptance. Private captures remain local; mutable `latest.png` is not an evidence reference.
