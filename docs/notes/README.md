# Reverse Engineering and Validation Notes

[Documentation home](../README.md) · [Current status](../STATUS.md) · [Roadmap](../ROADMAP.md) · [Archive](../archive/README.md)

2026-10-14 cleanup: 6 notes that were fixed, replaced by current documents, or tied only to their original date were moved to the [archive](../archive/README.md) and are no longer listed in this index.

2026-10-11 cleanup: expired handoffs, phase records, fix timelines and one-time measurements were moved to `archive/notes/` in the [archive](../archive/README.md). This directory now holds **60** topic notes, listed by subject, each appearing only once. File names and original evidence are kept. This directory does not duplicate version release or task progress.

## How to use

| Marker | How to read |
|---|---|
| Reference | Maintained topic mechanisms or operating instructions. Old tests in them still cover only the original version and conditions. |
| Historical | Measurements, fixes, handoffs and progress tied to a date, version or phase. "Current", "unreleased", PIDs and next steps in them applied only at that time. |
| Investigation | Problem analysis and its evidence limits. This marker does not mean the related Issue is still open. Current status is in STATUS and the roadmap. |
| Draft | Design, research, requirements or release copy. Not evidence of implementation, and not automatically withdrawn because it is old. |

Category totals: Reference 33, Historical 15, Investigation 10, Draft 2.

Check current status first for implementation, validation, acceptance and release, then go to the evidence below. Before running an old command, check the source, tools and inputs. `out/`, absolute local paths and screenshot numbers are locator information from the original investigations. They are not distributed with the repository. Local artifacts that are now missing are marked as historical paths and cannot be treated as re-validated in this round.

## Common topic entry points

[FG integration reference](reusable-fg-game-integration.md) · [Settings menu](settings-menu.md) · [Render capture](render-state-capture.md) · [Shader preparation](shader-preparation.md) · [First-use pipeline stall plan](pipeline-first-use-stalls.md) · [CPU optimization guide](cpu-performance-optimization-guide.md) · [Release packaging](release-packaging.md) · [Save Anywhere limits](issue-74-save-anywhere-party-split.md)

## FG and SDK integration

| Document | Type |
|---|---|
| [Vulkan frame generation, FSR 4 and MetalFX](vulkan-fg-fsr4-metalfx.md) | Investigation |
| [Reusable FG game integration](reusable-fg-game-integration.md) | Reference |

## Super resolution and motion vectors

| Document | Type |
|---|---|
| [Geometric motion replay: implementation and validation boundary](motion-vector-implementation.md) | Reference |
| [Native DLSS Validation (P0 Foundation, P1 Temporal Inputs & P2 Checkpoint)](native-dlss-validation.md) | Historical |
| [DLSS 5 NR Integration Research](dlss5-nr-research.md) · [简体中文](dlss5-nr-research.zh-CN.md) | Investigation |

## TAA and jitter

