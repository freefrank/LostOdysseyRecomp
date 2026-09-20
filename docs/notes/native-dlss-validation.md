# Native DLSS Validation (P0 Foundation)

Date: 2026-09-20

Baseline: `main@5b765f617ec511a2f76aa9da7923a8c122679d2c`

Feature branch: `dlss`

SDK Reference: NVIDIA DLSS **310.9.1** (`374959484e79a640feaba44c93ac8cfb0a03f5b5`)
Status: **P0 gate 1 passed** (Attempt 3). Local foundation only. Frame Generation (FG) deferred.

## 1. Overview and Scope

This document records the verification evidence for the P0 phase of native Vulkan NVIDIA DLSS integration on the `dlss` development branch.

P0 scope is strictly limited to:
- Pinned official NGX SDK bootstrap dependency and Vulkan extension negotiation.
- Plume Vulkan bridge hooks (`VulkanExtensionHooks`, `VulkanExtensionStatus`, and external command boundaries).
- Capability discovery, query parameter lifecycle, and optimal resolution settings via `LoNativeDlssProbe` and `LoNativeDlssReportTest`.
- Fallback policies: implemented runtime fallback policies (reverting to baseline Vulkan without DLSS when the SDK is disabled at build time, when hardware is unsupported, or when the staged runtime is absent) and verified negative standalone probe behavior.

P0 does **not** implement Super Resolution evaluation (`NGX_VULKAN_CREATE_DLSS_EXT1` / `NGX_VULKAN_EVALUATE_DLSS_EXT`), temporal motion vector feeding, HUD separation, or Frame Generation. No game launch or visual/framerate acceptance is claimed.

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

## 5. Architectural and Licensing Boundaries

### Search Path vs. Loaded Runtime Version
The probe and runtime pass the directory containing the staged `nvngx_dlss.dll` via `NVSDK_NGX_FeatureCommonInfo.PathListInfo`. This specifies a search directory for NGX feature discovery. It does not provide absolute cryptographic proof of which specific runtime DLL binary image was mapped into the process by the NVIDIA driver or NGX Core; the driver or system NGX cache may resolve dependencies according to driver-defined precedence.

### Redistribution and Licensing Notice
- Lost Odyssey Recomp is distributed under the GNU General Public License v3 (GPLv3).
- NVIDIA DLSS SDK headers and static bootstrap libraries are governed by NVIDIA's proprietary license (`LICENSE.txt` SHA-256 `3027f23c...`).
- While optional static bootstrap linking permits local development, public redistribution of builds with `LO_ENABLE_DLSS=ON` bundled with or linking proprietary NGX components raises unresolved licensing questions.
- Binary redistribution remains unresolved, and no binary packages are released.

## 6. Next Steps

- **P1**: Frame plan integration, shared temporal input contracts (`TemporalFrameInputs`), separate internal render resolution scaling below 720p, and jitter coordinate contracts.
- **P4**: Frame Generation remains deferred until Super Resolution (P1–P3) is completed and verified.
