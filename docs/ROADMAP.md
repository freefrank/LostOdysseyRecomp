# Roadmap

[简体中文](ROADMAP.zh-CN.md) · [Development status](STATUS.md) · [Changelog](../CHANGELOG.md) · [Maintainer Project](https://github.com/users/freefrank/projects/3)

Reviewed on 2026-09-26 against live Issues, Project fields, merged commits and release records. `[x]` means the stated scope is delivered; `[~]` means a concrete remainder is open; `[ ]` means planned work. Closing a tracker does not establish unrecorded gameplay or hardware validation.

## Current delivery

- [x] **v0.7.0 published:** source `4142f23`, Release CI [36228746088](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/36228746088). Windows and Linux packages are available on the [release page](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.7.0).
- [x] **v0.7.1 source committed:** `b1cf166` adds Gameplay → Import discs & DLC, safe restart into the importer, selective replacement and failure rollback. Synthetic importer/menu/controller/host tests and the Windows development build passed. Both user-provided Asia GOD and USA/Europe ISO sources were recognized read-only; no real re-import was performed. Tagging and release remain pending.

## Completed features and reconciled trackers

- [x] **DLSS/DLAA and FSR SR:** the v0.7.0 scope passed user acceptance within recorded Windows/native Linux coverage. FSR P1/P2 are complete; frame generation remains below.
- [x] **PlayStation prompts:** host and guest face/shoulder/Start/Back glyphs accepted and published in v0.7.0.
- [x] **Mod API v1 and Wiki:** `457ba24` / PR #68 delivers manifest resolution, disable/fallback, reload, LOTEX1/PNG tooling, native-menu atlas/font replacements and mod guides. Windows/Linux Mod API and Wiki CI passed. Arbitrary guest texture/model replacement and real MO2 acceptance are not included in this completed scope.
- [x] **IME, idle cursor and controller improvements:** shipped in v0.6.19; the old IME “Not started” item is closed. Broader device combinations remain regression coverage.
- [x] **PortForge manifest:** Windows/Linux manifest in `9abda30`; Issue #37 is closed. Local tracking now respects the Project's Done status.
- [x] **Issue triage automation:** deployed Issue-opened and `@codex` replies, including a verified [real public response](https://github.com/freefrank/LostOdysseyRecomp/issues/21#issuecomment-5669773986). Future bug repairs and answer-quality improvements are separate work.
- [x] **Audio trackers #54/#55:** both Issues and Project items are closed. This records maintainer closure, not proof that PR #66's diagnostic changes fixed every language or missing-line report. Prior feedback remains in the item evidence.
- [x] **Earlier shipped work:** live AF, Quit to Desktop/Title, Save Anywhere preference persistence, cheat navigation, ultrawide controls, portable Vulkan shaders, Linux AppImage, online PPC compilation, and bounded rendering/runtime repairs. Historical measurements and release details remain in [STATUS](STATUS.md) and [CHANGELOG](../CHANGELOG.md), rather than the active queue.

## Active work

- [~] **Open reports:** #49 physical-pixel window coordinates, #64 DLSS/FSR behavior and #67 Grand Staff sky flicker remain open. Existing sizing recovery and diagnostics do not close these reports.
- [~] **Issue #40 remaining mod scope:** PlayStation prompts, the v1 framework and Wiki are delivered; broader guest texture/model consumers and real external-manager integration remain. The Issue is still open.
- [ ] **Feature requests:** DoF controls (#30) and motion-sickness options (#48).
- [~] **Native motion and temporal color:** geometric/rigid/skinned replay infrastructure and qualified SDR inputs are implemented, with bounded battle and Hybrid SR evidence. Remaining work covers unmapped draws, broader skeletal/scene coverage, HDR/exposure and D3D12 replay PSO failure `0x80070057`.
- [~] **Linux/Steam Deck:** Linux x64 runtime, menus, importer, updater and AppImage are released; native AMD 8060S RADV evidence exists. Steam Deck hardware, Steam runtime/Flathub and broader playthrough coverage remain. APEX 15W evidence is not Deck hardware equivalence.
- [~] **Performance and shader startup:** bounded city/cache improvements and notified waits are already on main. Remaining stalls, two retained shader translation failures and broader 15W/full-game performance targets remain open; earlier unpublished-branch descriptions are historical.
- [ ] **Graphics follow-ups:** D3D12 portable shader pack, standalone experimental TAA removal, resource-only PSO coverage, and profile-led cache/motion optimizations. Spatial-AA fallback alone does not implement TAA-option removal or prove all flicker resolved.
- [ ] **Broader regression:** full playthrough, chapter/disc/save compatibility, audio/languages, controller/rumble, mixed-DPI/fullscreen and multi-GPU coverage. Individual completed fixes are not held open solely for these broader goals.

## v0.8.0 plans

- [~] **P0:** common temporal contracts are implemented; Streamline/NGX coexistence Gate 1 remains unpassed.
- [ ] **P3:** production frame-generation presentation infrastructure, leases and UI separation.
- [ ] **P4:** fixed 2× DLSS Frame Generation on Windows Vulkan, including DLSS/DLAA/FSR combinations and safe suspension/recovery.
- [ ] **FSR Frame Generation:** independent target; API, platform and multiplier are not predetermined.
- [ ] **Optional native 120 FPS:** independent game presentation, not generated frames; requires pacing, Ring, audio and cutscene validation, with 60 FPS as default.
- [ ] **Linux AArch64 and macOS AArch64 / Apple Silicon:** platform delivery targets; no official packages or hardware acceptance claimed.
- [ ] **Remove the PM4 translator:** moved from v0.9.0 to v0.8.0; replacement architecture remains undecided.
- [ ] **Experimental Android:** exploratory platform target, no APK or device validation yet.

## Later backlog

D3D12 DLSS FG and dynamic MFG remain deferred rather than mandatory v0.8.0 goals. DX11, HDR output, higher-resolution shadows, SSAO/depth access, GI/reflections, ray tracing and the paused Switch work remain independent proposals. WMV playback, temporary protagonist damage controls and other unproven items retain their current Project scope; they were not marked complete without evidence.

See the [Project](https://github.com/users/freefrank/projects/3) for individual evidence and the [historical roadmap](archive/ROADMAP-2026-09-10.md) for earlier detail. This reconciliation did not rerun builds, games or tests.

<!-- Historical link compatibility. -->
<a id="v070-frame-generation"></a>
<a id="v070-upscaling"></a>
<a id="v080-frame-generation-macos"></a>
<a id="v090-pm4-translator"></a>
<a id="v050-pc-graphics"></a>
<a id="next-major-milestone-v050--pc-vulkan-and-direct3d-11"></a>
<a id="near-term-priorities"></a>
<a id="current-feedback-and-regression-work"></a>
<a id="published-milestone-v042--repairs-and-validation"></a>
<a id="phase-1-produce-compilable-output"></a>
<a id="phase-2-reach-the-main-menu"></a>
<a id="phase-3-work-toward-a-complete-playthrough"></a>
<a id="phase-4-modernization"></a>
<a id="phase-5-optional-exploration"></a>
