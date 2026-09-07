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
| `shader-index` | Resource shader index/scanner fixtures |
| `shaders` | Resource scanning, CPX decoding and bounded dynamic-VS fixtures |
| `pipeline` | Pipeline recipe validation, corruption/truncation and atomic-write fixtures |
| `storage` | Built `LoStorageTest`: disc selection and save round trips |
| `hid` | Built `LoHidTest`: controller and keyboard input |
| `startup` | Built `LostOdysseyRecomp`: synthetic headless Unicode startup-path checks |

Fixture executables go to `out/tests/bin`. Synthetic temporary input is cleaned up automatically. Preserve logs or reports needed to review a result; remove only disposable files created by the current task, never user saves, profiles or unrelated artifacts.

The older `tools/test_shader_index.bat` and `tools/test_shader_preparation.bat` remain forwarding entry points; preparation selects `shaders pipeline`.

## Manual presentation and timing checks

The v0.4.0 development targets below are excluded from default builds and are not suite names accepted by `tools/test.bat`. Select the target relevant to the change and run its built executable explicitly; the examples are alternatives, not a required full sequence.

| Target | Scope and prerequisites |
|---|---|
| `LoMenuRenderTest` | Windows GDI host-menu rasterization at 720p, 1080p, 4K, 1920×1200 and portrait sizes; dimensions, opacity, text pixels, aspect fit and invalid-size rejection. No guest generation or runtime PCH required. |
| `LoFramePacerTest` | Pure host deadline calculations, FPS changes, long-stall recovery and scoped guest interval/flag mapping, including the experimental 120 gate. No GPU, guest generation or runtime PCH required; this does not test gameplay speed. |
| `LoTemporalMathTest` | CPU camera-reference math with independent analytic point, translation/yaw, viewport/Y-sign/half-pixel and invalid-input checks. No GPU, guest generation or runtime PCH required. Static round trips and these fixtures do not establish runtime frame association, motion vectors or TAA. |
| `LoTemporalSceneTest` | CPU scene-observation ordering, frame reset, depth-allocation identity, full extents and ambiguity rejection. No GPU, guest generation or runtime PCH required. It does not validate the renderer's actual scene/UI selection. |
| `LoTemporalAATest` | Standalone D3D12 resolve/display and copied-history checks: identity, depth/reactive rejection, neighborhood clamp, alpha, invalid-call output preservation, resource lifetime, external source overwrites and frame/epoch/reset handling. Stable-grid cases separate color/depth jitter coordinates and test camera motion, material edges and static silhouettes. Moving silhouettes use the segment analysis below; optional `--replay` compares independent baseline/candidate histories on captured traces. Requires configured D3D12/Plume dependencies; no guest generation or runtime PCH. It does not enable or accept game TAA. |
| `LoPresentationTest` | D3D12 GPU readback for native identity, letterboxing, FXAA/SMAA edges, flat regions, padding, resize, scaling and checkerboard reductions. Pre-UI scene-AA/UI-composition cases include a repeated-AA negative control. Optional `--capture` mode replays one raw frame through six AA/quality combinations. Requires configured D3D12/Plume dependencies; the target builds independently without reusing the main executable's PCH. No game-scene acceptance is implied. |

```powershell
# Host menu rendering
cmake --build out/build/release --target LoMenuRenderTest
.\out\build\release\LostOdysseyRecomp\LoMenuRenderTest.exe
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

The scene-AA fixture first checks that FXAA/SMAA changes a scene edge, then adds opaque UI strokes and requires native-size composited presentation to preserve the result. Its negative control deliberately filters the UI a second time and must change the UI mask. These GPU cases passed; actual game captures also show scene processing. Final-presentation AA bypass and partial-resolve/clear handling still need integrated validation, so this is not yet a completed game-text fix. The separate `scene_aa_provenance_test.cpp` CPU fixture covers frame/allocation identity and full/partial/clear treatment; its 32 passing checks do not establish actual renderer propagation.

For a controlled comparison of a captured frame, run the already-built presentation test in replay mode:

```powershell
.\out\build\release\LostOdysseyRecomp\LoPresentationTest.exe --capture path\to\inputRGBA.bin 1280 720 out\presentation-replay-new
```

Supply the input's actual width and height. The file must contain exactly `width × height × 4` bytes of packed RGBA8 pixels, with no header or row padding. The output directory must not already exist; the tool preserves earlier evidence by rejecting an existing directory. It writes six 1920×1080 PPM files, from `aa0-quality0.ppm` through `aa2-quality1.ppm`: AA values 0/1/2 mean Off/FXAA/SMAA, and quality values 0/1 mean Standard/bilinear and High/bicubic. Replay mode uses the production presentation path and skips the synthetic suite; it does not launch the game. Inspect the outputs against the same source frame. Success establishes file generation and GPU execution, while visual findings remain specific to the sampled content and do not prove temporal stability, all-language text quality or performance.

## CI and build boundaries

Importer, shader and pipeline workflows are independent, path-filtered checks for main pushes, pull requests and manual dispatch. Tags do not repeat these jobs. The runtime workflow is manual only and selects one of `storage`, `hid` or `startup`, with its own explicit generation/build steps. CMake test targets are excluded from the default build; request the needed targets explicitly.

Release packaging accepts a manual `release_tag` and checks out that existing tag. Changing the workflow on main does not change the tagged game sources or require retagging them.

Release packaging is separate from test CI. A build or fixture pass is not gameplay or visual acceptance. Passing checks should not be repeated or expanded without a new change, failure or unresolved concern. Avoid tests for reversible low-impact edits and tests that only mirror implementation details.
