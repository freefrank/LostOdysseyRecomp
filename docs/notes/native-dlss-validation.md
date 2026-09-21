# Native DLSS Validation (P0 Foundation, P1 Temporal Inputs & P2 Checkpoint)

Date: 2026-09-21
Baseline: `main@5b765f617ec511a2f76aa9da7923a8c122679d2c`
Feature branch: `dlss` (prior pushed baseline: `91bf37e`; P2 checkpoint commits: `d018be7` adapter, `744ab91` scene-copy path)
SDK Reference: NVIDIA DLSS **310.9.1** (`374959484e79a640feaba44c93ac8cfb0a03f5b5`)
Status: **P0 gate 1 passed**; **P1 gate 2 passed** (historical evidence retained). P2 development resumed on 2026-09-21 with focused fixes and additional tests; it is **not complete or Gate 3 approved**. The earlier formal re-review was cancelled. In-game SR remains blocked by unqualified color encoding. Frame Generation (FG) deferred.

## 2026-09-21 Development Follow-up

The earlier paused checkpoint has received five focused implementation/test changes on `dlss`; see [P2 follow-up evidence and remaining blockers](native-dlss-p2-progress-2026-09-21.md) for commits, reproducible commands and exact scope. Historical RTX results below were not rerun and must not be attributed to the new revisions.

New evidence covers CPU contracts, actual renderer translation-unit compilation, pinned SDK ON/OFF adapter compilation and report tests, and software Vulkan execution of production FP16 composite shaders. It does not qualify runtime game color, full target-map transitions, fatal submission/device-loss recovery, or NVIDIA in-game SR. Gate 3 remains unapproved.

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

### P2 Scope (Experimental Work in Progress, Paused)
- Persistent per-device NGX feature session managed by `gpu::dlss::Controller`.
- Feature creation (`NGX_VULKAN_CREATE_DLSS_EXT1`) and evaluation recording (`NGX_VULKAN_EVALUATE_DLSS_EXT`) with checked Vulkan command buffer reset, begin, and end operations.
- Renderer-side SR dispatch routing, monotonic submission-serial tracking for renderer and presentation queue batches, and request-level failure latches.
- Unknown color space bypass to protect unverified game tonemapping pipelines from improper upscaling.
- Destination target promotion architecture with parked low-resolution fallback mappings and diagnostic capture hooks (`p2-oracle.jsonl`).
- Production destination resample (`DrawPromotionResample`) validated via build entry-point self-test (`--self-test-scene-copy-promotion`).

P0 and P1 results remain closed and reused. P2 is recorded in checkpoint commits `d018be7` and `744ab91`, paused for handoff; no actual in-game Super Resolution dispatch has occurred, and formal Gate 3 re-review was cancelled at user direction and is not approved. Frame Generation remains deferred.

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
- Result: **PASS** (exit 0). Prior P0/P1 results (62 CPU checks, 112 GPU checks) were reused and not rerun.

#### Lane B: Production Destination Resample and Renderer Routing
- Implemented SR dispatch routing, monotonic submission-serial tracking for Vulkan renderer and presentation queues, and diagnostic capture hooks (`p2-oracle.jsonl`).
- Implemented unknown color space bypass: static code review confirms that while runtime tonemapping encoding remains unqualified, SR evaluation is bypassed in-game. This is static review confirmation of the guard, not runtime game validation, and tonemapped inputs are unqualified rather than invalid.
- Entry-point self-test (`--self-test-scene-copy-promotion` guarded by `LO_RENDERER_P2_SELFTEST`) executed on the real RTX 5080 hardware using production `DrawPromotionResample`. Readback confirmed 4×4 to 8×8 RGBA upscaling (64 destination pixels) with 0 mismatches and 1-byte alpha tolerance (`out/build/windows-clang/p2-bootstrap-cwd/evidence/native-dlss-p2-resample.json`, exit 0).
- Scope limits: The resample test validates isolated image upscaling only. It does not validate active target promotion mapping, guest alpha preservation, mid-frame Flush survival, UI/draw ordering, texture aliasing, fence synchronization, dynamic window resizing, or full-game behavior. These missing areas are acceptable for an in-progress work checkpoint and do not require completion before pausing.

