# Roadmap

[简体中文](ROADMAP.zh-CN.md) · [Current status](STATUS.md)

Updated against v0.2 and user acceptance on **2026-09-06**. `[x]` means the stated scope has evidence, not that the entire game is complete. Dated notes retain earlier experiment states; [STATUS.md](STATUS.md) is the current release and validation ledger.

Legend: `[ ]` planned / outstanding · `[~]` in progress · `[x]` validated within the stated scope.

## Pending release: v0.2.1

The candidate adds next-frame F1 render capture with automatic ZIP and combined SDL controller/keyboard input with E/R triggers. Local build and scoped tests passed; hosted release verification and publication are pending. This is not an AMD rendering fix. See [capture](notes/render-state-capture.md) and [input](notes/controller-input.md).

## Latest milestone: v0.2

v0.2 is published: both audited editions, edition-specific text/voice choices, import before setup and automatic installed-disc selection are included. The sets correspond to [Redump 11817: USA, Europe, version 0.0.0.3](https://redump.info/disc/11817) and [Redump 39111: Europe, Asia, version 0.0.0.4](https://redump.info/disc/39111). Import-time XEX hashes remain strict; this is not a complete ISO-to-Redump hash verification. Both original disc managers passed controlled 1 → 2 → 3 → 4 → 1 sequences, not chapter-boundary story progression. See [v0.2 notes](RELEASE-v0.2.md) and [disc selection](notes/disc-selection.md).

v0.1 remains the historical first Windows release with the importer, first-launch setup, five-language interface, shader index/parallel preparation and dialogue, ground-shadow, poster and window-responsiveness repairs. See [v0.1 notes](RELEASE-v0.1.md).

**The reported voice issue is resolved and user-confirmed.** The original and repaired vehicle dialogue sound normal. XMA continuation-packet handling now preserves new frames without changing sample rate or volume: the tested dialogue recovered from 2,129 to 4,062 frames, and its timing slope against the original changed from 0.547 to 1.000. Another 34 multichannel buffers / 4,264 frames decoded without errors. Other scenes and loop subframe boundaries remain routine regression coverage, not an open status for the resolved dialogue report. See [audio evidence](notes/audio-output.md).

## Near-term priorities

1. Diagnose intermittent GPU query/wait failures and long-session stability.
2. Fix fire-hit checker effects and black crate-destruction effects; resolve the two failing resource shaders.
3. Expand chapter-boundary, save/reload, encounter and multilingual regression, including both editions. Automatic path selection is implemented; actual story transitions still need coverage.
4. Complete fullscreen/exclusive, mouse and mixed-DPI acceptance. Map 13 shadow improvement needs controlled regression, not a reopened defect report.

Modern graphics remain later work. **Text-language complement patch research was paused by the user on 2026-09-06**: no finished patch, no runtime code change and no inclusion in v0.2. Voice changes are outside that research scope. See [paused research](notes/text-language-patch.md).

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
- [ ] Map 13 character-surface shadow regression: the user reported improvement and was satisfied with the result. Controlled regression remains outstanding; a shared cause with the poster defect is not proven.
- [ ] Fire-hit black/red checker effects; use original-console references because Xenia also glitches.
- [x] Ring outer ring: visible and changing in tested encounters; releasing RT produced Good and 101 damage in a recorded test. The user confirmed normal controller behavior. This closes the reported path, not every battle scenario. [Ring evidence](notes/battle-ring-resource.md).
- [ ] Black crate-destruction effects in the second map.
- [x] Debug map ID/localized name: unknown-title state, Hypocenter/Gorge and the map 2-to-3 transition tested. [Map information](notes/debug-map-info.md).
- [~] Save-anywhere: backend, non-save-point slot 03 save/restart/reload, restored permissions, native save points and camp transition tested. The reorganized F1 toggle has isolated UI size/callback tests; broader desktop/gameplay acceptance remains. [Save-anywhere](notes/save-anywhere.md).
- [~] Camp/window hangs: big-endian critical-section ownership/recursion repaired with four-thread and selected-route tests; window-message-pump changes are included. Separate GPU query/wait pointer corruption remains unresolved. [Critical sections](notes/critical-section-endian.md) · [GPU waits](notes/third-map-hang.md).
- [x] Voice issue resolved and user-confirmed: output and vehicle-dialogue timing repairs are published. Camp → vehicle sequence → city-gate control passed without the former stable decoder errors. Other scenes remain routine audio regression coverage; long-session stability is tracked separately. [Audio](notes/audio-output.md).

## Phase 4: Modernization

Compatibility and stability come first. Future graphics features below have no committed release date.

- [x] Parallel preparation with logical CPU threads minus one, minimum one: 15 workers on the test PC reduced 2,000-shader preparation from 53.4 to 6.7 seconds; successful outputs are byte-identical.
- [x] Startup preparation for known shaders, persistent cache and progress UI. Warm-cache reuse and damaged-DXIL recovery passed for the earlier 184-microcode set. [Preparation](notes/shader-preparation.md).
- [x] Discover 2,000 microcodes across four discs; 1,998 compile successfully. Progress display, cache reuse and an isolated Map 12 run passed.
- [x] Use the built-in 52-file shader index for supported resources, with scanning fallback for unfamiliar layouts. Local discovery dropped from 36.2 to 1.1 seconds; discovery and compilation timings are separate measurements.
- [ ] Resolve two microcode compilation failures, uncovered containers/runtime variants and actual graphics-pipeline (PSO) precreation. First-use stalls in unseen scenes remain possible.
- [x] Replace settings while retaining original game options; add five-language UI, language choice, FXAA and aspect-preserving output scaling. [Settings](notes/settings-menu.md).
- [ ] Complete desktop acceptance for fullscreen/exclusive modes, mouse and mixed DPI.
- [ ] Add more anti-aliasing options beyond FXAA, including investigation of temporal AA.
- [ ] Support higher internal rendering resolutions, render scaling and widescreen with correct UI layout.
- [ ] Investigate modern upscaling, including DLSS/FSR. Existing output scaling is not temporal upscaling; DLSS is a disabled placeholder in v0.2.
- [ ] Investigate frame generation (FG); the v0.2 option is a disabled placeholder.
- [ ] Unlock frame rates and identify logic fixed at 30 fps.
- [ ] Add HDR output and tone mapping.
- [ ] Add higher-resolution shadows.
- [ ] Investigate SSAO and expose a clean depth buffer for ReShade.
- [ ] Implement and validate Vulkan gameplay and Steam Deck support.
- [x] Ship the game importer and first-run setup with the hosted Windows v0.1 release. [Installation](INSTALLING.md) · [Packaging](notes/release-packaging.md).

## Phase 5: Optional exploration

- [ ] Screen-space GI / SSR.
- [ ] Hardware ray-traced shadows and reflections.
- [ ] High-resolution texture replacement and a mod loader.
