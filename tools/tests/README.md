# Test suites

Run commands from the repository root. Select checks appropriate to the changed behavior; this entry point does not imply that every suite is required for every change.

```powershell
tools\test.bat --list
tools\test.bat importer
tools\test.bat shaders pipeline
```

`tools/test.bat` forwards to `tools/tests/run.py`. Python is required; native fixture compilation also needs `clang-cl` and the Windows SDK discovered by `tools/setup_windows.bat`. Runtime suites use existing build outputs and never trigger an implicit full build. `--build-dir` defaults to `out/build/release`; it takes the CMake build root, not the directory containing the executable. The runner appends `LostOdysseyRecomp/<target>.exe`. A configured `out/build/windows-clang` can be supplied instead.

For runtime checks, choose the relevant pair below after configuring the build and generating its required game sources; these are alternatives, not a required full sequence:

```powershell
# Storage changes
cmake --build out/build/release --target LoStorageTest
tools\test.bat storage --build-dir out/build/release
```

```powershell
# Input changes
cmake --build out/build/release --target LoHidTest
tools\test.bat hid --build-dir out/build/release
```

```powershell
# Startup-path changes
cmake --build out/build/release --target LostOdysseyRecomp
tools\test.bat startup --build-dir out/build/release
```

| Suite | Scope |
|---|---|
| `importer` | Importer input and extraction checks |
| `shader-index` | Bare-resource and CPX layout-bound direct extraction, including automatic selection between same-name/same-size layouts by FPI identity; required-block/microcode validation, unknown-layout and failed-extraction fallback, duplicate/empty skips, unread-modification boundaries, strict/direct manifest separation, source equivalence, SHA256 vectors and explicit progress units |
| `shaders` | Resource scanning, CPX decoding and bounded dynamic-VS fixtures |
| `pipeline` | Pipeline recipe validation, corruption/truncation and atomic-write fixtures |
| `storage` | Built `LoStorageTest`: disc selection and save round trips |
| `hid` | Built `LoHidTest`: controller and keyboard input |
| `startup` | Built `LostOdysseyRecomp`: synthetic headless Unicode startup-path checks |

Fixture executables go to `out/tests/bin`. Synthetic temporary input is cleaned up automatically. Preserve logs or reports needed to review a result; remove only disposable files created by the current task, never user saves, profiles or unrelated artifacts.

The older `tools/test_shader_index.bat` and `tools/test_shader_preparation.bat` remain forwarding entry points; preparation selects `shaders pipeline`.

CPX fast discovery binds the small FPI digest, FPD name/size and exact extent locations; it deliberately does not verify unread blocks or same-size changes in skipped empty/non-CPX/duplicate content. Required microcode must pass validation before a package's sources are published. Unknown layouts return to full-package reads and identity matching; unknown contents or failed extraction use complete fallback. Fixtures distinguish these paths instead of treating fast discovery as whole-resource integrity validation.

At the renderer entry point, `LO_SHADER_FULL_SCAN=1` disables all three indexes and enables strict scanning on every run. Direct and strict v5 manifests are distinct; strict does not reuse even an earlier strict manifest. Preserve this distinction when invoking `Scan` directly: the renderer passes empty index spans as well as `strict=true`. See the [current discovery contract and measurements](../../docs/notes/shader-preparation.md).

## Recompiler word-switch regression

```powershell
python -B tools/tests/recompiler_switch_test.py --output out/tests/recompiler-switch/run-01
```

Use a new output directory. This independent fixture builds the production instruction decoder and recompiler, emits native functions from synthetic PPC words, then compiles and executes them with Clang `/O2`. It requires no game assets and does not build or launch the game. It is an explicit entry point, not a `tools/test.bat` suite.

