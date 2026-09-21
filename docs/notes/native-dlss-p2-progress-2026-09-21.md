# Native DLSS P2 follow-up — 2026-09-21

## Status

P2 development has advanced on `dlss` through commit `c2f0602`, and a bounded live-game production run now confirms NGX SR on an RTX 5080: Quality `1707x960 -> 2560x1440`, `DisplayEncoded`, reversed-Z. The run retained one successful Create and 24 successful Evaluate records under a 128-record cap; isolated use `10684` at serial `23614` was accepted/submitted and remained in flight in the snapshot, while `completed through 23612` is the earlier SR completion watermark. No failure was recorded. P2 is **not complete, accepted, or Gate 3 approved**. Visual quality and player acceptance are not claimed. Historical P0/P1, native5, composite, f11889 and f2347 evidence retains its original limits; no release was published and no SDK/runtime binaries were committed.

## Implemented and pushed changes

| Commit | Change | Evidence boundary |
|---|---|---|
| `acaf83a` | Register the orphaned P2 routing fixture, add CPU-only CMake/CTest and Windows/Linux CPU CI | CPU contracts, not GPU execution |
| `a2cc639` | Reserve promotion uploads without rotating slots; restore before borrowing attachments; restore on depth/storage/extent/frame/epoch changes; keep mappings across normal Flush; clear them only after resize drains | 29 policy checks plus actual renderer translation-unit compilation; full renderer mapping remains unexecuted |
| `7c32c69` | Fence-qualified submission-use ledger; idempotent submit; discard only unsubmitted uses; release/recreate features at safe frame boundaries while retaining the device session | 26 lifetime checks; SDK-enabled and disabled builds; no new RTX execution |
| `ee464e8` | Shared production FP16 shaders; retain fallback RGBA outside SR scratch content instead of out-of-range sampling | Actual Vulkan raster/readback: 256 exact RGBA pixel comparisons |
| `3e07a63` | Reject overflowing input-region origins/extents and wire its fixture into CPU CI | 17 frame-input checks, including 10 bounds/overflow checks |
| `59e9dce` | Fix production renderer path failed recording, submit, or fence wait fatal latch and queue drain | Catches recording/submit/wait errors, marks fatal state, drains queue, suppresses further work |
| `19415e4` | Execute production Prepare/Activate/Record/Flush/Restore and mock NGX dispatch on Vulkan | Vulkan mock fixture exercises production pipeline boundaries and state restoration |
| `0a2af46` | Carry reversed-Z input conventions through NGX feature creation flags | Propagates reversed-Z flags to NGX feature descriptor |
| `4047d6f` | Add `LoNativeDlssRendererTest` exercising real `gpu::dlss::Controller` and production `Renderer` | Native test driver exercising production controller and renderer orchestration |
| `9375608` | Isolate unused game entrypoints in embedded renderer test fixture | Decouples renderer test target from full game entry dependencies |
| `c2f0602` | Link real portable-pack zstd codec dependency on Windows | Resolves portable-pack compression symbols in Windows test builds |

Normal configuration changes and deferred `NeedsReconfigure` responses retire old feature state at `ApplyInternalResolution`, before guest draw uploads or attachments are borrowed. Core target mapping, slot-safe promotion/restoration, and recording/submit/wait fatal latches with queue draining are implemented in the production renderer. The `LO_NO_RENDERER` shutdown boundary and legacy execution fixture's unchecked `void` fence-wait gap remain open for broader acceptance.

## Historical P2 evidence (Retained)

The following historical checks and runs from earlier P2 phases remain verified and preserved:

- Local GCC 14.2 Debug and Clang 17 Release: all **5 CPU fixtures passed**. They exercise production planner/routing, promotion reservation/boundaries, submission retirement, and temporal input-region validation.
- GitHub Actions Ubuntu: **actual `renderer.cpp` translation unit compiled** using the existing `motion_renderer_compile` boundary.
- Pinned NVIDIA SDK `374959484e79a640feaba44c93ac8cfb0a03f5b5` (310.9.1): `LoNativeDlssReportTest` and `LoNativeDlssExecutionTest` compiled with SDK OFF and ON; report tests executed successfully.
- `LoSceneCopyCompositeTest` executed on **llvmpipe (LLVM 20.1.2, 256 bits)**, through Vulkan, using the same production pixel-shader source as the renderer. Four cases passed: RGBA 4×4→8×8; full 8×8 RGB/alpha composite; 6×5 SR scratch into an 8×8 target; and a scaled 4×4 base with 5×7 scratch into 8×8. All **256 RGBA16F pixels matched exactly**. Test data includes varying alpha and RGB above 1.0. The runner installed Khronos validation layers; the successful test log contained no validation errors/VUID messages.

