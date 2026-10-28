# Native 90/120 FPS: Implementation and Local Validation

[简体中文](native-90-120fps.zh-CN.md)

> **Topic reference.** Implementation and measurements below apply only to the versions and scope stated for each. For current delivery and acceptance status, see [project status](../STATUS.md) and the [roadmap](../ROADMAP.md).

This branch keeps the existing 30→60 FPS path, adds 90 FPS, and connects the old hidden 120 FPS switch to the normal settings. It changes the rate at which the game submits real frames. It is independent of the generated-frame multiplier of DLSS/FSR Frame Generation. The default remains 30 FPS, and the high frame-rate options keep an experimental label.

## Timing and presentation

The table below describes the original path with VRR Off. For the new independent VRR switch, the display refresh-rate frame cap, and the FG budget, see [FreeSync / G-SYNC Compatible integration](vrr-freesync-gsync-compatible.md).

| Native target | Interval of the recognized game Present call | Host target period | Host VSync policy |
|---|---|---|---|
| 30 FPS | Keep 2 | 33.333 ms | Keep the swap chain's original policy |
| 60 FPS | 2→0 (immediate) | 16.667 ms | Keep the swap chain's original policy |
| 90 FPS | 2→0 (immediate) | 11.111 ms | Request off; the host deadline caps the rate |
| 120 FPS | 2→0 (immediate) | 8.333 ms | Request off; the host deadline caps the rate |

From 2026-10-10, the 60 FPS tier also uses immediate. With the original interval 1, any frame longer than 16.667 ms waited for the next virtual VBlank, and the frame rate dropped straight to 30 FPS (Steam Deck players saw both the frame rate and GPU usage fall to half). Now the host deadline caps the rate, so a slow frame loses only the time it exceeds. The host VSync policy does not change, so this does not cause tearing. The 30 FPS tier keeps the original interval 2.

On the same day, Graphics → VSync was added (Off / On / FreeSync / G-SYNC Compatible, `vsync=0/1` plus the existing `variable_refresh_rate`). The host VSync column in the table above now follows this setting. When it is on, every native tier keeps the swap chain's original sync policy, including 90 and 120. When it is off, every tier uses immediate, and the guest interval for 30 FPS also becomes immediate. Old configurations without a `vsync` key migrate with their original behavior: `frame_rate` at or below 60 means on, and 90/120 means off. On macOS, Metal always syncs, and the menu offers only the ProMotion switch.

Interval changes are strictly limited to the path from caller `0x827B4A4C` with an original interval of 2. Other callers, other intervals, and flag bits are unchanged, and the original function still forwards only once. Game time deltas, the PPC timebase, kernel timing, and the audio clock are not scaled. The virtual VBlank stays at 60 Hz. The original deadline algorithm is unchanged. A slow frame does not wait an extra full period, and after a long pause, expired frames are not sent in a burst.

A new `gpu/frame_rate.h` serves as the shared definition for configuration validation, menu ordering, and the host/guest presentation policy. `settings.ini` stores the actual frame-rate value, not a menu index. An old `frame_rate=120` needs no migration, and invalid values still fall back to 30.

The host VSync setting is applied at the shared `PreparePresentation` entry point, before acquire. It covers real game frames and the host overlay after a pause. A Vulkan mode change reuses the existing swap-chain rebuild flow: first FG resource handover, then waiting for in-flight submissions, then rebuilding the images. Going back to 30/60 restores the swap chain's original VSync policy. Vulkan DLSS-G's own requirement for immediate remains in force while it applies. After the swap chain is replaced, the baseline is read again.

If the platform cannot provide immediate, the code logs a warning and accepts the platform fallback. This avoids rebuilding every frame by repeatedly requesting a mode that keeps failing. Turning VSync off may cause tearing. The actual display result also depends on the monitor, the window system, the driver, and the VRR settings. This change does not guarantee that the target rate is reached steadily on every machine or in every scene, and it does not count FG-generated frames as new native game frames.

## Usage

Choose 90 FPS or 120 FPS under Graphics → Frame rate, then save. It uses the existing settings apply flow, so no restart is needed for a frame-rate change. `LO_EXPERIMENTAL_120` is no longer checked. If it remains in the environment, it has no effect on the new setting.

