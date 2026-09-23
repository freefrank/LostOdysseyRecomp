# Native DLSS Validation (P0 Foundation, P1 Temporal Inputs & P2 Checkpoint)

Date: 2026-09-21
Baseline: `main@5b765f617ec511a2f76aa9da7923a8c122679d2c`
Feature branch: `dlss` (prior pushed baseline: `91bf37e`; current HEAD: `c2f0602`)
SDK Reference: NVIDIA DLSS **310.9.1** (`374959484e79a640feaba44c93ac8cfb0a03f5b5`)
Status: **P0 gate 1 passed**; **P1 gate 2 passed** (historical evidence retained). P2 development has advanced on `dlss` through commit `c2f0602`; a bounded live-game production run now confirms NGX SR on RTX 5080 at Quality `1707x960 -> 2560x1440`, `DisplayEncoded`, reversed-Z. The runtime record contains one successful Create and 24 retained successful Evaluate records under a 128-entry cap. Accepted/submitted use `10684` at serial `23614` remained in flight in the snapshot; `completed through 23612` is the earlier SR completion watermark, and no failure was recorded. P2 is **not complete, accepted, or Gate 3 approved**. Visual quality and player acceptance are not claimed. Historical RTX and P0/P1 results remain preserved with their original limits. Frame Generation (FG) remains deferred.

## 2026-09-21 Development Follow-up and Current Evidence Boundary

Development continued on `dlss` across commits `59e9dce`, `19415e4`, `0a2af46`, `4047d6f`, `9375608`, and `c2f0602`. See [P2 follow-up evidence and remaining blockers](native-dlss-p2-progress-2026-09-21.md) for detailed commit listings, reproducible commands, and exact boundary definitions.

Earlier claims that destination mapping and fatal error handling are entirely unimplemented are now obsolete. The core mapping, slot-safe promotion and restoration, and recording/submit/fence wait fatal latches with queue draining are implemented in the production renderer. Two concrete edge cases remain open for broader acceptance: the `LO_NO_RENDERER` shutdown boundary and the legacy execution fixture's unchecked `void` fence wait.

### Active CI Evidence

1. **`dlss` commit `c2f0602` CPU CI**: [Run 35629006599](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35629006599) passed all 6 CPU tests.
2. **Workbench Linux CI (checkout candidate `c2f06023b7dfddc92d4413eae2ac7541492c1354`)**: [Run 35629333836](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35629333836) passed 7 CPU tests; `LoNativeDlssRendererTest` passed under llvmpipe in software mode and returned skip code 77 when native NGX hardware was not present.
3. **Workbench Windows CI (checkout `9375608` + workbench link fix)**: [Run 35628539860](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/35628539860) compiled successfully without execution. There is currently no verified compilation run for exact HEAD `c2f0602` on Windows; do not claim unproven exact-HEAD Windows build evidence.

### Read-Only Oracle Review Technical Gaps and Resolved Fixes

1. **Target extent-growth promotion drops overlap data (`renderer.cpp:1753–1773, 3158–3199`)**:
   During destination promotion, when target extents grow, the implementation restores to `parkedLow`. A subsequent `GetRenderTarget` call then reallocates and retires the old target without preserving overlapping pixel data.
   - *Final fix status*: The extent-growth repair was completed and accepted in review. It strictly constrains preservation to matching scale, matching guest width, and color height growth (`oldTarget->format == tex->format`, `oldTarget->resolutionSize == tex->resolutionSize`, `oldTarget->guestWidth == tex->guestWidth`, `oldTarget->width == tex->width`, `tex->guestHeight > oldTarget->guestHeight`, `tex->height >= oldTarget->height`), executing a 1:1 `copyTextureRegion`. If `Begin()` fails during reallocation, it logs an error, safely retires resources, and returns `nullptr`. Obsolete production test APIs were removed.
   - *Hardware test verification*: Validated via `LoNativeDlssRendererTest --extent-only` (`RunExtentGrowth`). The test exercises a promotion transition from guest 1280×736 to 1280×768 (internal 160×90 to output 320×180, 2× scale) with physical allocation going from 160×92 to 160×96 (promoted 320×184). It validates non-uniform uploaded patterns, partial clear on promoted active target, automatic restore-growth on `GetRenderTarget`, and partial clear on the grown target. All 14,720 overlapping pixels matched exactly on local RTX hardware (`.cache/evidence/p2-native-run/extent_growth_regression_rev3.log`). Initial 142-check uniform checks and rev2 failure (fixture accessing retired `HostTexture` `0xDDDDDDDD`) are superseded and not used as passing evidence.
2. **Missing presentation drain in `LO_NO_RENDERER` fallback (`video.cpp:522–545`)**: In paths running without `g_renderer`, a failure in `WaitForPresentGpu` still proceeds to destroy presentation resources. The renderer `Shutdown` drain only executes when `g_renderer` is non-null; `video.cpp` requires its own terminal drain boundary.
3. **Legacy execution fixture unchecked Plume fence wait (`native_dlss_execution_test.cpp:156–159, 180–183, 199–202, 231–234`)**: Older execution tests use Plume `executeCommandLists()` and `waitForCommandFence()`, both of which return `void`. They publish serials and call `ReleaseCompletedThrough` unconditionally without checking underlying native submit or fence-wait return values, leaving fatal error latches unproven in that fixture. This gap remains in `native_dlss_execution_test.cpp`. In this round, priority is placed on the new `LoNativeDlssRendererTest` production orchestration to avoid duplicating legacy sentinels, but the legacy test gap itself remains tracked.

