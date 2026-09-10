# Test suites

Run commands from the repository root. Select checks appropriate to the changed behavior; this entry point does not imply that every suite is required for every change.

```powershell
tools\test.bat --list
tools\test.bat importer
tools\test.bat shaders pipeline
```

`tools/test.bat` forwards to `tools/tests/run.py`. Python is required; native fixture compilation also needs `clang-cl` and the Windows SDK discovered by `tools/setup_windows.bat`. Runtime suites use existing build outputs and never trigger an implicit full build. `--build-dir` defaults to `out/build/release`; it takes the CMake build root, not the directory containing the executable. The runner appends `LostOdysseyRecomp/<target>.exe`. A configured `out/build/windows-clang` can be supplied instead.

## Installer window checks

`python -B tools/tests/test_installer_ui.py` selects the new Windows/Tk window checks only. The recorded nine passing cases cover resize hit targets, narrow layout/scrolling, long paths, cancel/retry/close, native frame styles, unchanged polling, DPI metrics and non-activating minimize. Two affected existing controller checks also passed; no importer backend suite was repeated. Subsequent copy reduction used real normal/minimum-size renders, without repeating these checks. Evidence and limitations: [desktop UI validation](../../docs/notes/desktop-ui-modernization.md), with the local report in `out/v0.5.0/ui-modernization/installer/REPORT.md`. These checks do not launch the game or establish physical multi-monitor interaction.

## Updater window checks

The focused updater fixture is recorded in `out/v0.5.0/ui-modernization/updater/fixture.log` and `manifest.json`. It passed native window/control creation, native styles, known and unknown progress, unchanged-value redraw caching, verification cancellation boundaries, ready-state controls, minimize, teardown, Chinese narrow layout, download close cancellation and late-progress handling. Final normal, unknown-total and narrow Chinese renders were refreshed separately and reviewed; those captures do not repeat the functional fixture. No game, network download, package transaction or updater helper was run, and physical monitor moves and live user-desktop gestures remain untested.

The separate `LoUpdaterTest --version-policy` run from `out/build/windows-clang/LostOdysseyRecomp/LoUpdaterTest.exe` passed 14/14 cases. It covers numeric ordering, differing or identical suffixes, `v` prefixes and ignored build metadata. It is a focused policy check only; no network request, download, package transaction, helper, game launch or publication check was performed. Existing clients require a build containing the updated updater code.

The archive staging check uses the same `shutil.make_archive` ZIP writer as the release packager. After building `LoUpdaterTest`, run:

```powershell
python -B tools/tests/updater_archive_test.py out/build/windows-clang/LostOdysseyRecomp/LoUpdaterTest.exe
```

The recorded run passed 12/12 cases in `out/updater-fix/archive-test.log`. It verifies release-style staging with an explicit root directory entry, implicit-root and root-last ordering, and rejection of multiple roots, top-level files, absolute or parent roots, traversal, duplicate and unlisted payloads, SHA256 mismatch and a missing root manifest. This is archive staging coverage; it does not establish a network update, package transaction or live native dialog.

`LoUpdaterStandaloneTest` passed 43 checks in the local Release /MT host build. It covers installed-manifest/EXE validation, no-argument and malformed-argument routing, injected check results, Unicode/unrelated cwd, exact-path fake game processes, and the real helper replacing synthetic files and launching a WIN32 probe after the standalone parent exits. No real game or public download is involved. Evidence: `out/standalone-host/REPORT.md` and `fixture.log`.

`python -B tools/tests/package_suffix_version_test.py` passed 7 focused cases covering full suffix identity, invalid suffixes, exact tag/source/commit matching and retained clean-checkout/build guards. Earlier updater fixtures were reused.

## Original menu asset checks

The selected native menu-asset implementation is documented in `out/v0.5.0/final-preparation/menu-assets/INTEGRATION-REPORT.md`. Its bounded run-05/run-06 evidence covers the selected English/Simplified Chinese asset paths, three decoded-output byte comparisons, cache/malformed/LZO negative cases and the final whole-string coverage result: 41 original-font whole-string draw calls and 2 GDI whole-string fallback draws. The older source-0.4.22 run had two Simplified Chinese strings containing `锯` and `帧` on GDI fallback; source 0.4.23 changes those labels to `反走样` and `画面速率`. No game, CI, production package or user visual acceptance is established by these checks.

The 0.4.23 Settings replacement-flow fixture is recorded in `out/v0.5.0/settings-replacement-flow/{fixture-result.json,fixture.log}`. It includes the actual `menu.cpp` with only external boundaries replaced and passed save/apply, failure recovery, Now/Later restart and Back-path checks; it does not prove actual guest runtime. The reviewed `graphics-aa.png` and `graphics-rate.png` previews show the corrected Simplified Chinese labels `反走样` and `画面速率`.