| Document | Type |
|---|---|
| [Cutscene tour and 45 reviewed jitter pairs — 2026-10-04](jitter-cutscene-tour-2026-10-04.md) | Historical |
| [Mansion battle lights and a two-view cutscene (#212) — 2026-10-04](jitter-212-2026-10-04.md) | Historical |
| [Temporal jitter coverage review — 2026-09-25](jitter-coverage-2026-09-25.md) | Historical |
| [TAA Bell-Stand Flicker Investigation (Issue #46 adjacent)](TAA_BELL_FLICKER_INVESTIGATION.md) · [简体中文](TAA_BELL_FLICKER_INVESTIGATION.zh-CN.md) | Investigation |

## GPU rendering and picture issues

| Document | Type |
|---|---|
| [Display gamma ramp (#78, #179) — 2026-10-04](display-gamma-179-2026-10-04.md) | Historical |
| [Brightness / Gamma page — 2026-10-04](brightness-gamma-2026-10-04.md) | Reference |
| [Live anisotropic filtering](anisotropic-filtering.md) | Reference |
| [Fire-hit lighting investigation (2026-09-05)](fire-hit-rendering.md) | Investigation |
| [Experimental HDR output](hdr-output.md) | Reference |
| [Linux/Vulkan HDR research — 2026-10-02](linux-vulkan-hdr.md) | Investigation |
| [Host GPU occlusion queries (#118)](occlusion-queries.md) | Reference |
| [Character black silhouettes: physical address aliasing and occlusion queries (2026-09-04)](../archive/notes/physical-alias-rendering.md) | Historical |
| [Post-battle white screen and EDRAM format switch (2026-09-04)](../archive/notes/post-battle-whiteout.md) | Historical |
| [Debug Menu render-state capture](render-state-capture.md) | Reference |
| [Rendering validation (2026-09-04)](rendering-validation.md) · [简体中文](rendering-validation.zh-CN.md) | Historical |
| [Tall and non-standard aspect layout — 2026-10-01](tall-aspect-layout.md) | Reference |
| [Title dynamic background restore (2026-09-04)](../archive/notes/title-packed-mips.md) | Historical |
| [Shadow resolution and screen-space ambient occlusion](shadow-ambient-occlusion.md) | Reference |

## Performance and frame rate

| Document | Type |
|---|---|
| [CPU performance optimization guide: 3C6T budget, feasible parallelism and anti-patterns](cpu-performance-optimization-guide.md) · [简体中文](cpu-performance-optimization-guide.zh-CN.md) | Reference |
| [Native 90/120 FPS: Implementation and Local Validation](native-90-120fps.md) · [简体中文](native-90-120fps.zh-CN.md) | Reference |
| [FreeSync / G-SYNC Compatible: Application-Side VRR Integration](vrr-freesync-gsync-compatible.md) · [简体中文](vrr-freesync-gsync-compatible.zh-CN.md) | Reference |
| [Cost of Graphics Enhancement Options: v0.9.22, RTX 5080, 2026-10-10](graphics-options-cost-2026-10-10.md) · [简体中文](graphics-options-cost-2026-10-10.zh-CN.md) | Reference |
| [Cost of graphics options (1080p output): main f7799aec, RTX 5080 and psvita, 2026-10-15](graphics-options-cost-1080p-2026-10-15.md) · [简体中文](graphics-options-cost-1080p-2026-10-15.zh-CN.md) | Reference |
| [Merging Render Passes: v1.0.0 Design Research](render-pass-merging-design-2026-10-10.md) · [简体中文](render-pass-merging-design-2026-10-10.zh-CN.md) | Draft |
| [Render pass merging phase 0: tablet measurements](render-pass-merging-phase0-2026-10-10.md) · [简体中文](render-pass-merging-phase0-2026-10-10.zh-CN.md) | Investigation |

## Shader preparation and overrides

| Document | Type |
|---|---|
| [Startup Shader Preparation (2026-09-05)](shader-preparation.md) · [简体中文](shader-preparation.zh-CN.md) | Reference |
| [One-file shader store and the 2026-10-02 packs](shader-store-2026-10-02.md) | Reference |
| [Built-in shader resource index](shader-resource-index.md) | Reference |
| [First-Use Pipeline Stalls: Plan (2026-10-05)](pipeline-first-use-stalls.md) · [简体中文](pipeline-first-use-stalls.zh-CN.md) | Reference |

## UI, input and debugging

| Document | Type |
|---|---|
| [Built-in cheats and LT speed control](cheats.md) | Reference |
| [Multiple controllers and keyboard input](controller-input.md) | Reference |
| [Debug Map ID and Localized Names (2026-09-05)](debug-map-info.md) · [简体中文](debug-map-info.zh-CN.md) | Reference |
| [Debug character teleport: reverse-engineering basis and validation boundaries (2026-09-04)](debug-teleport.md) · [简体中文](debug-teleport.zh-CN.md) | Historical |
| [Issue #40 UI Resource Map and Asset Archive](issue-40-ui-resource-map.md) · [简体中文](issue-40-ui-resource-map.zh-CN.md) | Historical |
| [Original Settings menu assets](menu-original-assets.md) | Reference |
| [Settings menu and display options](settings-menu.md) · [简体中文](settings-menu.zh-CN.md) | Reference |

## Runtime, saves and game flow

| Document | Type |
|---|---|
| [Audio Output and Dialogue Fix (2026-09-05)](audio-output.md) · [简体中文](audio-output.zh-CN.md) | Historical |
| [Guest critical-section byte order](critical-section-endian.md) | Reference |
| [Automatic selection of imported discs](disc-selection.md) | Reference |
| [USA/Europe edition support](europe-support.md) | Reference |
| [Issue #74: After Saving Anywhere During a Party Split, Character Switching Is Lost (Investigation of 2026-09-29)](issue-74-save-anywhere-party-split.md) · [简体中文](issue-74-save-anywhere-party-split.zh-CN.md) | Investigation |
| [Issue #12 root cause: garbage-collected materials drawn by a stale scene proxy](../archive/notes/issue12-root-cause.md) | Historical |
| [Issue #114: field interaction during a pending battle request](ISSUE_114_FIELD_INTERACTION_20261001.md) | Investigation |
| [Recompilation Configuration Notes](recomp.md) · [简体中文](recomp.zh-CN.md) | Reference |
| [Recompiler width and control-flow audit](recompiler-width-audit.md) | Reference |
| [Save anywhere: menu permissions and validation](save-anywhere.md) · [简体中文](save-anywhere.zh-CN.md) | Reference |
| [Manual save failure (2026-09-04)](../archive/notes/save-storage.md) | Historical |
| [Game Data Sources and XEX](xex.md) · [简体中文](xex.zh-CN.md) | Reference |

## Mods, language packs and asset replacement

| Document | Type |
|---|---|
| [High-Resolution / High-Frame-Rate CG Replacement: Feasibility Study](hd-cutscene-replacement.md) · [简体中文](hd-cutscene-replacement.zh-CN.md) | Investigation |
| [Language Packs (#369): Design and Feasibility for Export, Import and New Languages](language-pack-research.md) · [简体中文](language-pack-research.zh-CN.md) | Draft |

## Platforms and developer tools

| Document | Type |
|---|---|
| [GitHub Actions checks and releases](ci.md) | Reference |
| [Android ARM64 DXC Build (2026-10-02)](android-dxc-build-2026-10-02.md) · [简体中文](android-dxc-build-2026-10-02.zh-CN.md) | Reference |

## Triage and release

| Document | Type |
|---|---|
| [Automated issue triage](issue-triage.md) | Reference |
| [Installer and Windows release pipeline](release-packaging.md) | Reference |

## Related records

[Documentation cleanup record](../archive/audits/documentation-2026-09-30.md) explains this round's sources, status corrections and Basic Memory knowledge organization. The [archive directory](../archive/README.md) keeps replaced global ledgers and expired notes. Expired notes were moved to `archive/notes/` with `git mv`, and the links that pointed to them were fixed. No redirect pages were left behind. New notes should be added to this index, with the applicable version, evidence scope and current entry points.
