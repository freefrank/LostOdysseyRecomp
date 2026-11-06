# FreeSync / G-SYNC Compatible: Application-Side VRR Integration

[简体中文](vrr-freesync-gsync-compatible.zh-CN.md)

> **Topic reference.** Implementation and measurements below apply only to the versions and scope stated for each. For current delivery and acceptance status, see [project status](../STATUS.md) and the [roadmap](../ROADMAP.md).

Date: 2026-09-28. Development branch: `feature/native-90-120fps`.

## Usage and limits

Graphics has a new `FreeSync / G-SYNC Compatible` switch. It is Off by default. Choose On, then save. It takes effect while the game is running, with no restart needed. The configuration key is `variable_refresh_rate=0/1`. Old configurations without this key keep their original behavior. The option controls the game's presentation and frame-limiting policy. It does not replace turning on VRR in the monitor or driver, and it does not claim to detect that the monitor is changing its refresh rate.

First enable the matching adaptive sync feature in the monitor's menu, and enable FreeSync or G-SYNC Compatible in the graphics driver. For NVIDIA windowed use, the control panel's "Windows and full screen" scope must also match. For the first check, use borderless full screen with FG Off. On a non-VRR screen or an unsupported window system, turning this option on may cause tearing. Turning the option off restores this branch's original sync policy. Native modes above 60 still use the original immediate policy.

## Implementation

`gpu/vrr_policy.h` provides one definition for the host VSync request, native frame limiting, and FG output budget. Normal game swaps get their effective host cap through `video::GetFramePacingTarget`. The saved 30/60/90/120 targets are unchanged. `PreparePresentation` applies the asynchronous presentation request before acquire. It continues to use the existing swap-chain rebuild, in-flight submission wait, and FG resource handover flow. While paused, the host overlay uses a frame cap that does not include the FG multiplier.

At the recognized game Present call site `0x827B4A4C`, interval=2 changes to immediate when VRR is On. This includes the 30 and 60 tiers, so that caps such as 57 fps are not quantized by 60 Hz integer VBlanks. Other callers, other intervals, and flag bits stay unchanged. The PPC timebase, kernel and audio clocks, the engine delta, and the 60 Hz virtual VBlank are not scaled.

The refresh rate is read by the window thread and handed to the presentation thread through an atomic snapshot. On Windows it comes from the current display mode of the monitor the window is on. On other platforms it comes from the SDL2 current-display-mode query. It refreshes every 500 ms, and a re-query is requested after moving the window, after a display or size change, and after regaining focus. The render thread never accesses the SDL window. When no valid refresh rate is available, the code records 0, keeps the existing target, and does not guess 60 Hz. This is the refresh rate of the current mode. **It is not detection of the monitor's VRR range or of its actual scan rate.**

