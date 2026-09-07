# Roadmap

[简体中文](ROADMAP.zh-CN.md) · [Current status](STATUS.md)

Updated against v0.4.0 publication on **2026-09-07**. `[x]` means the stated scope has evidence, not that the entire game is complete. Dated notes retain earlier experiment states; [STATUS.md](STATUS.md) is the current release and validation ledger.

Legend: `[ ]` planned / outstanding · `[~]` in progress · `[x]` validated within the stated scope.

## Published milestone and remaining coverage: v0.4.0

[v0.4.0](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.4.0) published at 2026-09-07 21:56:34 UTC from `40362d78`; release CI and official-package checks passed. Both audited editions passed isolated official-package Map2 startup at Auto 1080p/TAA with verified bundled compiler libraries. This is a bounded static scene check, not full-game or new player visual acceptance. Post-v0.3.0 test tooling is included. See [current release evidence](STATUS.md).

The earlier v0.4.0 checkpoint completed the recorded implementation and bounded validation for: text/UI, Debug, spatial AA/scaling, experimental TAA, sampled 60-FPS behavior and DLSS/FSR feasibility research. Wider gameplay remains regression coverage; no new user acceptance is recorded. See the [implementation checkpoint and acceptance criteria](notes/v0.4.0-development.md). The implementation is included in v0.4.0; the validation and acceptance limits below remain.

1. [x] Improve text clarity through output-resolution host Settings and supported pre-UI scene AA. Same-real-frame Navigation replays, production GPU controls and v7 actual final-AA skips verify removal of repeated AA from later UI; actual seven-row Graphics layout also passed. That v7 build kept guest UI at 720p; unsupported/CPU fallback and broader glyph/language quality are separate regression coverage.
2. [x] Add an independent English / Simplified Chinese Debug menu: fixtures, startup and actual Map2 tutorial-state EN → SC → EN checks passed, including dynamic map/no-battle/waiting-for-control text and restored language. Capture render state remains first. Other battle/status/layout variants are regression coverage; no new user acceptance is claimed.
3. [x] Add SMAA 1x: the upstream HIGH three-pass/LUT path passes selected GPU and actual pre-UI processing checks. Actual Graphics selection, Keep and same-process reopening passed, including switching SMAA to TAA. Further reference/scene comparisons are regression work.
4. [x] Add experimental camera-based TAA: normal AA3 settings, scene depth, jitter, stable-grid history/reset and final-AA bypass passed bounded v7 static/moving validation. Actual menu selection/Keep/reopening also passed. Unsupported paths use SMAA; native object motion is future work and broader scene quality remains regression coverage.
5. [x] Match actual output automatically with Standard/bilinear and High/bicubic quality: selected GPU/configuration and real-frame replay checks passed. Actual seven-row Graphics verified preview without disk changes, 15-second High rollback, Standard Keep and same-process close/reopen. This v7 spatial-filter change kept guest size fixed. True internal resolution is now an explicit follow-up below; process-restart/display coverage remains regression work.
6. [x] Implement 60 FPS with bounded correct-speed validation: sampled movement/dialogue, normal Apply/Keep/reopening and the 59.79827-FPS static camp passed. Natural 30/60 battle samples also show matching Ring timer/progress and long-hold timeout sequences; the observed 1.399/1.415-second phase boundary difference is within sampling granularity. Precise release/Perfect, damage and broader gameplay remain regression coverage; this is not whole-game locked 60. The optional unvalidated 120 candidate may be deferred.
7. [x] Complete DLSS/FSR feasibility research: official contracts, actual scene/depth/camera/jitter evidence, API/hardware/distribution risks and the staged implementation route are documented. Native object/skeletal motion, measured exposure/color-space semantics, broader scene coverage and vendor backends remain future engineering.

The user reports a closed-source implementation with correct speed at higher frame rates; its name and exact frame-rate coverage are unknown. It is a feasibility lead. Another implementation's limitation above 60 FPS does not rule out 120 FPS, but the latest user feedback permits a 60-FPS-only delivery if 120 FPS is difficult. Existing shadow investigations and unrelated backlog retain their prior status outside this new scope.