### Sizing Session Persistence Fix and RTX Verification (`dlss_ngx.cpp`, `video.cpp`, `native_dlss_probe.cpp`)

1. **Persistent Session in Controller**: `gpu::dlss::Controller::QueryOutputSizing` now verifies if `vulkanInterface` and `device` match the current session. If the session was not initialized, it explicitly invokes `EnsureSession(device)` before querying capabilities. Error branches return `std::nullopt` for `ngxResult`.
2. **Video Logging & Diagnostics**: `video.cpp` now logs stage `QueryOutputSizing` with selected quality, state, optional raw result, output extents, and device epoch whenever the active sizing mode is not `Ready`.
3. **Probe Cleanliness**: `native_dlss_probe.cpp` adds `ScopeDrain` to guarantee `ShutdownAfterGpuDrain` before device teardown and returns genuine `ProbeExitCode`.
4. **RTX Hardware Verification**: The production sequence (`ProbeOnce` -> `Query 1280x720` -> `Query 2560x1440`) was executed on local RTX 5080 hardware via `LoNativeDlssProbe --production-sizing` (`.cache/evidence/native-dlss-p2-sizing-fix.json`). Both output resolutions succeeded (`status: ok`, sizing state `Ready`), reporting optimal Quality inputs of 853×480 (for 720p) and 1707×960 (for 1440p). These adjustments are compiled and confirmed; sizing queries need not be rerun.

### Local Native RTX 5080 Execution Evidence (`LoNativeDlssRendererTest --native`)

The standalone test fixture `LoNativeDlssRendererTest` was built from commit `c2f0602` in `out/build/motion-replay-p1` using the existing Ninja clang-cl Debug cache with `LO_ENABLE_DLSS=ON` and `LO_DLSS_STAGE_RUNTIME=ON`. DXC dependency at `tools/XenosRecomp/thirdparty/dxc-bin/bin/x64/dxcompiler.dll` was verified operational. No tracked sources were modified.

- Executable: `out/build/motion-replay-p1/LoNativeDlssRendererTest.exe` (SHA-256 `7a00626f12a2215f24943d6fceaf91c07e8022e609267f2b3f7a8f847bba58fd`).
- Staged DLL: `nvngx_dlss.dll` (SHA-256 `3975567b8943c53acce397f2b72380092f84f162d00b0d2c7d08a1025c563983`).
- Execution: isolated CWD `out/tmp/isolated-native-p2`, exit code 0 (`PASS: actual NGX + production Renderer mapping, submit, alpha and restore verified on hardware; no gameplay or motion-response quality claim`).
- 15-line log preserved in `.cache/evidence/p2-native-run/test_run.log` (`DEVICE=NVIDIA GeForce RTX 5080`, driver 616.56, `MODE=native_ngx`). Running `vulkaninfo` enumerates 8 layers and does not include `VK_LAYER_KHRONOS_validation`; no validation layer coverage is claimed.
  - All 5 native test cases completed with 0 alpha mismatches (`alpha_mismatches=0`): Quality (853×480->1280×720), Quality reuse, Balanced dynamic switch (742×418->1280×720), Performance 1080p reconfiguration (960×540->1920×1080), and Performance injected evaluation failure (`failure=1`) fallback. These results may be reused while behavior and configuration remain unchanged; rerun only for a relevant change or new failure evidence.

### Capabilities and Limits of `LoNativeDlssRendererTest`

- **Verified by fixture**: Quality mode reuse, Balanced mode switching, Performance mode 1080p reconfiguration, non-zero finite RGB/alpha readback, incompatible target restoration, and evaluation failure fallback on hardware.
- **Unverified by fixture**: Motion vectors, jitter response, reversed-Z depth correctness (it uses constant 0.5 depth, zero motion vectors, and uniform color), real swapchain presentation, `video.cpp` queue synchronization, guest `DrawImpl` pipeline integration, or live performance metrics. Software mock fixtures test platform routing boundaries rather than NVIDIA driver execution.

### 2026-09-21 Foreground Game Capture & Color Pipeline Verification

`LostOdysseyRecomp` was executed in a visible foreground window (`PID 42720`, isolated CWD `out/p2-game-cwd-c2f0602`, assets from `LostOdysseyRecompLib/private/disc1`). The user triggered render state capture via `F1`.

