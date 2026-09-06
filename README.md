<div align="center">

# Lost Odyssey Recompiled

**An experimental native PC port of Lost Odyssey for Xbox 360.**

Windows x64 · Direct3D 12 · PowerPC static recompilation

<img src="docs/images/title-screen.png" alt="Lost Odyssey title screen — Press START" width="960">

### [Download v0.2.1](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.2.1) · [Installation guide](docs/INSTALLING.md) · [Report an issue](https://github.com/freefrank/LostOdysseyRecomp/issues)

[简体中文](README.zh-CN.md) · [Project status](docs/STATUS.md) · [Roadmap](docs/ROADMAP.md) · [Build from source](docs/BUILDING.md)

</div>

> [!IMPORTANT]
> **v0.2.1 is an early testing release.** Opening areas and selected scenes have been tested; a complete playthrough has not. Rendering and stability issues remain. You must supply your own supported game files.

## New in v0.2.1

**v0.2.1 is released.** It adds F1 next-frame render capture with automatic ZIP output and simultaneous SDL-mapped controllers/keyboard input, including E/R triggers. Capture is diagnostic, not an AMD fix.

## New in v0.2

**v0.2 is released.** It adds the audited USA, Europe four-disc edition, edition-specific game and voice language choices, import before first-run setup, and automatic access to imported discs. Once all four discs are imported, no manual disc swap is required. Both editions passed controlled original-manager switching tests; chapter-boundary gameplay and a complete playthrough remain unverified. See the [v0.2 release notes](docs/RELEASE-v0.2.md).

## Start playing

1. **Download and extract** the entire Windows release ZIP to a writable folder.
2. **Run `LostOdysseyRecomp.exe`** and import your game files when prompted. The importer accepts an extracted folder, `default.xex`, an XDVDFS ISO or a GOD container.
3. **Choose your language and graphics settings.** The game continues after setup and shader preparation.

No Python or Visual Studio installation is needed for the release package. Later launches reuse the shader cache. Keep your save and profile folders when updating.

| Requirement | Supported configuration |
| :--- | :--- |
| System | Windows x64, AVX-capable CPU, Direct3D 12 graphics driver |
| Game data | Audited Europe, Asia or USA, Europe edition; Disc 1 is required to start |
| Additional discs | Import with `InstallGame.exe`; later-disc progression is not fully verified |

See the [installation guide](docs/INSTALLING.md) for accepted disc versions, file locations and updating.

## In-game screenshots

| Ring combat | City exploration |
| :---: | :---: |
| ![Kaim attacking with the Ring timing interface](docs/images/ring-battle.png) | ![Exploring the industrial city](docs/images/city-exploration.png) |

*Unmodified screenshots from development builds leading up to v0.1.*

## Included in v0.2

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

The v0.2 package passed hosted Windows CI, manifest and importer checks, and a 30-second isolated rendered launch. Ground-shadow and poster fixes have targeted validation; the user confirmed Ring timing works with RT. The accelerated-dialogue issue is resolved and user-confirmed; the repair passed comparison against the original audio in the tested vehicle scene.

**Open defects:** fire-hit and broken-crate effects, and intermittent GPU query/wait failures. Character-surface shadows, additional audio scenes and broader progression remain regression coverage. Two known shader-preparation failures remain. These results do not establish full-game compatibility.

[Detailed status and evidence](docs/STATUS.md) · [v0.2 release notes](docs/RELEASE-v0.2.md)

<details>
<summary><strong>Game edition and compatibility details</strong></summary>

The two supported editions correspond to [Lost Odyssey (Europe, Asia) (En,Ja,Zh,Ko) (Disc 1), Redump 39111](https://redump.info/disc/39111) and [Lost Odyssey (USA, Europe) (En,Ja,Fr,De,Es,It) (Disc 1), Redump 11817](https://redump.info/disc/11817). The former is called the Asian edition here: Disc 1 has title ID `4D5307FA`, media ID `39F7D748`, title/base version `0.0.0.4` and XeMID `MS204204H0X14`. The USA, Europe Disc 1 has media ID `368DE6DD`, version `0.0.0.3` and XeMID `MS204203W0X14`. These identities match the audited sets; a complete ISO hash comparison against Redump has not been performed. The importer strictly checks each supported XEX hash; a region label alone is insufficient.

v0.2 also supports the audited **USA, Europe version 0.0.0.3** four-disc set, with strict XEX checks and protection against mixing editions. Game-language choices follow the installed edition: English/Japanese/German/French/Spanish/Italian for USA, Europe; the audited Europe, Asia resources retain English/Japanese/Korean/Traditional Chinese/Simplified Chinese choices. The Redump Zh label does not independently establish that Simplified Chinese is present in every retail copy. The settings interface retains its existing five translations. **This support is not included in v0.1.** See [USA, Europe support and validation](docs/notes/europe-support.md).

v0.2 automatically selects an already imported disc when the original game requests it, with no disc-selection button required. With all four discs imported, players do not need to swap discs manually. Storage tests and controlled original-manager 1 → 2 → 3 → 4 → 1 sequences passed for both audited editions. Chapter-boundary gameplay remains unverified. This feature is not included in v0.1; see [disc-selection evidence](docs/notes/disc-selection.md).

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

| Action | Keyboard |
| :--- | :--- |
| Start / Back | Enter / Backspace |
| A / B / X / Y | Z / X / A / S |
| D-pad / left stick | Arrow keys / I, J, K, L |
| Left / right shoulder | Q / W |
| Left / right trigger (v0.2.1) | E / R |
| Debug menu | F1 |

**v0.2.1 input update:** all SDL-mapped controllers and the keyboard can be used without selecting an active device. Inputs merge into player 1; this is not multiplayer support. Hotplug and mixed input passed SDL virtual-device tests; physical models and gameplay switching still need validation. Unmapped joysticks need an SDL controller mapping. See [input evidence](docs/notes/controller-input.md).

Rumble is disabled by default; `LO_CONTROLLER_RUMBLE=1` enables it. For Ring actions, published v0.2 uses the controller's right trigger; v0.2.1 also maps R to RT. A shoulder binding is not a trigger binding.

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
