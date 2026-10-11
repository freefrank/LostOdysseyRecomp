# Vulkan depth-clear performance — 2026-09-13

> **Historical checkpoint.** Version, issue, process and pending-work statements below describe their original date. Recheck current delivery and acceptance in [STATUS](../STATUS.md) and [ROADMAP](../ROADMAP.md).

The Vulkan depth-clear path was optimized after 720 EDRAM tile rectangles were shown to be exactly coalescible into one rectangle. `gpu/depth_clear_layout.h` now provides a conservative coalescing helper, used by the Vulkan renderer. The D3D12 mapping output is unchanged, and holes and uncleared regions remain preserved. The same diagnostic candidate also applies texture-hash avalanche mixing, caches captured-shader identities, skips redundant Plume rebinding for the same framebuffer, and uses an immutable 32-slot descriptor cache per `GpuSlot` with one 24-byte push. The two-slot/fence contract is retained.

## Controlled result

The authoritative comparison is `out/vulkan-perf-20260913/clear-coalesce-comparison.json`. Both 45-second runs used a copied `user01` save at a static view, 3840x2160 internal rendering, Vulkan, AA3, a 60 FPS cap and a 60 W AMD AI MAX+395 setting. The control executable is `vulkan-submit-01`, SHA-256 `1b9c3c01b1a6863cedb91050c9bc09f77b3ed04f697409c99ea8a72948015ef3`; the candidate is `clear-coalesce-01`, SHA-256 `bca61967e96fbcb2017118cb1c0136ec980770bd6a112ef521d9727cf5653a5f`. Both are source-0.5.7 / HEAD `1c93c397af7bbeb240feabf90d895f9dfdac8a9c` with dirty local candidate changes; the only comparison code change was clear coalescing, with diagnostic-marker placement outside the measured window.

| Metric | Control | Candidate |
|---|---:|---:|
| Mean FPS | 7.49638 | 43.47614 |
| 1% low FPS | 6.21177 | 34.46842 |
| GPU time (ms) | 132.91221 | 21.06364 |
| Record time (ms) | 37.30861 | 4.93429 |
| Draw time (ms) | 44.84368 | 15.90172 |
| Process CPU (ms/frame) | 75.25888 | 41.74892 |
| Frames | 338 | 1,963 |
| Draws/frame | 2,302.99 | 2,294.91 |
| HWiNFO package power (W) | 58.367 | 57.956 |

The result is a strong same-view Vulkan diagnostic improvement. It does not establish 4K60, 1080p60, a 15 W result, whole-game behavior, or player acceptance. The user will verify 1080p separately. The release check confirms v0.5.8 was published at 2026-09-13T15:11:11Z from commit `6e6f11cf56ef69082f5be5b049e5d48d58154415`; the installed executable SHA-256 is `89f1ec23c3d6a7c16b9f7fc8f66e38a3059cc90132cf80f071eb671322ac106d`. That released runtime's earlier approximately 38.5 FPS observation was D3D12, so it is not evidence that Vulkan menu performance was normal.

Delivery is recorded in `out/vulkan-perf-20260913/delivery.json`: the installed-directory candidate is `LostOdysseyRecomp-vulkan-optimized-20260913.exe`, launched by `Vulkan-optimized-20260913.cmd` with a session-only Vulkan override and `LO_NO_UPDATE`. The release executable, settings, saves and profile hashes were unchanged. This retained source-0.5.7 dirty candidate predates the published release and remains historical evidence.

## Validation boundary

The old native city probe localized a clear operation with two approximately 47 ms frame-1250 samples. The new candidate's frame-1250 probe was load-only and had only three draws, so it cannot be used for the city-group comparison; the old city probe still supports clear-operation localization. The formal 45-second result above remains valid. `clear-coalesce-01/shot_1726.png` shows a consistent view without an obvious issue, but is not whole-game visual acceptance.