### Follow-up requested on 2026-09-07

The user reported no visible upscale improvement in any scene, following the earlier TAA feedback, and explicitly requested internal resolution up to 4K, preferably following output. The earlier execution checks remain valid for their builds; perceptible improvement in the new build is awaiting user review. The recorded development builds use `c548b48` as their base and include the follow-up changes; the subsequent implementation is included in v0.4.0. See the [follow-up handoff](notes/handoff-v0.4.0-followup.md).

1. [x] Automatically match combined Asian/USA-Europe bare-FPD and CPX metadata to imported resource identities, without a region option. Both editions index 52 resource files with zero CPX fallback and preserve all 20,686 source names/hashes against retained strict full scans; Disc 2 entry reuses the sibling-disc manifest. Existing profiles, unknown/failed extraction fallback, unread-content boundaries and explicit full-scan mode remain preserved. Fixtures and main build passed. Resource reads are 142,093,208/142,289,816 bytes; the observed 12.537/12.631 s are not a controlled speedup, startup or FPS result. The new package passed both-edition Map2 Auto 1080p checks with verified bundled libraries and source hashes; user data was preserved. Original battle/full-game acceptance remains outside this scope; previous single-edition timings remain [historical evidence](notes/shader-preparation.md).
2. [x] Add real internal-resolution Auto/output matching up to 3840×2160 plus manual 720p/1080p/1440p/2160p, independent of output and spatial-filter quality. The eight-row five-language settings implementation passed 212 CPU/menu checks and ten GDI renders; 37 dimension checks and 16 translated-shader DXC programs passed. GPU sampling and invalid-allocation recovery checks passed. At fixed 1080p output/AA3, Map2 still/moving runs verified native 720p, Auto 1080p and 4K source/depth/TAA inputs and observed history reuse without CPU/allocation fallback; inspected thin geometry and ground detail improved. Build-v3 Auto 4K output, legible Chinese eight-row Graphics, preview without disk changes, 15.017-second rollback, 1440p Keep and actual 1440p scene return/same-process reopening passed. The earlier v3 Auto 4K window reused history in 108/256 logged records; the identified camera-reference error is addressed below. New-build user visual acceptance and broader scenes/hardware remain unverified.
3. [x] Repair Issue #5's missing indirect-call entry at `0x82AFA388` and the analogous `0x82AFD150`. Four-disc boundary audit, regeneration, integrated build and 56 actual generated-dispatch/branch/script-cursor checks passed. The reported USA, Europe save was not supplied; original battle reproduction and user acceptance remain pending.
4. [x] Add diagnostic coverage for Issue #6 startup allocation failures: original OS error, failing operation, mapping and memory context. 186 injected checks and real alias allocation/release passed without changing the mapping contract. Root cause and recovery on the reporting machine remain unresolved; the supplied log does not establish shared causation with #5.

5. [x] Correct the diagnosed TAA camera-history false rejection: the fixed .001 probe can fall at the projection infinity boundary, even with unchanged VP. Positive-W interval selection preserves pixel reprojection, validity thresholds, the quarter-screen limit and other history guards. Default CPU checks total 141; retained 512-pair replay passes 3,215 checks and reproduces all 158 original refusals. Integrated build and temporal GPU fixture passed. Native-720p and Auto 4K Map2 runs each recorded 256/256 history reuses, including 140/157 after input release, with no rejection gates or jitter misses and matching source/TAA/depth sizes. The same 24 active engine-tick input produced different final positions; this is not an identical-trajectory or FPS comparison. Broad TAA motion quality and user visual acceptance remain separate.

## Published milestone: v0.3.0

- [x] Implement and locally validate CPX/FPI discovery, verified XEX sources, bounded VS variants and previously recorded pipeline preparation; see [shader evidence](notes/shader-preparation.md).
- [x] v0.3.0 published at 2026-09-07 06:55:19 UTC from unchanged tag `fba7ae4`; release CI 34091301175, package/installer checks and a short official first-battle smoke passed. No new player visual/stutter acceptance is implied; post-tag test tooling remains Unreleased.