Historical validation runs:
- [CPU CI for the first change](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35579359865)
- [Promotion policy and renderer boundary](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35580071328)
- [Lifetime and SDK ON/OFF](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35581228426)
- [FP16 Vulkan composite](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35582125864)

### Historical SDK header line endings and DXC hashes

The pinned Linux checkout's `include/nvsdk_ngx_vk.h` SHA-256 is `2d364ce7132881eb669e9498fd570d74cba563b1fbebc82c235f8e1ad2dd8b6d`. The earlier Windows-recorded value `3be54d0103cce02174fd34912c60c9f40c73b5eec919e42613a2268c589c53ad` is reproduced by changing LF to CRLF. Only text line endings were normalized for this comparison; the exact immutable upstream commit was also checked. The Linux bootstrap archive matched the previously recorded SHA-256 exactly: `3321c4ca9adf71f345550d10bf2148ed38003151b03ec5f81c99d00949ab8202`. No binary hash mismatch was ignored.

The Linux DXC archive used was Microsoft's `v1.8.2505.1` Linux archive `linux_dxc_2025_07_14.x86_64.tar.gz`, SHA-256 `f2213da1fc99dc8778c8823078e16ba97c7f80f86a1d4520ab1adf4b462bc48c`.

### Historical reproduction commands without game assets

CPU-only:
```sh
cmake -S tools/tests/native_dlss -B out/native-dlss-cpu -DLO_NATIVE_DLSS_CPU_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build out/native-dlss-cpu --config Release --target LoNativeDlssCpuTests --parallel 2
ctest --test-dir out/native-dlss-cpu -C Release -L cpu --output-on-failure
```

Vulkan composite fixture:
```sh
cmake -S tools/tests/motion_replay -B out/p2-composite -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_SCAN_FOR_MODULES=OFF
cmake --build out/p2-composite --target motion_renderer_compile LoSceneCopyCompositeTest --parallel 2
ctest --test-dir out/p2-composite -R '^LoSceneCopyCompositeTest$' --output-on-failure -V
```

## Current CI evidence (Commit `c2f0602` and workbench runs)

1. **GitHub Actions CI (`dlss` c2f0602)**:
   - [Run 35629006599](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35629006599): 6 CPU test jobs passed.
2. **Workbench Linux CI (checkout candidate `c2f06023b7dfddc92d4413eae2ac7541492c1354`)**:
   - [Run 35629333836](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35629333836): 7 CPU jobs passed. CTest registers two separate tests: `LoNativeDlssRendererTest` (mock path) passed under llvmpipe; `LoNativeDlssRendererNativeTest` (`--native`) reported skip code 77 in the absence of NVIDIA NGX hardware.
3. **Workbench Windows CI (checkout `9375608` + workbench link fix)**:
   - [Run 35628539860](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35628539860): compiled successfully without execution. There is no verified Windows build run for exact HEAD `c2f0602` in GitHub Actions CI.

## Local native RTX 5080 execution evidence (`LoNativeDlssRendererTest --native`)

The standalone test fixture `LoNativeDlssRendererTest` was built locally from commit `c2f0602` in `out/build/motion-replay-p1` using the existing Debug cache with Ninja, clang-cl, `LO_ENABLE_DLSS=ON`, and `LO_DLSS_STAGE_RUNTIME=ON`. The tool dependency `LO_DXC_PATH` was supplied from `tools/XenosRecomp/thirdparty/dxc-bin/bin/x64/dxcompiler.dll` and verified operational. No tracked source files were modified.

- Target executable: `out/build/motion-replay-p1/LoNativeDlssRendererTest.exe` (SHA-256 `7a00626f12a2215f24943d6fceaf91c07e8022e609267f2b3f7a8f847bba58fd`).
- Staged runtime library: `nvngx_dlss.dll` (SHA-256 `3975567b8943c53acce397f2b72380092f84f162d00b0d2c7d08a1025c563983`).
- Execution environment: isolated CWD `out/tmp/isolated-native-p2`.
- Log output: preserved in `.cache/evidence/p2-native-run/test_run.log` (15 lines), exit code 0 (`PASS: actual NGX + production Renderer mapping, submit, alpha and restore verified on hardware; no gameplay or motion-response quality claim`).
- Device: `NVIDIA GeForce RTX 5080` (driver 616.56, `MODE=native_ngx`). Running `vulkaninfo` enumerates 8 layers and does not include `VK_LAYER_KHRONOS_validation`; no validation layer coverage is claimed.

