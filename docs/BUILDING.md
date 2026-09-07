# Building and running

[Overview](../README.md) · [Current status](STATUS.md)

## Prerequisites

The tested environment is Windows x64 with Direct3D 12, Visual Studio 2022 Build Tools and Windows SDK, LLVM clang-cl, CMake 3.28+ and Ninja. The runtime requires Clang; the `windows-msvc` preset is not a supported runtime alternative. Generated code uses AVX instructions. Windows 10 1803+ is required by the current memory-mapping path; this is not a guarantee for every GPU/driver combination.

The scripts [build_runtime.bat](../tools/build_runtime.bat), [build_tools.bat](../tools/build_tools.bat) and [CMakePresets.json](../CMakePresets.json) discover Visual Studio with `vswhere` and resolve tools from `PATH`. Use `LO_VCVARS64` or `LLVM_ROOT` for custom installations. Keep personal overrides in the ignored `CMakeUserPresets.json`.

## Game data and dependencies

1. Supply your own extracted Asian multilingual edition, matching the [XEX details](notes/xex.md). Place Disc 1 at `LostOdysseyRecompLib/private/disc1/`, including `default.xex`, `LO.fpi` and its resource archives. Four-disc integration is not complete.
2. Initialize submodules and apply the [project patches](../tools/patches/README.md). Do not reapply patches over a modified dependency tree.
3. Build the generator tools, then generate local PowerPC sources. Generated code and game data are ignored by Git.

From the repository root, after preparing dependencies and data:

```powershell
.\tools\build_tools.bat
.\out\build\tools\XenonRecomp\XenonRecomp\XenonRecomp.exe .\LostOdysseyRecompLib\config\LostOdysseyRecomp.toml .\tools\XenonRecomp\XenonUtils\ppc_context.h
.\tools\build_runtime.bat
```

The generator takes the configuration and context header as arguments. See [recompilation notes](notes/recomp.md) for function boundaries and switch-table maintenance. These commands describe the checked-in scripts; a new-machine end-to-end bootstrap has not been retested as part of this documentation update.

Audio configuration fetches the pinned Xenia FFmpeg source via CMake FetchContent, so first configuration needs network access. See [ffmpeg.cmake](../thirdparty/ffmpeg.cmake) and its [license](../thirdparty/ffmpeg-LICENSE.txt). This is a frame-level XMAFRAMES decoder, not a system FFmpeg executable requirement.

## Launch with a consistent working directory

```powershell
$gameData = (Resolve-Path .\LostOdysseyRecompLib\private\disc1).Path
Push-Location .\out\build\windows-clang\LostOdysseyRecomp
.\LostOdysseyRecomp.exe --game $gameData --quiet-kernel
Pop-Location
```

With explicit `--game`, `save/`, `profile/`, `cache/` and default `logs/` remain relative to the
process working directory, allowing isolated regression runs. Launching without `--game`
first selects the executable directory, then resolves `game-path.txt` or the adjacent `game`
folder. A fresh installation opens initial settings before guest startup. `LO_PROFILE_DIR`
overrides the profile location. Back up saves before testing; use independent save/profile copies.

| Setting | Effect |
|---|---|
| `LO_LOG_FILE=<path>` | Append logs to a selected file; `0` disables the duplicate file sink. Default: a separate timestamped file under `logs/`. |
| `LO_BACKGROUND=1` | Hidden rendering window; background audio is muted by default. |
| `LO_DEBUG_MENU_OPEN=1` | Open the Windows debug panel at startup for UI validation; default is closed. |
| `LO_HEADLESS=1` | No video device/window; not equivalent to hidden rendering. |
| `LO_AUDIO_MUTE=1` | Mute device output. |
| `LO_AUDIO_CAPTURE=<path>` | Up to 60 seconds of raw 48kHz stereo float PCM before mute. |
| `LO_CONTROLLER_RUMBLE=1` | Enable controller rumble; default is off. |

Clear test-only environment variables before manual play. Do not treat a window staying open, a heartbeat, or nonzero PCM as proof a scene is correct.

The shader compiler loads `dxcompiler.dll` from the application/search path, `LO_DXC_PATH`, or an installed Windows SDK. Supply a matching DXC distribution for custom installations.

Generated baseline mappings, branch targets, import listings and Ghidra exports are local analysis artifacts. They are ignored; regenerate them from your own data when extending the recompiler configuration. Checked-in TOML and manual boundary/switch overrides remain the build inputs.

## Verification

Use `tools\test.bat --list` to select checks, then run only the relevant suites, for example `tools\test.bat shaders pipeline`. Runtime suites use an existing CMake build root (`--build-dir out/build/release` by default), with their target built explicitly; the runner does not implicitly build the game. See the [test guide](../tools/tests/README.md) for per-suite build commands, prerequisites and CI separation.

[Rendering tests](notes/rendering-validation.md) cover memory aliases, shader ALU, stencil and texture layout. [Audio notes](notes/audio-output.md) cover `LoXmaLoopTest`; [storage notes](notes/save-storage.md) describe `LoStorageTest`. Some investigation targets and input hooks remain local changes; consult [status](STATUS.md) before expecting them in a clean checkout.