Use the existing `LoMenuRenderTest` target for host raster checks; menu-asset decoding is part of the affected native fixture and is not an implicit default suite. Reuse the frozen report for documentation and packaging preparation rather than repeating passed checks without a new failure or source/input change.

## DLC import and content reading

The later real-package observation is recorded in `out/v0.5.0/dlc-validation/REPORT.md`: three imports and intact duplicate recognition passed, while one historical game run faulted after partial content reads. Preserve that failure and the earlier synthetic results separately; no reward or dungeon acceptance is implied.

The installer’s automatic content-import UX uses one Files/Folder selection flow. It recognizes game discs, STFS DLC and mixed sources from content, presents one review, and commits discs before the shared-path save and DLC transaction. The focused result recorded 13 new cases plus 2 directly affected GUI cases, all passing on the first run in 0.934 seconds. It reused 20 unchanged DLC cases, two native modes and the independent STFS review. Evidence: `out/v0.5.2/auto-import/installer/REPORT.md`, `result.json` and `tests-initial.log`. This check did not start Tk, the game, audio, a build or packaging.

`python -B tools/tests/test_dlc_import.py` selects the DLC parser/importer checks without the old disc suites. The historical v0.5.1 result has 22 passing focused cases, including independent STFS block-address vectors and windowless installer controller checks. `--runtime-fixture <new-directory>` creates a synthetic STFS package and imports it through the production Python reader for the native handoff.

Build `LoStorageTest` only when the affected native inputs change. Its `dlc <new-isolation-directory> <imported-game-root>` and `dlc-restart <new-isolation-directory> <imported-game-root>` modes call the actual guest content and file imports, without a game window, renderer or audio. Both recorded modes passed, reading all 5,940 payload bytes in each process. Reproduction commands and retained results are in `out/v0.5.1/dlc-import/runtime/REPORT.md`; Python evidence is in the adjacent `importer/REPORT.md`. Synthetic tests do not establish real DLC rewards, areas or edition compatibility. Reuse these results for packaging and version changes.

## Selected native targets

### TAA shader-source upload fixture

The focused `collection_upload_request_test.cpp` fixture covers the nonblocking F1 upload request contract: bounded ownership, busy/full skip behavior, zero hot-path allocation, stalled HTTP completion, stop/destruction and retry-state boundaries. The current recorded run passed 124 checks; its retained output is [the F1 upload fixture log](../../out/tests/shader-source-collection/f1-upload-request/test.log). It reuses the existing 168 source-collection and 3,104 summary-collection checks rather than repeating them. This is client/helper contract evidence and does not establish a production client build, game runtime or player acceptance. The live service roundtrip is recorded separately in `out/v0.5.0/shader-source-collection/live-roundtrip.json`.

### TAA binding evidence fixtures

The standalone CPU fixtures `taa_binding_collection_test.cpp` and `taa_binding_producer_test.cpp` cover the bounded schema 3 queue, producer snapshots, revocation and zero-allocation producer paths. Compile and run them from isolated output directories when the binding contract changes. The Worker protocol fixture is run with `npm run test:taa-bindings` from `tools/taa-collector`; it covers schema 3 serialization, validation, canonicalization and deduplication. These checks do not launch the game or establish visual acceptance.

### Compact diagnostic receiver fixture

The compact receiver checks are run from `tools/taa-collector` with `node --test collection-diagnostics.test.js`. The recorded 9/9 run covers schema 4 bounds, canonicalization, privacy allowlists, backend capabilities, references, receipt handling, deduplication, HTTP failures and compatibility with older receipts. The C++ fixture passed 68 checks with zero allocations, and the ledger/archive checks passed separately. These checks do not launch the game or establish visual acceptance.

The following CMake targets are `EXCLUDE_FROM_ALL`; they are not `tools/test.bat` suite names and are never run implicitly. Select only the target relevant to the change, build it explicitly, and run the resulting executable from an isolated working directory when it writes captures or caches:

`LoFolderPickerTest`, `LoDebugMenuInteractionTest`, `LoGameWindowPixelsTest`, `LoShaderPreparationQueueTest`, `LoShaderStartupCacheTest`, `LoBackendCacheTest`, `LoBackendSelectionTest`, `LoBackendDeviceTest`, `LoRestartTest`, `LoGamePathTest`, `LoUpdaterTest`, `LoUpdaterStandaloneTest`, `LoUpdaterProgressTest`, `LoUpdaterHelperContextTest`, `LoUpdaterProbe`, `LoVulkanBackendTest`, and `LoPollWaitTest`.

