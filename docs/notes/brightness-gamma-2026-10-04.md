# Brightness / Gamma page — 2026-10-04

The retail "Brightness calibration" screen is only a reference pattern for adjusting the monitor: it has no slider and never rewrites the display gamma ramp. Graphics > **Brightness / Gamma** now opens a host page with two settings and a before/after preview; the retail screen is a button on it and returns to the page with the unsaved values.

## Settings

- `display_brightness` (-20..20, default 0) moves the black point: the curve is `clamp((x - k) / (1 - k), 0, 1)` with `k = -brightness / 200`, so +20 lifts black to about 23/255 and -20 crushes the bottom 10 % while white stays at 1.
- `display_gamma` (50..150 hundredths, default 100) then applies `x^(100 / gamma)`; higher is brighter midtones.
- Both apply live after saving. The settings menu previews its unsaved values; gameplay uses the saved ones (`settings::GetBrightnessCalibration`).

## Presentation

- The curve lives in the `.w` channel of the display gamma ramp's 256-entry constant buffer (`.rgb` is the guest ramp), so no layout, descriptor or push-constant change. The three buffers rotate when the guest ramp or the curve changes; only draws that set `displayAdjust`, `brightnessPreview` or `hdrCalibration` load new curve values.
- Scanout order in `finishFrame`: guest ramp, Expanded RGB range, player curve, HDR output conversion. Putting the curve after the expansion keeps black at 0 and white at 1 with or without Expanded RGB range.
- The preview (`outputFlags` 512) is not tied to linear output: the left half is the game image (ramp and expansion), the right half adds the curve, and both go through the same `outputSignal` conversion as gameplay. Without a frozen scene it shows a grey ramp, ten steps and eight near-black patches. The HDR calibration scene also gets the curve (4096).

## Frozen scene

- The game resolves every frame to one frontbuffer (0x714000 in Uhra), so its own menu overwrites the scene before the host menu opens; the earlier HDR preview copied at host-menu entry and therefore only worked for overlays opened over gameplay.
- The renderer counts depth-tested, depth-writing draws per frame and records the count on each resolve (`renderer::ResolvedScene`). Gameplay draws about 1,180; the game's menus and their blurred backdrop draw 5. The tone-map pass is not a usable signal: it also runs for menu frames.
- Gameplay frames copy the resolve into two textures in turn every 250 ms. The format is fixed when they are allocated (the FP16 extended-gamma twin if the first frame has one, otherwise RGBA8); later frames without that twin are skipped instead of reallocating. The first frame without the scene, or a host menu, freezes the older copy: the frame where the game's menu starts its blur still draws the 3D world. The next gameplay frame unfreezes and starts fresh copies.
- The two copies stay allocated for the session: at a 4K FP16 render resolution about 130 MB, at 1080p RGBA8 about 16 MB. The previous design held no memory during gameplay but never had a scene when opened from the game's menu.

## Validation

- Windows RTX 5080, Uhra save, 1600x900, 120 FPS, opened from Y > System > Settings: the page shows the last field frame on D3D12 in SDR and HDR and on Vulkan in HDR (`extended=true`); the HDR peak page shows it too, and LB / RB switch scene and pattern on both pages. With Expanded RGB range on and gamma 1.30 saved, D3D12 SDR gameplay luminance quantiles moved 1 % 0→0, median 92→116 (predicted 116.4), 99.9 % 251→252.
- LoMenuFlowTest (page open, edit, retail hand-off and return, Back over A), LoMenuRenderTest and LoPresentationTest (14/14 on D3D12 and Vulkan) pass.
- Not checked: Metal, Android.

## Current clarification — 2026-10-15

The calibration scene is now captured once when the game's native menu opens, before its transition and blur draw, then published after the GPU fence completes. That single scene remains available through the host Settings and calibration pages and is released after the native menu closes. Normal gameplay no longer allocates or refreshes periodic scene copies; opening the native game menu still incurs one capture and synchronization even when Settings is not opened.

The earlier validation above is preserved as historical evidence for the original implementation.

The final synthetic lifecycle run also verified HDR FP16 gain greater than 1, including high-depth blur and low-depth menu frames, both calibration pages, Settings reopen, and release after menu close. The `/Zs` video and renderer checks and `LoMenuFlowTest` passed; `LoBackendLifecycleTest` passed with the warm cache on D3D12 and Vulkan. The real game path in camp `u52_0` with HDR display output remains unverified.
