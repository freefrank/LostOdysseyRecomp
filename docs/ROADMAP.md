# Roadmap

[简体中文](ROADMAP.zh-CN.md) · [Current status](STATUS.md) · [Changelog](../CHANGELOG.md) · [Historical roadmap snapshot](archive/ROADMAP-2026-09-10.md)

`[ ]` outstanding · `[~]` in progress · `[x]` has bounded evidence. The [public maintainer Project](https://github.com/users/freefrank/projects/3) is the current work-item source of truth. This mirror keeps direction, open work and validation limits; implementation, player acceptance and release status are separate.

## Delivery

[v0.5.3](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.3) is the latest release, published on 2026-09-10. Build, package and public-download verification passed. See [STATUS](STATUS.md) for the release evidence and validation limits.

<a id="v050-pc-graphics"></a>
<a id="next-major-milestone-v050--pc-vulkan-and-direct3d-11"></a>
## PC graphics direction

D3D12 remains the working baseline. Windows Vulkan has bounded RTX 5080 scene validation and remains separate from wider GPU and full-game coverage. Direct3D 11, Linux/Steam Deck and Switch are independent future platform work; no completion or release date is implied.

<a id="near-term-priorities"></a>
## Active work and acceptance boundaries

- [~] **Updater repair:** v0.5.3 includes the ZIP root-entry repair with single-root validation and English-only updater UI. The retained updater target and 12 archive cases passed; package and public-download verification passed for v0.5.3. A full update transaction, game run and user acceptance remain open.
- [~] **Optional shader collection:** v0.5.3 includes compact opt-in diagnostics and schema 3 binding evidence; Worker schemas 1–4 and the private archive plus ledger are integrated. The compact collection uses a 32-frame/180-second CPU window, at most 24 pairs and 8 bindings, within 32 KiB. All 18 ledger cases remain pending review. Schema 3 covers the two e810 binding pairs only; new candidates still need program review and sufficient final-binding, producer-timing and jitter evidence. Visual repair, player acceptance, broad GPU coverage and all-submitted-draw coverage remain open.
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