- **Capture trigger**: Capture bundle `out/p2-game-cwd-c2f0602/captures/render-17900119851502951-f11889.zip` contains 3 complete frames (11889, 11890, 11891). Note that `LO_CAPTURE_REQUEST` only triggers legacy single-frame dumps, whereas `F1` captures the full multi-frame state. Users do not need to recapture this pipeline.
- **Tonemap & Color Lineage**:
  - HDR buffer: Allocation 9, address `0x09fa0000`, guest format 32.
  - Draw 632: VS `9b81c55ca39bb529`, PS `b4b4d54a7a2d6b96`, output EDRAM Allocation 3 (guest base 720 decimal / `0x2d0`, guest format 0 mapping to host FP16 / format 10, 1280×736, 1280×720 active rect), `colorMask = 7` (predicate requires `(colorMask & 7) == 7`).
  - Constant `c10.x = 0x3ee8ba2e` (`0.4545454383` ≈ `1.0 / 2.2`) across all 3 frames. Math is `pow(saturate(M + 0.1725 * bloom), 1.0 / 2.2)`, outputting **display-encoded SDR** (gamma 2.2 curve) for this specific analyzed capture pipeline. It is not an exact standard sRGB transfer, not sRGB-decoded, and not linear HDR.
  - Resolve to Allocation 20 (`R8G8B8A8_UNORM`) with RB swap. Sampled in Draw 659 (VS `8bbd4da701845d16`, PS `cda578aef1724fdc`) with `shared_texture_info = 0x00160a00`, `sign = 0` (fetch BGR), copy blend `ONE / ZERO / ADD`, `colorMask = 15`. Net transfer is identity.
  - 24 intermediate draws have `(colorMask & 7) == 0` (depth/stencil or alpha-only passes). UI draws start at Draw 660 onto frontbuffer Allocation 4 (`0x00714000`).
  - Write ordinals across frames 11889/11890/11891: Alloc 9 (207457/207475/207493), Alloc 20 (207464/207482/207500), Alloc 4 (207465/207483/207501).
  - Planning state: `candidate_ready = true`, `temporal_history_verified = false`. `activePlan.consumer = 0 (None)`, `input = 1280x720`, `output = 1280x720`, `epoch = 1`.
  - *Historical sizing capture observation*: The running executable (`PID 42720`) recorded the pre-fix sizing failure caused by `ProbeOnce` calling `Shutdown(true)` before later `QueryOutputSizing` calls. The Sizing Session Persistence fix is now verified on RTX hardware via `LoNativeDlssProbe --production-sizing`, and the later live-game record proves qualified `1707x960 -> 2560x1440` production SR. The old process and its `1280x720 -> 1280x720` capture remain historical observations, not current implementation limits.
- SDR qualification tracking and the runtime geometric quad predicate are implemented and covered by the CPU boundary evidence; live qualification accepts the actual physical uploaded quad, exact resolve ordinal and net RGB view, while other candidate paths remain `Unknown`. The detailed design is recorded in [docs/notes/native-dlss-color-qualification-plan.md](native-dlss-color-qualification-plan.md).

### Current live-game production SR record

The final bounded runtime record is `.cache/evidence/game-sr-runtime.json`.
On an RTX 5080, Quality SR ran at `1707x960` and output at `2560x1440`, with
`DisplayEncoded` color and inverted depth (`reversed-Z`). It records one
successful Create and 24 retained successful Evaluate records under a 128-entry
cap, not a total-call count. Isolated use `10684` at serial `23614` was
accepted and submitted and remained in flight in the snapshot. `completed
through 23612` is the earlier SR completion watermark. No failure was recorded.

This confirms live production execution only. Visual quality, motion response,
occlusion, UI, reset behavior, performance and player acceptance remain
unclaimed. The next work is targeted acceptance of those areas; completed
sizing, color-qualification, extent-growth and cold-start evidence is reused
while behavior and configuration remain unchanged; rerun only for a relevant
change or new failure evidence.

### Next Steps before Broad Game-Level Acceptance

Gate 3 remains unapproved. Targeted visual, motion, occlusion, UI and reset
acceptance remains to be performed. The `LO_NO_RENDERER` shutdown boundary and
legacy execution fixture's unchecked `void` fence-wait gap remain open.

## 2026-09-21 Second Development Follow-up

See [latest implementation, evidence and next RTX run](native-dlss-p2-next-run.zh-CN.md). Actual Renderer target-map/Flush/restore methods execute in software Vulkan tests (1,536 checks), in addition to the 256 FP16 shared-shader comparisons. The reversed-Z producer convention is explicitly carried into NGX feature flags. Windows/Linux SDK ON/OFF builds of the new `--native` Renderer fixture pass; NVIDIA execution of that mode is represented by the later bounded live-game record above, not by this software/SDK lane. SDK-disabled skip=77 is not a GPU pass.

CPU failure-stop handling was already implemented in `59e9dce`. The read-only checker triages the three per-frame oracle JSONL files with 13 Python tests and optional CTest. It never qualifies color or approves P2: manual CLI triage always reports `color_encoding=unknown` and `p2_accepted=false`. Python is an optional development tool and has no runtime dependency. Runtime game-color qualification beyond the documented narrowed proof, complete guest DrawImpl/UI/presentation behavior, real driver DeviceLost, gameplay and performance remain unaccepted. Historical RTX results below retain their original boundaries.

## 1. Overview and Scope

This document records verification evidence for native Vulkan NVIDIA DLSS integration on the `dlss` development branch across P0, P1, and current experimental P2 work.

### P0 Scope
- Pinned official NGX SDK bootstrap dependency and Vulkan extension negotiation.
- Plume Vulkan bridge hooks (`VulkanExtensionHooks`, `VulkanExtensionStatus`, and external command boundaries).
- Capability discovery, query parameter lifecycle, and optimal resolution settings via `LoNativeDlssProbe` and `LoNativeDlssReportTest`.
- Fallback policies: runtime fallback policies (reverting to baseline Vulkan without DLSS when the SDK is disabled at build time, when hardware is unsupported, or when the staged runtime is absent) and verified negative standalone probe behavior.

