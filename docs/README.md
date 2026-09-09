# Documentation

[English README](../README.md) · [简体中文](../README.zh-CN.md)

| Start here | Purpose |
|---|---|
| [Changelog / 更新日志](../CHANGELOG.md) | Single history of completed changes, separating unpublished work from verified releases |
| [v0.4.2 changes](../CHANGELOG.md#v042--2026-09-08) | Council/PPC repairs, crash logs, battle TAA coverage and background exports; [validation scope](STATUS.md) |
| [Issue #7 Council investigation](notes/issue7-cutscene-crash.md) / [PPC semantics audit](notes/recompiler-width-audit.md) | Root cause, generated-code regressions, native save/reload and remaining coverage |
| [Issue #12 funeral crash investigation](notes/issue12-funeral-crash.md) / [Claude handoff](notes/issue12-claude-handoff.md) | Official v0.4.2 reproduction, execute-null object evidence and bounded writer/lifetime follow-up |
| [v0.4.1 release](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.4.1) | Previous published release: accepted Map3 TAA repair and three-frame captures with logs; [validation scope](STATUS.md) |
| [v0.4.0 release](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.4.0) | Internal resolution up to 4K, AA/text, frame-rate controls and both-edition indexed discovery; [official validation](STATUS.md) |
| [v0.4.0 development evidence](notes/v0.4.0-development.md) | Recorded implementation and validation; [follow-up evidence](notes/handoff-v0.4.0-followup.md) and remaining acceptance limits |
| [DLSS/FSR temporal-upscaling feasibility](notes/temporal-upscaling-feasibility.md) | Official input contracts, current depth/camera/jitter evidence, missing native motion/color inputs and future SDK integration |
| [v0.3.0 release](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.3.0) | Published shader discovery and recorded-pipeline preparation; [validation boundaries](notes/shader-preparation.md) |
| [v0.2.2 release notes](notes/release-0.2.2.md) | Published AMD rendering and Unicode startup/save-path fixes |
| [Unicode paths](notes/save-path-unicode.md) | Startup/storage validation and Issue #4 limits |
| [v0.2 release notes](RELEASE-v0.2.md) | Published edition support, languages and automatic disc selection |
| [Supported editions](notes/europe-support.md) | USA, Europe and Europe, Asia evidence; exact XEX validation |
| [Automatic disc selection](notes/disc-selection.md) | Implemented manager flow and remaining story-transition coverage |
| [Text-language patch research](notes/text-language-patch.md) | Paused by the user on 2026-09-06; no finished patch or runtime change |
| [Installation](INSTALLING.md) | Import ISO, extracted folders/XEX or GOD discs |
| [Release packaging](notes/release-packaging.md) | Windows build artifacts, dependencies and CI input |
| [Shader index](notes/shader-resource-index.md) | Built-in locations, fallback and cold-start comparison |
| [Work report](WORK_REPORT_2026-09-05.md) | Historical 2026-09-05 repair outcomes and evidence |
| [Status ledger](STATUS.md) | Published changes, resolved reports, open defects and regression coverage |
| [Synchronization](PUBLISHING.md) | One public history, Gitea/GitHub dual push and archive policy |
| [Build guide](BUILDING.md) | Prerequisites, generation, running and storage |
| [Maintainer Project (public)](https://github.com/users/freefrank/projects/3) | Work-item status, priorities and sources |
| [Roadmap mirror](ROADMAP.md) / [简体中文](ROADMAP.zh-CN.md) | Synchronized repository TODO and bounded completion criteria |
| [v0.5.0 PC graphics plan](ROADMAP.md#v050-pc-graphics) / [简体中文](ROADMAP.zh-CN.md#v050-pc-graphics) | Planned Windows PC Vulkan and DX11; implementation and acceptance pending. [Backend handoff](notes/switch-vulkan-handoff.md) includes later Switch alignment |
| [v0.5.0 QOL requirements](notes/v0.5.0-qol-requirements.md) | Twelve planned Windows QOL items awaiting completion and validation; no implementation or version change in this record |
| [Handoff](notes/handoff.md) | Continuation context and source map |
| [Debug requirements](debug-menu-requirements.md) | Available and requested controls |
| [Settings menu](notes/settings-menu.md) | Options, language, presentation and validation boundaries |
| [Documentation agent](agents/documentation.md) | On-demand synchronization workflow and review checks |
| [Project management agent](agents/project-management.md) | Event-triggered Project/TODO synchronization, evidence and documentation handoff |
| [Project import audit](project-management/import-audit.md) | Historical coverage, requirement mapping and initial synchronization results |
| [Research index](notes/README.md) | Subsystem evidence |
| [Archive](archive/README.md) | Superseded snapshots |

The main README is English, with a separate Chinese translation. Most engineering notes are Chinese. Dated experiments preserve their original scope and are not current support promises.

Also see [dependency patches](../tools/patches/README.md), [Ghidra scripts](../tools/ghidra/README.md). Third-party and skill-submodule documentation is maintained upstream.
