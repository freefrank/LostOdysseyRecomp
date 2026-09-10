# Roadmap

[简体中文](ROADMAP.zh-CN.md) · [Current status](STATUS.md) · [Changelog](../CHANGELOG.md) · [Historical roadmap snapshot](archive/ROADMAP-2026-09-10.md)

`[ ]` outstanding · `[~]` in progress · `[x]` has bounded evidence. The [public maintainer Project](https://github.com/users/freefrank/projects/3) is the current work-item source of truth. This mirror keeps direction, open work and validation limits; implementation, player acceptance and release status are separate.

## Delivery

[v0.5.2](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.2) is the latest release, published on 2026-09-10. Build, package and public-download verification passed. See [STATUS](STATUS.md) for the release evidence and validation limits.

<a id="v050-pc-graphics"></a>
<a id="next-major-milestone-v050--pc-vulkan-and-direct3d-11"></a>
## PC graphics direction

D3D12 remains the working baseline. Windows Vulkan has bounded RTX 5080 scene validation and remains separate from wider GPU and full-game coverage. Direct3D 11, Linux/Steam Deck and Switch are independent future platform work; no completion or release date is implied.

<a id="near-term-priorities"></a>
## Active work and acceptance boundaries

- [~] **Updater repair:** the v0.5.2 local fix accepts the explicit ZIP root-directory entry while retaining single-root validation, and updater UI/errors now use English only. The updater target built successfully; 12 archive cases passed, including real `shutil.make_archive` layouts and malformed/archive-safety rejection cases. Render-only inactive-desktop checks found English strings for language 7, fitting controls and unchanged foreground. No game, network update, full update transaction, user acceptance or publication evidence exists.
- [~] **Optional shader collection:** collection development and the controlled D3D12 Map16 upload path are validated. The source-0.5.0 executable produced 55 cache sources and matching D1 records while rendering continued. v0.5.2 is published; product F1 capture, a new ZIP manifest and immediate upload have not been verified. Vulkan, AMD, reported visual defects and player acceptance remain open. Cumulative coverage and all-submitted-draw audit remain in progress.
- [ ] **Capture export size:** a new F1 ZIP capture timed out after 60.049 seconds. This records a failure; compact export and exporter repair are still unimplemented.
- [~] **4K TAA and ground/shadow feedback:** captures 17468–17470 came from an older executable that predates the four-path c7 repair. They do not reopen the later accepted four-path Sol repair, diagnose a new cause or establish broad visual coverage. Continue scene, hardware and reporter-specific validation through the Project.
- [ ] **Presentation and input:** fullscreen, Alt+Enter, mixed-DPI and mouse acceptance remain open. Preserve normal aspect-ratio black bars when assessing reported underscan.
- [ ] **Gameplay and stability:** later progression, save/readback, encounters, long-running stability, remaining rendering reports and cross-GPU/package evidence remain tracked work. Existing bounded repairs do not prove a complete playthrough.
- [ ] **Performance and shader startup:** investigate actual remaining stalls and the two retained compiler failures; do not generalize a fixed-scene measurement to the full game.

<a id="current-feedback-and-regression-work"></a>
## Validated scope retained for reference

The user accepted the limited D3D12/Vulkan real-scene boundary on 2026-09-08. The four c7 paths were accepted in one Ghost Town scene over about 11.37 seconds; that is not continuous-video, cross-scene, cross-GPU or full-game coverage. Earlier published repairs and their evidence remain available through [STATUS.md](STATUS.md), [CHANGELOG.md](../CHANGELOG.md) and the historical snapshot.

## Navigation

Use the [Project](https://github.com/users/freefrank/projects/3) for item status, dependencies and detailed outstanding requirements. Use [STATUS.md](STATUS.md) for current validation and release evidence, [CHANGELOG.md](../CHANGELOG.md) for published changes, and [the archived roadmap](archive/ROADMAP-2026-09-10.md) for the retained detailed history.

<!-- Compatibility anchors retained for existing Project and note links. -->
<a id="published-milestone-v042--repairs-and-validation"></a>
<a id="phase-1-produce-compilable-output"></a>
<a id="phase-2-reach-the-main-menu"></a>
<a id="phase-3-work-toward-a-complete-playthrough"></a>
<a id="phase-4-modernization"></a>
<a id="phase-5-optional-exploration"></a>
