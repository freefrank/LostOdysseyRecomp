<div align="center">

# Lost Odyssey Recompiled

**An experimental native PC port of Lost Odyssey for Xbox 360.**

Windows x64 · Direct3D 12 · PowerPC static recompilation

<img src="docs/images/title-screen.png" alt="Lost Odyssey title screen — Press START" width="960">

### [Download v0.1](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.1) · [Installation guide](docs/INSTALLING.md) · [Report an issue](https://github.com/freefrank/LostOdysseyRecomp/issues)

[简体中文](README.zh-CN.md) · [Project status](docs/STATUS.md) · [Roadmap](docs/ROADMAP.md) · [Build from source](docs/BUILDING.md)

</div>

> [!IMPORTANT]
> **v0.1 is an early testing release.** Opening areas and selected scenes have been tested; a complete playthrough has not. Rendering and stability issues remain. You must supply your own supported game files.

## Start playing

1. **Download and extract** the entire Windows release ZIP to a writable folder.
2. **Run `LostOdysseyRecomp.exe`** and choose your language and graphics settings.
3. **Import your game files** when prompted. The importer accepts an extracted folder, `default.xex`, an XDVDFS ISO or a GOD container. The game continues after import and shader preparation.

No Python or Visual Studio installation is needed for the release package. Later launches reuse the shader cache. Keep your save and profile folders when updating.

| Requirement | Supported configuration |
| :--- | :--- |
| System | Windows x64, AVX-capable CPU, Direct3D 12 graphics driver |
| Game data | Tested Asian multilingual edition; Disc 1 is required to start |
| Additional discs | Import with `InstallGame.exe`; later-disc progression is not fully verified |

See the [installation guide](docs/INSTALLING.md) for accepted disc versions, file locations and updating.

## In-game screenshots

| Ring combat | City exploration |
| :---: | :---: |
| ![Kaim attacking with the Ring timing interface](docs/images/ring-battle.png) | ![Exploring the industrial city](docs/images/city-exploration.png) |

*Unmodified screenshots from development builds leading up to v0.1.*

## Included in v0.1

| Feature | What to expect |
| :--- | :--- |
| Game importer | Folder, XEX, ISO and GOD input; original source files are copied |
| First-launch setup | Language and graphics settings before game initialization |
| Language settings | English, Japanese, Korean, Traditional and Simplified Chinese interface options; game language selection |
| Graphics settings | FXAA, output resolution and display-mode controls; fullscreen and mixed-DPI behavior need more testing |
| Shader preparation | Built-in resource index, parallel compilation and cache reuse |
| Input and debug | Controller and keyboard input; F1 menu with map information and same-map POI teleport |

DLSS and frame generation are **disabled placeholders**. Higher internal rendering resolutions, unlocked frame rates, HDR, Linux and Vulkan gameplay remain development targets.

## Development status

Reliable gameplay and faithful rendering come first. The project translates PowerPC code into C++ with **XenonRecomp**, implements Xbox 360 services on the host, and renders translated Xenos shaders through **plume**.

The v0.1 package passed hosted Windows CI, importer checks and an isolated cold launch. Ground-shadow and poster fixes have targeted validation; the user confirmed Ring timing works with RT. The accelerated-dialogue issue is resolved and user-confirmed; the repair passed comparison against the original audio in the tested vehicle scene.

**Open defects:** fire-hit and broken-crate effects, and intermittent GPU query/wait failures. Character-surface shadows, additional audio scenes and broader progression remain regression coverage. Two known shader-preparation failures remain. These results do not establish full-game compatibility.

[Detailed status and evidence](docs/STATUS.md) · [v0.1 release notes](docs/RELEASE-v0.1.md)

<details>
<summary><strong>Game edition and compatibility details</strong></summary>

Development uses the **Asian multilingual release**. Disc 1 has title ID `4D5307FA`, media ID `39F7D748`, title/base version `0.0.0.4` and region mask `0x00FFF900`. The importer checks the supported XEX hashes; a region label alone is insufficient.

Language options do not imply a complete playthrough in every language. Other regional executables, title updates and modified XEX files are not validated. See [edition evidence](docs/notes/xex.md).

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

| Action | Keyboard |
| :--- | :--- |
| Start / Back | Enter / Backspace |
| A / B / X / Y | Z / X / A / S |
| D-pad / left stick | Arrow keys / I, J, K, L |
| Left / right shoulder | Q / W |
| Debug menu | F1 |

Rumble is disabled by default; `LO_CONTROLLER_RUMBLE=1` enables it. Use a controller's right trigger for Ring actions; a shoulder binding is not a trigger binding.

### Development

| Directory | Contents |
| :--- | :--- |
| `LostOdysseyRecomp/` | Host kernel, graphics, audio, input and debugging |
| `LostOdysseyRecompLib/` | Configuration; ignored `private/` game data and generated `ppc/` code |
| `tools/` | Recompilers, dependency patches and Ghidra scripts |
| `thirdparty/` | Rendering, audio and other dependencies |
| `docs/` | Current status, guides, research and historical archives |

[Roadmap](docs/ROADMAP.md) · [Handoff](docs/notes/handoff.md) · [Rendering tests](docs/notes/rendering-validation.md) · [Audio](docs/notes/audio-output.md) · [Archive](docs/archive/README.md)

</details>

## Credits and game data

With research and tools from [UnleashedRecomp](https://github.com/hedge-dev/UnleashedRecomp), [re:Blue](https://github.com/zolaware/reblue), [XenonRecomp](https://github.com/hedge-dev/XenonRecomp), [XenosRecomp](https://github.com/hedge-dev/XenosRecomp), [plume](https://github.com/renderbag/plume) and [Xenia](https://github.com/xenia-project/xenia). Audio uses the pinned [Xenia FFmpeg fork](https://github.com/xenia-project/FFmpeg), with its [license](thirdparty/ffmpeg-LICENSE.txt).

Lost Odyssey and its assets belong to their respective owners. This is an unofficial project. Supply data extracted from your own discs; do not submit game executables, resource archives, textures, audio, video, generated game code or captures. Dependencies retain their respective licenses.
