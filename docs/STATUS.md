# Project status

Reviewed **2026-09-06** against the v0.2.1 release, recorded tests and subsequent user feedback. This page describes current results; dated investigation notes retain the history of individual experiments.

## Published release

[v0.2.1](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.2.1) is the latest full release (`draft=false`, `prerelease=false`), built from `906d7c039f7e709c57af2c5278a43e259bfba8e5`; both remotes have the tag. [Hosted CI 34053765472](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34053765472) passed every step, including `LoHidTest`.

The official ZIP is 38,203,314 bytes, SHA256 `cd583a6a28b47a1b2e7e31984052cd4eb6c953f5df333198f41540114eb4afb3`, matching the public API digest. All 44 manifest hashes passed and the packaged installer self-test exited 0 (`out/release-v0.2.1/package-validation.json`). An isolated cold-cache launch ran for 54 seconds with a visually confirmed German main menu. Its frame-400 capture ZIP contained 60 entries in 12,307,207 bytes; CRC and each entry SHA256 matched the raw files. ZIP completion at 43.798 seconds was followed by about 30 fps at 48.758–54.757 seconds. Evidence: `out/release-v0.2.1/smoke/runtime.log` and its captures. The test process exited, the main executable remained at `135DCA79`, and user saves were preserved.

## Previous release: v0.2

[v0.2](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.2) was published on **2026-09-06 at 17:16 UTC** as a full release, not a draft or prerelease. Its tag points to `dcc946299cdc2984783793ad5871a0ad0b90a2c9`; the code and tag were pushed to both remotes. [Hosted Windows CI](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/34047138263) passed, including importer and disc-set tests.

The official `LostOdysseyRecomp-windows-x64-v0.2.zip` is 38,185,445 bytes, SHA256 `b125dae559a8d62d6ff79d0bcc80fd6af44fee6a41100db787b8bfca61facab6`. The downloaded ZIP matches the public API digest and checksum file. All manifest hashes, commit/version fields and `development_build=false` were verified; no original game files are included. The packaged importer self-test passed, followed by a 30-second isolated launch with a minimal PATH and a visually confirmed German title menu. Evidence: `out/eu-import-audit/release-v0.2-result.json` and `out/eu-import-audit/release-v0.2-smoke`.

The preceding [v0.1](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.1) was built from `2a3ffcc` and retains its historical release contents. Dated local/uncommitted notes describe their original test stage; USA/Europe and automatic-disc-selection changes are now included in v0.2. Source builds apply the tracked [dependency patches](../tools/patches/README.md).

**Validation remains limited to opening areas, selected scenes and controlled disc-manager requests. Chapter-boundary story transitions and a complete playthrough remain unverified.**

[Release notes](RELEASE-v0.2.md) · [Installation](INSTALLING.md) · [Build and packaging evidence](notes/release-packaging.md)

## v0.2 implementation and validation

