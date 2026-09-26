<div align="center">

# Lost Odyssey Recomp

**An experimental native PC port of Lost Odyssey for Xbox 360.**

Windows x64 · Direct3D 12 · Vulkan · PowerPC static recompilation

<img src="docs/images/title-screen.png" alt="Lost Odyssey title screen — Press START" width="960">

Optional diagnostics are off by default and can be disabled in Settings. See [Privacy](PRIVACY.md).

### [Latest download](https://github.com/freefrank/LostOdysseyRecomp/releases/latest) · [Installation guide](docs/INSTALLING.md) · [Report an issue](https://github.com/freefrank/LostOdysseyRecomp/issues)

[简体中文](README.zh-CN.md) · [Changelog](CHANGELOG.md) · [Developer tools](tools/README.md) · [Projects](https://github.com/users/freefrank/projects/3) · [Build from source](docs/BUILDING.md)

</div>

> [!IMPORTANT]
> **This project is still in early testing.** Opening areas and selected scenes have been tested; a complete playthrough has not. Rendering and stability issues remain. You must supply your own supported game files.

### Upscaling (DLSS & FSR)

NVIDIA DLSS (Super Resolution & DLAA) and AMD FSR 3.1 are supported as experimental upscalers on Windows and Linux; technical details and validation boundaries are documented in [development status](docs/STATUS.md). When DLSS or DLAA is unavailable or disabled, a saved TAA selection falls back to SMAA while preserving other anti-aliasing choices.

### Automatic PlayStation controller prompts

The recomp automatically detects the most recently active gamepad via SDL and updates button prompts:
- **Controller prompts**: Updates ABXY action buttons, shoulder buttons (LB/RB/LT/RT to L1/R1/L2/R2), and Start/Select (Options/Share and Options/Create) across host menus (settings, installer, debug overlay) and in-game pause menu and cutscenes.
- **Technical reference**: Texture replacement, atlas hash matching, and GPU upload lifecycle details are documented in the [Issue #40 UI resource map](docs/notes/issue-40-ui-resource-map.md) and [development status](docs/STATUS.md).
- **Status & acceptance**: Verified in development testing and accepted by the user after pause menu and cutscene review; included in v0.7.0.
- **Validation limits**: User acceptance is bounded to tested controller hardware and verified scenes, without claiming universal controller hardware compatibility or complete full-game playthrough coverage. Issue #40 mod support was not implemented in this scope.

## Roadmap

The **v0.7.0** release delivered performance optimizations, quality-of-life improvements, PlayStation controller prompts, the v1 Mod API, and native DLSS/DLAA and FSR upscaling. Planned roadmap targets for **v0.8.0** include DLSS Frame Generation (fixed 2× DLSS FG on Windows Vulkan, alongside D3D12 DLSS FG), dynamic Multi-Frame Generation (dynamic MFG, with target APIs, platforms, or generation multipliers not predetermined), independent FSR Frame Generation, native independent 120 FPS candidate evaluation (`LO_EXPERIMENTAL_120=1`, evaluating native presentation pacing), removal of the legacy PM4 packet translation layer, a Flatpak release package (packaging tooling implemented on dev with Flathub PR submission planned, without current public release), Linux AArch64, macOS AArch64 (Apple Silicon), and experimental Android support. All v0.8.0 targets represent uncompleted roadmap planning rather than current implementation, test verification, user acceptance, or release delivery; development sequencing is tracked in the [roadmap](docs/ROADMAP.md).

Past release notes and detailed changes are recorded in the [changelog](CHANGELOG.md).

## Start playing

1. **Download and extract** the entire Windows release ZIP to a writable folder.
2. **Run `LostOdysseyRecomp.exe`** and import your game files when prompted. The importer accepts an extracted folder, `default.xex`, an XDVDFS ISO or a GOD container.
3. **Choose your language and graphics settings.** The game continues after setup and shader preparation.

No Python or Visual Studio installation is needed for the release package. Later launches reuse the shader cache. Keep your save and profile folders when updating.

The published updater checks GitHub's latest Release: a higher numeric version updates, and an equal numeric version with a different `-suffix` also triggers an update. The updater additionally permits recovery from an empty updater-only folder and stale or malformed local metadata. After a successful update, the helper asks whether to launch the game and defaults to **No**; silent runs complete without launching. Download integrity checks, safe extraction and rollback remain enabled.

| Requirement | Supported configuration |
| :--- | :--- |
| System | Windows x64, AVX-capable CPU, Direct3D 12 or Vulkan graphics driver |
| Game data | Audited Europe, Asia or USA, Europe edition; Disc 1 is required to start |
| Additional discs | Import additional discs or DLC from the built-in importer in `LostOdysseyRecomp.exe`; later-disc progression is not fully verified |

See the [installation guide](docs/INSTALLING.md) for accepted disc versions, file locations and updating.

## In-game screenshots

| Ring combat | City exploration |
| :---: | :---: |
| ![Kaim attacking with the Ring timing interface](docs/images/ring-battle.png) | ![Exploring the industrial city](docs/images/city-exploration.png) |

*Unmodified screenshots from development builds leading up to v0.1.*

## Current features

| Feature | What to expect |
| :--- | :--- |
| Game importer | Folder, XEX, ISO and GOD input; originals stay untouched, and staged copies check final writes before publication |
| First-launch setup | Language and graphics settings before game initialization |
| Language settings | English, Japanese, Korean, Traditional and Simplified Chinese interface options; game language selection |
| Graphics settings | Auto/manual internal resolution (config/legacy fallback), 16:9 / 21:9 resolution presets with Widescreen toggle, Off/FXAA/SMAA/experimental TAA, upscaler options (Off/DLSS with Quality/Balanced/Performance/DLAA), Standard/High filtering, 30/60 FPS and output/display controls; fullscreen and mixed DPI need more testing |
| Settings menu | Original game fonts, scrollable overflowing lists and menu styling; one-click Graphics save/apply, Start/Enter focus-jump to Save without saving, and Now/Later restart choices |
| Shader preparation | Bundled portable Vulkan shader pack (.lospv), memory-adaptive parallel compilation, interactive skip, and cache reuse |
| CPU use | Reduced unnecessary polling and reuse of rendering work |
| Input and debug | Controller and keyboard input; English/Simplified Chinese in-game overlay debug menu (F1 or LB+RB) with capture, map information and same-map POI teleport |

Published packages import game discs and supported DLC with **Files** or **Folder** in `LostOdysseyRecomp.exe`. The current development build also adds **Gameplay → Import discs & DLC** to reopen the importer and replace selected discs and DLC; this menu entry is not yet released. Official public releases provide Windows ZIP and Linux AppImage packages. An automated Flatpak packaging tool (`tools/package_flatpak.py`) is available for building Freedesktop 26.08 bundles from source; Flathub submission is planned, while official public releases remain Windows ZIP and Linux AppImage. See the [installation guide](docs/INSTALLING.md#automatic-content-import), [build instructions](docs/BUILDING.md#packaging-flatpak), and [development status](docs/STATUS.md) for validation limits.

Validation progress and remaining work are tracked in the [Maintainer Project](https://github.com/users/freefrank/projects/3).

<details>
<summary><strong>Game edition and compatibility details</strong></summary>

The two supported editions correspond to [Lost Odyssey (Europe, Asia) (En,Ja,Zh,Ko) (Disc 1), Redump 39111](https://redump.info/disc/39111) and [Lost Odyssey (USA, Europe) (En,Ja,Fr,De,Es,It) (Disc 1), Redump 11817](https://redump.info/disc/11817). The former is called the Asian edition here: Disc 1 has title ID `4D5307FA`, media ID `39F7D748`, title/base version `0.0.0.4` and XeMID `MS204204H0X14`. The USA, Europe Disc 1 has media ID `368DE6DD`, version `0.0.0.3` and XeMID `MS204203W0X14`. These identities match the audited sets; a complete ISO hash comparison against Redump has not been performed. The importer strictly checks each supported XEX hash; a region label alone is insufficient.

The **USA, Europe version 0.0.0.3** four-disc set is supported, with strict XEX checks and protection against mixing editions. Game-language choices follow the installed edition: English/Japanese/German/French/Spanish/Italian for USA, Europe; the audited Europe, Asia resources retain English/Japanese/Korean/Traditional Chinese/Simplified Chinese choices. See [edition details](docs/notes/europe-support.md).

With all four discs imported, the game selects them automatically; no manual disc swap is needed. See [disc handling](docs/notes/disc-selection.md).

Language options do not imply a complete playthrough in every language. Regional builds outside the audited sets, title updates and modified XEX files are not validated. See [edition evidence](docs/notes/xex.md).

</details>

<details>
<summary><strong>Build commands and repository layout</strong></summary>

### Build and run

Prepare your own extracted data, dependencies and generated sources using the [build guide](docs/BUILDING.md). Helper scripts discover installed tools; custom paths can be supplied through environment variables.

```powershell
.\tools\build_runtime.bat
$gameData = (Resolve-Path .\LostOdysseyRecompLib\private\disc1).Path
Push-Location .\out\build\windows-clang\LostOdysseyRecomp
.\LostOdysseyRecomp.exe --game $gameData --quiet-kernel
Pop-Location
```

Keep the working directory consistent so the intended save/profile folders are used.

**Startup and failure logs.** Each normal launch writes `logs/runtime-<timestamp>.log` in the working directory and mirrors output to `stderr`; set `LO_LOG_FILE=<path>` to choose another file, or `LO_LOG_FILE=0` to disable the duplicate file sink. v0.5.11 additionally records the Windows build, process/native architecture, source/build revision, PE image metadata, compiler, startup memory baseline, GPU, raw driver version, vendor/type and `reported_device_memory_bytes`. When reporting a startup or renderer failure, attach the complete current runtime log and include the executable/source version, backend, GPU and driver details recorded near startup. The diagnostic records preserve raw API codes and the failed resource or allocation context, but they are investigation evidence and do not by themselves identify a root cause. See the [build and logging guide](docs/BUILDING.md) for the path and retention rules.

For a visual issue, press **F1** while it is visible and choose **Capture render state**. Wait for the background archive to finish, then attach the archive at the path shown by the status message: Windows produces a `.zip` archive and Linux produces a `.tar.gz` archive. If archiving fails, the raw capture folder is retained for recovery.

| Action | Keyboard |
| :--- | :--- |
| Start / Back | Enter / Backspace |
| A / B / X / Y | Z / X / A / S |
| D-pad / left stick | Arrow keys / I, J, K, L |
| Left / right shoulder | Q / W |
| Left / right trigger | E / R |
| Debug menu | F1 / Gamepad LB+RB |

SDL-mapped controllers and the keyboard can be used together for player 1. Unmapped joysticks need an SDL controller mapping. See [input details](docs/notes/controller-input.md).

Rumble is disabled by default; `LO_CONTROLLER_RUMBLE=1` enables it. For Ring actions, use the controller's right trigger or the R key.

### Development

| Directory | Contents |
| :--- | :--- |
| `LostOdysseyRecomp/` | Host kernel, graphics, audio, input and debugging |
| `LostOdysseyRecompLib/` | Configuration; ignored `private/` game data and generated `ppc/` code |
| `tools/` | Recompilers, dependency patches, Ghidra scripts and the optional [assembly profiler](tools/asm-profiler/README.md) |
| `thirdparty/` | Rendering, audio and other dependencies |
| `docs/` | Current status, guides, research and historical archives |

[Roadmap](docs/ROADMAP.md) · [Handoff](docs/notes/handoff.md) · [Rendering tests](docs/notes/rendering-validation.md) · [TAA live debug](docs/TAA_LIVE_DEBUG.md) · [Audio](docs/notes/audio-output.md) · [Archive](docs/archive/README.md)

</details>

## Sponsors

Thank you to **Cristian** and **Whitesun** for supporting the project on Ko-fi.

## Credits and game data

With research and tools from [UnleashedRecomp](https://github.com/hedge-dev/UnleashedRecomp), [re:Blue](https://github.com/zolaware/reblue), [XenonRecomp](https://github.com/hedge-dev/XenonRecomp), [XenosRecomp](https://github.com/hedge-dev/XenosRecomp), [plume](https://github.com/renderbag/plume) and [Xenia](https://github.com/xenia-project/xenia). Audio uses the pinned [Xenia FFmpeg fork](https://github.com/xenia-project/FFmpeg), with its [license](thirdparty/ffmpeg-LICENSE.txt).

Lost Odyssey and its assets belong to their respective owners. This is an unofficial project. Supply data extracted from your own discs; do not submit game executables, resource archives, textures, audio, video, generated game code or captures. Dependencies retain their respective licenses.