## Previous milestone: v0.2.2

[v0.2.2 is published](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.2.2), including the accepted AMD resolve fix and Unicode startup/save paths. CI, official-package checks, eight startup-path cases and a Chinese-working-directory Map 12 smoke run passed. The complete Issue #4 gameplay crash remains unreproduced; see [release notes](notes/release-0.2.2.md).

## Previous milestone: v0.2.1

[v0.2.1 is published](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.2.1), adding next-frame F1 render capture with automatic ZIP and combined SDL controller/keyboard input with E/R triggers. Hosted CI, including LoHidTest, and official-package hash, installer and isolated rendering/capture checks passed. Physical controller and gameplay switching coverage remains outstanding. This is not an AMD rendering fix. See [capture](notes/render-state-capture.md) and [input](notes/controller-input.md).

## Previous milestone: v0.2

v0.2 is published: both audited editions, edition-specific text/voice choices, import before setup and automatic installed-disc selection are included. The sets correspond to [Redump 11817: USA, Europe, version 0.0.0.3](https://redump.info/disc/11817) and [Redump 39111: Europe, Asia, version 0.0.0.4](https://redump.info/disc/39111). Import-time XEX hashes remain strict; this is not a complete ISO-to-Redump hash verification. Both original disc managers passed controlled 1 → 2 → 3 → 4 → 1 sequences, not chapter-boundary story progression. See [v0.2 notes](RELEASE-v0.2.md) and [disc selection](notes/disc-selection.md).

v0.1 remains the historical first Windows release with the importer, first-launch setup, five-language interface, shader index/parallel preparation and dialogue, ground-shadow, poster and window-responsiveness repairs. See [v0.1 notes](RELEASE-v0.1.md).

**The reported voice issue is resolved and user-confirmed.** The original and repaired vehicle dialogue sound normal. XMA continuation-packet handling now preserves new frames without changing sample rate or volume: the tested dialogue recovered from 2,129 to 4,062 frames, and its timing slope against the original changed from 0.547 to 1.000. Another 34 multichannel buffers / 4,264 frames decoded without errors. Other scenes and loop subframe boundaries remain routine regression coverage, not an open status for the resolved dialogue report. See [audio evidence](notes/audio-output.md).

## Near-term priorities

The following is the retained compatibility and regression backlog. The v0.4.0 section above defines the new development scope.

1. [ ] Kaim's opening-battle body-shadow investigation remains suspended after the requested v0.4.0 recheck and final one-minute limit on 2026-09-07. Official-package tests on Radeon 8060S did not confirm the RX 9060 XT report; no root cause or fix is established. Resume with reporter video/log/settings and normal/abnormal captures. Map 13 remains unresolved, and a relationship is unknown. [Evidence and limits](notes/kaim-body-shadow-v040.md).
2. [~] Collect player acceptance for the expanded shader and recorded-pipeline preparation shipped in v0.3.0. Local tests establish source coverage and real draw reuse, not complete stutter elimination or unseen first-use PSO coverage.
3. [~] Investigate blurry or garbled text against original-console and Xenia references; work in progress. This is separate from the paused text-language complement patch.
4. [ ] Regress shadow flicker during the fire-breath phase. The same player reported that it appeared to disappear; this is one observation, not a comprehensive fix claim. Fire-hit checker effects remain a separate open issue.
5. [ ] Assess DLC import as a later request; no delivery date is committed.

Existing work remains queued: intermittent GPU query/wait failures and long-session stability; fire-hit checker/crate effects and the two failing resource shaders; chapter-boundary, save/reload, encounter and multilingual regression for both editions; fullscreen/exclusive, mouse and mixed-DPI acceptance. Map 13 body/environment-shadow flicker remains open; the accepted ground-shadow fix remains regression coverage.

## v0.2.2 delivery — 2026-09-06 local

- [x] AMD resolve fix `43ce0e53` accepted by the user and merged as `ab0d038`; NVIDIA RTX 5080 targeted checks and manual acceptance passed.
- [x] Unicode startup/save-path fix `7dbb668` merged as `04dd7d0`; eight startup-path cases and eight storage runs passed. The complete Issue #4 gameplay crash remains unreproduced; path tests do not establish in-game acceptance.
- [x] `v0.2.2` was published on 2026-09-07 at 03:11:10 UTC; tag `f03efe370d444db1a8a9c1213c240da697f58504` is on both remotes and CI `34077788392` passed. Official-package CRC/44 manifest hashes, installer self-test, eight startup-path cases and a Chinese-working-directory Map 12 smoke run passed.

The requested AA, scaling and frame-rate work now belongs to v0.4.0. **Text-language complement patch research was paused by the user on 2026-09-06**: no finished patch, no runtime code change and no inclusion in v0.2. Voice changes are outside that research scope. See [paused research](notes/text-language-patch.md).

## Phase 0: Preparation

- [x] Establish the repository skeleton and submodules.
- [x] Extract all four discs with `tools/god_extract.py` into the ignored `LostOdysseyRecompLib/private/disc1..4` directories.
- [x] Identify both supported XEX sets: Europe, Asia version 4 and USA, Europe version 3; title updates remain outside the audited sets. Match executable details and hashes rather than the region label alone; see [XEX evidence](notes/xex.md).
- [x] Generate the initial 841 jump tables with XenonAnalyse; install Ghidra 12.1.3 and XEXLoaderWV and import `default.xex` headlessly.
- [x] Run Xenia Canary through the opening battle and capture matching-camera references. See [comparison](notes/xenia-render-comparison.md).

## Phase 1: Produce compilable output

- [x] Populate the TOML configuration: save/restore addresses, invalid instructions, 80 explicit function boundaries, and setjmp/longjmp.
- [x] Generate XenonRecomp output with zero errors and zero unimplemented instructions in run 15; patches added 30 instructions.
- [x] Compile all 251 generated C++ files with clang-cl 22 and `/O2`, with zero errors. MSVC is not a supported compiler target.
- [ ] Translate every shader with XenosRecomp and record unsupported instructions; two known resource shaders still fail preparation.

## Phase 2: Reach the main menu

- [x] Restore the animated title background by correcting the level-0 packed-mip offset for small textures. [Evidence](notes/title-packed-mips.md).
- [x] Implement kernel HLE for threads, synchronization, memory, filesystem and XAM user profiles. The title loop runs in tested builds; its 60-fps swap rate is not an unlocked gameplay claim.
- [x] Render through plume and D3D12; the Press START title screen works. [GPU notes](notes/gpu.md).
- [x] Implement audio through Xenia FFmpeg XMAFRAMES decoding and SDL 48 kHz stereo output. Loop-end handling, cursor ownership, file-read races, command-register handling and dialogue packet skips have scoped fixes. The shared-handle test improved from 932 errors in 8,000 reads to zero; 32 consecutive MMIO kick/clear operations passed. The user has accepted the voice repair; other voices, long tracks and loop subframes remain routine regression coverage. Opening WMV playback remains unresolved. [Audio](notes/audio-output.md).
- [x] Support SDL controllers with keyboard fallback.
- [x] Disable controller rumble by default during development; `LO_CONTROLLER_RUMBLE=1` enables it.
- [x] Reach the title, main menu and settings after login and storage checks.

## Phase 3: Work toward a complete playthrough

- [~] Later encounters: repair opening-resource leftovers that caused Kaim's T-pose and stalled attacks. Normal attacks, counters and natural victories passed in independent tests; enemy poses and flicker still need investigation. [Encounter animation](notes/encounter-animation.md).
- [x] Provide the Windows debug victory path through the game's victory stage and result initialization; user-tested battle skipping works.
- [ ] Diagnose fire-hit rendering glitches with original-console references; Xenia also shows issues and is not a sufficient visual target by itself.
- [ ] Add temporary protagonist attack/damage adjustments to the debug menu, reversible and excluded from saves. [Requirements](debug-menu-requirements.md).
- [~] Save system: asynchronous completion, thumbnail ABI, persistent enumeration, NT writes and CREATE_ALWAYS semantics repaired. Manual saves, overwrite and independent-process reload passed. Broader compatibility remains unverified. [Storage](notes/save-storage.md).
- [x] Automatically select imported discs and reload their original indexes; both editions passed storage routing and original-manager switching tests. [Evidence](notes/disc-selection.md).
- [ ] Validate real chapter-boundary story transitions and subsequent save/reload on both editions; controlled manager tests do not establish full progression.
- [ ] Validate cutscenes, battles, A Thousand Years of Dreams and the world map individually.
- [~] Follow the walkthrough using isolated saves: Hypocenter wreckage/Ram and Ring tutorial, natural victories, early Wasteland encounters, Gorge camp, vehicle sequence and city-gate control have scoped evidence. The user reached Map 13. This is not a complete playthrough. [Walkthrough](notes/walkthrough-testing.md) · [Ring](notes/battle-ring-resource.md).
- [x] Fix black character silhouettes in the opening battle through correct physical-address aliasing: A/C share memory; E has a one-page offset. Windows/Linux alias tests and scene checks passed. [Evidence](notes/physical-alias-rendering.md).
- [x] Fix opening-battle broken geometry, later outline offsets and missing texture gamma through 16-bit index alignment, resolve dimensions and texture decoding. D3D12 numeric tests and scene comparisons passed. [Evidence](notes/rendering-index-and-resolve.md).
- [x] Restore opening-battle highlights through stencil support, D3D12 reference-value handling and D24 clear corrections. [Evidence](notes/lighting-stencil-depth-clear.md).
- [x] Fix whiteout in the real-time sequence after the first battle by converting EDRAM before drawing; the scene and muzzle fire return and the heavy-tank battle starts. [Evidence](notes/post-battle-whiteout.md).
- [ ] Resolve remaining crashes and complete a playthrough. The earlier RPBattle__Scene memset-truncation crash is fixed, but this does not close other stability issues. [Recompilation](notes/recomp.md) · [Handoff](notes/handoff.md).
- [x] Add same-map character teleport, coordinate bookmarks, return and axis adjustments. Native teleport/return, resumed walking and rejection during game menus tested. [Evidence](notes/debug-teleport.md).
- [x] Enumerate loaded-map POIs for saves, exits, mechanisms and pickups. Hypocenter save-point/mechanism destinations, exit proximity and rejection of old IDs tested. [Evidence](notes/debug-teleport.md).

## Current feedback and regression work

- [x] Ground projections: corrected stale guest pixel-shader use in mode-5 stencil draws; camp movement and targeted GPU tests passed, and the user reports projections are basically fixed. More maps/encounters remain regression targets. [Shadows](notes/shadow-texture-lod.md).
- [x] Map 12 poster black patches: polygon-offset repair passed a 300-frame A/B and shallow-depth occlusion regression. Broader near-plane/map coverage remains. [Poster evidence](notes/map12-poster-depth.md).
- [ ] Map 13 body/environment-shadow flicker remains open after the later user report; accepted ground projections are a separate repair. A shared cause with the poster defect is not proven.
- [ ] Fire-hit black/red checker effects; use original-console references because Xenia also glitches.
- [x] Ring outer ring: visible and changing in tested encounters; releasing RT produced Good and 101 damage in a recorded test. The user confirmed normal controller behavior. This closes the reported path, not every battle scenario. [Ring evidence](notes/battle-ring-resource.md).
- [ ] Black crate-destruction effects in the second map.
- [x] Debug map ID/localized name: unknown-title state, Hypocenter/Gorge and the map 2-to-3 transition tested. [Map information](notes/debug-map-info.md).
- [~] Save-anywhere: backend, non-save-point slot 03 save/restart/reload, restored permissions, native save points and camp transition tested. The reorganized F1 toggle has isolated UI size/callback tests; broader desktop/gameplay acceptance remains. [Save-anywhere](notes/save-anywhere.md).
- [~] Camp/window hangs: big-endian critical-section ownership/recursion repaired with four-thread and selected-route tests; window-message-pump changes are included. Separate GPU query/wait pointer corruption remains unresolved. [Critical sections](notes/critical-section-endian.md) · [GPU waits](notes/third-map-hang.md).
- [x] Voice issue resolved and user-confirmed: output and vehicle-dialogue timing repairs are published. Camp → vehicle sequence → city-gate control passed without the former stable decoder errors. Other scenes remain routine audio regression coverage; long-session stability is tracked separately. [Audio](notes/audio-output.md).

## Phase 4: Modernization

Compatibility and stability remain validation requirements. SMAA/TAA, resolution-aware scaling and stable, correct-speed 60 FPS are v0.4.0 targets; 120 FPS is optional and may be deferred. DLSS/FSR input feasibility remains research. Other features below remain later work; no release date is committed.

- [x] Parallel preparation with logical CPU threads minus one, minimum one: 15 workers on the test PC reduced 2,000-shader preparation from 53.4 to 6.7 seconds; successful outputs are byte-identical.
- [x] Startup preparation for known shaders, persistent cache and progress UI. Warm-cache reuse and damaged-DXIL recovery passed for the earlier 184-microcode set. [Preparation](notes/shader-preparation.md).
- [x] Discover 2,000 microcodes across four discs; 1,998 compile successfully. Progress display, cache reuse and an isolated Map 12 run passed.
- [x] Use the built-in 52-file shader index for supported resources, with scanning fallback for unfamiliar layouts. Local discovery dropped from 36.2 to 1.1 seconds; discovery and compilation timings are separate measurements.
- [ ] Resolve two microcode compilation failures, uncovered containers/runtime variants and actual graphics-pipeline (PSO) precreation. First-use stalls in unseen scenes remain possible.
- [x] Replace settings while retaining original game options; add five-language UI, language choice, FXAA and aspect-preserving output scaling. [Settings](notes/settings-menu.md).
- [ ] Complete desktop acceptance for fullscreen/exclusive modes, mouse and mixed DPI.
- [x] SMAA 1x and experimental camera-based TAA are implemented and verified at the bounded actual-game and menu selection/Keep/reopen scope above. Broader TAA quality remains regression coverage.
- [x] Add automatic output-resolution matching and Standard/High quality for v0.4.0: selected checks and actual seven-row Graphics preview/rollback/Keep/same-process reopening passed. True Auto/manual internal resolution up to 4K was subsequently implemented and validated at the follow-up scope above.
- [ ] Support widescreen/FOV changes with correct UI layout as later work.
- [x] Complete DLSS/FSR temporal-input feasibility research for v0.4.0, including actual evidence, missing inputs and future integration gates. No vendor backend is implemented; the published v0.3.0 DLSS placeholder is removed in v0.4.0 Settings.
- [ ] Investigate frame generation (FG) as later work; its disabled published control is removed in v0.4.0 Settings.
- [x] Implement 60 FPS and complete the bounded movement/dialogue, UI/camp and Ring core-timing/timeout checks for v0.4.0. Precise Ring release and broader gameplay remain regression coverage; whole-game locked 60 and the optional 120 candidate are not claimed.
- [ ] Add HDR output and tone mapping.
- [ ] Add higher-resolution shadows.
- [ ] Investigate SSAO and expose a clean depth buffer for ReShade.
- [ ] Implement and validate Vulkan gameplay and Steam Deck support.
- [x] Ship the game importer and first-run setup with the hosted Windows v0.1 release. [Installation](INSTALLING.md) · [Packaging](notes/release-packaging.md).

## Phase 5: Optional exploration

- [ ] Screen-space GI / SSR.
- [ ] Hardware ray-traced shadows and reflections.
- [ ] High-resolution texture replacement and a mod loader.
