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

- [x] **v0.5.4 release:** published 2026-09-11T01:36:30Z from `2ad94d418bb0478417ab9589109f1f685ed92eb3`; CI 34550200618 passed. The 44,237,061-byte ZIP hashes to `104ced8b60c16cd1b9013543a3940c9ed8d7cf904c3d05a6a8ef8d591f51d218`; package provenance, version, all 50 file hashes/CRCs and four anonymous asset downloads passed. This records release delivery only; each item's runtime and player-acceptance limits remain below.
- [x] **Installer drag crash:** v0.5.4 publicly includes the locally accepted `PostMessageW` repair for the v0.5.3 drag stutter/exit. The message-only HWND DragDispatch case passed 1/1; the legacy fixture remains limited by an unchanged foreground setup assertion. The user accepted the reported drag path on 2026-09-10; broader installer interaction coverage remains separate.
- [~] **Updater repair:** v0.5.3 includes the ZIP root-entry repair with single-root validation and English-only updater UI. The retained updater target and 12 archive cases passed; package and public-download verification passed for v0.5.3. A full update transaction, game run and user acceptance remain open.
- [~] **Optional shader collection:** v0.5.3 includes compact opt-in diagnostics and schema 3 binding evidence; Worker schemas 1–4 and the private archive plus ledger are integrated. The compact collection uses a 32-frame/180-second CPU window, at most 24 pairs and 8 bindings, within 32 KiB. All 18 ledger cases remain pending review. Schema 3 covers the two e810 binding pairs only; new candidates still need program review and sufficient final-binding, producer-timing and jitter evidence. Visual repair, player acceptance, broad GPU coverage and all-submitted-draw coverage remain open.
- [~] **Capture export timeout:** two F1 ZIP archives timed out after roughly 60 seconds. The archive worker now waits up to 180 seconds (`60000` → `180000` ms) before treating the child as timed out; its existing process, job-object and `Optimal` compression behavior is unchanged. The v0.5.3 EXE linked successfully, its `build.json` source identity was checked, and the installed replacement hashes to `CF70EA663ED230334145E1135CA97A58DFE3A34C65ED7D43E9E428C41B53270B`; old EXE/metadata are backed up under `out/f1-zip-180s/backup`. The overall build ended with a DXC-copy failure because a source dependency DLL was absent; the already-installed DLL was copied into the build output without recompiling. No timeout recovery test, game run or player acceptance is recorded; v0.5.4 package and public-download verification passed.
- [~] **4K TAA and ground/shadow feedback:** captures 17468–17470 came from an older executable that predates the four-path c7 repair. They do not reopen the later accepted four-path Sol repair, diagnose a new cause or establish broad visual coverage. Continue scene, hardware and reporter-specific validation through the Project.
- [~] **PPC generated-source provenance guard:** the generated-tree provenance sub-scope is complete: `ppc_codegen generate` produced 247 C++ units with 843 `.u32` and no `.u64` selectors, while 246 instruction-stream hashes remained unchanged. Seven guard fixtures passed for stale output, input/tool/context drift and return-0-without-output recovery; the historical 3,258/109/843-table-44,523 evidence was reused without rerun. CMake configuration and the compile-order guard dependency are verified; the broader PPC-contract audit remains open. Issue #14's old v0.5.0 log still does not identify a root cause, a released-build condition or cage-scene fix; no game run or player acceptance is recorded.
- [ ] **Presentation and input:** fullscreen, Alt+Enter, mixed-DPI and mouse acceptance remain open. Preserve normal aspect-ratio black bars when assessing reported underscan.
- [ ] **Gameplay and stability:** later progression, save/readback, encounters, long-running stability, remaining rendering reports and cross-GPU/package evidence remain tracked work. Existing bounded repairs do not prove a complete playthrough.
- [~] **Assembly-level performance analysis:** the external Win64 tool now records wall-clock RIP snapshots, resolves DbgHelp PDB symbols/source locations, and generates Capstone x64 HTML/JSON hotspots, thread CPU-time/filtering and function self-sample rankings. Seven focused report cases, an MSVC Release build and a synthetic end-to-end capture passed; actual-game sampling and player acceptance remain open. GPU instruction profiling is not supported. Its PPC annotation is generated-source context, not an exact guest PC, and requested sampling intervals are not actual frequency.
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
