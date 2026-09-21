# Native DLSS P2 follow-up — 2026-09-21

## Status

P2 has advanced, but is **not complete or accepted**. The game still rejects
unqualified `ColorEncoding::Unknown` inputs. No game assets, player settings or
saves were used, no release was published, and no SDK/runtime binaries were
committed. P0/P1 evidence is retained; historical RTX results are not new results.

## Implemented and pushed changes

| Commit | Change | Evidence boundary |
|---|---|---|
| `acaf83a` | Register the orphaned P2 routing fixture, add CPU-only CMake/CTest and Windows/Linux CPU CI | CPU contracts, not GPU execution |
| `a2cc639` | Reserve promotion uploads without rotating slots; restore before borrowing attachments; restore on depth/storage/extent/frame/epoch changes; keep mappings across normal Flush; clear them only after resize drains | 29 policy checks plus actual renderer translation-unit compilation; full renderer mapping remains unexecuted |
| `7c32c69` | Fence-qualified submission-use ledger; idempotent submit; discard only unsubmitted uses; release/recreate features at safe frame boundaries while retaining the device session | 26 lifetime checks; SDK-enabled and disabled builds; no new RTX execution |
| `ee464e8` | Shared production FP16 shaders; retain fallback RGBA outside SR scratch content instead of out-of-range sampling | Actual Vulkan raster/readback: 256 exact RGBA pixel comparisons |
| This follow-up | Reject overflowing input-region origins/extents and wire its fixture into CPU CI | 17 frame-input checks, including 10 new bounds/overflow checks |

Normal configuration changes and deferred `NeedsReconfigure` responses now
retire old feature state at `ApplyInternalResolution`, before guest draw uploads
or attachments are borrowed. This removes the previous permanent-stall path for
reconfiguration; it is not proof of live window-resize or quality-switch visuals.

The promotion policy preserves one promotion per renderer frame and refuses to
Flush from its late draw-preparation stage. Restore uploads occur before any
new descriptors are acquired. Restoration failure suppresses the affected plan
rather than letting the caller continue with incompatible attachments.

## Checks actually executed

- Local GCC 14.2 Debug and Clang 17 Release: all **5 CPU fixtures passed**.
  They exercise production planner/routing, promotion reservation/boundaries,
  submission retirement, and temporal input-region validation. They do not
  execute renderer target-map ownership, Vulkan submission failure, or NGX.
- GitHub Actions Ubuntu: **actual `renderer.cpp` translation unit compiled**
  using the existing `motion_renderer_compile` boundary. Its guest-ABI shim is
  already part of that fixture. This is not a complete game link.
- Pinned NVIDIA SDK `374959484e79a640feaba44c93ac8cfb0a03f5b5`:
  `LoNativeDlssReportTest` and `LoNativeDlssExecutionTest` compiled with SDK OFF
  and ON; report tests executed successfully. The execution executable was
  **compiled, not run on NVIDIA hardware**. Runtime staging was OFF.
- `LoSceneCopyCompositeTest` executed on **llvmpipe (LLVM 20.1.2, 256 bits)**,
  through Vulkan, using the same production pixel-shader source as the renderer.
  Four cases passed: RGBA 4×4→8×8; full 8×8 RGB/alpha composite; 6×5 SR scratch
  into an 8×8 target; and a scaled 4×4 base with 5×7 scratch into 8×8.
  All **256 RGBA16F pixels matched exactly**. Test data includes varying alpha
  and RGB above 1.0. The runner installed Khronos validation layers; the successful
  test log contained no validation errors/VUID messages. This is software Vulkan,
  not NVIDIA DLSS, and does not invoke production target-map transitions.

Successful validation runs (public source only):

- [CPU CI for the first change](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35579359865)
- [Promotion policy and renderer boundary](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35580071328)
- [Lifetime and SDK ON/OFF](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35581228426)
- [FP16 Vulkan composite](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35582125864)

The last two runs retain `native-dlss-p2-validation` artifacts with the candidate
SHA, SDK byte-verification JSON and CTest logs, subject to Actions retention.
The first composite attempt failed due to a **new test-fixture** descriptor
layout error (an array at t0 instead of separate t0/t1). That fixture was fixed
before the successful run and before the feature was pushed to `dlss`.