`LoGameWindowPixelsTest` checks hidden Windows/SDL client and drawable pixel sizes, cross-thread presentation dimensions, DPI messages and thread-context restoration. Build this optional target only when its window policy or inputs change:

```powershell
cmake --build out/build/release --target LoGameWindowPixelsTest
Start-Process .\out\build\release\LostOdysseyRecomp\LoGameWindowPixelsTest.exe -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput .\out\build\release\LostOdysseyRecomp\LoGameWindowPixelsTest.log
```

The rendering-fixes-only pass is the only new native check for the v0.5.0 window changes; run it explicitly when those inputs change:

```powershell
Start-Process .\out\build\release\LostOdysseyRecomp\LoGameWindowPixelsTest.exe -ArgumentList '--rendering-fixes-only' -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput .\out\build\release\LostOdysseyRecomp\LoGameWindowPixels.rendering-fixes.log
```

The recorded local run passed physical 640×360 and 1280×720 clients on one 96-DPI (100%) display. The 120/144/192-DPI (125%/150%/200%) checks exercised SDL's native message contract; they do not establish physical-monitor coverage at those scales or a real mixed-DPI monitor move. It does not change desktop DPI, show windows, initialize audio/GPU, or launch the game. Evidence: `out/v0.5.0/window-pixels/run-01/REPORT.md` and `manifest.json`. This target is outside default builds and CI; registering it does not require repeating the passed fixture.

The v0.5.0 release candidate retained the recorded backend lifecycle evidence in
`out/v0.5.0/backend-lifecycle/REPORT.md`; it is a hidden-window video/presentation/Plume
contract check and does not establish guest, renderer, audio, draw, present, capture, full-game
or cross-GPU coverage. The release guard added 25 focused cases covering dirty-state, submodule,
version/tag/build provenance and binary/hash consistency; packaging checks are documented in the
[release preparation matrix](../../docs/RELEASE-v0.5.0.md). The candidate was packaged without
rerunning the game, installer or feature suites.

`LoPollWaitTest` is a CPU-only contract fixture for the scoped query/shared-value polling paths: it does not launch the game, require guest sources, execute generated guest/APC code, or establish runtime performance. Build it explicitly with `cmake --build out/build/release --target LoPollWaitTest` and run the resulting executable from the build output directory. The fixture verifies the helper contract; source review separately confirms preservation of the generated loop/return/APC checks and the renderer diagnostic snapshot/precedence.

`LoShaderStartupCacheTest` is the narrow CPU/native fixture for v0.4.17 startup-cache contracts. Build it explicitly with `cmake --build out/build/release --target LoShaderStartupCacheTest` and run the resulting executable from the build output directory. It covers ordered bundle metadata and HLSL VS/PS, DXIL, SPIR-V, common/header/record/footer corruption and truncation, length checks, consumer/post-identity failure rollback, preservation of the original complete file, snapshot changes, and rejection of negative-cache reuse after real DXC or source identity changes. It does not launch the game; the separate original-directory prepare-only pair passed with no guest startup. Evidence: `out/v0.5.0/shader-startup-recurrence/fixture-run.log` and `startup-1/startup-2/result.json`.

`LoBackendCacheTest` is the focused native fixture for backend cache identity and isolation. Its recorded run passed 91 contract checks, 14 isolated builtin checks, 10 independent restart checks and 5 framing checks; it made two actual DXC calls for the cold builtin paths and zero warm/restart calls. The fixture covers typed backend/compiler/validator/translator/options/variant/format identity, envelope length/SHA validation, legacy-success rejection, negative-cache isolation and reserved DXBC recognition. It does not launch the game or establish DX11 runtime support. Evidence: `out/v0.5.0/backend-completion/cache/REPORT.md` and `result.json`.

`LoBackendSelectionTest` and `LoBackendDeviceTest` are focused selection and native-device checks. The selection fixture passed requested/actual parsing and precedence, minimum-capability checks, 21 layered failure/exception rollback cases, explicit unsupported-DX11 fallback and bounded dual-failure diagnostics. The RTX 5080 device probe passed the required D3D12/Vulkan capabilities and native resource lifecycles without a window or draw submission. These checks do not replace the accepted scene evidence or establish full-game and cross-GPU compatibility. Evidence: `out/v0.5.0/backend-completion/selection/checks.json` and its adjacent logs.