2026-10-07 update ([PR #289](https://github.com/freefrank/LostOdysseyRecomp/pull/289) is merged to main and not yet released; the latest release is v0.8.53): the runtime has moved from SDL2 to SDL 3.4.18. The paragraph above that says "queried through the SDL2 current display mode" describes v0.8.53 and earlier SDL2 versions. The current source calls `SDL_GetCurrentDisplayMode` on non-Windows platforms. SDL3 reports fractional refresh rates (for example, a 120 Hz display reports 119.99). `PollDisplayRefresh` in `gpu/video.cpp` rounds this to an integer Hz before passing it to `vrr::OutputLimit`, so the "current mode refresh rate minus 3" budget rule is unchanged. Windows still reads the current display mode of the monitor the window is on. The validation record for the migration has no VRR or frame-limiting measurements, so the measurement scope of this document is unchanged.

D3D12 reuses Plume's existing flip-discard, capability queries, and `ALLOW_TEARING` at creation and ResizeBuffers. This change adds a check of the actual full-screen state at Present time. `DXGI_PRESENT_ALLOW_TEARING` is passed only for windowed or borderless windows where tearing is allowed and SyncInterval=0. Exclusive full screen does not pass this flag. Vulkan uses Plume's existing asynchronous mode selection. When immediate is not supported, the existing backend fallback is accepted and a warning is logged. The swap chain is not rebuilt in a loop. The window system or driver may still limit the presentation rhythm.

2026-10-10 update: on Windows, Linux, and Android, this switch was merged into Graphics → VSync as a third option (Off / On / FreeSync / G-SYNC Compatible). The configuration is still `variable_refresh_rate=1`, and it takes priority over the new `vsync=0/1`. The VRR option's own behavior is unchanged. macOS still has a separate ProMotion switch. See [Native 90/120 FPS](native-90-120fps.md) for details.

## Frame limiting and FG

This project uses an output budget of the current mode refresh rate minus 3. This leaves margin for integer refresh rate reports, fractional refresh rates, and scheduling error. It is an engineering policy, not an AMD or NVIDIA rule, and it is not a guarantee that the limit is never exceeded.

| Configuration | Effective host cap |
|---|---:|
| 90 fps, 90 Hz, FG Off | 87 fps |
| 120 fps, 120 Hz, FG Off | 117 fps |
| 120 fps, 144 Hz, FG Off | 120 fps |
| 120 fps, 144 Hz, fixed 2× FG | 70 fps, nominal total output 140 fps |
| 120 fps, 144 Hz, fixed 4× FG | 35 fps, nominal total output 140 fps |

Fixed FG divides the budget by a multiplier that includes the original frame. D3D12 uses the configuration that has been applied and is supported at runtime. Vulkan keeps the current fixed 2× session. When FG is ready on D3D12 but the scene temporarily cannot interpolate, the multiplier budget is still reserved conservatively. In such scenes the native frame rate may therefore be lower. Vulkan chooses the multiplier from the previous frame's session availability. During a mode or scene transition, the previous budget may be used briefly. This is not a per-frame guarantee of a physical display limit.

Dynamic DLSS MFG limits the effective SDK target to the output budget. A value of 0 (automatic target) becomes that budget. A lower explicit target is kept. The native cap is also not higher than this effective dynamic target, but it is not mechanically divided by the maximum MFG multiplier. Switching away from full screen, changing the refresh rate, and turning VRR On/Off update the effective request through the existing FG reconfigure. VRR adjusts the host frame cap and the effective output target for dynamic MFG. A fixed FG multiplier still uses the applied configuration. Saving VRR alone does not overwrite the FG provider, mode, multiplier, or target on disk. When the FG provider or multiplier is actually switched, the existing menu logic resets the mode and target. If an SDK request fails, the existing failure and fallback path applies, and the code does not pretend support.

DLSS FG and FSR FG cannot be swapped within one process that keeps the current Streamline/NGX session. Saving blocks this switch and clearly prompts "Switching DLSS FG to FSR FG needs a restart; restart now / later." Choosing later keeps the FSR configuration, leaves the current FG Off, and the status line also says a restart is required. This switch was observed being blocked in a foreground run. `LoMenuFlowTest` and `LoRestartTest` verified the prompt and restart status path. The game was not restarted in this validation, so no runtime result after starting with FSR FG is claimed.

A normal exit also once hit an access violation in the execute path of `NGX D3D12 ReleaseFeature` after FG Off/On runs. The current two-stage fix runs after the renderer/GPU drain. `D3D12Bridge::ReleaseFeatureAfterGpuDrain` calls a checked, repeatable FG feature release (`slFreeResources`) while both the FG and SR sessions are still alive. Then it runs SR's ReleaseFeature, parameters, and Shutdown1. Last, `g_d3dFg.reset()` runs the Streamline shutdown and unload, while the device and queue are still valid. This keeps the NGX session needed to release FG resources. An earlier coexistence probe showed that closing the NGX session first made `slFreeResources` return `FeatureNotFound`. A full Windows Clang game build passed again after the two-stage fix. The runtime exit has not been validated again, so this is not treated as accepted. The unload fallback paths for DLSS FG initialization failing partway through, and for queue or swap chain creation failing after a new DLSS bridge is created, still need separate handling.

`LO_FPS=0` still means diagnostic uncapped, and it bypasses the automatic VRR cap. For normal validation, clear `LO_FPS`, `LO_GUEST_INTERVAL=0`, and any unnecessary FG environment overrides. When the game's frame rate is too low, the code does not compensate by changing clocks. Low-frame-rate compensation on the monitor is handled by the external display system.

## Logging and validation

The `VRR pacing` log records `requested`, `refresh_hz`, `native_target`, `host_cap`, `fg_multiplier`, and `dynamic`. It reports a change at most once per second. `hardware_vrr=unverified` always states clearly that real VRR is not detected. `native presentation` also records the requested and reported VSync state. The target in `frame timing` is the effective host frame cap. It must not be read as the monitor's actual scan rate, or as the FPS after frame interpolation.

Passed locally in this round: three independent CPU tests (VRR policy, real configuration parsing and writing with preview isolation, and FG request selection). The menu interaction fixture's 396 checks passed. These include a check that switching VRR On and Off alone and saving keeps the provider, mode, multiplier, and target of Dynamic MFG. The menu raster fixture's 21 checks passed. The native frame-pacer test passed. The menu and configuration fixtures isolate the platform and game services. They do not mean a real game was run. The Plume patch passed the apply check against the pinned upstream source.

```sh
cmake -S tools/tests/vrr -B out/vrr-tests
cmake --build out/vrr-tests --config Release
ctest --test-dir out/vrr-tests -C Release --output-on-failure
```

The full `LostOdysseyRecomp` target build with Windows Clang `RelWithDebInfo`, `LoFramePacerTest`, and `LoReusableFgCoreTest` passed. The isolated directory `out/pr80-runtime-smoke` ran `--prepare-shaders-only` on Vulkan and D3D12, using the existing game data and shader packs. Both exited normally, found the shader pack, and reported renderer ready. Each backend also had one background window start segment, using `LO_BACKGROUND=1`, VRR On, native target 120, FG Off, and 1280×720. The game thread ran for about 20 seconds, and the log had no warning or error. The background segment's log shows `refresh_hz=144`, `host_cap=120`, `requested/reported_vsync=false`, `guest_refresh=60`, and `clocks=unchanged`. The steady segment is about 60 presents/s with `game_time_ratio` about 1. This does not prove 120 FPS or a complete live-play experience. In the same scene, the user confirmed that RTSS output stayed steadily below 144. The source ran about 47 FPS and the output about 141/s. A foreground FG Off run lasted more than 20 seconds without a crash, and DLSS SR kept submitting. It then switched to Fixed 2× and also ran normally. But the later normal exit triggered the NGX access violation described above. The fixed exit path has not been run again. The user confirmed that the visual artifact disappeared with FG Off. With Fixed 2×, the ghosting and breakup of moving object edges looked about the same with VRR On and Off. This picture-quality issue is recorded as a separate FG picture-quality acceptance item. It cannot be attributed to VRR or recorded as VRR tearing passed. The user also confirmed the refresh-rate change and G-SYNC enablement through the monitor and driver indicators. The software log still reports `hardware_vrr=unverified`, which means the program does not detect the hardware state itself. These runs used an isolated copy of the install and saves, and the D drive install was not changed. Still to cover: 60/90/120 tiers, reopening the settings, dynamic MFG, moving across monitors, Alt+Enter, minimize/restore, and pause with settings. Also compare walking over the same real time, Aim Ring, and audio-video sync. This commit does not claim AMD or NVIDIA certification, or that all target FPS values are stably reached.

## Vendor interface references

- [Microsoft: Variable refresh rate displays](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/variable-refresh-rate-displays)
- [Microsoft: DXGI_PRESENT, exclusive full screen and ALLOW_TEARING limits](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-present)
- [SDL2: SDL_GetCurrentDisplayMode](https://wiki.libsdl.org/SDL2/SDL_GetCurrentDisplayMode)
- [Khronos: VkPresentModeKHR](https://docs.vulkan.org/refpages/latest/refpages/source/VkPresentModeKHR.html)
- [AMD: Enable AMD FreeSync](https://www.amd.com/en/resources/support-articles/faqs/DH3-013.html)
- [NVIDIA: Set up G-SYNC](https://www.nvidia.com/content/Control-Panel-Help/vLatest/en-us/mergedProjects/nvdsp/To_use_variable_refresh_rates.htm)
