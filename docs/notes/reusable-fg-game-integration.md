# Reusable FG game integration

Development branch: `feature/reusable-fg-fsr-d3d12-mfg`.

## Code scope

Windows D3D12 game presentation selects one reusable provider: standalone FSR FG, DLSS FG, or capability-gated DLSS dynamic MFG. Native device/queue rendering is retained; creation hooks install the selected SDK's swapchain. The existing Windows Vulkan fixed-2x DLSS FG path remains separate and its FSR SR + DLSS FG maintainer acceptance is preserved.

`video.cpp` now forwards submission/present, unsubmitted cancellation, resize/window quiesce and owner-thread shutdown to the selected D3D12 bridge. `renderer.cpp` exports same-frame depth/motion snapshots for SR and native-resolution FG, retains submission ownership, and validates the actual final resolve identity before presentation. Input and SDK completion remain separate from generation/display telemetry. Unknown GPU completion remains a fatal safe-stop rather than an early resource release.

SR Off + FG On captures the depth/motion required by interpolation without invoking either SR SDK, selecting a fake SR consumer, enabling jitter, or changing the guest frame rate. SR Off + FG Off does not enable this path. Final composited color is used; HUD/UI separation is still outside this scope.

## Build

Use the existing full-game build process, additionally enabling one or both options:

- `LO_ENABLE_D3D12_DLSS_FG=ON` and `LO_STREAMLINE_SDK_ROOT` pointing at the official Streamline 2.14.1 SDK.
- `LO_ENABLE_FSR_FG=ON`, `LO_FSR_SDK_ROOT` pointing at FidelityFX 1.1.4, and `LO_FSR_FG_RUNTIME` pointing at its official `amd_fidelityfx_dx12.dll`.

The new FSR FG adapter does not require native DLSS SDK/SR or FSR SR to be enabled. Build options remain off by default. The generic adapter target is C++20, including in a C++23 parent project, to avoid the pinned Streamline header's C++23 alias issue.

## Run

Select D3D12 in the game's graphics settings and launch from PowerShell with one of these settings:

```powershell
# Independent AMD FSR frame generation, fixed 2x.
$env:LO_FG_PROVIDER='fsr'
$env:LO_FG_MODE='fixed'
$env:LO_FG_MULTIPLIER='2'
# Optional override for the packaged runtime DLL:
# $env:LO_FSR_FG_RUNTIME='C:\path\amd_fidelityfx_dx12.dll'
```

```powershell
# NVIDIA DLSS frame generation, fixed multiplier.
$env:LO_FG_PROVIDER='dlss'
$env:LO_FG_MODE='fixed'
$env:LO_FG_MULTIPLIER='2'
```

```powershell
# NVIDIA SDK-native dynamic MFG; actual support is queried at runtime.
$env:LO_FG_PROVIDER='dlss'
$env:LO_FG_MODE='dynamic'
$env:LO_FG_TARGET_FPS='144' # 0 requests SDK display-refresh detection.
```

`LO_FG_PROVIDER=off` overrides legacy `LO_DLSS_FG=1`. Unsupported provider/mode combinations are reported, never silently replaced with another algorithm. Dynamic MFG is not claimed for FSR or Vulkan by this integration.

## In-game settings and session behavior

Graphics settings contain an FG section with `Off`, `DLSS`, and `FSR` on D3D12 builds. DLSS exposes 2×–6× multiplier requests (6× is the most DLSS multi-frame generation supports; `settings.ini` values above 6 fall back to 2× and `LO_FG_MULTIPLIER` accepts 2 to 6); FSR is fixed at 2× and hides the multiplier row. Save graphics settings to apply the request. The focused FG rows report `Pending`, `Ready`, `Unavailable`, or `Off`; `Ready` means the session and swapchain exist, not proof of generated frames. Unsupported requests use ordinary rendering.

Changing the provider drains renderer, host presentation and SDK work, releases the old swapchain before unloading its SDK, replaces the native queue while retaining the Plume wrapper address, and recreates presentation. DLSS multiplier changes reconfigure the existing session. Changes apply within the current process; switching graphics backends still requires restart. Explicit diagnostic environment overrides retain priority over saved settings.

## Validation boundary

The pre-integration reusable adapter CI passed on Windows and Linux. For this game wiring, local Linux renderer/video compilation, 19 game-input policy checks, 55 reusable core checks, and existing camera-constants math passed. The new workflow compiles the actual Windows renderer/video translation units and links the bridge with both adapters; its result must be read from the matching commit's CI, not inferred from earlier checks. This is not a full generated-guest executable build. The original checkpoint above predates the bounded D3D12 runtime checks recorded below; it does not claim image-quality, physical display-rate, pacing, resize or full-playthrough acceptance.

## Acceptance status (2026-09-27)

The Windows runtime has now linked with both D3D12 adapters while reusing the unchanged PPC precompiled libraries and the pinned official SDKs. The matching Windows CI run for `a75fd9e` (`36370253775`) stopped at the barriers failure gate; this is recorded as a CI failure, not as runtime acceptance evidence. The local repair calls the barrier operation through the base render-command interface, reuses the adapter's `sl_security` implementation when both Vulkan and D3D12 are enabled, configures the pinned FidelityFX 1.1.4 frame ID before `prepare`, and removes the Vulkan-only condition from the same-queue marker while retaining queue, device, and submission-serial checks. D3D12 bridge telemetry now records generated intervals and actual presents.