All 5 native test cases completed with 0 alpha mismatches (`alpha_mismatches=0`):
1. Quality mode (853×480 -> 1280×720): initial feature creation, destination promotion, evaluation recording, and alpha composite.
2. Quality mode reuse (853×480 -> 1280×720): confirmed existing feature reuse across frames without recreation.
3. Balanced mode dynamic switch (742×418 -> 1280×720): verified feature reconfiguration and target promotion.
4. Performance mode reconfiguration (960×540 -> 1920×1080): verified resolution and mode transition handling.
5. Injected evaluation failure (`failure=1`): verified graceful fallback, isolated buffer exclusion, and state restoration on Performance 1080p.

These 5 cases provide verified hardware evidence for controller orchestration, mode switching, feature reconfiguration, and error fallback. They are not repeated in subsequent steps. They do not validate motion vectors, camera jitter response, reversed-Z visual quality (constant 0.5 depth and zero MV were used), presentation queues, or full-game performance.

## Oracle review findings and concrete gaps

1. **Target extent-growth promotion drops overlap data (`renderer.cpp:1753–1773, 3158–3199`)**:
   During destination promotion, when target extents grow, the implementation restores to `parkedLow`. A subsequent `GetRenderTarget` call then reallocates and retires the old target without preserving overlapping pixel data.
   - *Final fix status*: The extent-growth repair was finalized and accepted in review. It strictly constrains preservation to identical scaling, matching guest width, and color height growth (`oldTarget->format == tex->format`, `oldTarget->resolutionSize == tex->resolutionSize`, `oldTarget->guestWidth == tex->guestWidth`, `oldTarget->width == tex->width`, `tex->guestHeight > oldTarget->guestHeight`, `tex->height >= oldTarget->height`), executing a 1:1 `copyTextureRegion` without scale artifacts. If `Begin()` fails during reallocation, the path safely logs an error, retires resources, and returns `nullptr`. The obsolete production test API was removed.
    - *Independent verification*: Tested via `LoNativeDlssRendererTest --extent-only` (`RunExtentGrowth`). The test exercises a realistic promotion transition from guest 1280×736 to 1280×768 (internal 160×90 to output 320×180, 2× scale) with physical allocation going from 160×92 to 160×96 (promoted 320×184). It validates non-uniform uploaded patterns, partial clear on promoted active target, automatic restore-growth on `GetRenderTarget`, and partial clear on the grown target. All 14,720 overlapping pixels matched exactly on local RTX hardware (`.cache/evidence/p2-native-run/extent_growth_regression_rev3.log`). Initial 142-check uniform test and rev2 assertion failure, with UAF evidence from the fixture accessing retired `HostTexture` memory `0xDDDDDDDD`, are superseded; no process crash was demonstrated.
2. **Missing standalone drain in `LO_NO_RENDERER` presentation fallback (`video.cpp:522–545`)**:
   In paths running without `g_renderer`, if `WaitForPresentGpu` fails, the path still proceeds to destroy presentation resources. The existing renderer `Shutdown` drain only executes when `g_renderer` is non-null. `video.cpp` requires its own terminal drain boundary.
3. **Legacy execution fixture unchecked Plume fence wait (`native_dlss_execution_test.cpp:156–159, 180–183, 199–202, 231–234`)**:
   The older standalone execution test uses Plume `executeCommandLists()` and `waitForCommandFence()`, both of which return `void`. It publishes submission serials and calls `ReleaseCompletedThrough` unconditionally without checking underlying native submit or fence-wait return values. This gap remains in `native_dlss_execution_test.cpp`. In this round, priority is placed on the new `LoNativeDlssRendererTest` production orchestration to avoid duplicating legacy sentinel checks, while the legacy gap remains tracked.

None of these issues constitute a P0 or P1 blocker preventing native RTX test execution or offline `Unknown` color capture, but they preclude Gate 3 approval.

## Current live-game production evidence

The final bounded runtime record is `.cache/evidence/game-sr-runtime.json`. On
an RTX 5080, Quality SR ran at render extent `1707x960` and output extent
`2560x1440`, with `DisplayEncoded` color and inverted depth (`reversed-Z`).
The record contains one successful Create and 24 retained successful Evaluate
records; the retained-record count is limited by a 128-entry cap and is not a
total-call count. Isolated use `10684` at submission serial `23614` was
accepted and submitted and remained in flight in the snapshot. `completed
through 23612` is the earlier SR completion watermark. No failure was recorded.

This confirms a live production execution path only. It does not claim visual
quality, motion response, occlusion, UI, reset behavior, performance or player
acceptance. The next work is targeted acceptance of those areas, rather than
rerunning completed sizing, color-qualification, extent-growth or cold-start
evidence while behavior and configuration remain unchanged. The
`LO_NO_RENDERER` shutdown boundary and legacy
execution fixture's unchecked `void` fence-wait gap remain open.