Focused validation passed: `LoTextureKeyTest` 54,877 checks; `LoCapturedShaderTest` 2,082 checks; `LoVulkanBackendTest --render-pass-rebind-only` 288 draws, 384 components and three fences; `LoVulkanTextureReuseTest` 48 draws, 48 pixels, 192 components, two slots, six submissions, 39 hits, 36 misses and 24 pool rewrites. `LoDepthClearLayoutTest --coalesced-only` covered 1,501,504 pixels, nine MSAA pairs and 720-to-1 coalescing, partial, hole, overlap and empty cases. `LoDepthClearGpuTest --vulkan --coalesced-only` covered 3840x2208 in two fixtures with zero per-pixel differences, preserved bottom 48 rows and partial-hole reduction from 11 to 4. Logs are `clear-coalesce-cpu-test.log`, `clear-coalesce-gpu-test.log` and `clear-coalesce-build.log` under `out/vulkan-perf-20260913/`.

The retained binary predates the published source commit and also contains pre-existing uncommitted work, so its measurements are not a clean-build benchmark of the published package. Remaining work includes the user's separate 1080p60-at-15W observation and player acceptance.

## Binding-cache follow-up

This follow-up uses source version 0.5.7 at dirty HEAD `47a9cd4d5d1d49cb1db000eaf711f6827a53cdeb`, including pre-existing local changes. The final diagnostic executable SHA-256 is `85a2c81d3a63a243acb82abdf3708ef723122ba6b24e25a0906a5c8156116256`; provenance and preserved installation hashes are recorded in `out/vulkan-perf-20260913/binding-followup-result.json`.

The persistent Plume patch (`tools/patches/plume-lostodyssey.patch`) now suppresses same-handle graphics descriptor rebinding, invalidates all bindings on a graphics-layout change or command-list begin, and invalidates higher handles when an uncached lower handle disturbs the handle set. `LoVulkanTextureReuseTest --binding-cache-only` passed 32 pixels and 128 components across duplicate and replacement cases, incompatible prefix A/B/A, and two command lists with four submissions after a fence. Evidence: `out/vulkan-perf-20260913/binding-cache-test.log`.

The attempted masked-load register SIMD path passed 777,794 correctness checks but was rejected on performance evidence: constant time changed from 1.531 to 2.157 ms and the measured FPS was 42.71775. The header and fixture were fully reverted; the retained rejection record is `out/vulkan-perf-20260913/register-simd-rejected/`.

The final bind-only candidate was measured in `out/vulkan-perf-20260913/bind-only-comparison.json` for one 45-second fixed-view window under the same internal 4K, Vulkan, AA3, 60 FPS cap and 60 W conditions. The reused clear-coalesce control measured 43.47614 FPS, 4.93429 ms record time, 21.06364 ms GPU time, 41.74892 ms/frame process CPU, 57.95567 W and 2557.05 MHz; the bind-only candidate measured 43.01324 FPS, 4.75117 ms record time, 21.17160 ms GPU time, 44.16216 ms/frame process CPU, 57.31544 W and 2519.24 MHz. This does not establish an overall FPS or CPU-total improvement: only the small record-time reduction is a bounded observation, and the different clocks and single-window readings are not a controlled frequency result. The candidate was retained as `out/vulkan-perf-20260913/run/LostOdysseyRecomp-bind-only.exe`; it did not replace the delivered clear-coalesce executable or launcher. No new commit, push, release or acceptance resulted.

A new native probe at frame 2000 in `out/vulkan-perf-20260913/clear-coalesce-gpu-02/gpu-draw-groups.json` recorded 2,338 draws and 585 groups; group 578 took 4.40584 ms and included draws 2312–2315. The interval also contains intervening copies, barriers, clears and queue gaps, so it is not an isolated 4.4 ms TAA cost. The probe's TAA activity is inside `bindTextures`; the old loading probe remains invalid for city comparison, while the old city probe retains its clear-operation localization value.

## Published delivery

[v0.5.9](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.9) was published on 2026-09-13 at 19:50:00 UTC from `d26ee8b021784d7232b5319d816227867f98050d`. [CI 34778434518](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34778434518) passed. The clean source-0.5.9 ZIP is 44,269,995 bytes, SHA-256 `fd71bf65f92b242f81a350b5b6e97ea7f1991107a2c30e91ece2dd261bac0591`. All 50 manifest payload hashes and four anonymous public asset downloads matched. Release verification is in `out/v0.5.9/release-verification.json` in the release worktree. Prior functional checks were reused; no new gameplay run or 15 W acceptance was performed.