### P1 Scope
- CPU frame planning with versioned 24-word snapshot packets transmitting true lower internal rendering resolution, request signatures, geometry epochs, and exact NGX output sizing.
- Demand-driven NGX sizing cache with independent request-level DLSS disable latches (decoupled from legacy out-of-memory retry step-downs).
- Renderer separation: captures pre-TAA color, R32 current depth, and unjittered geometric motion vectors in input pixel units (`previousPixel - currentPixel`).
- Explicit history reset conditions (camera cuts, extent changes, geometry epochs, format changes) and GPU fence-qualified resource retirement.
- Local diagnostic mode `LO_DLSS_INPUT_PROBE=1` driving true lower-resolution rendering and spatial presentation without executing legacy TAA.
- Ordinary DLSS configuration requests continue to fall back to legacy rendering paths until P2.

### P2 Scope (Active Work in Progress, Historical Checkpoint Retained)
- Persistent per-device NGX feature session managed by `gpu::dlss::Controller`.
- Feature creation (`NGX_VULKAN_CREATE_DLSS_EXT1`) and evaluation recording (`NGX_VULKAN_EVALUATE_DLSS_EXT`) with checked Vulkan command buffer reset, begin, and end operations.
- Renderer-side SR dispatch routing, monotonic submission-serial tracking for renderer and presentation queue batches, and request-level failure latches.
- Unknown color space bypass to protect unverified game tonemapping pipelines from improper upscaling.
- Destination target promotion architecture with parked low-resolution fallback mappings and diagnostic capture hooks (`p2-oracle.jsonl`).
- Production destination resample (`DrawPromotionResample`) validated via build entry-point self-test (`--self-test-scene-copy-promotion`).

P0 and P1 results remain closed and reused. P2 is an active work-in-progress on the `dlss` development branch (progressed through commit `c2f0602`); bounded live-game production SR execution is confirmed in `.cache/evidence/game-sr-runtime.json`, the earlier formal Gate 3 re-review was cancelled at user direction, and Gate 3 is not approved. Frame Generation remains deferred.

## 2. Pinned SDK Provenance and Cryptographic Hashes

The build system does not perform automatic downloads. Developers supply an audited local checkout of the official NVIDIA DLSS repository at commit `374959484e79a640feaba44c93ac8cfb0a03f5b5` (version 310.9.1).

The integration uses the static CRT (`MultiThreaded` / `/MT`) bootstrap libraries on Windows and static archive on Linux:

| Relative Path within SDK Root | Role | SHA-256 |
|---|---|---|
| `include/nvsdk_ngx_vk.h` | NGX Vulkan C API header | `3be54d0103cce02174fd34912c60c9f40c73b5eec919e42613a2268c589c53ad` |
| `include/nvsdk_ngx_helpers.h` | NGX helper definitions | `4569aa7566667d8c0d93c8e50ddd3f2c4fa1c89da536ffdc2571bbdc97a539e9` |
| `LICENSE.txt` | NVIDIA DLSS SDK License | `3027f23ca5a46dd9cb8183fbd522983a86f64d7daac5982912bf9f214671f294` |
| `lib/Windows_x86_64/x64/nvsdk_ngx_s.lib` | Windows static-CRT release bootstrap | `4e5d355086d2bc11e1a0842457d2519ea528ee1f3e112c45679a84960c07dff3` |
| `lib/Windows_x86_64/x64/nvsdk_ngx_s_dbg.lib` | Windows static-CRT debug bootstrap | `f62f94f7a444ddffa118e71bc412298d803784bf042464be178a989915198cab` |
| `lib/Windows_x86_64/rel/nvngx_dlss.dll` | Windows staged SR runtime | `3975567b8943c53acce397f2b72380092f84f162d00b0d2c7d08a1025c563983` |
| `lib/Linux_x86_64/libnvsdk_ngx.a` | Linux static bootstrap | `3321c4ca9adf71f345550d10bf2148ed38003151b03ec5f81c99d00949ab8202` |
| `lib/Linux_x86_64/rel/libnvidia-ngx-dlss.so.310.9.1` | Linux staged SR runtime | `7561f16cab74e2b7ccf791bf44bb9cc43abb44bcf78f33c5663520ad4e857e02` |

Local SDK files are outside Git tracking and must never be committed.

## 3. Build Options and Reproduction Commands

### CMake Configuration Flags
- `LO_ENABLE_DLSS` (default `OFF`): Master switch for DLSS integration.
- `LO_DLSS_SDK_ROOT`: Path to the local checkout of NVIDIA DLSS repository.
- `LO_DLSS_STAGE_RUNTIME` (default `ON`): Copies `nvngx_dlss.dll` (or `.so`) to the target output directory during build.

### Standalone Probe Build (Windows)
```bash
RC="C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/rc.exe" \
cmake -S tools/tests/native_dlss -B out/build/native-dlss-standalone -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER="C:/Program Files/LLVM/bin/clang-cl.exe" \
  -DCMAKE_CXX_COMPILER="C:/Program Files/LLVM/bin/clang-cl.exe" \
  -DLO_ENABLE_DLSS=OFF
cmake --build out/build/native-dlss-standalone --target LoNativeDlssReportTest LoNativeDlssProbe --parallel 4
ctest --test-dir out/build/native-dlss-standalone --output-on-failure -R "LoNativeDlss(Probe|ReportTest)"
```