### SDK header line endings

The pinned Linux checkout's `include/nvsdk_ngx_vk.h` SHA-256 is
`2d364ce7132881eb669e9498fd570d74cba563b1fbebc82c235f8e1ad2dd8b6d`.
The earlier Windows-recorded value
`3be54d0103cce02174fd34912c60c9f40c73b5eec919e42613a2268c589c53ad`
is reproduced by changing LF to CRLF. Only text line endings were normalized
for this comparison; the exact immutable upstream commit was also checked.
The Linux bootstrap archive matched the previously recorded SHA-256 exactly:
`3321c4ca9adf71f345550d10bf2148ed38003151b03ec5f81c99d00949ab8202`.
No binary hash mismatch was ignored.

## Reproduce without game assets

CPU-only, no submodules, Vulkan loader or SDK required:

```sh
cmake -S tools/tests/native_dlss -B out/native-dlss-cpu -DLO_NATIVE_DLSS_CPU_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build out/native-dlss-cpu --config Release --target LoNativeDlssCpuTests --parallel 2
ctest --test-dir out/native-dlss-cpu -C Release -L cpu --output-on-failure
```

For the Vulkan fixture, first initialize the existing public Plume/XenosRecomp
submodules and apply `tools/patches/plume-lostodyssey.patch` as documented in the
main handoff. Supply a working DXC library; `LO_DXC_PATH` accepts its full path.
The successful Linux run used Microsoft's `v1.8.2505.1` Linux archive
`linux_dxc_2025_07_14.x86_64.tar.gz`, SHA-256
`f2213da1fc99dc8778c8823078e16ba97c7f80f86a1d4520ab1adf4b462bc48c`.

```sh
cmake -S tools/tests/motion_replay -B out/p2-composite -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_SCAN_FOR_MODULES=OFF
cmake --build out/p2-composite --target motion_renderer_compile LoSceneCopyCompositeTest --parallel 2
ctest --test-dir out/p2-composite -R '^LoSceneCopyCompositeTest$' --output-on-failure -V
```

The Vulkan fixture fails rather than reporting success when the device, shader
compiler, pipeline, submission, fence or pixel comparison fails. To reproduce
software Vulkan on Linux, select the installed lavapipe ICD explicitly through
`VK_ICD_FILENAMES`; do not infer an NVIDIA result from this lane.

## Remaining blockers before P2 can be enabled/accepted

1. **Runtime color qualification is missing.** Capture at least three consecutive
   frames with runtime PS constants c0–c10/c255 and producer→resolve→scene-copy
   allocation lineage. Confirm actual c10.x for `b4b4d54a7a2d6b96` and classify the
   color transfer before selecting NGX color flags. Keep `Unknown` guarded off.
2. **Execute the complete mapping/lifetime path.** The new CPU policy tests and
   shader GPU test do not drive `PrepareSceneCopyDestination` → guest copy →
   `RecordSceneCopyDlss` → restore in a running renderer. Test incompatible
   depth/stencil, storage aliases, extent growth, next-frame access, ordinary
   mid-frame Flush, dynamic resize/quality changes and transparent UI ordering.
3. **Submission-failure/device-loss handling still needs work and injection.**
   Static inspection found that `Flush` returns after a failed
   `SubmitRendererBatch` without a terminal recording-state latch; the
   `RecordSceneCopyDlss` DeviceLost branch also leaves a closed prefix. Do not
   claim those batches can safely resume. Audit the next `Begin`/slot reuse and
   checked fence waits, prevent reuse after fatal errors, then test those paths.
   The new use ledger protects ordinary ownership transitions, not this gap.
4. **RTX Windows/Linux validation remains outstanding for these revisions.**
   Run the NGX execution fixture, then isolated-CWD gameplay. Confirm movement
   response, color/alpha/UI, reset transitions, failure fallback and performance;
   compile/report success is insufficient. Preserve the original runtime-isolation
   requirements (`LO_GRAPHICS_API=vulkan`, `LO_NO_UPDATE=1`, separate CWD).

Do not remove the color guard or mark Gate 3 passed just to make the DLSS menu
selection appear effective. No automatic shader-wide MV replacement, FG,
OptiScaler integration or D3D12 removal was introduced.