The v0.4.16 candidate's unified Release build exited 0, and the only new runtime fixture run passed the exception/nesting, guest-longjmp TLS reset, four-thread ready/stop and timer-failure cases. This remains helper-contract evidence, not a scheduler-latency or CPU-performance measurement. The separate bounded D3D12 A/B sample is documented in `out/v0.5.0/cpu-optimization/REPORT.md`; the new Vulkan run lacks a paired ETW/CPU sample. Fixture evidence: `out/v0.5.0/cpu-optimization/poll-test-result.txt`.

```powershell
cmake --build out/build/release --target LoVulkanBackendTest
.\out\build\release\LostOdysseyRecomp\LoVulkanBackendTest.exe
```

`LoVulkanBackendTest` supports `--front-face-only` and `--depth-resolve-only`. `LoPresentationTest`, `LoTemporalAATest` and `LoResolveCopyGpuTest` accept `--vulkan`; `LoShaderTool` accepts `--vulkan` and `--jobs N` with `N` from 1 through 64. These are selected synthetic or host checks. Do not schedule the full shader corpus or launch the game merely because a target exists. The Vulkan targets require a Windows driver with the requested Vulkan 1.2/Win32 WSI capabilities; no separate Vulkan SDK is required for the Release dependency path.

For runtime checks, choose the relevant pair below after configuring the build and generating its required game sources; these are alternatives, not a required full sequence:

```powershell
# Storage changes
cmake --build out/build/release --target LoStorageTest
tools\test.bat storage --build-dir out/build/release
# Directory-filter regression; use a fresh isolated output directory.
LoStorageTest directory-filter <new-isolation-directory>
```

The `directory-filter` mode is a focused native regression: the old object fails and the corrected object passes 30 actual `NtQueryDirectoryFile` calls, including first/continuation/null/empty patterns, `RestartScan`, independent handles, replacement filters, fresh null scans, no-match and buffer/IOSB boundaries. It does not start the game or establish DLC rewards, dungeon areas or complete DLC gameplay acceptance. Evidence: `out/v0.5.0/dlc-validation/directory-filter/REPORT.md`.

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
| `LoShaderLogTest` | Windows shader JSONL routing, shared runtime timestamps, distinct hash namespaces, UTF-8/JSON escaping, complete real DXC failure diagnostics, concurrent snapshots, runtime/shader group retention, normal return and explicit `_Exit` flush, custom/disabled/alias paths, and snapshot ZIP contents. Run through `python -B tools/tests/shader_log_test.py --exe <LoShaderLogTest.exe> --dxc <dxcompiler.dll> --output <new-directory>`. Uses two tiny real DXC compilations and the production ZIP helper; no GPU, guest or game assets. Does not claim live F1 input or game-exit validation. |
| `LoMenuRenderTest` | Windows GDI host-menu rasterization at 720p, 1080p, 4K, 1920×1200 and portrait sizes; dimensions, opacity, text pixels, aspect fit and invalid-size rejection. No guest generation or runtime PCH required. |
| `LoRenderResolutionTest` | CPU Auto/manual internal-size selection, 4K cap, aspect fit, scaled dimensions, target limits and logical texel-coordinate rules. No GPU or game assets; does not establish physical scene rendering. |
| `LoRenderResolutionShaderTest` | Windows production translator/DXC checks for VS/PS normalized and denormalized fetches, signed texel offsets, texture weights and implicit LOD. Requires DXC DLLs discoverable by the built executable; no GPU or game assets. Shader compilation does not establish sampled pixels. |
| `LoRenderResolutionGpuTest` | D3D12 numerical sampling with helpers extracted from production translation: 1×/1.5×/3× physical textures, guest dimensions versus ordinary uploaded textures, normalized/denormalized coordinates, signed offsets, weights and implicit/level-zero samples. Also checks invalid-width allocation rejection followed by a valid allocation, without OOM pressure. Requires GPU/Plume/DXC; uses a synthetic gradient, not a game scene. |
| `LoFramePacerTest` | Pure host deadline calculations, FPS changes, long-stall recovery and scoped guest interval/flag mapping, including the experimental 120 gate. No GPU, guest generation or runtime PCH required; this does not test gameplay speed. |
| `LoTemporalMathTest` | CPU camera-reference math with independent analytic point, translation/yaw, viewport/Y-sign/half-pixel and invalid-input checks. No GPU, guest generation or runtime PCH required. Static round trips and these fixtures do not establish runtime frame association, motion vectors or TAA. |
| `LoTemporalSceneTest` | CPU scene-observation ordering, frame reset, depth-allocation identity, full extents and ambiguity rejection. No GPU, guest generation or runtime PCH required. It does not validate the renderer's actual scene/UI selection. |
| `LoTemporalJitterTest` | CPU production jitter and shadow-reconstruction checks across all 32 phases at 720p/1080p/1440p/4K, including the current Map16 extension (20,635 recorded checks total; 3,348 Map16 checks). Retained constant fixtures independently emulate tire and battle depth/material/lighting position paths, skinned transforms and shadow reconstruction, including negative controls, clip-derived sampling coordinates, preserved Z/W and depth UV, and Off/atlas/identity rejection guards. No GPU, game assets, guest generation or runtime PCH required; this does not establish actual draw coverage or player-visible stability. |
| `LoPositionEvidenceTest` | Native C++ rule/guard checks, retained capture-corpus classification, schema 2 Worker protocol acceptance and source-0.5.0 incremental build passed. The generated 343B JSON was accepted, schema 1 behavior remained compatible, and no D1 migration was needed. No game launch or visual acceptance is included; see `out/v0.5.0/performance-fix/position-evidence-0.5.0/REPORT.md`. |
| `LoRegisterSnapshotTest` | CPU ordered bulk-register snapshot and MMIO fallback/bounds checks used by the renderer's constant reads. No game, GPU or performance claim. Evidence: `out/v0.5.0/rendering-fixes/performance/register-snapshot-result.json`. |
| `render-timing` fixture | CPU contract checks for accepted-present intervals, renderer GPU-batch timestamp aggregation, invalid/stale query handling and timing scope labels. No game or display-latency claim. Evidence: `out/v0.5.0/rendering-fixes/render-timing/result.json`. |
| `LoGameWindowPixelsTest` | Hidden Windows/SDL physical client and drawable-pixel checks, scoped DPI context, display transitions, placement restoration and session-only Alt+Enter state. The corrected fixture passed its recorded physical 150% DPI checks; this does not establish live fullscreen or gameplay acceptance. Evidence: `out/v0.5.0/rendering-fixes/window/REPORT.md`. |
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