### Runtime and Test Builds with SDK Enabled (Windows)
To build the standalone tests in an existing configured root tree:
```bash
cmake --build out/build/windows-clang --target LoNativeDlssProbe LoNativeDlssReportTest --parallel 4
out/build/windows-clang/tools/tests/native_dlss/LoNativeDlssProbe.exe
```
To compile the runtime executable with DLSS enabled (requires pre-generated PPC sources under `LostOdysseyRecompLib/ppc` and `LO_BUILD_RUNTIME=ON`):
```bash
cmake -S . -B out/build/windows-clang \
  -DLO_ENABLE_DLSS=ON \
  -DLO_DLSS_SDK_ROOT="C:/Users/freefrank/ownCloud/Git/LostOdysseyRecomp/.cache/deps/nvidia-dlss-37495948"
cmake --build out/build/windows-clang --target LostOdysseyRecomp --parallel 4
```

### Intended Standalone Linux Configuration (Untested)
```bash
cmake -S tools/tests/native_dlss -B out/build/native-dlss-standalone-linux \
  -DLO_ENABLE_DLSS=ON \
  -DLO_DLSS_SDK_ROOT=/path/to/audited/nvidia-dlss-37495948
cmake --build out/build/native-dlss-standalone-linux --target LoNativeDlssProbe LoNativeDlssReportTest
```
*Note: Linux native verification was skipped in P0 because no native Linux GPU test environment was available.*

## 4. Verification Evidence and Test Results

Evidence is preserved in `.cache/evidence/native-dlss-p0.json`.

### 4.1 Plume Vulkan Bridge Command Recording Fixture
The Plume Vulkan extension hooks and external command demarcation were verified via `LoPlumeBridgeTest`. The test exercises interface/device query hooks, extension deduplication, `beginExternalCommands()`, render pass termination, descriptor cache invalidation, and `endExternalCommands()`.
- Result: **PASS** (reused bridge fixture; CPU command recording only, no GPU submission, no synthetic device-lost injection).

### 4.2 SDK Disabled Build (`LO_ENABLE_DLSS=OFF`)
- Runtime build passed: `LostOdysseyRecomp.exe` compiles cleanly without DLSS symbols.
- `LoNativeDlssProbe.exe` exit code: **77** (`ProbeState::SdkDisabled` correctly mapped to CTest skip code).
- `LoNativeDlssReportTest.exe`: **PASS** (0 errors).

### 4.3 SDK Enabled on NVIDIA Hardware (`LO_ENABLE_DLSS=ON`)
Execution on local hardware:
- GPU: **NVIDIA GeForce RTX 5080**
- Driver: **616.56** (reported driver version string: 616.56)
- Exit code: **0** (`ProbeState::Available`)
- Capability parameters returned:
  - `SuperSampling_Available`: 1
  - `SuperSampling_NeedsUpdatedDriver`: 0 (minimum required driver 470.0)
  - `SuperSampling_FeatureInitResult`: 1 (`NVSDK_NGX_Result_Success`)
- Vulkan instance extensions requested: `VK_KHR_get_physical_device_properties2`.
- Vulkan device extensions requested: `VK_NVX_binary_import`, `VK_NVX_image_view_handle`, `VK_KHR_buffer_device_address`, `VK_KHR_push_descriptor`.
- Optimal input dimensions for requested 1920×1080 output:
  - **Quality**: 1280×720
  - **Balanced**: 1114×626
  - **Performance**: 960×540
- All 15 sequential NGX bootstrap API calls succeeded (`raw = 1`).

### 4.4 Missing Staged Runtime Failure Injection
To verify controlled error handling when the SDK bootstrap is present but the runtime DLL is absent:
1. Temporarily renamed `nvngx_dlss.dll` to `nvngx_dlss.dll.hidden-for-fallback`.
2. Executed `LoNativeDlssProbe.exe`.
3. Process exited with code **1** (`ProbeState::ApiError`), with `GetFeatureRequirements` returning raw NGX error code `-1160773614` (`0xBAD00012`, `NVSDK_NGX_Result_FAIL_NotImplemented`).
4. Staged DLL was restored.

### 4.5 Production Report & Capability Decision Fixture
`LoNativeDlssReportTest.exe` directly tests the production decision logic in `dlss_ngx.cpp`:
- Verifies base Vulkan interface/device error exit mapping (exit 1).
- Verifies capability getter failures, unavailable Super Sampling flag, driver update requirements, and `FeatureInitResult` classifications.
- Verifies optimal settings dimension validation and exit code policies.
- Result: **PASS** (exit 0).

### 4.6 P1 Sizing Query on NVIDIA Hardware (1280×720 Output)
To verify resolution planning across multiple output targets, NGX optimal settings queries were executed for 1280×720 output on the local RTX 5080:
- Command: `out/build/native-dlss-p1-sdk/Debug/LoNativeDlssProbe.exe --sizing 1280 720`
- Exit code: **0**
- Optimal settings returned (`raw = 1`):
  - **Quality**: 853×480 (min 640×360, max 1280×720)
  - **Balanced**: 742×418 (min 640×360, max 1280×720)
  - **Performance**: 640×360 (min 640×360, max 1280×720)

### 4.7 P1 CPU Frame Planner Verification
The standalone CPU planner fixture (`LoP1FramePlanTest`) tests production planner logic in `gpu/frame_plan.h` and `gpu/upscaling_plan.cpp`:
- 62 checks passed across mailbox error reporting, request-level DLSS disable latches (independent of legacy OOM retry state), low-720 floor terminal stability, 24-word wire packet encoding/decoding, and sizing cache synchronization.
- Result: **PASS** (62 checks).