## Sizing session persistence fix & RTX verification (`dlss_ngx.cpp`, `video.cpp`, `native_dlss_probe.cpp`)

To resolve the root cause where `QueryOutputSizing` failed because `ProbeOnce` exited with `Shutdown(true)`:
1. **Persistent Session in Controller**: `gpu::dlss::Controller::QueryOutputSizing` now verifies if `vulkanInterface` and `device` match the current session. If the session was not initialized, it explicitly invokes `EnsureSession(device)` before querying capabilities. Error branches cleanly return `std::nullopt` for `ngxResult` rather than guessing raw codes.
2. **Video Logging & Diagnostics**: `video.cpp` now logs stage `QueryOutputSizing` with selected quality, state, optional raw result, output extents, and device epoch whenever the active sizing mode is not `Ready`.
3. **Probe Cleanliness**: `native_dlss_probe.cpp` adds `ScopeDrain` to guarantee `ShutdownAfterGpuDrain` before device teardown and returns genuine `ProbeExitCode`.
4. **RTX Sizing Evidence**: The production sequence (`ProbeOnce` -> `Query 1280x720` -> `Query 2560x1440`) was executed on local RTX 5080 hardware via `LoNativeDlssProbe --production-sizing` (`.cache/evidence/native-dlss-p2-sizing-fix.json`). Both output resolutions succeeded (`status: ok`, sizing state `Ready`), reporting optimal Quality inputs of 853×480 (for 720p) and 1707×960 (for 1440p). These adjustments are compiled and confirmed; sizing queries need not be rerun.

## 2026-09-21 Game capture evidence and color analysis

On 2026-09-21, `LostOdysseyRecomp` (built with `LO_ENABLE_DLSS=ON`) was executed in an isolated CWD (`out/p2-game-cwd-c2f0602`, assets from `LostOdysseyRecompLib/private/disc1`) in a normal visible foreground window (`PID 42720`).

### Capture trigger correction
The capture mechanism was triggered via in-game `F1` ("Capture Render State"), successfully producing the complete 3-frame bundle:
- File: `out/p2-game-cwd-c2f0602/captures/render-17900119851502951-f11889.zip`
- Sequence: 3 complete consecutive frames (11889, 11890, 11891) verified by `capture-info.txt`.
- *Correction*: Previously, documentation described `LO_CAPTURE_REQUEST` as a 3-frame capture trigger. In reality, `LO_CAPTURE_REQUEST` only sets `captureFrame` for the legacy single-frame root dump (e.g. frame 2909) and does not invoke `RequestDebugCapture`. The in-game `F1` key is the actual mechanism that produced the complete 3-frame render state trace. Users do not need to recapture this sequence for color pipeline analysis.

### Read-only oracle color pipeline verification
Read-only analysis of `p2-oracle.jsonl`, frame render states, and shaders verified the complete tonemapping and resolve lineage:
1. **HDR scene**: Allocation 9, address `0x09fa0000`, guest format 32.
2. **Tonemapping pass (Draw 632)**:
   - VS `9b81c55ca39bb529`, PS `b4b4d54a7a2d6b96`.
   - Output: EDRAM Allocation 3, guest base 720 decimal (`0x2d0`), guest color format 0 (maps to host FP16 / format 10), 1280×736 buffer (1280×720 active rect).
   - `colorMask = 7` (RGB written, Alpha preserved; predicate requires `(colorMask & 7) == 7`).
   - Pixel shader constant `c10.x = 0x3ee8ba2e` (`0.4545454383` ≈ `1.0 / 2.2`), identical across all 3 frames.
   - Math: Computes `pow(saturate(M + 0.1725 * bloom), 1.0 / 2.2)`, outputting **display-encoded SDR** (gamma 2.2 curve) for this specific analyzed capture pipeline. It is not an exact standard sRGB transfer, not sRGB-decoded, and not linear HDR.
3. **Resolve & Copy (Draw 659)**:
   - Resolved to Allocation 20, address `0x0b0d9000`, guest format 6 (`R8G8B8A8_UNORM`, host 20) with Red/Blue swap.
   - Sampled in Draw 659 (VS `8bbd4da701845d16`, PS `cda578aef1724fdc`) with `shared_texture_info = 0x00160a00`, `sign = 0` (fetch BGR) and copy blend `ONE / ZERO / ADD`, `colorMask = 15`. Net RB swap + fetch BGR is identity transfer without extra gamma.
   - Intermediate draws: 24 intermediate draws between 632 and 659 have `(colorMask & 7) == 0` (depth/stencil or alpha-only passes).
   - UI draws begin at Draw 660, directly targeting frontbuffer Allocation 4 (address `0x00714000`).
   - Write ordinals across frames 11889 / 11890 / 11891:
     - Allocation 9: 207457 -> 207475 -> 207493.
     - Allocation 20: 207464 -> 207482 -> 207500.
     - Allocation 4 (Frontbuffer): 207465 -> 207483 -> 207501.
