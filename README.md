<div align="center">

<img src="assets/lost-odyssey-recomp.png" alt="Lost Odyssey Recomp logo" width="112">

# Lost Odyssey Recomp

**An experimental native PC port of Lost Odyssey for Xbox 360.**

[![Latest release](https://img.shields.io/github/v/release/freefrank/LostOdysseyRecomp?label=release)](https://github.com/freefrank/LostOdysseyRecomp/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/freefrank/LostOdysseyRecomp/total?label=downloads)](https://github.com/freefrank/LostOdysseyRecomp/releases)
[![Stars](https://img.shields.io/github/stars/freefrank/LostOdysseyRecomp?style=flat)](https://github.com/freefrank/LostOdysseyRecomp/stargazers)
[![License: GPL-3.0](https://img.shields.io/badge/license-GPL--3.0-blue)](LICENSE)
[![Last commit](https://img.shields.io/github/last-commit/freefrank/LostOdysseyRecomp?label=last%20commit)](https://github.com/freefrank/LostOdysseyRecomp/commits/main)
[![Open issues](https://img.shields.io/github/issues/freefrank/LostOdysseyRecomp?label=issues)](https://github.com/freefrank/LostOdysseyRecomp/issues)
[![Support on Ko-fi](https://img.shields.io/badge/Ko--fi-support-FF5E5B?logo=kofi&logoColor=white)](https://ko-fi.com/dotslash)

![Windows x64](https://img.shields.io/badge/Windows-x64-0078D6)
![Linux x64](https://img.shields.io/badge/Linux-x64-FCC624?logo=linux&logoColor=black)
![macOS arm64 (experimental)](https://img.shields.io/badge/macOS-arm64%20%28experimental%29-000000?logo=apple&logoColor=white)
![Android arm64 (experimental)](https://img.shields.io/badge/Android-arm64%20%28experimental%29-3DDC84?logo=android&logoColor=white)

![Direct3D 12](https://img.shields.io/badge/Direct3D-12-5E5E5E)
![Vulkan](https://img.shields.io/badge/Vulkan-AC162C?logo=vulkan&logoColor=white)
![Metal](https://img.shields.io/badge/Metal-147EFB)

### [Download](https://github.com/freefrank/LostOdysseyRecomp/releases/latest) · [Installation guide](docs/INSTALLING.md) · [简体中文](README.zh-CN.md)

[Changelog](CHANGELOG.md) · [Report an issue](https://github.com/freefrank/LostOdysseyRecomp/issues) · [Project board](https://github.com/users/freefrank/projects/3) · [Build from source](docs/BUILDING.md)

</div>

> [!IMPORTANT]
> **The port is still in early testing.** Opening areas and selected scenes have been tested; a complete playthrough has not. Rendering and stability issues remain. Supply your own supported game files.

## Contents

- [Start playing](#start-playing)
  - [macOS (experimental)](#macos-experimental)
  - [Android (experimental)](#android-experimental)
  - [HDR (experimental)](#hdr-experimental)
  - [Latest changes](#latest-changes)
- [Current features](#current-features)
- [Controls](#controls)
- [Debug menu](#debug-menu)
  - [Overview: captures and game actions](#overview-captures-and-game-actions)
  - [Teleport: positions within the current map](#teleport-positions-within-the-current-map)
  - [Cheats: speed and game-data tools](#cheats-speed-and-game-data-tools)
- [Files and folders](#files-and-folders)
- [Command-line options](#command-line-options)
- [Reporting a problem](#reporting-a-problem)
- [In-game screenshots](#in-game-screenshots)
- [Development](#development)
- [Sponsors](#sponsors)
- [Credits and game data](#credits-and-game-data)

## Start playing

Choose a package from the [latest release](https://github.com/freefrank/LostOdysseyRecomp/releases/latest). The current published version is **v0.8.44**.

| Platform | Package | First launch |
| :--- | :--- | :--- |
| Windows x64 | `LostOdysseyRecomp-windows-x64-v0.8.44.zip` | Extract the whole ZIP to a writable folder and run `LostOdysseyRecomp.exe`. Needs a CPU with AVX. |
| Linux x64 | `LostOdysseyRecomp-linux-x64-v0.8.44.AppImage` | Make it executable with `chmod +x`, then run it. |
| Linux x64 | `LostOdysseyRecomp-linux-x64-v0.8.44.flatpak` | Install the Freedesktop 26.08 runtime, then the bundle ([commands](docs/INSTALLING.md#flatpak)). |
| macOS arm64 (experimental) | `LostOdysseyRecomp-macos-arm64-v0.8.44.dmg` | Drag `LostOdysseyRecomp.app` to Applications. Needs an Apple Silicon Mac with macOS 15 or later. See [macOS](#macos-experimental) for the first launch. |
| Android arm64 (experimental) | `LostOdysseyRecomp-android-arm64-v0.8.44.apk` | Install the APK and open it once. Needs a 64-bit Android 8.0+ device with Vulkan. See [Android](#android-experimental). |

1. **Import your game data.** The importer opens when no game is found. Use **Files** or **Folder** to select an extracted game folder, `default.xex`, an ISO or GOD data.
2. **Choose the languages and graphics options.** On the first start the game offers to download precompiled shaders for your renderer; if you skip, it compiles them on your PC once.
3. **Add the other discs and DLC when you need them** in Settings → **Gameplay → Import discs & DLC**. With all four discs imported, the game switches discs on its own.

Disc 1 is required to start. Use one of the supported four-disc sets (Asian multilingual or USA/Europe) and don't mix editions; the [installation guide](docs/INSTALLING.md) shows how to check yours. Keep your saves and profiles when updating.

### macOS (experimental)

The app is not notarized, so macOS blocks the first launch. Try to open it once, then open **System Settings → Privacy & Security** and choose **Open Anyway**. The update check only opens the release page; replace the app yourself to update. Only one Mac has run the game so far. [Step-by-step guide](docs/INSTALLING.md#macos).

### Android (experimental)

- **Game data:** opening the app creates `Android/data/io.github.freefrank.lostodyssey/files/game/`. Copy your extracted `disc1`–`disc4` into it over USB (about 20 GB), or import disc images on the device with **Game folder → Import disc images…**. **CTRL → Game folder** can also point the game at any folder or an SD card.
- **Qualcomm devices:** the **GPU driver** page opens before the first start, because the phone's own driver draws some menu text invisible. Download a Turnip driver there. If a driver cannot run the game, the page tells you why.
- **Controls:** a controller hides the touch controls automatically. **CTRL** opens their size, opacity and layout settings. CTRL itself can be moved in the layout editor and follows the opacity setting. When you leave it alone for a few seconds, it slides to the nearest screen edge; tap the small tab to bring it back.
- **Updates:** the app checks for updates at startup; a new APK installs over the old one and keeps your saves.
- **Saves:** **CTRL → Saves** exports them to a ZIP and imports ZIPs from a PC, Xenia or the RGH save converter.
- **Bug reports:** attach the files from `Android/data/io.github.freefrank.lostodyssey/files/logs/` ([how](docs/INSTALLING.md#android-logs)).

Only one tablet has been tested so far. [Step-by-step guide](docs/INSTALLING.md#android).

### HDR (experimental)

Turn on **HDR** in Graphics and save; it switches right away (with frame generation on, after a restart). **HDR peak brightness** opens a calibration page: an SDR preview on the left, HDR on the right, and **LB / RB** switch between the game scene and a test pattern. **Auto** uses the brightness your display reports, or 1000 nits if it reports none. HDR works with every anti-aliasing mode and upscaler; with frame generation it stays on only for DLSS on Vulkan. Available on Windows (Direct3D 12 and Vulkan), Linux and macOS.

### Latest changes

[v0.8.44](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.8.44) keeps the status and navigation bars hidden on Android, lets you move the on-screen **CTRL** button and slides it to the screen edge when it is not used (#253), trims one full-resolution copy per frame from FSR, DLSS and XeSS upscaling (#172), and explains an incomplete disc image in the importer (#251). [v0.8.39](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.8.39) kept compiled pipelines on disk on Vulkan and DirectX 12 and cleaned up the shader cache. Earlier releases are in the [changelog](CHANGELOG.md).

## Current features

| Feature | What you get |
| :--- | :--- |
| Import | Folder, XEX, ISO and GOD sources, DLC and disc replacement. Your original files are not changed. |
| Languages | English, Japanese, Korean, Traditional Chinese and Simplified Chinese menus. Game languages depend on your edition. |
| Display | 16:9 and 21:9 resolutions; taller screens such as 16:10 and 4:3 are filled with the 3D scene. Off/FXAA/SMAA/TAA (experimental), DLSS, FSR 3.1, XeSS (Windows Direct3D 12) or MetalFX upscaling, and [HDR](#hdr-experimental). **Brightness / Gamma** in Graphics adjusts the picture next to the game's default, on the last game scene or a test pattern. |
| Shadows and AO | Shadow resolution 1×/2×/4× and experimental SSAO/GTAO. |
| Frame rate | 30/60/90/120 FPS targets and FreeSync / G-SYNC Compatible VRR. |
| Frame generation | Windows Direct3D 12: DLSS (the multipliers your GPU supports), FSR 2× or XeSS 2×. Windows Vulkan: DLSS 2×–6×. Changing the provider may need a restart. |
| Shaders | Precompiled shader download on first start; otherwise compiled once and cached. |
| Mods | Texture, menu and font replacements and PlayStation button prompts. See the [modding guide](docs/wiki/Modding.md). |
| Input | Controllers, keyboard and rumble; touch controls on Android. |
| Debug Menu | Render captures, Save Anywhere, No Random Encounters, Encounter Every Step, teleport, fast-forward and cheats. See [Debug menu](#debug-menu). |

Full playthroughs, later discs, Linux and macOS hardware, fullscreen and mixed-DPI setups still need testing. Planned work is in the [roadmap](docs/ROADMAP.md).

## Controls

Controllers and the keyboard work together for player 1. If your controller is not recognized, see the [input reference](docs/notes/controller-input.md).

| Game action | Keyboard |
| :--- | :--- |
| Start / Back | Enter / Backspace |
| A / B / X / Y | Z / X / A / S |
| D-pad / left stick | Arrow keys / I, J, K, L |
| Left / right shoulder | Q / W |
| Left / right trigger | E / R |
| Debug Menu | F1 |

Press **Back** to zoom the minimap. Hold it for about half a second to hide the minimap, and press it again to show it.

For Ring actions, use the controller's **right trigger** or **R**. **Vibration** in the Audio settings sets the rumble strength; at the minimum, rumble is off.

## Debug menu

Press **F1**, or **LB+RB** on a controller (**L1+R1** on PlayStation layouts), to open or close the Debug Menu. **The game pauses while it is open.** It has three pages: **Overview**, **Teleport** and **Cheats**, and uses the keyboard or a controller, not the mouse.

| Action | Keyboard | Controller |
| :--- | :--- | :--- |
| Select a row | ↑ / ↓ | D-pad up / down |
| Change a value | ← / → | D-pad left / right |
| Confirm | Enter | A |
| Return or close | Esc | B |
| Previous / next page | Q or Tab / E | LB / RB |
| Change Cheats category | Select the category row, then ← / → | LT / RT |
| Open or close the overlay | F1 | LB+RB |

### Overview: captures and game actions

**Overview** shows the current map and has the menu language, **Capture render state**, **Save Anywhere**, **No Random Encounters**, **Encounter Every Step**, and an action that wins the current battle.

To report a rendering problem, select **Capture render state**, confirm, then **close the menu** so rendering can continue. The capture saves an archive in `captures/` and shows its path. It contains screenshots, rendering data and logs; look through it before sharing.

**Save Anywhere** turns on the game's own **System → Save**. Close the Debug Menu, then open the System menu to save.

**No Random Encounters** stops random battles in the field. Story battles still happen.

**Encounter Every Step**, next to it, starts a random battle on every step in areas that have random battles. It turns off when you restart the game, and turning one of these two switches on turns the other off.

> [!WARNING]
> **Keep a normal save too.** Loading a Save Anywhere save made while the party is split can lose RB character switching ([#74](https://github.com/freefrank/LostOdysseyRecomp/issues/74)). Save Anywhere now stays off while the party is split, and **Force RB Party Switch** in the F1 menu repairs an older save of this kind.

### Teleport: positions within the current map

**Teleport** has a position bookmark, editable X/Y/Z coordinates and the map's points of interest. On the coordinate row, press **Enter** to pick X, Y or Z and **←/→** to move it. Confirm a teleport or a point of interest, then close the menu to move.

**Debug Event Room**, at the bottom of the page, jumps to the game's own event-debug map (z0g_9) when you close the menu. Hold **LB** and press **Up** there to open Scenario Jump. It works only while you control a character on a map.

### Cheats: speed and game-data tools

| Category | What it does |
| :--- | :--- |
| **Quick tools** | Fast-forward, **Allow memory edits**, gold and HP/MP. |
| **Characters** | HP/MP, EXP (0–99, not the level) and skills. |
| **Inventory** | Set items and materials to 1, 10, 50 or 99, or fill whole categories. Sort the in-game inventory to refresh it. |
| **Equipment** | Experimental weapon, ring and accessory changes. |
| **Party** | Experimental party members, rows and field character. Some changes need a reload. |
| **Developer** | Experimental access to the original **EDIT MENU**: enable it, close F1, press **LT+RT**. Turn it off afterwards. |

**Fast-forward** needs a controller or Android's on-screen controls: hold **LT** (**Hold**) or press it to toggle (**Toggle**), at 2×–8×. It pauses while a menu is open.

**Memory edits** are off by default. Back up your save first and stand somewhere you can move, outside battle. Turn on **Allow memory edits**, choose an action, confirm **Yes**, then close F1 so it runs. Edited values can end up in your normal saves.

## Files and folders

The Windows ZIP is **portable**: everything stays in the folder you extracted it to. The AppImage, Flatpak and macOS app keep your files in your user folders:

| Package | Settings | Saves, cache and game data | Logs |
| :--- | :--- | :--- | :--- |
| Windows ZIP | folder with `LostOdysseyRecomp.exe` | same | same |
| Linux AppImage | `~/.config/lost-odyssey-recomp/` | `~/.local/share/lost-odyssey-recomp/` | `~/.local/state/lost-odyssey-recomp/` |
| Linux Flatpak | `~/.var/app/io.github.freefrank.LostOdysseyRecomp/config/lost-odyssey-recomp/` | `~/.var/app/io.github.freefrank.LostOdysseyRecomp/data/` | `~/.var/app/io.github.freefrank.LostOdysseyRecomp/.local/state/lost-odyssey-recomp/` |
| macOS app | `~/Library/Application Support/LostOdysseyRecomp/` | same | `~/Library/Logs/LostOdysseyRecomp/` |
| Android | inside the app | inside the app; game data in `Android/data/io.github.freefrank.lostodyssey/files/game/` | `Android/data/io.github.freefrank.lostodyssey/files/logs/` |

| What | Where | Notes |
| :--- | :--- | :--- |
| Saves | `save/` | Keep when updating. Xenia and Xbox 360 saves can be [imported](docs/INSTALLING.md#importing-saves). |
| Profiles | `profile/` | Keep when updating. |
| Settings | `settings.ini` | Delete it to start over with default settings. |
| Imported game | `game/` with `disc1/`–`disc4/` and `dlc/` | The importer's default location; `game-path.txt` remembers another one. |
| Shader cache | `cache/shaders/` | Rebuilt if deleted. |
| Downloaded shaders | `shaders/` | |
| Logs | `logs/runtime-*.log` | The last three runs are kept. |
| Render captures | `captures/` | |
| Mods | `mods/` | |

More detail is in [file locations](docs/INSTALLING.md#file-locations).

## Command-line options

| Option | Effect |
| :--- | :--- |
| `--game <path>` | Use this game folder (with `default.xex` or `disc1/`) or `default.xex` file, skipping the importer. |
| `--install` | Open the importer even when a game is already set up. |
| `--setup` | Windows: run the first-launch setup again, then start the game. |
| `--prepare-shaders-only` | Prepare all shaders, then exit without starting the game. |

Write `--game <path>` as two arguments; `--game=<path>` is ignored. The program prints nothing to a console, so check the log.

```bash
LostOdysseyRecomp.exe --game "D:\Games\Lost Odyssey"
./LostOdysseyRecomp-linux-x64-v0.8.44.AppImage --game ~/Games/LostOdyssey
flatpak run io.github.freefrank.LostOdysseyRecomp --game ~/Games/LostOdyssey
```

| Environment variable | Effect |
| :--- | :--- |
| `LO_GRAPHICS_API` | `d3d12` or `vulkan` on Windows. |
| `LO_FPS` | Frame-rate cap; `0` means uncapped. |
| `LO_NO_UPDATE=1` | Skip the update check. |
| `LO_MODS=0` | Disable mods. |
| `LO_AUDIO_MUTE=1` | Mute audio. |
| `LO_CONTROLLER_RUMBLE=0` | Turn rumble off. |
| `LO_OPTISCALER_PATH` | Experimental: load your own `OptiScaler.dll` on Windows ([setup](docs/notes/vulkan-fg-fsr4-metalfx.md#optional-optiscaler-loading-on-windows)). |

## Reporting a problem

[Open an issue](https://github.com/freefrank/LostOdysseyRecomp/issues) with the version, operating system, graphics backend, GPU and driver, game edition and disc, and the steps or scene that show the problem. Attach the newest `logs/runtime-<timestamp>.log`.

For a visual problem, make a [render capture](#overview-captures-and-game-actions) while it is on screen, and look through the archive before sharing it. Do not attach game files, saves or personal data.

Optional diagnostics are off by default; see [Privacy](PRIVACY.md).

## In-game screenshots

<img src="docs/images/title-screen.png" alt="Lost Odyssey title screen — Press START" width="960">

| Ring combat | City exploration |
| :---: | :---: |
| ![Kaim attacking with the Ring timing interface](docs/images/ring-battle.png) | ![Exploring the industrial city](docs/images/city-exploration.png) |

*Unmodified screenshots from development builds leading up to v0.1.*

## Development

See [Building](docs/BUILDING.md) for dependencies and build commands, [Developer tools](tools/README.md) for the utilities, [development status](docs/STATUS.md) for validation records, and the [documentation index](docs/README.md) for everything else.

## Sponsors

Thank you to **Frenzy Fresh**, **José Antonio Martínez Godoy**, **C_BAR**, **Arakon**, **Efren V**, **Torresmo**, **doc_haz**, **Whitesun** and **Cristian** for supporting the project on [Ko-fi](https://ko-fi.com/dotslash).

## Credits and game data

Thanks to everyone who sent pull requests and patches: [MikeRavenelle](https://github.com/MikeRavenelle), [dj5927](https://github.com/dj5927), [Xarishark](https://github.com/Xarishark), [navjack](https://github.com/navjack), [frankzzz](https://github.com/frankzzz) and [cngjd](https://github.com/cngjd). The macOS port started from MikeRavenelle's Apple Silicon work.

With research and tools from [UnleashedRecomp](https://github.com/hedge-dev/UnleashedRecomp), [re:Blue](https://github.com/zolaware/reblue), [XenonRecomp](https://github.com/hedge-dev/XenonRecomp), [XenosRecomp](https://github.com/hedge-dev/XenosRecomp), [plume](https://github.com/renderbag/plume) and [Xenia](https://github.com/xenia-project/xenia). Audio uses the [Xenia FFmpeg fork](https://github.com/xenia-project/FFmpeg) ([license](thirdparty/ffmpeg-LICENSE.txt)). The Android Turnip GPU drivers come from the driver list of the [Eden](https://git.eden-emu.dev/eden-emu/eden) emulator and load through [libadrenotools](https://github.com/bylaws/libadrenotools).

Lost Odyssey and its assets belong to their respective owners. This is an unofficial project. Supply data extracted from your own discs; do not commit game executables, resource archives, textures, audio, video, generated game code or captures to this repository. Dependencies retain their respective licenses.