The 109 checks cover word-sized bounds, ordinary cases, guest default branches, nonzero register high words, `0xFFFFFFFF + 1`, full-width `-1 + 1`, return values and guest CTR. A mutation control restores the old `.u64` selector: 20 ordinary cases pass, then the carry input must reach a Clang unreachable-sanitizer trap (`0xC000001D`). The trap makes selector undefined behavior observable; it does not claim to reproduce the original game's exact host access violation. Results, generated sources and input hashes remain in the selected output directory. See the [Council investigation](../../docs/notes/issue7-cutscene-crash.md).

## Recompiler semantics regression

```powershell
python -B tools/tests/recompiler_semantics_test.py --output out/tests/recompiler-semantics/run-01
```

Use a new output directory. This explicit entry point needs Python, CMake, Ninja, `clang-cl`, the Windows SDK and the checked-out recompiler dependencies. It builds a fresh production decoder/generator, emits bounded synthetic PPC functions, compiles their actual C++ output with Clang `/O2`, then executes 3,258 native checks. It requires no ROM, GPU, game build or game process and is not a `tools/test.bat` suite.

Checks cover update-address carry and aliases, all `RLWIMI` mask pairs, `SRAW`/`SRAD` results and CA, missing and existing Rc forms, atomic/absolute memory addressing, all `BDNZF` BI positions, BLRL linkage and CTR target alignment. Independent Python expectations exclude undefined division inputs and mask undefined result bits. Memory checks use sparse guest mappings with distinct decoy pages; indirect lookup uses a safe observer. The `skip_lr=true` cases preseed LR and do not establish general LR tracking or game-path reachability. The separately recorded [Council and save/reload regression](../../docs/notes/issue7-cutscene-crash.md#2026-09-07-follow-up-semantics-implementation) is not part of this fixture command.

For development negative controls, append `--baseline-dir <snapshot-directory>` containing the historical `recompiler.cpp` and `ppc_context.h`. That optional run builds and checks both source snapshots. The frozen pre-fix source used for this repair fails 1,533 matching cases while 48 existing Rc controls pass; expected old failures do not count as current success unless the current comparisons all pass. Ordinary use needs no historical snapshot. The output directory retains inputs, generated code, source hashes, observations, summaries and command logs and is never silently overwritten. See the [repair and validation boundaries](../../docs/notes/recompiler-width-audit.md#2026-09-07-implementation-and-regression).

## Regenerating resource metadata

The optional generators accept a repeatable `--additional-root` to merge another edition into one metadata output. Use separate edition roots containing `disc1` through `disc4`; this does not permit mixing editions inside an imported installation. Choose new output filenames and review the generated metadata before replacing built-in headers. Normal builds use the checked-in headers and do not run these commands.

```powershell
python tools/generate_shader_index.py "D:/Games/LO-Asia/game" out/metadata-review/resource_index.inl --additional-root "D:/Games/LO-USA-Europe/game"
python tools/generate_cpx_shader_index.py "D:/Games/LO-Asia/game" out/metadata-review/cpx_resource_index.inl --additional-root "D:/Games/LO-USA-Europe/game" --build-dir out/tools/cpx-index-review
```

The bare-FPD generator requires Python; the CPX wrapper also builds its CPU decoder helper using the native compiler environment described above. These commands read the supplied game resources and generate metadata only. Keep generation separate from the selected `shader-index` fixture and actual-edition source-equivalence validation; producing a header alone does not establish extraction or gameplay correctness.

## Manual presentation and timing checks

The development targets below are excluded from default builds and are not suite names accepted by `tools/test.bat`. Select the target relevant to the change and run its built executable explicitly; the examples are alternatives, not a required full sequence.

| Target | Scope and prerequisites |
|---|---|
| `LoLogCaptureTest` | CPU current-process logger snapshots and default-log retention: flush/copy while open, error/alias preservation, concurrent whole records, current-plus-two numeric timestamp selection, active/undeletable files and later retry. Requires the configured native compiler and fmt dependency; no GPU, guest generation or runtime PCH. Actual startup routing, ZIP contents and rendering require separate runtime checks. |
| `LoCaptureArchiveTest` | Windows real-file background ZIP publication, source cleanup after success, path quoting, existing ZIP/partial conflicts, read/cleanup failures, caller progress and future shutdown waiting. Requires the configured native compiler and system Windows PowerShell/.NET; no GPU, game assets, guest generation or runtime PCH. It does not run F1 or verify game-frame progress. |
| `LoMenuRenderTest` | Windows GDI host-menu rasterization at 720p, 1080p, 4K, 1920×1200 and portrait sizes; dimensions, opacity, text pixels, aspect fit and invalid-size rejection. No guest generation or runtime PCH required. |
| `LoRenderResolutionTest` | CPU Auto/manual internal-size selection, 4K cap, aspect fit, scaled dimensions, target limits and logical texel-coordinate rules. No GPU or game assets; does not establish physical scene rendering. |
| `LoRenderResolutionShaderTest` | Windows production translator/DXC checks for VS/PS normalized and denormalized fetches, signed texel offsets, texture weights and implicit LOD. Requires DXC DLLs discoverable by the built executable; no GPU or game assets. Shader compilation does not establish sampled pixels. |
| `LoRenderResolutionGpuTest` | D3D12 numerical sampling with helpers extracted from production translation: 1×/1.5×/3× physical textures, guest dimensions versus ordinary uploaded textures, normalized/denormalized coordinates, signed offsets, weights and implicit/level-zero samples. Also checks invalid-width allocation rejection followed by a valid allocation, without OOM pressure. Requires GPU/Plume/DXC; uses a synthetic gradient, not a game scene. |
| `LoFramePacerTest` | Pure host deadline calculations, FPS changes, long-stall recovery and scoped guest interval/flag mapping, including the experimental 120 gate. No GPU, guest generation or runtime PCH required; this does not test gameplay speed. |
| `LoTemporalMathTest` | CPU camera-reference math with independent analytic point, translation/yaw, viewport/Y-sign/half-pixel and invalid-input checks. No GPU, guest generation or runtime PCH required. Static round trips and these fixtures do not establish runtime frame association, motion vectors or TAA. |
| `LoTemporalSceneTest` | CPU scene-observation ordering, frame reset, depth-allocation identity, full extents and ambiguity rejection. No GPU, guest generation or runtime PCH required. It does not validate the renderer's actual scene/UI selection. |
| `LoTemporalJitterTest` | CPU production jitter and shadow-reconstruction checks across all 32 phases at 720p/1080p/1440p/4K. Retained constant fixtures independently emulate tire and battle depth/material/lighting position paths, skinned transforms and shadow reconstruction, including negative controls, clip-derived sampling coordinates, preserved Z/W and depth UV, and Off/atlas/identity rejection guards. No GPU, game assets, guest generation or runtime PCH required; this does not establish actual draw coverage or player-visible stability. |
| `LoTemporalHistoryDiagnosticTest` | CPU history-rejection reasons/masks, raster/half-pixel changes and retained projection coordinates/depth on rejection. Checks the production positive-W reference-depth selection with analytic finite/infinite/orthographic cameras and actual float32 VP pairs at 720p/1080p/4K; camera-cut, invalid endpoint and previous-W negative controls remain active. An optional retained camera-pair manifest exercises production continuity and diagnostics together. No GPU, game assets, guest generation or runtime PCH required; this does not establish game output or broad TAA quality. |
| `LoTemporalAATest` | Standalone D3D12 resolve/display and copied-history checks: identity, depth/reactive rejection, neighborhood clamp, alpha, invalid-call output preservation, resource lifetime, external source overwrites and frame/epoch/reset handling. Stable-grid cases separate color/depth jitter coordinates and test camera motion, material edges and static silhouettes. Moving silhouettes use the segment analysis below; optional `--replay` compares independent baseline/candidate histories on captured traces. Requires configured D3D12/Plume dependencies; no guest generation or runtime PCH. It does not enable or accept game TAA. |
| `LoPresentationTest` | D3D12 GPU readback for native identity, letterboxing, FXAA/SMAA edges, flat regions, padding, resize, scaling and checkerboard reductions. Pre-UI scene-AA/UI-composition cases include a repeated-AA negative control. Optional `--capture` mode replays one raw frame through six AA/quality combinations. Requires configured D3D12/Plume dependencies; the target builds independently without reusing the main executable's PCH. No game-scene acceptance is implied. |

```powershell
# Current-process logger snapshot and default retention contracts; no game launch
cmake --build out/build/release --target LoLogCaptureTest
.\out\build\release\LostOdysseyRecomp\LoLogCaptureTest.exe
```

Default retention is called only after a successful default log open: retain the current file plus the two greatest numeric `runtime-<digits>.log` timestamps, regardless of modification time or clock rollback. Busy, read-only or otherwise undeletable older logs can remain until a later launch; cleanup failure must not evict a newer retained file or disable logging. Custom `LO_LOG_FILE` sinks do not trigger rotation, and nonmatching names, directories and capture contents are excluded. The extended Windows fixture passes with both the separate working-tree crash diagnostics and clean-HEAD retention sources; POSIX locking was not executed on this host. Existing snapshot/alias/error and concurrent-line cases remain passing.

```powershell
# Real ZIP files and failure recovery; no game launch
cmake --build out/build/release --target LoCaptureArchiveTest
.\out\build\release\LostOdysseyRecomp\LoCaptureArchiveTest.exe out/tests/capture-archive/run-01
```

The archive fixture requires a new output directory and rejects an existing one. It retains synthetic ZIPs and failure cases for independent inspection. It checks Unicode and shell-special path characters, nonblocking caller progress, publish-before-cleanup, existing ZIP/partial preservation, unreadable sources, a saved ZIP whose source cannot be fully removed, future destruction waiting, and rejection outside a `captures/render-*` child. Independent Python validation of the retained three ZIPs passes CRC checks and confirms the 8 MiB payload byte for byte (`out/capture-background/archive-fixture-validation.json`). This fixture does not validate game rendering during compression, the actual SDL close route, or every timeout/forced-exit case. See [development behavior and runtime scope](../../docs/notes/render-state-capture.md#background-archive-dev).

Separate runtime evidence in `out/capture-background/runtime-validation.json` verifies a 720p title/menu three-frame ZIP, UTF-8 current-log prefix, source removal and rendering progress throughout compression on EXE `5fc9190c…`. The first timing window includes capture stalls; later complete windows establish bounded progress, not zero overhead. Actual SDL-close and timeout trials remain untested. Startup retention is independently checked by five default and one custom-log missing-game launches (expected exit 1), not by treating a fixture pass as proof of startup integration.

```powershell
# Internal-resolution dimensions and sampling contract; select either target as needed
cmake --build out/build/release --target LoRenderResolutionTest LoRenderResolutionShaderTest
.\out\build\release\LostOdysseyRecomp\LoRenderResolutionTest.exe
.\out\build\release\LostOdysseyRecomp\LoRenderResolutionShaderTest.exe
```

```powershell
# Host menu rendering
cmake --build out/build/release --target LoMenuRenderTest
.\out\build\release\LostOdysseyRecomp\LoMenuRenderTest.exe
```

```powershell
# Internal-resolution GPU sampling, separate from actual scene validation
cmake --build out/build/release --target LoRenderResolutionGpuTest
.\out\build\release\LostOdysseyRecomp\LoRenderResolutionGpuTest.exe
```

```powershell
# Host pacing and guest interval mapping
cmake --build out/build/release --target LoFramePacerTest
.\out\build\release\LostOdysseyRecomp\LoFramePacerTest.exe
```

```powershell
# CPU camera-reference math
cmake --build out/build/release --target LoTemporalMathTest
.\out\build\release\LostOdysseyRecomp\LoTemporalMathTest.exe
```

```powershell
# CPU scene-observation contract
cmake --build out/build/release --target LoTemporalSceneTest
.\out\build\release\LostOdysseyRecomp\LoTemporalSceneTest.exe
```

```powershell
# CPU per-draw jitter and shadow-reconstruction contract
cmake --build out/build/release --target LoTemporalJitterTest
.\out\build\release\LostOdysseyRecomp\LoTemporalJitterTest.exe
```

The current battle fixture passes 17,287 checks (`out/battle-taa-fix/LoTemporalJitterTest.log`), covering six newly supported position paths across 32 phases and four sizes. Independent corrected depth/material/lighting position calculations agree exactly. Keeping the material unjittered reproduces up to 0.487771 pixels of layer separation; the corrected physical offset differs from the requested jitter by at most 0.000426 pixels. These are separate arithmetic metrics, not a measured visual improvement. Skinned paths, clip-derived mask sampling, preserved Z/W, observer ambiguity and Off/viewport/depth/camera rejection are also checked.

The retained r2 fixture passed 8,192 checks (`out/v0.4.1-tire-v2/LoTemporalJitterTest.log`). Its three-layer tire cases cover 32 phases, two captured world transforms and three physical sizes, reproducing up to 0.487799 pixels of separation when the material pass remains unjittered. Those cases and the d55 shadow-reconstruction checks remain in the current suite. Runtime draw and visual results are recorded separately in the [shadow investigation](../../docs/notes/shadow-texture-lod.md).

For an isolated actual-game run, set `LO_TEMPORAL_DRAW_LOG_START_FRAME` to a decimal renderer frame before launching the process. It logs selected submitted depth, opaque-material, lighting and shadow draws for at most 32 renderer frames, including original/uploaded VP and shadow constants, phase, extent, sampled-depth identity and application/rejection results. `LO_TEMPORAL_DRAW_LOG_INDEX_COUNT` optionally limits the selected static-mesh passes by index count, retaining at most one shadow and one selected character sample per frame to reduce logging. `LO_TEMPORAL_DRAW_LOG_VS` optionally replaces the default shader selection with one hexadecimal VS hash; combining it with the index-count filter further narrows the output. Normal TAA settings still control jitter; these diagnostics do not enable it.

Alternatively, `LO_TEMPORAL_DRAW_LOG_WITH_RESOLVE_TRACE=1` logs the same selected submitted draws while a directed resolve trace is active, independently of the fixed 32-frame logging window. It does not start the resolve trace or enable jitter. The default selection includes the reviewed battle static/skinned paths. `slot` is the production shader-mapping result; `log_slot` also permits reading the reviewed bank when a comparison baseline rejects that shader. A populated `log_slot` is diagnostic evidence only and cannot authorize a draw.

```powershell
# Example environment for the next isolated test process
$env:LO_TEMPORAL_DRAW_LOG_START_FRAME = "2000"
$env:LO_TEMPORAL_DRAW_LOG_INDEX_COUNT = "336" # Optional tire-mesh filter
```

Remove these environment variables after launching the intended test process. F1 register capture precedes the upload adjustments, so guest constants alone do not prove jitter was applied. Its readback/write stalls can exceed the 250 ms frame-gap threshold and change subsequent TAA/jitter conditions; a three-frame export is not an undisturbed timing comparison. Draw logs establish submitted constants, not the absence of visible flicker; inspect continuous output separately and exclude diagnostic logging from performance comparisons. See [current shadow investigation](../../docs/notes/shadow-texture-lod.md).

The retained battle comparison (`out/battle-taa-fix/runtime-comparison.md` and JSON) requires 32 consecutive raw/triplet frames and timestamp coverage for `COMPLETE_UPLOAD_AND_TRACE_EVIDENCE`. A missing temporal-summary window is `NOT_COVERED`, with history reuse and summary gap flags unknown; it must not become a successful empty check. Observed CPU upload/resolve intervals do not establish GPU frame time or the interval preceding the first upload. Compare per-phase camera banks as well as pixels: separate encounter processes can reach the same phase at different camera/animation times. Current candidate-package TAA and Off runs have complete 32-phase upload/trace evidence, while player acceptance and whole-game coverage remain separate.

The separate candidate-package Map3 tire regression (`out/battle-taa-fix/map3-regression.json`) covers frames 2519–2550, all 352 logged uploads and 64 matching tire triplets. Its 32 temporal summaries confirm readiness/completion/history reuse, no gap flags and no jitter misses. Near/far tire mask and depth regions match the accepted r2 output byte for byte by phase; particle timing prevents a source/TAA color byte-identity claim. Do not transfer the tire run's summary results to the battle windows. The [four-disc static audit](../../docs/notes/taa-coverage-audit.md) is candidate discovery and source analysis, with additional paths awaiting runtime evidence; it is not an exhaustive GPU or visual regression suite. Its proposed runtime-link provenance and draw-coverage reporting are not implemented, and its 408 normalized position groups cannot authorize jitter.

The later f5446–5448 enemy-disappearance capture on the same `0.4.2-dev` package exposes an omitted c230 path, `4bd8985d84983b83`, with matched depth/material descriptors and early armor/weapon black polygons. It has no new production repair, actual-upload log, undisturbed 32-phase comparison or Off control. Its 0.706–0.900-second F1 stalls exceed the frame-gap threshold; passing the existing 17,287 checks does not cover this new path or establish the cause of the visible defect. See the [new diagnostic boundary](../../docs/notes/shadow-texture-lod.md#enemy-death-f5446).

Current `jitter_misses` accounting requires a recognized `temporalSlot >= 0`; unknown VS paths do not increment that counter. Zero misses therefore does not establish complete VS coverage, and the default draw-log hash filter is also a bounded selection. Future coverage reports must retain pass/state distinctions and identify the hash namespace: command-processor `.bin` names in `LO_SHADER_DUMP_DIR` use word-based `HashWords`, while `renderer::GetShader` uses byte-based Fnv1a. Those two identities are not directly comparable.

```powershell
# CPU history-rejection diagnostics; no GPU or game launch
cmake --build out/build/release --target LoTemporalHistoryDiagnosticTest
.\out\build\release\LostOdysseyRecomp\LoTemporalHistoryDiagnosticTest.exe
```

The initial diagnostic-only target passed 60 CPU checks on 2026-09-07 (`out/v0.4.0-followup/quality/temporal-history-diagnostic-cmake.log`). The extended continuity fixture passes 141 checks without arguments. With the retained 512-pair manifest, it passes 3,215 checks and reproduces all 158 original `InvalidWorldW` refusals through the unchanged pixel `Reproject` rule while accepting those pairs through corrected camera-continuity sampling. The quarter-screen threshold and other history identity guards remain unchanged. Evidence: `out/v0.4.0-followup/quality/temporal-camera-pole-fix-cpu.log` and `taa-fixed-golden-cmake.log`; actual game-output validation is separate.

```powershell
# Optional CPU replay when the retained investigation manifest is available
.\out\build\release\LostOdysseyRecomp\LoTemporalHistoryDiagnosticTest.exe out/v0.4.0-followup/quality/taa-camera-golden-512.txt
```

This CPU manifest starts with a row count, followed by each row's frame, original rejection mask, seven viewport fields and 16 current plus 16 previous VP coefficients. It is a retained diagnostic artifact, separate from the GPU color/depth trace replay format below; the default analytic fixture needs no manifest.

```powershell
# Standalone GPU temporal resolve, display and history ownership
cmake --build out/build/release --target LoTemporalAATest
.\out\build\release\LostOdysseyRecomp\LoTemporalAATest.exe out/tests/temporal-aa/run-01
```

The optional `LoTemporalAATest` output-directory argument stores phase PPM images, `phases.csv` and `summary.json`, including separate legacy and stable-grid silhouette cases. Choose a fresh directory for each comparison to preserve earlier evidence. Inspect reference error, edge width and phase behavior alongside variance: lower variance alone does not establish convergence, and diagnostic output does not imply a passing antialiasing result. The [development record](../../docs/notes/v0.4.0-development.md) separates v1/v2 limitations, v3's bounded geometry results and the earlier game reset-identity check.

For an already generated moving fixture, analyze each stationary segment and the revealed-background/old-edge residuals separately. This Python analysis reads retained phase images and writes a new JSON file; it does not launch the GPU or game. Whole-run variance across the movement steps is not a flicker measurement.

```powershell
python tools/tests/temporal_geometry_analysis.py out/tests/temporal-aa/run-01/stable-grid-depth-moving --output out/tests/temporal-aa/run-01/moving-analysis.json
```

Temporal trace replay uses a validated fixed camera and consecutive 1280×720 source/depth traces. The manifest contains 16 hexadecimal VP words followed by decimal renderer-frame numbers; the caller must establish that this camera applies to every listed pair. The new output directory retains separate baseline/candidate color and acceptance-mask outputs. Replay executes GPU work, skips the synthetic suite and does not launch the game. Compare scene-specific responsiveness and stability together; replay completion or greater history rejection is not visual acceptance.

```powershell
.\out\build\release\LostOdysseyRecomp\LoTemporalAATest.exe --replay path\to\trace-directory path\to\manifest.txt out\temporal-replay-new
```

```powershell
# GPU presentation behavior
cmake --build out/build/release --target LoPresentationTest
.\out\build\release\LostOdysseyRecomp\LoPresentationTest.exe
```

An optional output-directory argument to `LoMenuRenderTest` writes PPM images for inspection. Numerical text-pixel checks do not establish readable glyphs or correct live interaction; inspect relevant images and separately verify the integrated game path. Pacing fixtures do not establish correct animation, audio, cutscene or Ring timing at higher FPS.

The scene-AA fixture first checks that FXAA/SMAA changes a scene edge, then adds opaque UI strokes and requires native-size composited presentation to preserve the result. Its negative control deliberately filters the UI a second time and must change the UI mask. These GPU cases passed; actual game captures also show scene processing. The subsequent integrated v7 traces verified final-presentation bypass at their recorded scene/coverage scope; the internal-resolution follow-up now has bounded 720p/1080p/1440p/4K game and menu checks, while new-build user visual acceptance remains pending. See the [follow-up record](../../docs/notes/handoff-v0.4.0-followup.md). The separate `scene_aa_provenance_test.cpp` CPU fixture covers frame/allocation identity and full/partial/clear treatment; its 32 passing checks do not establish actual renderer propagation.

For a controlled comparison of a captured frame, run the already-built presentation test in replay mode:

```powershell
.\out\build\release\LostOdysseyRecomp\LoPresentationTest.exe --capture path\to\inputRGBA.bin 1280 720 out\presentation-replay-new
```

Supply the input's actual width and height. The file must contain exactly `width × height × 4` bytes of packed RGBA8 pixels, with no header or row padding. The output directory must not already exist; the tool preserves earlier evidence by rejecting an existing directory. It writes six 1920×1080 PPM files, from `aa0-quality0.ppm` through `aa2-quality1.ppm`: AA values 0/1/2 mean Off/FXAA/SMAA, and quality values 0/1 mean Standard/bilinear and High/bicubic. Replay mode uses the production presentation path and skips the synthetic suite; it does not launch the game. Inspect the outputs against the same source frame. Success establishes file generation and GPU execution, while visual findings remain specific to the sampled content and do not prove temporal stability, all-language text quality or performance.

## Crash capture diagnostics

`LoCrashCaptureTest` is a Windows target excluded from default builds and is not a suite name accepted by `tools/test.bat`. It compiles the production crash handler and log sink with synthetic guest state; it needs the configured native compiler, fmt, xxHash and DbgHelp, but no GPU, game assets, generated guest library or runtime PCH.

```powershell
# Actual faults in isolated fixture children; no game launch
cmake --build out/build/release --target LoCrashCaptureTest
python tools/tests/crash_capture_test.py --exe out/build/release/LostOdysseyRecomp/LoCrashCaptureTest.exe --output out/tests/crash-capture/run-01
```

Choose a new `--output` directory to retain per-case logs and `results.json`; an existing directory is rejected. Omitting `--output` uses a temporary directory that is removed after the run. The Python runner checks expected fatal exit codes, so a child crash is the intended stimulus rather than a test failure.

Coverage includes main/worker access violations while logger and CRT stream locks are held, absent sinks, a full redirected `stderr` pipe, Unicode log paths, preservation of existing log contents, invalid PPC context, malformed/unreadable guest dump operands, and explicit terminate, uncaught C++ exception and abort routes. It checks that fault identity and readable guest context precede optional symbol work. MSVC routes an uncaught main-thread C++ exception through native `0xE06D7363`; a new worker's default terminate reaches the `SIGABRT` diagnostic path because the terminate handler is per-thread.

These synthetic crashes validate reporting and termination behavior. They do not reproduce a game cutscene, validate external-kill/fail-fast or stack-exhaustion handling, or establish a gameplay fix. See the [Issue #7 investigation](../../docs/notes/issue7-cutscene-crash.md) for evidence and unresolved scene coverage.

## Guest dispatch and startup memory diagnostics

These targets are also excluded from default builds and are invoked directly, not through `tools/test.bat` suite names.

| Target | Scope and prerequisites |
|---|---|
| `LoGuestDispatchTest` | Calls the actual generated `0x82AFA388` and `0x82AFD150` entries through the runtime dispatch table with synthetic guest state; checks command branches, preserved flags and script progression. Requires regenerated guest sources and the runtime dependencies/PCH. It does not launch the game or reproduce the reported battle/save. |
| `LoMemoryFailureTest` | Windows fault injection through the production allocator: static-startup capture, ten failure branches, original OS errors surviving cleanup, view/address/size/offset context and preferred-address fallback. Includes the allocator implementation itself; do not compile it a second time into this target. No game assets or GPU. |
| `LoMemoryAliasTest` | Real OS guest-memory allocation/release and A/C/E alias behavior. No game assets or GPU; success on the local machine does not explain or resolve another machine's allocation failure. |

```powershell
# Generated indirect-call repair, after regenerating the guest library
cmake --build out/build/release --target LoGuestDispatchTest
.\out\build\release\LostOdysseyRecomp\LoGuestDispatchTest.exe
```

```powershell
# Allocation diagnostics and unchanged real alias mapping
cmake --build out/build/release --target LoMemoryFailureTest LoMemoryAliasTest
.\out\build\release\LostOdysseyRecomp\LoMemoryFailureTest.exe
.\out\build\release\LostOdysseyRecomp\LoMemoryAliasTest.exe
```

See the [follow-up record](../../docs/notes/handoff-v0.4.0-followup.md) for current results and unresolved scene/machine coverage. Retain failure-operation and OS-error evidence when investigating a real allocation report; a generic 4 GiB allocation message alone is insufficient to identify the cause.

## CI and build boundaries

Importer, shader and pipeline workflows are independent, path-filtered checks for main pushes, pull requests and manual dispatch. Tags do not repeat these jobs. The runtime workflow is manual only and selects one of `storage`, `hid` or `startup`, with its own explicit generation/build steps. CMake test targets are excluded from the default build; request the needed targets explicitly.

Release packaging accepts a manual `release_tag` and checks out that existing tag. Changing the workflow on main does not change the tagged game sources or require retagging them.

Release packaging is separate from test CI. A build or fixture pass is not gameplay or visual acceptance. Passing checks should not be repeated or expanded without a new change, failure or unresolved concern. Avoid tests for reversible low-impact edits and tests that only mirror implementation details.