#### Review Checkpoint Status
Gate 3 initial review attempt 1 identified 4 action items (addressing readback SR guard, validation define leakage, JSON bracket formatting, and self-test failure resource cleanup). All four items were implemented and verified locally (including clean self-test queue draining and resample verification in `out/build/windows-clang/p2-bootstrap-cwd/evidence-cleanup/native-dlss-p2-resample.json` with 0 mismatches). Formal re-review was cancelled at user direction; Gate 3 is not approved, and P2 implementation is paused as an incomplete working-tree checkpoint. Detailed implementation handoff and developer guidelines are provided in [docs/notes/native-dlss-handoff.zh-CN.md](native-dlss-handoff.zh-CN.md).

#### Color Qualification Status
Offline analysis of game post-processing pixel shader `b4b4d54a7a2d6b96` confirmed that the final pass computes `exp2(c10.x * clamped log2(v))`. The exponent `c10.x` is a runtime pixel shader constant. Known UNORM storage format does not establish whether the underlying game color encoding is linear or gamma-curve. Because runtime constants and producer-chain textures are not yet qualified, the color encoding remains unknown, and unknown color space remains a bypass condition. Remaining capture requirements to qualify this pipeline are runtime PS constants `c0`–`c10`, `c255`, and the producer-resolve-copy allocation chain via `p2-oracle.jsonl`.

## 5. Architectural and Licensing Boundaries

### Search Path vs. Loaded Runtime Version
The probe and runtime pass the directory containing the staged `nvngx_dlss.dll` via `NVSDK_NGX_FeatureCommonInfo.PathListInfo`. This specifies a search directory for NGX feature discovery. It does not provide absolute cryptographic proof of which specific runtime DLL binary image was mapped into the process by the NVIDIA driver or NGX Core; the driver or system NGX cache may resolve dependencies according to driver-defined precedence.

### Redistribution and Licensing Notice
- Lost Odyssey Recomp is distributed under the GNU General Public License v3 (GPLv3).
- NVIDIA DLSS SDK headers and static bootstrap libraries are governed by NVIDIA's proprietary license (`LICENSE.txt` SHA-256 `3027f23c...`).
- While optional static bootstrap linking permits local development, public redistribution of builds with `LO_ENABLE_DLSS=ON` bundled with or linking proprietary NGX components raises unresolved licensing questions.
- Binary redistribution remains unresolved, and no binary packages are released.

### Known Limits and Boundary Conditions
- **No In-Game DLSS Dispatch**: Actual in-game Super Resolution evaluation is bypassed due to unqualified runtime color encoding. DLSS cannot be claimed as usable in gameplay.
- **P2 Work In Progress**: Development resumed with committed changes on 2026-09-21. Gate 3 is not approved; no completed formal re-review is claimed.
- **Linux Evidence Scope**: CPU tests, renderer translation-unit compilation, SDK ON/OFF builds/report tests and software Vulkan composites ran on Linux. NVIDIA NGX execution and Linux gameplay remain untested for these revisions.
- **Self-Test Scope**: The promotion resample test covers isolated resample math; it does not prove end-to-end target promotion, guest alpha preservation, UI ordering, or mid-frame flush safety.
- **Renderer Recognition Scope**: The focused GPU fixture verifies single motion finalization before consumers, but does not exercise separate HDR and SDR scene-recognition branches.
- **Allocation Geometry**: The follow-up adds nonzero subregion origins and integer-overflow CPU cases, plus FP16 composites with an 8×8 target and smaller SR scratch. These do not establish padded NGX/game allocations or the complete renderer mapping path.
- **Temporal & Config Discontinuities**: The test suite does not simulate temporal frame-time discontinuities (such as 250 ms stalls). Persisted configuration handling remains unexecuted in focused fixtures.
- **Jitter Proof**: The GPU fixture verifies unjittered motion vector inputs with a synchronized raster jitter sample, but does not provide end-to-end full renderer jitter coverage.

## 6. Next Steps

- Complete Gate 3 code review and address review findings.
- Qualify runtime tonemapping color encoding and capture producer-chain constants via `p2-oracle.jsonl`.
- Validate guest alpha preservation, mid-frame flush handling, and UI pass ordering.
- P4 Frame Generation remains deferred until Super Resolution (P1–P3) is completed and verified.