The initial bounded local checks were explicitly background, muted runs. `run02-dlss2-offsr` used DLSS2 with SR Off for 75 seconds and exited with code 0, `forced_stop=false`, and `baseline_preserved=true`; after the same-queue repair, `metadata`, `ordered`, and `matched` were true, with `active=true` and `accepted=true`. Its hidden resize returned from 2560×1440 to 2048×1152 and back to 2560×1440 with runtime recovery logs, but `generated_intervals=0`. `run03-fsr2-offsr` used FSR FG with SR Off for 75 seconds and also exited cleanly with the baseline preserved; its hidden resize reached both target sizes and restored runtime state, and it recorded 2,459 FSR generation dispatches. These are SDK dispatches, not display-frame measurements; the generic `actual_presents=0` field is unfilled for this FSR path.

`run04` used dynamic DLSS FG with target 144 and FSR SR Quality for 75 seconds. It exited cleanly with the baseline preserved, and its hidden resize reached 2048×1152 and restored 2560×1440 with runtime recovery. `run05-fsr2-dlsssr` used FSR FG with DLSS SR Quality for 65 seconds, exited with code 0, `forced_stop=false`, and `baseline_preserved=true`; the runtime reported `active=true` and `accepted=true` and recorded 1,682 FSR generation dispatches.

The first foreground muted Uhra check, `run06-front-dlss2-offsr`, exited with code 0, `forced_stop=false`, and `baseline_preserved=true`. Its final sample recorded `source=3720`, `generated_intervals=2703`, and `actual_presents=5407`; a desktop capture showed the expected Uhra scene with OSD 120. PresentMon started too late for this run and produced no usable CSV. `run07-front-dynamic-fsrsr` used dynamic DLSS FG target 144 with FSR Quality, exited cleanly with the baseline preserved, and recorded `source=3240`, `generated_intervals=2247`, and `actual_presents=6752`. A continuous interval showed 120 source frames to approximately 360 presents, about 3× the running source count. PresentMon captured 1,428 display events on one swapchain, all `Composed: Flip`, with mean 6.954472 ms, median 6.9444 ms, and p95 6.9691 ms, approximately 144 OS display events per second. These are OS display events rather than physical scanout measurements; desktop captures showed the Uhra scene with OSD 144.

The remaining foreground checks completed cleanly. `run08-front-fsr2-offsr` ran FSR FG with SR Off for 75 seconds, exited 0 with `forced_stop=false` and `baseline_preserved=true`, and recorded 2,965 FSR generation dispatches. PresentMon captured 1,192 `Composed: Flip` events on one swapchain with mean 8.34884077 ms, median 6.9485 ms, and p95 13.8963 ms, approximately 119.8 OS display events per second; the desktop capture showed Uhra with OSD 119. `run09-front-fgoff` ran SR Off with `LO_FG_PROVIDER=off` overriding `LO_DLSS_FG=1` for 65 seconds, exited 0 with the baseline preserved, and recorded zero snapshot or ordered-input samples because the provider was not initialized. PresentMon captured 596 events with mean 16.72094614 ms, median 13.9121 ms, and p95 27.7699 ms, approximately 59.8 OS display events per second.

All nine run summaries are retained in `out/acceptance/results.json`; all exited 0 without forced stop and preserved the baseline, and no `[error]` log entries were recorded. Each foreground DLSS run retained seven SDK warnings (repeated `SetOptions`, default backbuffer extent, long-frame timer reset, resource alignment override, and native swapchain refcount 1 on exit); these runs are not described as SDK or validation clean. The foreground checks cover only the local RTX 5080 with driver 616.56, the Uhra route and 65–75 second windows. Hidden resize evidence is limited to the three background runs. Long runs, additional scenes, frame-by-frame interpolation image quality, HUD/UI separation and physical scanout measurement remain open.

The nine historical runs above predate the in-game FG settings section and do not validate menu-driven save or same-session reconfiguration.

## Menu and hot-switch validation (2026-09-27 local / 2026-09-28 UTC)

The Windows game build with both D3D12 providers passed. Settings persistence and environment-selection tests passed; the menu flow test passed all 12 groups, and menu rendering passed against real game assets. English and Simplified Chinese previews cover the FG section inside Graphics, DLSS multiplier navigation, hidden FSR/Off multipliers, scrolling and pointer selection. These checks are separate from GPU generation evidence.

Muted isolated `out/fg-menu-runtime/run01` kept one process for Off → DLSS 2× → DLSS 3× → FSR 2× → Off. Saving applied each request; changing only the DLSS multiplier retained the swapchain. During a brief foreground interval at DLSS 3×, telemetry reached 88 generated intervals; the FSR phase recorded 347 generation dispatches. The earlier DLSS 2× phase had no recorded generated intervals. Other applications frequently owned the foreground, so background activity is not used as proof of DLSS generation. This run exited 0 without forced stop and preserved its baseline.

`run03` used the final Graphics-section executable and no FG environment override. In one process, menu saves applied Off → FSR 2× → DLSS 2× → Off → DLSS 2×. After the second DLSS initialization, the game returned to the Uhra scene and a confirmed foreground interval recorded 1,908 generated intervals and 5,457 cumulative SDK presents. This verifies generation resumes after disabling and re-enabling DLSS without restarting. The process exited 0 through its owned window-close path, without forced stop, and preserved the baseline. `run02` exited cleanly but did not successfully navigate the menu, so it adds no hot-switch evidence.

These runs retained their configuration, logs, captures, process identity and termination summaries under `out/fg-menu-runtime/`. No `[error]` entries were found in their runtime logs; SDK warnings remain, so they are not described as validation clean. Generation counters and dispatches are SDK observations, not physical scanout or interpolation-quality measurements. Failure injection, unsupported-hardware behavior, additional scenes and long-duration switching remain unverified. This feature is implemented and locally checked; user acceptance and release publication are separate.

This development acceptance record covers the local Uhra route and selected bounded provider combinations. It does not establish complete playthrough, HUD/UI separation, broad scene coverage, or cross-title provider parity. The implementation was published in v0.7.9; publication does not expand these validation limits.
