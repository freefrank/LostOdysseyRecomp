<div align="center">

# Lost Odyssey Recomp

**An experimental native PC port of Lost Odyssey for Xbox 360.**

Windows x64 · Direct3D 12 · Vulkan · PowerPC static recompilation

<img src="docs/images/title-screen.png" alt="Lost Odyssey title screen — Press START" width="960">

Optional diagnostics are off by default and can be disabled in Settings. See [Privacy](PRIVACY.md).

### [Download v0.5.1](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.1) · [Installation guide](docs/INSTALLING.md) · [Report an issue](https://github.com/freefrank/LostOdysseyRecomp/issues)

[简体中文](README.zh-CN.md) · [Changelog](CHANGELOG.md) · [Projects](https://github.com/users/freefrank/projects/3) · [Build from source](docs/BUILDING.md)

</div>

> [!IMPORTANT]
> **This project is still in early testing.** Opening areas and selected scenes have been tested; a complete playthrough has not. Rendering and stability issues remain. You must supply your own supported game files.

## New in v0.5.2 (unreleased)

- Include original VS/PS shader programs in F1 render captures.
- Add optional incremental diagnostic uploads in the background, also triggered after F1 capture, without waiting on the game thread.

Earlier release details are maintained in the [changelog](CHANGELOG.md).

## Start playing

1. **Download and extract** the entire Windows release ZIP to a writable folder.
2. **Run `LostOdysseyRecomp.exe`** and import your game files when prompted. The importer accepts an extracted folder, `default.xex`, an XDVDFS ISO or a GOD container.
3. **Choose your language and graphics settings.** The game continues after setup and shader preparation.

No Python or Visual Studio installation is needed for the release package. Later launches reuse the shader cache. Keep your save and profile folders when updating.

The updater checks GitHub's latest Release. A higher numeric version updates normally; an equal numeric version with a different `-suffix`, such as `0.5.0-hotfix1`, also triggers an update. The installed build must contain this updater policy before it can take effect.

| Requirement | Supported configuration |
| :--- | :--- |
| System | Windows x64, AVX-capable CPU, Direct3D 12 or Vulkan graphics driver |
| Game data | Audited Europe, Asia or USA, Europe edition; Disc 1 is required to start |
| Additional discs | Import with `InstallGame.exe`; later-disc progression is not fully verified |

See the [installation guide](docs/INSTALLING.md) for accepted disc versions, file locations and updating.

## In-game screenshots

| Ring combat | City exploration |
| :---: | :---: |
| ![Kaim attacking with the Ring timing interface](docs/images/ring-battle.png) | ![Exploring the industrial city](docs/images/city-exploration.png) |

*Unmodified screenshots from development builds leading up to v0.1.*

## Current features

| Feature | What to expect |
| :--- | :--- |
| Game importer | Folder, XEX, ISO and GOD input; original source files are copied |
| First-launch setup | Language and graphics settings before game initialization |
| Language settings | English, Japanese, Korean, Traditional and Simplified Chinese interface options; game language selection |
| Graphics settings | Auto/manual internal resolution up to 4K, Off/FXAA/SMAA/experimental TAA, Standard/High filtering, 30/60 FPS and output/display controls; fullscreen and mixed DPI need more testing |
| Settings menu | Original game fonts and menu styling; one-click Graphics save/apply and Now/Later restart choices |
| Shader preparation | Built-in resource index, parallel compilation and cache reuse |
| CPU use | Reduced unnecessary polling and reuse of rendering work |
| Input and debug | Controller and keyboard input; English/Simplified Chinese F1 menu with capture, map information and same-map POI teleport |

Open `InstallGame.exe` and choose **Files** or **Folder** to import game discs and supported DLC. See the [installation guide](docs/INSTALLING.md#automatic-content-import).

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

| Action | Keyboard |
| :--- | :--- |
| Start / Back | Enter / Backspace |
| A / B / X / Y | Z / X / A / S |
| D-pad / left stick | Arrow keys / I, J, K, L |
| Left / right shoulder | Q / W |
| Left / right trigger | E / R |
| Debug menu | F1 |

SDL-mapped controllers and the keyboard can be used together for player 1. Unmapped joysticks need an SDL controller mapping. See [input details](docs/notes/controller-input.md).

Rumble is disabled by default; `LO_CONTROLLER_RUMBLE=1` enables it. For Ring actions, use the controller's right trigger or the R key.

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