4. **Planning boundary & root cause of downscaling absence**:
   - `candidate_ready = true`, `scene_copies = 1`, `rejections = 0`, but `temporal_history_verified = false`.
   - `activePlan.consumer = 0 (None)`, `input = 1280x720`, `output = 1280x720`, `geometryEpoch = 1`.
   - The sizing session persistence fix above addresses the startup `0xBAD00012` sizing cache failure.
   - **Historical capture limitation**: In the old foreground executable (`PID 42720`), downscaling did not activate and internal lower resolution was not exercised. That observation belongs to the pre-fix capture stage; the later live-game record confirms qualified `1707x960 -> 2560x1440` production SR.
   - *Display environment for future capture*: Display monitor `DISPLAY3` is a 4K 144Hz panel supporting 1440p. Future game capture runs will target a 1440p Quality window, with actual output dimensions determined strictly from runtime logs.

The full design for runtime SDR qualification tracking and geometric quad predicate is recorded in [docs/notes/native-dlss-color-qualification-plan.md](native-dlss-color-qualification-plan.md). The focused CPU qualification boundary passed, and current qualified SR proof covers the actual physical uploaded quad, exact resolve ordinal and net RGB view; other candidate paths remain `Unknown`.

## Capabilities and limits of `LoNativeDlssRendererTest`

The `LoNativeDlssRendererTest` fixture exercises `gpu::dlss::Controller` with the production `Renderer`:
- **What it can verify upon execution**: Quality mode reuse, Balanced mode switching, Performance mode 1080p reconfiguration, non-zero finite RGB/alpha values, restoration on incompatible targets, and graceful fallback when evaluation fails.
- **What it cannot verify**: Motion-vector responsiveness, camera jitter correctness, reversed-Z depth rendering fidelity (it uses constant 0.5 depth, zero motion vectors, and uniform color), real swapchain presentation, `video.cpp` queue synchronization, guest `DrawImpl` pipeline integration, or live performance metrics.
- The software mock fixture tests platform routing boundaries rather than NVIDIA driver execution.

## Next-round game capture plan (Planned, Not Executed)

With native test fixture execution verified on hardware (`LoNativeDlssRendererTest --native`), the extent-growth promotion fix verified via `RunExtentGrowth`, and sizing session persistence confirmed on RTX 5080, subsequent work proceeds to foreground game capture.

1. **Review and incremental build**: Review the pending SDR qualification implementation, then perform a single clean incremental build of `LostOdysseyRecomp` in `windows-clang`.
2. **Foreground game capture (`Unknown` color guard active)**: Launch the game in a visible foreground window for direct user observation targeting a 1440p Quality window on `DISPLAY3` (isolated CWD, verified game directory path, `LO_GRAPHICS_API=vulkan`, and `LO_NO_UPDATE=1`). Verify runtime geometric quad coverage and log output.
3. **True SR activation**: Only after SDR qualification review passes and runtime predicate is confirmed in game capture will real in-game Super Resolution motion, UI, reset, and performance testing be scheduled.

### Game build and foreground capture commands
In `out/build/windows-clang`, enable DLSS and build incrementally:

```sh
cmake -S . -B out/build/windows-clang -DLO_ENABLE_DLSS=ON -DLO_DLSS_SDK_ROOT="C:/Users/freefrank/ownCloud/Git/LostOdysseyRecomp/.cache/deps/nvidia-dlss-37495948"
cmake --build out/build/windows-clang --target LostOdysseyRecomp --parallel 4
```

Execute in an isolated CWD with a visible foreground window, pointing `--game` to the verified game directory.

## Remaining blockers before P2 can be enabled/accepted

1. **Video presentation drain boundary (`video.cpp`)**: Add explicit drain when `WaitForPresentGpu` fails under `LO_NO_RENDERER`.
2. **Runtime SDR color qualification**: Complete implementation and review of SDR tracking and runtime geometric quad predicate, then verify in foreground game capture before lifting the `ColorEncoding::Unknown` bypass.
3. **In-game motion response, jitter, and UI verification**: Verify real gameplay visual fidelity and performance in-game once color qualification is operational.
