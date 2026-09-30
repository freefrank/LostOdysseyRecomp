# Gate 1 host repair follow-up — 2026-09-27

This note records the host-side repair work on the Gate 1 validation branch. It is development evidence, not a release or acceptance record.

The repair was merged into [`main`](https://github.com/freefrank/LostOdysseyRecomp/commit/95f96e161f33a1fd0c35ee9316f683cb5f49d01c) on 2026-09-27. No release or tag was created.

## Scope

The repair covers renderer descriptor-set reuse, point-size translation, and shader-cache invalidation. Reused descriptor sets now clear stale image bindings before the next recording pass, preventing a released FSR scratch binding from crossing slot generations. Point-size translation accepts the builtin and guest-register/default or vertex-export forms, applies the host point-size limit, and updates the cache key. The shared constants block remains 1024 bytes and its transfer field remains the 16-byte region at offset 240, so the transfer ABI is unchanged. The Vulkan square-point limitation for non-square guest point sizes remains outside this evidence and is not a claim of complete cross-platform point-size equivalence.

The P2 oracle JSON now writes `color_allocation: null` for depth-only output. This metadata correction does not change the runtime color allocation and does not expand the validation scope.

## Checks and bounded runtime evidence

The Windows native build completed successfully. The focused shader and CPU checks completed 22 shader checks and 27 position checks through the recorded agent run; DXIL/SPIR-V and motion-replay compilation completed. These checks were reused and were not rerun for documentation.

The completed FSR+FG validation run is `out/gate1-repair-20260927/run01-fsr-layer-resize`:

- exit code 0, no forced stop, and the isolated baseline was preserved;
- submitted/completed serials were 820/820, with both cleanup checks complete;
- validation reported 10 logged `PRESENT-AFTER-WRITE` messages (the layer's duplicate cap, not a total fault or frame count), while `VUID-vkDestroyImageView-imageView-01026`, `VUID-VkGraphicsPipelineCreateInfo-topology-08773`, and framebuffer `04533/04534` were absent;
- the cold shader preparation delayed the first resize to 41.371 seconds, before FG became active at 47.992 seconds; after restoration at 51.124 seconds, FG became active at 53.535 seconds. This run therefore does not establish two active-FG resize cycles;
- early runtime sampling and the Streamline log reported SDK errors and feature-creation errors as zero.

A separate 70-second foreground muted FSR Quality run completed with exit code 0, no forced stop, and the isolated baseline preserved. Submitted/completed serials were 2975/2975, both cleanup checks completed, `generated_intervals=1980`, `actual_presents=4955`, SDK errors 0, and feature-creation failures 0. FG was enabled for 1981 of 2975 sampled periods including startup; after startup it only turned off during exit, with no scene hard-off observed. The source screenshot `scene_1558.png` shows normal Uhra rendering, but it is not a generated-frame capture.

PresentMon recorded 4,161 rows, 3,418 with `MsUntilDisplayed`; all rows were classified as `Application`/`ComposedFlip` and the displayed interval mean was 17.3415 ms. The capture does not cover the complete game path and does not prove generated frames reached the display or physical 120 FPS.

## Status and limits

Gate 1 was **accepted as passed by the maintainer on 2026-09-27**, with the known SDK synchronization exception recorded as backlog. The remaining `PRESENT-AFTER-WRITE` validation report and insufficient display classification still require synchronization/ownership tracing and stronger display evidence, but they no longer block Gate 1. Native CPU checks do not constitute D3D12 GPU acceptance. The non-square Vulkan point-size path currently uses a `max` approximation and does not claim complete cross-platform point-size equivalence. Native failure injection and settings-restart validation remain outstanding.

The SDK pacer trace and official sample reproduction are recorded in the [SDK attribution note](v0.8.0-fg-sdk-sync-attribution-20260926.md). Upstream synchronization reports [#84](https://github.com/NVIDIA-RTX/Streamline/issues/84) and [#112](https://github.com/NVIDIA-RTX/Streamline/issues/112) remain open at this review; no released fix was found in v2.14.1. Further validation requires a corrected SDK or a demonstrated valid integration fix, alongside stronger display evidence.

See the [bounded validation report](../../out/gate1-repair-20260927/REPORT.md) for run artifacts and the foreground evidence.