### 4.8 P1 Vulkan GPU Temporal Input Verification
The standalone motion replay fixture (`motion_replay_gpu_test --p1-inputs-only`) executed on local NVIDIA GeForce RTX 5080 hardware:
- Verified single-channel R32 depth and pre-TAA color capture for first and second frames.
- Verified absence of legacy TAA color resolve and lazy allocation of TAA history buffers.
- Verified unjittered input-pixel motion vector convention (`previousPixel - currentPixel`) with known `(-2, 0)` vector movement from actual motion replay.
- Verified explicit history reset triggers: camera cuts, extent changes, geometry epochs, and format changes.
- Verified fence-qualified GPU resource retirement and single motion finalization before consumers.
- Result: **PASS** (112 checks).

### 4.9 Compilation Scope
All changed translation units compiled without errors:
- Lane A: `gpu/frame_plan.cpp`, `gpu/upscaling_plan.cpp`, `gpu/video.cpp`, `gpu/dlss_ngx.cpp`, `settings/config.cpp`, `gpu/command_processor.cpp`.
- Lane B: `gpu/renderer.cpp`.

### 4.10 P2 Native NGX Execution and Destination Promotion Evidence

Evidence is preserved in `.cache/evidence/native-dlss-p2-a.json`, `.cache/evidence/native-dlss-p2-b.json`, and `out/build/windows-clang/p2-bootstrap-cwd/evidence/native-dlss-p2-resample.json`.

#### Lane A: Persistent NGX Session and Command Recording Fixture
The standalone execution test (`LoNativeDlssExecutionTest`) was compiled with MSVC against pinned SDK `310.9.1` and executed on an NVIDIA GeForce RTX 5080 (driver 616.56):
- Managed persistent feature lifetime across frames via `gpu::dlss::Controller`, verifying feature creation and evaluation recording.
- Raw NGX calls returned success: `Sizing_DLSS_GetOptimalSettings` returned `1`, `CREATE_DLSS_EXT1` returned `1`, and `EVALUATE_DLSS_EXT` returned `1`.
- Raw native Vulkan command buffer calls (`vkResetCommandBuffer`, `vkBeginCommandBuffer`, `vkEndCommandBuffer`) returned `0` (`VK_SUCCESS`).
- Three primary command buffers were submitted with a GPU fence. The output buffer started with a zero sentinel; post-execution readback confirmed only that the first RGBA16F pixel was finite and nonzero. This sentinel-change check does not prove motion-vector responsiveness, color fidelity, or alpha preservation.
- Host-injected evaluation failure confirmed exclusion of the isolated command buffer while preserving prefix spatial fallback readability.
- Runtime validation layer check printed `VALIDATION_LAYER_KHRONOS=unavailable`; no Khronos validation messages were captured.
- Result: **PASS** (exit 0). Prior P0/P1 results (62 CPU checks, 112 GPU checks) were reused; they remain reusable while behavior and configuration remain unchanged.

#### Lane B: Production Destination Resample and Renderer Routing
- Implemented SR dispatch routing, monotonic submission-serial tracking for Vulkan renderer and presentation queues, and diagnostic capture hooks (`p2-oracle.jsonl`).
- **Historical unknown-color checkpoint**: static code review confirmed that unqualified tonemapping inputs were bypassed in the earlier checkpoint. The current live-game record and narrowed qualification proof supersede the old blanket no-dispatch wording; other candidate paths still remain `Unknown`.
- Entry-point self-test (`--self-test-scene-copy-promotion` guarded by `LO_RENDERER_P2_SELFTEST`) executed on the real RTX 5080 hardware using production `DrawPromotionResample`. Readback confirmed 4×4 to 8×8 RGBA upscaling (64 destination pixels) with 0 mismatches and 1-byte alpha tolerance (`out/build/windows-clang/p2-bootstrap-cwd/evidence/native-dlss-p2-resample.json`, exit 0).
- Scope limits: The resample test validates isolated image upscaling only. It does not validate active target promotion mapping, guest alpha preservation, mid-frame Flush survival, UI/draw ordering, texture aliasing, fence synchronization, dynamic window resizing, or full-game behavior. These missing areas are acceptable for an in-progress work checkpoint and do not require completion before pausing.

#### Review Checkpoint Status (Historical Checkpoint)
Gate 3 initial review attempt 1 identified 4 action items (addressing readback SR guard, validation define leakage, JSON bracket formatting, and self-test failure resource cleanup). All four items were implemented and verified locally (including clean self-test queue draining and resample verification in `out/build/windows-clang/p2-bootstrap-cwd/evidence-cleanup/native-dlss-p2-resample.json` with 0 mismatches). Formal re-review was cancelled at user direction; Gate 3 was not approved, and the implementation was historically paused at that checkpoint. P2 development has since resumed on `dlss` through commit `c2f0602`, but remains incomplete and unaccepted. Detailed implementation handoff and developer guidelines are provided in [docs/notes/native-dlss-handoff.zh-CN.md](native-dlss-handoff.zh-CN.md).