For native high-refresh validation, first set Frame generation to Off. Super resolution and anti-aliasing may keep their settings. Check that the launch script has no `LO_FPS` override. It still takes priority over the setting. `0` means diagnostic uncapped, and it also releases the guest interval at recognized call sites. `LO_GUEST_INTERVAL=0` is still an A/B diagnostic switch. It turns off guest interval conversion and should not be used in normal high-refresh validation.

## Checks performed and evidence limits

`tools/tests/frame_pacer_test.cpp` was compiled and run with GCC C++20 and `-Wall -Wextra -Werror`. It covers the four-tier configuration and menu mapping, every pair of tier switches, overload, recovery after a long pause, switching to uncapped, the host VSync baseline and FG policy, and call-site and flag preservation. The test depends only on production headers. It does not start the game or the GPU.

The menu render samples and the menu fixture's header dependencies were updated. The existing Windows configuration fixture gained SaveConfig→INI read-back assertions for the four tiers. These Windows configuration and menu fixtures were not run in this environment. Full-game compilation, real Windows/Vulkan/D3D12 presentation on hardware, and gameplay speed validation are not yet complete. Source code and pure CPU tests are not treated as proof of stable 90/120 FPS.

A reproducible lightweight check:

```sh
g++ -std=c++20 -Wall -Wextra -Werror -ILostOdysseyRecomp \
    tools/tests/frame_pacer_test.cpp -o /tmp/lo-frame-pacer-test
/tmp/lo-frame-pacer-test
```

An existing build environment can also use the `LoFramePacerTest` target. On Windows, `LoSaveAnywhereConfigTest` must run in an isolated empty working directory so it does not touch real settings.

## Local game validation

Use the same save and scene that run normally at 60 FPS, keep the picture quality and resolution unchanged, and step through 60→90→120→60→30 in turn. After each step is saved, confirm that the option is kept after reopening the menu and after restarting the program. Also cover saving while paused, restoring after window resize or minimize, and stepping down from 120. D3D12 and Vulkan should be verified separately.

Compare walking distance, animation and effect durations, battle and Aim Ring timing windows, audio, and cutscene sync over the same real time. Cutscene videos should not be required to produce new content beyond their source frame rate. Complete the native check with FG Off first, then check the FG combination separately.

Setting `LO_FRAME_TIMING=1` at launch reads the existing one-second window, with new `source=guest_swap`, `engine_rate` and `game_time_ratio` fields. `rate` is the game swap processing frequency in that window. `engine_rate` is the engine tick frequency. `game_time_ratio=delta_sum/window` should be close to 1 in a stable, unpaused normal game. Short windows around switches, loading, or pauses cannot alone be used to judge speed multiples. When `LO_RENDER_TIMING` is enabled, it keeps the original per-frame records and suppresses this summary.

The logs help separate the native processing frequency from the total displayed FG frame rate. However, swap and tick counts alone cannot prove that each image is unique, and they cannot replace gameplay comparison. If the native rate stays near 60, the game runs fast or slow, Ring windows are abnormal, or sync errors appear, keep that scene's log and continue locating the cause. Do not slow the global clock to compensate.

## Known high-frame-rate issues in game logic

The game was written for 30 FPS. Some logic counts in frames or assumes a minimum frame time, and needs separate fixes at native 90/120:

- The Hungry Man errand timer counts in frames. `patches/hungry_man_timer.cpp` updates the count at a 30 FPS equivalent.
- The battle camera rotation smoother resamples its history with a 12.5 ms step, and needs at least 12.5 ms per frame. `patches/battle_camera_smoother.cpp` updates it only when a full step has accumulated (#117, [details](../archive/notes/battle-camera-120fps-2026-10-01.md)).
- The battle dialogue wait used to truncate the 60 Hz step of each actual script update and discard the fraction. At native 90/120 FPS, the wait could make no progress. `patches/battle_script_timer.cpp` now keeps the fractional tick between updates, then supplies the accumulated integer step before the wait consumer. The original PPC fixture and the negative control with the fix disabled both pass. The full runtime build, real scene runs, and player acceptance are still pending (#148, [details](../archive/notes/issue-148-battle-dialogue-timing.md)).

When investigating similar problems, first record the state of the relevant objects per frame. Then use a hardware write breakpoint to find the writer, and look in that function for fixed-step or frame-count assumptions.
