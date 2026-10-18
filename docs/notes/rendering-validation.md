# Rendering validation (2026-09-04)

[简体中文](rendering-validation.zh-CN.md)

> **Historical checkpoint.** Versions, Issues, processes and to-dos below are the records as of that date. For current delivery and acceptance status, see [project status](../STATUS.md) and [roadmap](../ROADMAP.md).

> 2026-09-05 status sync: the opening screenshot baseline on this page is a historical fixed scene. For current open items, see the [status page](../STATUS.md). The later Gorge camp was reached in the background, but that does not prove the user's window freeze is fixed.

## Build and regression tests

First prepare the submodules by following the [dependency patch notes](../../tools/patches/README.md), and prepare the local game data, recompiled output and Windows toolchain listed in the README.
Run from the repository root:

```powershell
.\tools\build_runtime.bat
.\tools\build_runtime.bat LoMemoryAliasTest
.\tools\build_runtime.bat LoShaderAluTest
.\tools\build_runtime.bat LoStencilTest
.\tools\build_runtime.bat LoTextureLayoutTest
.\out\build\windows-clang\LostOdysseyRecomp\LoMemoryAliasTest.exe
.\out\build\windows-clang\LostOdysseyRecomp\LoShaderAluTest.exe
.\out\build\windows-clang\LostOdysseyRecomp\LoStencilTest.exe
.\out\build\windows-clang\LostOdysseyRecomp\LoTextureLayoutTest.exe
```

The four tests do not read game assets themselves. The current overall CMake configuration still depends on the local project build environment.

| Test | Coverage | Platform verification |
|---|---|---|
| LoTextureLayoutTest | 14 packed mip block offset cases, including tiny BC textures and long strip textures | Windows |
| LoMemoryAliasTest | Shared writes from A/C/E, a one-page offset for E, and release | Windows, WSL Manjaro |
| LoShaderAluTest | Real GPU ALU old-value reads, later results, gamma, swizzle and resolve R/B round trip | Windows D3D12 |
| LoStencilTest | Left half writes stencil=3; GPU pixel readback for EQUAL/NOT_EQUAL; static assertions for D24 endpoints | Windows D3D12, LO_BUILD_GPU=ON |

> 2026-10 test cleanup: `LoStencilTest` has been removed from the repository. Its command and result above are kept only as history.

Separately, LoShaderTool compiled the 162 shaders captured at the opening scene, with 0 failures. The captured data is local only and is not a repository test asset.

## Live reproduction

Use Disc 1, with the default 30 fps, the default EDRAM transfer (draw + resolve) and shader cache v19.
Set the automatic input from the repository root, then start:

```powershell
New-Item -ItemType Directory -Force out/render-check | Out-Null
$env:LO_AUTO_PULSE = '6'
$env:LO_AUTO_BUTTONS = 's@300,a@600,b@900,u@1100,a@1300,s@2000,k@2100,a@2200,a@2800,a@3400'
$env:LO_GPU_STATS = '1'
$env:LO_RELATIVE_DRAW_STATS = '1'
$env:LO_SCREENSHOT_EVERY = '300'
$env:LO_SCREENSHOT_PATH = 'out/render-check/shot.ppm'
.\out\build\windows-clang\LostOdysseyRecomp\LostOdysseyRecomp.exe --game .\LostOdysseyRecompLib\private\disc1 --quiet-kernel 2> out/render-check/stderr.log
```

Automatic input counts by swap. In this run, frame 2400 is the attack target list, and frame 3000 is a close-up of the front-facing character.
If the state drifts, first check the menu and input timing. Do not treat a different camera shot as a rendering regression.
The logger writes to stderr. By default, each run is also written to `logs/runtime-<timestamp>.log` in the working directory; `LO_LOG_FILE` can change this. In this example, stderr and the screenshots are in the ignored `out/` directory. For the full working directory conventions, see the [build guide](../BUILDING.md).

Baseline for this run: `out/render-final/` (after the geometry and texture fixes).
Final lighting result: `out/render-light-depthpack/`. The red colour cast in the intermediate experiment `render-light-stencil-ref/` is not the final result.
For the Xenia comparison conditions, see the [comparison record](../archive/notes/xenia-render-comparison.md).

## Known scope

Verified: the opening material, model integrity, post-processing outlines and metallic highlight improvements. Not verified: the end of the heavy tank battle, clearing the game, or all shadow detail.
Later, the post-first-battle sequence and the heavy tank battle were verified. See the [white screen fix record](../archive/notes/post-battle-whiteout.md).
Exposure was not adjusted. The normal depth test, the object occlusion switch and depth of field were not turned off. The occlusion count is still an approximation.
Front and back stencil faces with different reference values/masks still have limits. This scene did not trigger the corresponding warnings.

## Background testing with a hidden window

Setting `LO_BACKGROUND=1` makes SDL create a hidden window from the start. D3D12 rendering and game screenshots still run, but no window is shown and focus is not taken. This is different from `LO_HEADLESS`, which disables video initialisation. Input is submitted in-process through `LO_TEST_INPUT_FILE`; system keyboard and mouse input is not injected. For run records, see [walkthrough testing](../archive/notes/walkthrough-testing.md). Background runs still use CPU and GPU, so stop any separate processes when the test ends.

For fatal resource-resolution errors, the log now records the raw resource name and the caller, then keeps the game's original error and exit behaviour. The log no longer shows only a generic dirty disc message.

### Request consecutive frames after reaching a scene

Set LO_SCREENSHOT_REQUEST to a request file path in a separate test directory. Write two integers: a non-zero serial and a frame count, for example `1 120`. Capture starts after the next Present and runs for at most 600 frames. It uses LO_SCREENSHOT_PATH and appends the swap number. The same serial is not captured again; a new serial can trigger again. A frame count of 0 cancels a request that has not finished. When not enabled, which is the default, the file is not read. Do not use this together with per-draw/resolve capture that forces GPU synchronisation when comparing performance.

Runtime build and independent background validation passed: 5 frames; keeping the same serial but changing it to 50 still gave 5 frames; a new serial with 3 frames gave 8 frames in total; a zero-frame request created no new files; all 8 PPM images could be decoded. Local evidence: out/screenshot-request-test.

Frame alignment: renderer draw/resolve frame F is displayed in screenshot swap F+1. Verified by final 0x714000 resolves against consecutive screenshots. Use the final resolve for exact trace matching; same-numbered swap images are the previous rendered frame.