#### Historical Color Qualification Checkpoint
Offline analysis of the historical f11889 capture's pixel shader `b4b4d54a7a2d6b96` confirmed that its final pass computes `exp2(c10.x * clamped log2(v))`. That checkpoint's `c10.x` evidence describes a gamma-2.2 display-encoding curve; it is not an exact standard sRGB transfer. The current narrowed qualification proof covers the actual physical uploaded quad, exact resolve ordinal and net RGB view; other candidate paths remain `Unknown`.

## 5. Architectural and Licensing Boundaries

### Search Path vs. Loaded Runtime Version
The probe and runtime pass the directory containing the staged `nvngx_dlss.dll` via `NVSDK_NGX_FeatureCommonInfo.PathListInfo`. This specifies a search directory for NGX feature discovery. It does not provide absolute cryptographic proof of which specific runtime DLL binary image was mapped into the process by the NVIDIA driver or NGX Core; the driver or system NGX cache may resolve dependencies according to driver-defined precedence.

### Redistribution and Licensing Notice
- Lost Odyssey Recomp is distributed under the GNU General Public License v3 (GPLv3).
- NVIDIA DLSS SDK headers and static bootstrap libraries are governed by NVIDIA's proprietary license (`LICENSE.txt` SHA-256 `3027f23c...`).
- While optional static bootstrap linking permits local development, public redistribution of builds with `LO_ENABLE_DLSS=ON` bundled with or linking proprietary NGX components raises unresolved licensing questions.
- Binary redistribution remains unresolved, and no binary packages are released.

### Known Limits and Boundary Conditions
- **Bounded In-Game DLSS Dispatch**: The live-game record confirms one bounded production SR run. It does not establish broad gameplay usability, visual quality or player acceptance.
- **P2 Work In Progress**: Development resumed with committed changes on 2026-09-21. Gate 3 is not approved; no completed formal re-review is claimed.
- **Linux Evidence Scope**: CPU tests, renderer translation-unit compilation, SDK ON/OFF builds/report tests and software Vulkan composites ran on Linux. NVIDIA NGX execution and Linux gameplay remain untested for these revisions.
- **Self-Test Scope**: The promotion resample test covers isolated resample math; it does not prove end-to-end target promotion, guest alpha preservation, UI ordering, or mid-frame flush safety.
- **Renderer Recognition Scope**: The focused GPU fixture verifies single motion finalization before consumers, but does not exercise separate HDR and SDR scene-recognition branches.
- **Allocation Geometry**: The follow-up adds nonzero subregion origins and integer-overflow CPU cases, plus FP16 composites with an 8×8 target and smaller SR scratch. These do not establish padded NGX/game allocations or the complete renderer mapping path.
- **Temporal & Config Discontinuities**: The test suite does not simulate temporal frame-time discontinuities (such as 250 ms stalls). Persisted configuration handling remains unexecuted in focused fixtures.
- **Jitter Proof**: The GPU fixture verifies unjittered motion vector inputs with a synchronized raster jitter sample, but does not provide end-to-end full renderer jitter coverage.

## 6. Next Steps

- Complete Gate 3 code review and address review findings.
- Perform targeted visual, motion, occlusion, UI and reset acceptance using the documented qualified SR path; retain the historical f11889/f2347 capture limits and reuse completed evidence while behavior remains unchanged.
- Validate guest alpha preservation, mid-frame flush handling, and UI pass ordering.
- P4 Frame Generation remains deferred until Super Resolution (P1–P3) is completed and verified.

## 7. 2026-09-22 Implementation and Verification Status (BR-01, BR-02, BR-03 & GraphicsRow)

This section documents implementation and targeted verification for BR-01, BR-02, BR-03, and graphics menu stability following the post-v0.6.7 audit. Historical baseline findings, prior gate evidence, and previously established boundaries are preserved above. Diagnostic-path item K-01 remains unchanged. Implementation and targeted tests are complete; production capture finalization and a fresh full executable rebuild remain pending, while visual gameplay acceptance and public release remain to be conducted.

### BR-01: Temporal Lifecycle Harmonization and Clock Advancement (Committed in 0625923)
- **Root Cause & Fix**: Prior code omitted `dlssSrRequested` when updating `temporalFrameTime` and choosing jitter across frame gaps, causing normal DLSS SR and DLAA to continuously reset temporal history after 250 ms. Implemented `gpu::temporal::TemporalConsumerActive` in `LostOdysseyRecomp/gpu/temporal_lifecycle.h` to unify consumer checks across frame start, interval detection, and frame end.
- **Boundary Correction**: Corrected the frame-end reset predicate to `decision.reset = !decision.complete || (decision.gap && !decision.gapAlreadyReset)`, guaranteeing that uncompleted frames reset history regardless of frame-start long-gap resets.
- **Validation**:
  - `LoTemporalLifecycleBr01Test` (cross-platform CPU target): 154 clock advancement and state-machine checks passed; 14 gap-specific checks passed following the logic correction.
  - `LoTemporalLifecycleBr01OwnerTest` (`WIN32 AND LO_BUILD_GPU` target): 12 hardware checks passed on an RTX 5080 (Direct3D 12 backend with motion stub textures, no NGX runtime dependency, no game launch), verifying lifecycle initialization, steady-state stepping, 300 ms gap handling (>250 ms threshold), recovery, and repeated-color invalidation. Results were preserved without re-runs.

