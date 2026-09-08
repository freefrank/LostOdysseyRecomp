# Initial Project import audit

Snapshot: 2026-09-08T07:16:56.575193+00:00. Repository source: `147bffb214cd3650f3eadb810e86f7237511ac23`.

[Roadmap](https://github.com/users/freefrank/projects/3/views/1) · [Backlog](https://github.com/users/freefrank/projects/3/views/2) · [History](https://github.com/users/freefrank/projects/3/views/3). The Project retains its existing private visibility.

## Reviewed source coverage

- 77 history records: 69 distinct features, repairs, infrastructure changes or bounded investigations, plus eight release milestones from v0.1 through v0.4.2.
- All 91 commits reachable from the source revision were reviewed: 84 are directly referenced; seven summary or paused-investigation checkpoints are accounted for in the [history coverage](history-coverage.md).
- The 44 additional pre-public-cleanup archive commits match canonical author timestamps and subjects. They are rewritten historical counterparts, not extra features or byte-identical trees.
- 65 pending, ongoing or suspended work items cover all 94 unfinished/in-progress checkbox occurrences across the two language mirrors, plus documented requirements and explicit verification gaps. The two language occurrences are not distinct requirements. See the [backlog coverage and source conflicts](backlog-coverage.md).
- All nine Issues present in the live snapshot are linked exactly once. Issues #1, #8 and #9 are open; #2 through #7 are closed. Three closed reports (#2, #3, #6) add independent records; the other six reuse matching history/backlog items.

The consolidated manifest therefore contains **145 items: 136 Project drafts and nine linked Issues**. This is feature/fix-level coverage of retained records, not a claim to recover every unrecorded conversation or validate every game scene.

During final reconciliation, the new Issue #9 was also added to both roadmap mirrors. Their unfinished/in-progress checkbox occurrences now total 96; the requirement was already included in the 65-item backlog, so this adds no duplicate Project item.

## Reconciliation decisions

- Issue #5 has reporter-confirmed same-encounter recovery; the old missing-save/retest-pending wording was corrected. Broader edition/encounter regression stays separate.
- Issues #3, #4, #6 and #7 are closed without a later reporter acceptance in the checked comments. Project workflow, delivery and the linked Issue's `Evidence` field retain this distinction. Pending reporter verification is represented separately where already requested.
- Issue #2 was closed after the maintainer explained the PyInstaller packaging and deferred installer changes. It is not an antivirus-clearance claim.
- Issue #9 arrived during the audit. It records CPU temperatures and TAA texture flicker as two unverified symptoms, without assuming a shared cause or merging the enemy-disappearance report.
- The independent AMD reports remain suspended. Map 13 has conflicting older status descriptions and is retained as controlled regression; no new root cause or fix is asserted.
- Historical date ranges show cited work/report checkpoints. Future requirements have no invented dates or estimates. Only the explicitly planned PC Vulkan/DX11 work is assigned to v0.5.0.

## Verification and execution

Source inventory checks, document-link checks and bilingual status checks were performed by the assigned agents and reused. The Terra/medium `project_manager` completed the authorized import on 2026-09-08:

| Check | Result |
| --- | --- |
| Import | 145 created; zero conflicts |
| Live Project readback | 145 items, 145 unique source keys, no unexpected items |
| Existing Issues | All nine URLs match the reviewed manifest |
| Project drafts | All 136 titles and bodies match |
| Mapped fields, including Evidence | Zero mismatches |
| Synchronization registry | 145 keys recorded |
| Subsequent read-only plan | 145 unchanged; zero operations or conflicts |
| Completed historical work | All 77 history/release records are Done; three independent closed reports also Done, for 80 completed items in History |
| Final bilingual mirrors | 112 task rows per language, same status order; 96 unfinished/in-progress occurrences across both |

Execution reports are retained locally in `out/project-management/initial-apply.json`, `initial-idempotence.json` and `verify-report.json`. A Windows file-sharing race during the initial local plan was resolved before remote import by writing the plan once and retrying only atomic local file replacement. Remote mutations were not blindly retried.

No game builds, runtime tests, Git commits/pushes, Issue-state changes or comments were performed for this synchronization. Repository documentation and synchronization files remain local, uncommitted changes; the GitHub Project import is already applied.

The saved primary view was reloaded in GitHub and verified as Roadmap, grouped by Release, using Start date and Target date at Month zoom. Backlog and History are separate saved table views. See [project.json](project.json) for the exact binding.

## Source-key index

The [manifest](items.json) retains full descriptions, requirements, acceptance boundaries and source references. After import, [sync-state.json](sync-state.json) maps these stable keys to remote item IDs.

| Source key | Work item | Workflow | Delivery |
| --- | --- | --- | --- |
| `static-recompilation-foundation` | Establish the static recompilation build | Done | Released |
| `reverse-engineering-tooling` | Add reproducible reverse-engineering tools | Done | Implemented |
| `initial-ppc-vmx-instructions` | Implement missing PPC and VMX translation coverage | Done | Released |
| `runtime-kernel-hle-foundation` | Implement guest memory, threads and kernel services | Done | Released |
| `d3d12-renderer-foundation` | Render the game through the D3D12 backend | Done | Released |
| `xenos-runtime-shader-translation` | Translate Xenos shaders to HLSL and DXIL | Done | Released |
| `controller-keyboard-player-one` | Combine controllers and keyboard into player one | Done | Released |
| `profile-signin-persistence` | Persist guest profiles and complete startup sign-in | Done | Released |
| `runtime-diagnostic-instrumentation` | Add persistent runtime and GPU-stall diagnostics | Done | Released |
| `conditional-return-memset-boundary` | Repair truncated guest memset analysis | Done | Released |
| `gpu-resolve-resource-lifetime` | Move resolves to the GPU and bound renderer caches | Done | Released |
| `guest-timebase-frequency` | Use the guest timebase frequency | Done | Released |
| `character-index-geometry` | Correct character mesh index alignment | Done | Released |
| `postprocess-texture-colors` | Correct postprocessing coordinates and texture colors | Done | Released |
| `physical-memory-render-aliases` | Restore character materials through physical memory aliases | Done | Released |
| `lighting-stencil-depth-clear` | Restore stencil-gated lighting and depth clears | Done | Released |
| `post-battle-whiteout` | Restore post-battle effects across EDRAM formats | Done | Released |
| `title-packed-mip-background` | Restore the animated title background | Done | Released |
| `xma-dialogue-playback` | Restore complete XMA dialogue playback | Done | Released |
| `manual-save-overwrite-reload` | Repair manual save, overwrite and reload | Done | Released |
| `encounter-animation-progression` | Restore random-encounter animation and attack progression | Done | Released |
| `ring-combat-resource` | Restore the localized Ring combat resource | Done | Released |
| `debug-victory-controls` | Add battle victory controls for debugging | Done | Released |
| `debug-map-identification` | Display the current map ID and localized name | Done | Released |
| `debug-same-map-teleport` | Add coordinate bookmarks and same-map POI teleport | Done | Released |
| `optional-save-anywhere` | Add an optional save-anywhere control | Done | Released |
| `guest-critical-section-deadlock` | Repair guest critical-section recursion byte order | Done | Released |
| `window-event-thread` | Keep Windows messages responsive during rendering stalls | Done | Released |
| `partial-depth-clear-layout` | Preserve shadow tiles during partial depth clears | Done | Released |
| `ground-shadow-stencil-pass` | Restore ground shadows after player movement | Done | Released |
| `batched-depth-clear-driver-crash` | Avoid the reproduced startup depth-clear driver crash | Done | Released |
| `map12-poster-depth-bias` | Remove black patches on the Map12 poster | Done | Released |
| `startup-shader-preparation` | Prepare discovered shaders before gameplay | Done | Released |
| `multilingual-settings-display` | Add multilingual settings and graphics controls | Done | Released |
| `portable-game-importer` | Provide a portable graphical game importer | Done | Released |
| `first-launch-setup` | Add first-launch language and graphics setup | Done | Released |
| `windows-portable-release-pipeline` | Automate portable Windows release packaging | Done | Implemented |
| `documentation-and-public-repository-workflow` | Establish documentation and public repository workflows | Done | Implemented |
| `usa-europe-edition-support` | Support the audited USA and Europe edition | Done | Released |
| `automatic-imported-disc-selection` | Automatically select requested imported discs | Done | Released |
| `f1-render-state-export` | Export render diagnostics from the F1 menu | Done | Released |
| `amd-placed-resolve-initialization` | Fix tested AMD black and dark resolve output | Done | Released |
| `unicode-startup-save-paths` | [Preserve Unicode startup and save paths on Windows](https://github.com/freefrank/LostOdysseyRecomp/issues/4) | Done | Released |
| `github-feedback-templates` | Add structured bug and feature request templates | Done | Implemented |
| `cpx-xex-shader-discovery` | Discover CPX and XEX shader resources before gameplay | Done | Released |
| `recorded-pipeline-precreation` | Persist and precreate previously used graphics pipelines | Done | Released |
| `selected-regression-ci` | Separate selected regression tests from release packaging | Done | Implemented |
| `bilingual-debug-menu` | Add independent English and Simplified Chinese Debug UI | Done | Released |
| `smaa-camera-taa-settings` | Add SMAA and experimental camera-based TAA | Done | Released |
| `output-resolution-text-pre-ui-aa` | Improve host text and avoid repeated scene anti-aliasing | Done | Released |
| `spatial-filter-quality` | Add Standard and High spatial scaling quality | Done | Released |
| `automatic-manual-internal-resolution` | Render internally up to 4K with safe graphics preview | Done | Released |
| `taa-camera-reference-rejection` | Prevent false TAA camera-history rejection | Done | Released |
| `saved-30-60-fps-controls` | Add saved 30 and 60 FPS controls with correct pacing | Done | Released |
| `dual-edition-cpx-metadata-index` | Index CPX resource blocks for both supported editions | Done | Released |
| `issue5-missing-indirect-entries` | [Restore missing battle indirect-call entries](https://github.com/freefrank/LostOdysseyRecomp/issues/5) | Done | Released |
| `startup-allocation-error-context` | Report actionable guest allocation failure context | Done | Released |
| `temporal-upscaling-feasibility` | Complete DLSS and FSR feasibility research | Done | Research complete |
| `map3-tire-taa-shadow-alignment` | Fix accepted Map3 tire-shadow flicker with TAA | Done | Released |
| `three-frame-capture-process-log` | Capture three frames with the current process log | Done | Released |
| `battle-terrain-taa-six-paths` | Align six additional battle TAA position paths | Done | Released |
| `four-disc-taa-contract-audit` | Audit the four-disc TAA shader contracts | Done | Research complete |
| `enemy-disappearance-capture-diagnosis` | Localize the enemy-disappearance flicker report | Done | Research complete |
| `background-capture-zip` | Archive completed render captures in the background | Done | Released |
| `default-three-log-retention` | Retain the current default runtime log and two recent logs | Done | Released |
| `council-word-switch-crash` | [Repair the reproduced Uhra Council cutscene crash](https://github.com/freefrank/LostOdysseyRecomp/issues/7) | Done | Released |
| `ppc-scalar-address-branch-correctness` | Correct nine further PPC translation defects | Done | Released |
| `native-crash-runtime-log-sink` | Write native crash context into automatic runtime logs | Done | Released |
| `bilingual-changelog-release-notes` | Generate release notes from the bilingual changelog | Done | Implemented |
| `release-v0-1` | Publish the first portable Windows release | Done | Released |
| `release-v0-2` | Publish dual-edition support in v0.2 | Done | Released |
| `release-v0-2-1` | Publish F1 exports and merged input in v0.2.1 | Done | Released |
| `release-v0-2-2` | Publish AMD resolve and Unicode path fixes in v0.2.2 | Done | Released |
| `release-v0-3-0` | Publish expanded shader and pipeline preparation in v0.3.0 | Done | Released |
| `release-v0-4-0` | Publish internal resolution and temporal AA in v0.4.0 | Done | Released |
| `release-v0-4-1` | Publish the accepted Map3 TAA fix in v0.4.1 | Done | Released |
| `release-v0-4-2` | Publish Council, PPC, TAA and capture repairs in v0.4.2 | Done | Released |
| `pc-backend-selection` | Add PC backend selection and capability checks | Todo | Not started |
| `vulkan-foundation` | Port reusable Vulkan foundations | Todo | Not started |
| `vulkan-shader-compilation` | Support SPIR-V shader compilation | Todo | Not started |
| `backend-cache-isolation` | Isolate caches by backend and compiler identity | Todo | Not started |
| `vulkan-runtime-integration` | Connect the Windows Vulkan renderer and WSI | Todo | Not started |
| `vulkan-temporal-presentation` | Adapt presentation and temporal rendering to Vulkan | Todo | Not started |
| `dx11-feasibility` | [Establish Direct3D 11 feasibility](https://github.com/freefrank/LostOdysseyRecomp/issues/1) | Todo | Not started |
| `dx11-runtime-integration` | Implement the Direct3D 11 backend | Todo | Not started |
| `pc-backend-scene-validation` | Validate PC backends in native-save scenes | Todo | Not started |
| `pc-backend-lifecycle-validation` | Validate backend lifecycle and cache recovery | Todo | Not started |
| `pc-backend-hardware-validation` | Validate PC backends across GPU vendors | Todo | Not started |
| `pc-backend-release-readiness` | Prepare v0.5.0 backend packages and documentation | Todo | Not started |
| `council-reporter-validation` | Confirm the Council repair on reporter systems | Awaiting validation | Awaiting validation |
| `ppc-semantics-gameplay-regression` | Regress PPC semantics across later chapters | Todo | Awaiting validation |
| `battle-terrain-player-acceptance` | Confirm the battle terrain TAA repair with the player | Awaiting validation | Awaiting validation |
| `enemy-disappearance-flicker` | Diagnose and repair enemy disappearance flicker | In Progress | In progress |
| `shader-log-coverage-audit` | Add independent shader logs and cumulative coverage auditing | Todo | Not started |
| `amd-opening-shadow-report` | Resume the earlier AMD opening-battle shadow report | Paused | Deferred |
| `rx9060xt-kaim-shadow-report` | Resume the RX 9060 XT Kaim shadow investigation | Paused | Deferred |
| `attack-animation-stutter` | Diagnose attack-animation stutter | Todo | In progress |
| `text-clarity-and-garbled-glyphs` | Investigate blurry and garbled game text | In Progress | In progress |
| `fire-breath-shadow-regression` | Regress shadows during fire-breath attacks | Todo | Awaiting validation |
| `dlc-import-support` | [Assess and implement DLC import support](https://github.com/freefrank/LostOdysseyRecomp/issues/8) | Todo | Not started |
| `shader-translation-failures` | Resolve the two remaining shader translation failures | Todo | Not started |
| `resource-only-pso-preparation` | Complete resource-only shader and first-use pipeline preparation | Todo | Not started |
| `ime-game-controls` | Prevent IME interference with gameplay controls | Todo | Not started |
| `enemy-encounter-poses` | Repair remaining enemy encounter poses | In Progress | In progress |
| `fire-hit-checker-effects` | Diagnose fire-hit checkerboard effects | Todo | Not started |
| `debug-protagonist-damage` | Add temporary protagonist damage controls | Todo | Not started |
| `save-content-compatibility` | Validate broader save and content compatibility | In Progress | Awaiting validation |
| `chapter-disc-save-regression` | Validate chapter and disc transitions on both editions | Todo | Not started |
| `full-playthrough-regression` | Complete native-save full-playthrough regression | In Progress | Awaiting validation |
| `map13-shadow-regression` | Recheck Map 13 body and environment shadows | Todo | Awaiting validation |
| `crate-destruction-black-effects` | Investigate black crate-destruction effects | Todo | Not started |
| `save-anywhere-acceptance` | Complete save-anywhere gameplay acceptance | In Progress | Awaiting validation |
| `gpu-query-pointer-corruption` | Diagnose GPU query and wait pointer corruption | In Progress | In progress |
| `desktop-display-acceptance` | Complete desktop display and input acceptance | Todo | Awaiting validation |
| `ultrawide-fov-layout` | Support ultrawide output and FOV changes | Todo | Not started |
| `frame-generation-research` | Evaluate frame generation integration | Todo | Not started |
| `hdr-output-tonemapping` | Add HDR output and tone mapping | Todo | Not started |
| `higher-resolution-shadows` | Add higher-resolution shadow rendering | Todo | Not started |
| `ssao-and-reshade-depth` | Evaluate SSAO and clean depth access | Todo | Not started |
| `linux-steamdeck-platform` | Port and validate Linux and Steam Deck | Todo | Not started |
| `screen-space-gi-ssr` | Explore screen-space GI and reflections | Todo | Not started |
| `hardware-raytracing` | Explore hardware ray-traced shadows and reflections | Todo | Not started |
| `texture-replacement-mod-loader` | Support texture replacement and a mod loader | Todo | Not started |
| `switch-platform-alignment` | Resume the Switch platform branch after PC Vulkan | Paused | Deferred |
| `cross-edition-text-patch` | Resume cross-edition text and font patch research | Paused | Deferred |
| `native-object-motion` | Develop native object and skeletal motion vectors | Todo | Not started |
| `temporal-color-exposure` | Define the temporal color and exposure contract | Todo | Not started |
| `vendor-temporal-upscaling` | Integrate vendor temporal upscaling after input validation | Todo | Not started |
| `taa-motion-quality` | Validate TAA motion and history quality across scenes | Todo | Awaiting validation |
| `internal-resolution-acceptance` | Complete internal-resolution visual acceptance | Todo | Awaiting validation |
| `60fps-gameplay-regression` | Extend correct-speed 60 FPS gameplay validation | Todo | Awaiting validation |
| `optional-120fps` | Evaluate the optional 120 FPS candidate | Todo | Awaiting validation |
| `audio-and-language-regression` | Extend audio and edition-language regression | Todo | Awaiting validation |
| `wmv-playback` | Implement and validate WMV movie playback | Todo | Not started |
| `debug-victory-special-battles` | Validate debug victory in special battles | Todo | Awaiting validation |
| `physical-controller-regression` | Extend physical controller and rumble coverage | Todo | Awaiting validation |
| `startup-allocation-reporter-validation` | Confirm Issue 6 recovery on the original machine | Awaiting validation | Awaiting validation |
| `recompiler-remaining-contracts` | Audit remaining PPC translation contracts | Todo | Not started |
| `embsec-segment-research` | Determine the purpose of the embedded code sections | Todo | Not started |
| `title-update-research` | Investigate official title updates and XEXP data | Todo | Not started |
| `edram-aliasing-completeness` | Audit remaining EDRAM aliasing contracts | Todo | Not started |
| `issue9-cpu-temperatures-taa-flicker` | [Investigate high CPU temperatures and TAA texture flicker](https://github.com/freefrank/LostOdysseyRecomp/issues/9) | Todo | Not started |
| `issue-2-report` | [installgame.exe flagged as malicious](https://github.com/freefrank/LostOdysseyRecomp/issues/2) | Done | Deferred |
| `issue-3-report` | [Save and Crash the game](https://github.com/freefrank/LostOdysseyRecomp/issues/3) | Done | Awaiting validation |
| `issue-6-report` | [[Bug]  failed to reserve the 4 GiB guest address space](https://github.com/freefrank/LostOdysseyRecomp/issues/6) | Done | Awaiting validation |