The historical battle fixture passed 17,287 checks (`out/battle-taa-fix/LoTemporalJitterTest.log`), covering six supported position paths across 32 phases and four sizes. Independent corrected depth/material/lighting position calculations agree exactly. Keeping the material unjittered reproduces up to 0.487771 pixels of layer separation; the corrected physical offset differs from the requested jitter by at most 0.000426 pixels. These are separate arithmetic metrics, not a measured visual improvement. Skinned paths, clip-derived mask sampling, preserved Z/W, observer ambiguity and Off/viewport/depth/camera rejection are also checked.

The current Map16 extension adds 3,348 checks, bringing the recorded fixture total to 20,635 (`out/v0.5.0/rendering-fixes/runtime/map16-cpu/result.json`). It covers the three repaired c7/c8 temporal paths across 32 phases at 3840×2160; the CPU fixture remains arithmetic and guard evidence, while the fixed-scene 32-phase visual run is recorded separately in `out/v0.5.0/rendering-fixes/runtime/candidate-legacy-01/presented-32phase-analysis.json`.

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

## Shader identity reuse

`shader_identity_test.cpp` is a standalone CPU fixture for the byte-hash cache used by renderer shader/pipeline lookup. Its 74 checks cover byte identity, content mutation, relocated sources, forced command-hash collisions, variable lengths and bounded eviction. Compile with `clang-cl /std:c++20 /EHsc /O2 /MT tools/tests/shader_identity_test.cpp` in the Windows SDK environment, directing `/Fo` and `/Fe` to an isolated output directory. The passing run is retained in `out/v0.5.0/performance-fix/candidate/test.log`; it does not establish GPU or whole-game behavior.

## Geometry preparation

`geometry_prepare_test.cpp` checks the scalar-compatible 16/32-bit index conversion for all endian modes, unaligned inputs and boundary counts, plus exact vertex sample comparisons and mutation detection. The retained run passed 3,206,492 value/guard assertions in `out/v0.5.0/performance-fix/geometry-candidate/geometry_prepare_test.log`. It is a standalone CPU fixture, compiled like the shader identity fixture above, and does not imply full-buffer coverage for large sampled vertex streams or GPU validation.

`deadline_wait_test.cpp` covers the reusable Windows pacing timer with 32 future deadlines in high-resolution mode and 32 in normal-timer fallback mode, plus already-expired deadlines. Both modes passed on this host. It preserves the existing `FramePacer` schedule and uses no busy wait or global timer-resolution change. Evidence: `out/v0.5.0/performance-fix/final-0.5.0/deadline_wait_test.log`.