### BR-02: Device Capability Snapshot (Committed in 0625923)
- **Root Cause & Fix**: The CPU frame planner directly accessed mutable NGX controller state while the GPU worker updated it during output sizing queries. Resolved by publishing a mutex-protected by-value `BackendDeviceSnapshot` during initialization, sizing, and cleanup (`PublishDeviceCapability` / `PublishedDeviceCapability`).
- **Validation**:
  - `LoDlssCapabilitySnapshotTest` (cross-platform CPU target): 43 checks passed in commit 0625923. Updated with revised execution-matching assertions and re-run cleanly (43 checks passed), then retained without further re-runs.

### BR-03 & Menu Stability: Execution-Layer Feedback and GraphicsRow Refactor
- **GraphicsRow Shared Enumeration**: Refactored menu row indexing into `GraphicsRow` (`0..10`), indexing help text, key navigation, action dispatch, and tests by ID to prevent position drift. Corrected screenshot indexing assertions for AA and frame-rate rows.
- **Execution Feedback Closed Loop**:
  - `Active` status now strictly requires matching a production observation where the renderer adopted DLSS RGB output and completed checked Vulkan command submission.
  - Granular fallback and failure reporting: distinguishes persistent capability/failure latches from dynamic fallbacks (motion pipeline pending, unknown color encoding, feature recreation, promotion mapping failure, no eligible drawables, waiting for initial results, input probe mode, and stopped status).
  - Triple identity filtering (device epoch, request signature, geometry epoch) prevents stale state propagation; intra-frame plan switches retain latest failure reasons; and immediate stopped status is published on `SubmitVulkan` or `WaitForGpuFence` errors.
  - Menu display extents are drawn directly from `execution.plan`, with uncommitted menu edit notes appended.
- **Validation**:
  - `LoDlssRuntimeStatusTest` (CPU target): 37 status transition and fallback classification checks passed.
  - `LoVideoSubmissionStopTest` (CPU error injection target): 14 checks passed, confirming stopped-status publication upon native submission and fence failures.
  - Hardware synthetic Vulkan execution on an RTX 5080 (`native_dlss_renderer_gpu_test.exe` without NGX): `--status-only` passed (target adoption, checked submission, input selection, frame-end publishing); `--plan-identity` passed (fallback->fallback, success->fallback, pre-submission plan switch retaining armed plan, non-empty batch non-demotion).
  - `LoMenuFlowTest`: passed with updated execution status strings and GraphicsRow indexing, producing 18 BMP captures and `notices.txt` under `out/br03-dlss-menu/`. Visual check verified notice formatting across short (18), long (05), Chinese (06), and uncommitted quality (08) entries without requiring changes to `menu_render`.
  - Affected production translation units compiled cleanly.

### Build and Verification Scope
- CMake configuration registers all new test targets (`LoDlssRuntimeStatusTest`, `LoVideoSubmissionStopTest`, etc.).
- Test result reuse boundary: Prior BR-01 CPU and hardware owner results were preserved without re-runs; `LoDlssCapabilitySnapshotTest` was updated for revised Active semantics and re-run cleanly (43 checks), then reused; existing unrelated GPU suites were not re-run.
- Active status indicates verified target adoption and checked submission, without asserting GPU execution completion, display presentation, or full-frame DLSS visual quality.
- `screenshot.bmp` captured via `F1` debug state capture reflects the final swapchain surface immediately before host presentation when the active rendering path employs DLSS, letterboxing, or post-processing, while `guest-frontbuffer.bmp` retains the renderer resolved host texture with each pair sharing matching frame/swap correlation tickets.
- Status logging, capture test coverage, depth lifetime fix, and Evaluate capture: new CPU suite `LoDlssStatusLogTest` (400 checks against real logger), presentation capture hardware fixture `LoPresentCaptureTest` (4 multi-frame/error cases, plus `--case close`), depth retirement suite `motion_replay_gpu_test.exe --depth-retirement-only` (26 checks on RTX 5080 Vulkan D32S8), and evaluate capture suites `LoDlssEvaluateCaptureContractTest.exe --evaluate-capture-contract-only` and `LoNativeDlssRendererTest.exe --evaluate-capture-only` passed; `motion_renderer_compile` compiled cleanly.
- Synchronous NGX Evaluate input/output capture: Controller records isolated pre-Evaluate color copy and post-Evaluate scratch output copy with `VK_IMAGE_LAYOUT_GENERAL` restoration before composite/UI. Captures export `dlss-evaluations.json`, `dlss-input-NNN.bin` / `-preview.bmp`, and `dlss-output-NNN.bin` / `-preview.bmp` (RGBA8/RGBA16F formats with jitter, reset, and plan metadata; fallback frames omit mock images).
- Full executable relink and verification baseline: the updated game executable target `LostOdysseyRecomp` completed linking successfully (`build/LostOdysseyRecomp/LostOdysseyRecomp.exe`, 93,635,072 bytes, SHA-256 `08d50e3774d18a02d4f6eaf2267472e9fab75db36e3ee970980aa96faf641e9d`, UTC 2026-09-23 02:32:58 / local 2026-09-22 20:32:58 -0600, built from `0625923` plus uncommitted changes with source ID `bdd9539ee4f176bdda0d9660bb5621b8a90a09acf8f8faa8427c10f2075c2688`; earlier 16:24:35 and 19:33:37 intermediate builds preserved). Live user session previously verified Quality -> DLAA switching without crashes; the user's next step is Quality/DLAA export with the new binary (Off baseline run does not need to be repeated). In-game visual quality verification, player acceptance, and release packaging have not been performed.
