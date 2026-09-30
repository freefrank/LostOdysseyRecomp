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

## Recent releases

| Version | Highlights |
| :--- | :--- |
| [v0.7.15](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.7.15) | Native 90/120 FPS targets and VRR pacing, RGB Range and F1 speed controls, the Hungry Man timer fix, portable game-path fallback, and a targeted sky-flicker TAA mapping. |
| [v0.7.10](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.7.10) | Refreshed bundled Vulkan shaders (+19 captured records) and a separate DX12 shader pack under `shaders/`. |
| [v0.7.9](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.7.9) | Windows D3D12 frame generation (Off/DLSS/FSR), applied on Save without restarting; Ubuntu 22.04 AppImage compatibility and updater improvements. |
| [v0.7.3](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.7.3) | D3D12 binding de-duplication and opt-in rendering diagnostics. |
| [v0.7.2](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.7.2) | D3D12 DLSS/FSR super-resolution routes and DLAA sizing correction. |
| [v0.7.1](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.7.1) | Standalone Flatpak and in-game disc/DLC selection and re-import. |

See the [roadmap](docs/ROADMAP.md) for current plans and the [changelog](CHANGELOG.md) for detailed release history.

## Start playing

### Windows
1. **Download and extract** the v0.7.15 Windows release ZIP (`LostOdysseyRecomp-windows-x64-v0.7.15.zip`) from the [latest release](https://github.com/freefrank/LostOdysseyRecomp/releases/latest) to a writable folder.
2. **Run `LostOdysseyRecomp.exe`** and import your game files when prompted. The importer accepts an extracted folder, `default.xex`, an XDVDFS ISO or a GOD container.
3. **Choose your language and graphics settings.** The game continues after setup and shader preparation.

### Linux (Flatpak or AppImage)
- **Flatpak bundle**: Ensure the Freedesktop 26.08 platform is installed:
  ```bash
  flatpak --system install flathub org.freedesktop.Platform//26.08
  ```
  Download the v0.7.15 standalone `.flatpak` bundle (`LostOdysseyRecomp-linux-x64-v0.7.15.flatpak`) and install:
  ```bash
  flatpak --user install --bundle LostOdysseyRecomp-linux-x64-v0.7.15.flatpak
  flatpak run io.github.freefrank.LostOdysseyRecomp
  ```
- **AppImage**: Download `LostOdysseyRecomp-linux-x64-v0.7.15.AppImage`, make it executable (`chmod +x`), and run directly.

No Python or Visual Studio installation is needed for the release package. Later launches reuse the shader cache. Keep your save and profile folders when updating.

The current branch updater checks GitHub's latest Release: a higher numeric version updates, and an equal numeric version with a different `-suffix` also triggers an update. The updater additionally permits recovery from an empty updater-only folder and stale or malformed local metadata. After a successful update, the helper asks whether to launch the game and defaults to **No**; silent runs complete without launching. After the download completes, the updater installs through ordinary HTTP/I/O handling, ZIP CRC parsing, path protection and rollback; it does not add SHA-256 or size authentication. The v0.7.9 Windows transition package carries the legacy SHA map once so an already published v0.7.3 updater can upgrade automatically; v0.7.10 ignores those values.

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
| Graphics settings | Auto/manual internal resolution (config/legacy fallback), 16:9 / 21:9 resolution presets with Widescreen toggle, Off/FXAA/SMAA/experimental TAA, upscaler options (Off/DLSS/FSR 3.1 with quality controls), Standard/High filtering, optional RGB Range expansion (Off/Expanded), 30/60/90/120 FPS, FreeSync / G-SYNC Compatible VRR and output/display controls; fullscreen, mixed DPI and broader upscaler scene coverage need more testing |
| Frame Generation settings | Graphics-page Off/DLSS/FSR controls, DLSS multipliers, fixed 2× FSR, session status and live application after saving; switching from DLSS FG to FSR FG requires a restart; bounded D3D12 Uhra validation |
| Settings menu | Original game fonts, scrollable overflowing lists and menu styling; one-click Graphics save/apply, Start/Enter focus-jump to Save without saving, and Now/Later restart choices |
| Shader preparation | Bundled portable Vulkan shader pack (.lospv), memory-adaptive parallel compilation, interactive skip, and cache reuse; the separate DX12 .lospd asset belongs under `shaders/` |
| CPU use | Reduced unnecessary polling and reuse of rendering work |
| Input and debug | Controller and keyboard input; English/Simplified Chinese in-game overlay debug menu (F1 or LB+RB) with capture, map information and same-map POI teleport |

Release packages import game discs and supported DLC with **Files** or **Folder** in `LostOdysseyRecomp.exe`. In v0.7.1, **Gameplay → Import discs & DLC** allows reopening the importer to replace selected discs and DLC. The v0.7.15 release provides Windows ZIP, Linux AppImage, and standalone Linux Flatpak packages, with a separate DX12 shader asset under `shaders/`. See the [installation guide](docs/INSTALLING.md#automatic-content-import), [build instructions](docs/BUILDING.md#packaging-flatpak), and [development status](docs/STATUS.md) for validation limits.

Validation progress and remaining work are tracked in the [Maintainer Project](https://github.com/users/freefrank/projects/3).

<details>
<summary><strong>Game edition and compatibility details</strong></summary>

The two supported editions correspond to [Lost Odyssey (Europe, Asia) (En,Ja,Zh,Ko) (Disc 1), Redump 39111](https://redump.info/disc/39111) and [Lost Odyssey (USA, Europe) (En,Ja,Fr,De,Es,It) (Disc 1), Redump 11817](https://redump.info/disc/11817). The former is called the Asian edition here: Disc 1 has title ID `4D5307FA`, media ID `39F7D748`, title/base version `0.0.0.4` and XeMID `MS204204H0X14`. The USA, Europe Disc 1 has media ID `368DE6DD`, version `0.0.0.3` and XeMID `MS204203W0X14`. The importer identifies supported data from title, media, version, base and disc metadata; it does not perform a complete ISO or per-file SHA-256 audit.

The **USA, Europe version 0.0.0.3** four-disc set is supported, with metadata checks and protection against mixing editions. Game-language choices follow the installed edition: English/Japanese/German/French/Spanish/Italian for USA, Europe; the audited Europe, Asia resources retain English/Japanese/Korean/Traditional Chinese/Simplified Chinese choices. See [edition details](docs/notes/europe-support.md).

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
