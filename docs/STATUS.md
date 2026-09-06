# Project status

Reviewed **2026-09-06** against the v0.1 release, recorded tests and subsequent user feedback. This page describes current results; dated investigation notes retain the history of individual experiments.

## Published release

[v0.1](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.1) was built from `2a3ffcc` by [hosted Windows CI](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34010819664). The downloaded package passed payload hashes, importer self-test and a 40-second isolated cold launch. The release ZIP is byte-identical to the reviewed CI artifact, with a versioned filename.

Recent dialogue, shadow, window-responsiveness, settings and startup-preparation changes are included in this release. Older notes describing them as local or uncommitted refer to the time of those experiments, not their current publication status. Source builds apply the tracked [dependency patches](../tools/patches/README.md).

**Validation remains limited to opening areas and selected scenes. There has been no complete playthrough or exhaustive four-disc compatibility test.**

[Release notes](RELEASE-v0.1.md) · [Installation](INSTALLING.md) · [Build and packaging evidence](notes/release-packaging.md)

## Implemented and validated features

| Area | Current result | Validation boundary |
|---|---|---|
| Importer | Portable GUI accepts folders, XEX, XDVDFS ISO and GOD; supports the tested four-disc Asian XEX set. All 60 imported files matched the original extraction. | Importing four discs does not establish later-disc progression or automatic disc-switch compatibility. |
| First launch | Language and graphics setup runs before game initialization; the importer opens when files are missing. | Fresh language settings reached the actual Chinese game menu in the same process. |
| Settings | Original options and save confirmation retained; horizontal tabs; English, Japanese, Korean, Traditional and Simplified Chinese UI/game-language choices; FXAA, output scaling and display modes. | Window preview, timeout rollback, save confirmation and selected localized menus tested. Fullscreen, mouse and mixed-DPI behavior need broader desktop testing. |
| Shader discovery | Built-in index covers 52 resource files and finds the same 2,000 shaders as the full scan. Unrecognized layouts retain a scan fallback. | Local discovery improved from 36.2 to 1.1 seconds; this is discovery time, not total startup time. |
| Shader preparation | Uses logical CPU threads minus one, minimum one, and persistent cache reuse. | On a 16-thread PC, 15 workers reduced preparation from 53.4 to 6.7 seconds in a controlled comparison. 1,998 successful DXIL outputs were byte-identical; two failures remain. Runtime variants and PSO creation can still cause first-use stalls. |
| Title and input | Animated title, menus, SDL controllers and keyboard input work in tested scenes. | Rumble is disabled by default; `LO_CONTROLLER_RUMBLE=1` enables it. |
| Saves and debug | Manual save/overwrite/reload, optional save-anywhere backend, map ID/name, coordinate bookmarks and same-map POI teleport implemented. F1 layout exposes the save toggle and supports resizing/scrolling. | Non-save-point save/reload, native save-point permissions and camp transitions tested. UI tests cover three sizes and checkbox callbacks; broader gameplay and multi-DPI acceptance remain. |

See [settings](notes/settings-menu.md), [shader preparation](notes/shader-preparation.md), [storage](notes/save-storage.md), [save-anywhere](notes/save-anywhere.md), [map information](notes/debug-map-info.md) and [teleport](notes/debug-teleport.md).

## Repairs and user feedback

| Report | Current result | Remaining verification |
|---|---|---|
| Startup driver crash during depth clear | Batched rectangle clearing replaced the failing full clear path; targeted tests and startup runs passed. | Broader driver/hardware coverage. [Evidence](notes/startup-depth-clear-crash.md). |
| Ground character shadows disappearing after movement | Stale guest pixel shaders are no longer bound for mode-5 stencil-volume draws. Camp movement and GPU stencil tests passed; the user confirms ground projections are basically fixed. | Cross-map and encounter regression; not a claim about all character-surface shadows. [Evidence](notes/shadow-texture-lod.md). |
| Map 12 poster black patches | Polygon-offset conversion corrected. Same-binary 300-frame A/B: legacy path had patches in 169 frames; corrected path had none. Shallow-depth occlusion test passed. | Broader maps and near-plane behavior. This approximates float24 behavior rather than fully emulating it. [Evidence](notes/map12-poster-depth.md). |
| Map 13 shadows flickering on characters | Latest user feedback reports that the flicker appeared to disappear and the result was satisfactory; further investigation was deferred in favor of the poster issue. | Treat as improved / awaiting controlled regression, not as an actively confirmed failure or a proven universal fix. A shared cause with the poster issue is unproven. |
| Ring outer ring missing | User confirmed normal behavior with a physical controller. Hold RT after confirming an attack, then release when the rings overlap. No additional rendering fix was required for the final report. | Other battle scenarios are not exhaustively tested. [Evidence](notes/battle-ring-resource.md). |
| Accelerated / unintelligible dialogue | **Resolved and user-confirmed.** XMA packet handling preserves new frames in continuation packets without changing sample rate or volume. The tested vehicle dialogue recovered from 2,129 to 4,062 frames; timing slope against the original changed from 0.547 to 1.000. User confirmed normal playback. | The reported voice defect is closed. Other scenes and loop subframe boundaries are routine regression coverage. 34 multichannel buffers / 4,264 frames also decoded without errors. [Evidence](notes/audio-output.md). |
| Camp deadlock and window unresponsiveness | Big-endian critical-section deadlock repair and window-message-pump changes are included. Selected routes and focused tests passed. | Separate intermittent GPU query/wait pointer corruption remains unresolved; successful routes do not establish complete stability. [Critical sections](notes/critical-section-endian.md) · [GPU waits](notes/third-map-hang.md). |

Older executable hashes in investigation notes identify test artifacts, not the current release executable. User save/profile preservation checks belong to those recorded runs.

## Open issues

- Fire-hit black/red checker effects and black broken-crate effects have no verified fix.
- Intermittent GPU query/wait failures and long-session stability still need diagnosis.
- Two resource-shader translations fail; unrecognized resources, runtime variants and PSO preparation remain incomplete.
- Map 13 character-surface shadows need a controlled regression despite positive user feedback.

## Regression coverage

The reported voice problem is resolved, as reconfirmed by the user on 2026-09-06. The following are coverage tasks, not evidence that the voice defect remains open.

- Additional audio scenes, encounters, cutscenes, A Thousand Years of Dreams and world-map progression require regression testing.
- WMV playback, later-disc integration and a complete playthrough are not validated.
- Fullscreen/exclusive modes, mouse interaction and mixed-DPI behavior need broader desktop acceptance testing.

## Next development goals

1. Improve compatibility and stability across more of the game, resolve outstanding rendering and stability defects and expand save/progression regression.
2. Add more anti-aliasing options beyond FXAA.
3. Add higher internal rendering resolutions and scaling/upscaling options, including investigation of DLSS/FSR.
4. Investigate frame generation (FG).

These are future goals with no committed release date. **DLSS and FG are disabled placeholders in v0.1.** Existing output-resolution scaling is not higher internal rendering resolution or temporal upscaling. Unlocked frame rates, HDR, Linux/Vulkan gameplay and Steam Deck validation also remain future work. See the [roadmap](ROADMAP.md).

## Maintenance

[Roadmap](ROADMAP.md) · [Handoff](notes/handoff.md) · [Research index](notes/README.md) · [Work report](WORK_REPORT_2026-09-05.md) · [Archive](archive/README.md)

Private game data and raw diagnostic captures are not distributed. Selected gameplay screenshots in `docs/images/` illustrate development builds. Record implementation, release inclusion, test scope and user feedback separately when updating this ledger.