The supported sets correspond to [Redump 11817, USA, Europe, version 0.0.0.3](https://redump.info/disc/11817) and [Redump 39111, Europe, Asia, version 0.0.0.4](https://redump.info/disc/39111). Their identity fields match the audited sets; a complete ISO-to-Redump hash comparison has not been performed. Both four-disc sets are accepted by the importer. Strict XEX hashes identify the edition; mixed editions are rejected both within an input set and when appending to an existing installation. Existing discs are checked from their actual XEX files. Runtime edition detection precedes setup/configuration, and missing-data first launch imports before presenting edition-specific game-language choices. USA/Europe offers English, Japanese, German, French, Spanish and Italian; the Asian game-language list and five interface translations remain unchanged.

The Release build, 19 importer tests and 11 isolated setup tests passed. All 60 files in a real four-disc import (23,063,969,792 bytes) matched the source ISO SHA256 hashes. All six language starts passed; English, German, French, Spanish and Italian title menus and the Japanese original brightness/settings page were visually confirmed. This covers menus/settings, not gameplay. The host voice selector now reads the resource-provided language list instead of assuming three choices; the five USA/Europe voice options and the three Asian voice options completed visually checked cycles. These changes are included in the **published v0.2 release**, not v0.1. The earlier language-support package passed all manifest hashes, frozen importer self-test and a 30-second rendered runtime check with PATH limited to System32. The main development executable and user save/profile were preserved. See [USA/Europe evidence and remaining coverage](notes/europe-support.md).

The published disc-selection change switches game paths to the requested installed disc before completing `XamSwapDisc`. It validates edition/disc identity, the original FPI and archive ranges, leaves existing handles open on their original files and preserves the current mount on failure. Both audited editions passed storage alias/read tests, and seven malformed/valid installation cases passed. The original disc manager completed 1 → 2 → 3 → 4 → 1 for both USA/Europe and Asia while reloading each target FPI and continuing rendering. The Asian final title menu was visually confirmed without a disc-change dialog. All 19 importer tests passed again, and the final Release build succeeded. The updated local package passed every manifest hash, frozen importer self-test and a 30-second rendered/captured launch with PATH limited to System32. Four storage write/read/overwrite/read-overwritten regressions also passed. This is controlled runtime coverage, not chapter-boundary gameplay or user acceptance. See [disc selection](notes/disc-selection.md).

## v0.2.1 capture

The capture and input changes below are included in the verified v0.2.1 release. Earlier local test results retain their original scope; final package validation is recorded above.

The F1 Debug Menu now queues **Capture render state** for the next complete frame, showing busy/progress/output status. It exports a resolved screenshot, draw register changes, translated shaders, intermediate resolve previews and raw resolve/depth data to `captures/render-<timestamp>-f<frame>` under the working directory, then automatically creates a sibling ZIP while retaining the raw files. The ZIP-enabled build and isolated archive validation passed: all 60 archived files matched their raw SHA256 values, with 91,363,708 bytes reduced to 11,701,650 bytes. The earlier build and two isolated RTX 5080 title/menu captures passed, including the actual F1 button command. Both exported matching BMP/PPM pixels, 16 correctly sized raw resolves and 21 shaders, then returned to about 30 fps. A deliberately invalid output directory reported failure without crashing and rendering continued; other disk failures, full layout/DPI and AMD behavior are not validated. This is included in published v0.2.1; it is not part of v0.2 and is not an AMD fix. Readback stalls invalidate performance comparisons, floating-point previews clamp values, and the output is not replayable. See [capture details](notes/render-state-capture.md).

## v0.2.1 input

All SDL-mapped controllers now merge into player 1 with the keyboard always available; no active-device selection is required. Hotplug uses stable instance IDs, buttons combine, triggers take their maximum and sticks choose the strongest vector above their deadzone. Keyboard events are captured on the event thread and cleared on focus loss; E/R add left/right triggers. Rumble remains opt-in and targets all opened pads. The actual HID implementation passed SDL virtual-device tests and the full build. The combined preview sustained an isolated title run near 30 fps, and a targeted Z event reached the SDL window loop; this is event-path coverage rather than physical keyboard/gameplay acceptance. Physical model/gameplay coverage remains unverified. This is included in published v0.2.1, and is not multiplayer support. See [input evidence](notes/controller-input.md).

## Implemented and validated features

| Area | Current result | Validation boundary |
|---|---|---|
| Importer | Introduced in v0.1; v0.2 accepts both audited four-disc editions from folders, XEX, XDVDFS ISO and GOD, with strict XEX hashes and mixed-edition rejection. | Complete imported-file hashes passed; import alone does not establish story progression. |
| First launch | Introduced in v0.1; v0.2 imports missing data before presenting edition-specific language/graphics setup, then initializes the game. | Eleven isolated setup tests and localized menu checks passed; the published-package smoke test also passed. |
| Settings | Original options/save confirmation, five interface translations, FXAA, output scaling and display modes retained. v0.2 chooses game text languages by edition and voice options from resources. | Both editions' voice selectors and six USA, Europe menu/settings languages tested. Fullscreen, mouse and mixed-DPI desktop coverage remains. |
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

## Regression coverage

The reported voice problem is resolved, as reconfirmed by the user on 2026-09-06. The following are coverage tasks, not evidence that the voice defect remains open.

- Map 13 character-surface shadows need controlled regression after positive user feedback; this is not a reopened defect.
- Additional audio scenes, encounters, cutscenes, A Thousand Years of Dreams and world-map progression require regression testing.
- Chapter-boundary saves/reloads and multilingual text/voice playback across both editions need coverage beyond the menu tests.
- WMV playback, chapter-boundary progression and a complete playthrough are not validated; controlled installed-disc selection is recorded separately above.
- Fullscreen/exclusive modes, mouse interaction and mixed-DPI behavior need broader desktop acceptance testing.

## Next development goals

The [roadmap near-term priorities](ROADMAP.md#near-term-priorities) are the active task queue: stability, confirmed rendering/shader defects, progression/save/multilingual regression, then display acceptance. Modern graphics follow after this compatibility work.

These are future goals with no committed release date. **DLSS and FG are disabled placeholders in v0.2.** Existing output-resolution scaling is not higher internal rendering resolution or temporal upscaling. Unlocked frame rates, HDR, Linux/Vulkan gameplay and Steam Deck validation also remain future work. See the [roadmap](ROADMAP.md).

## Paused research

The user paused the text-language complement patch on **2026-09-06**. There is no finished patch or runtime code change, and it is not included in v0.2. Voice is outside that research scope. Keep it out of active implementation priorities unless the user resumes it; see [research record](notes/text-language-patch.md).

## Maintenance

[Roadmap](ROADMAP.md) · [Handoff](notes/handoff.md) · [Research index](notes/README.md) · [Work report](WORK_REPORT_2026-09-05.md) · [Archive](archive/README.md)

Private game data and raw diagnostic captures are not distributed. Selected gameplay screenshots in `docs/images/` illustrate development builds. Record implementation, release inclusion, test scope and user feedback separately when updating this ledger.
