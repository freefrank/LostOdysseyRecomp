# Installing Lost Odyssey Recomp

[简体中文](INSTALLING.zh-CN.md)

Download the package for your platform, import the game from your own discs, and keep your saves when you update. The downloads contain no game files; disc 1 is required to start.

## Contents

- [Windows quick start](#windows-quick-start)
- [Importing game data](#automatic-content-import)
- [Adding or replacing discs and DLC](#adding-or-replacing-discs-and-dlc)
- [Linux packages](#running-on-linux)
  - [AppImage](#appimage)
  - [Flatpak](#flatpak)
- [macOS (Apple Silicon)](#macos)
- [Android](#android)
- [First launch and settings](#first-launch-and-settings)
  - [Shader preparation](#shader-preparation)
- [File locations](#file-locations)
- [Importing saves from Xenia or an Xbox 360](#importing-saves)
  - [From Xenia](#xenia-saves)
  - [From an Xbox 360 (RGH)](#console-saves)
- [Command-line options](#command-line-options)
  - [How the game is found](#how-the-game-is-found)
- [Updating and keeping user data](#updating-and-keeping-user-data)
- [Reporting a startup or rendering failure](#reporting-a-startup-or-rendering-failure)

## Windows quick start

You need Windows x64 and a CPU with AVX. The game uses Direct3D 12 by default; Vulkan can be chosen in the settings.

1. Download `LostOdysseyRecomp-windows-x64-v0.9.0.zip` from the [latest release](https://github.com/freefrank/LostOdysseyRecomp/releases/latest).
2. Extract the whole ZIP to a writable folder outside `Program Files`.
3. Run `LostOdysseyRecomp.exe`. The importer opens when no game is found.
4. Choose the languages and graphics options. The game may offer to download precompiled shaders first; see [Shader preparation](#shader-preparation).

<a id="automatic-content-import"></a>

## Importing game data

On the importer's source page, choose **Files** to pick files or **Folder** to scan a folder. The importer recognizes discs and DLC on its own; check what it found, then confirm.

<a id="supported-sources"></a>

Supported sources:

- an extracted game folder, or its `default.xex`;
- an ISO image;
- Games on Demand (GOD) data: the header file, its `.data` folder, or a folder with several discs.

Supported editions (Title ID `4D5307FA`):

| Edition | Version | Media IDs for discs 1–4 |
|---|---:|---|
| Asian multilingual | 4 | `39F7D748`, `0EF8CEA8`, `309E3386`, `7B21A91D` |
| USA/Europe | 3 | `368DE6DD`, `1888BE4E`, `6DD59D08`, `0C0E80B5` |

Do not mix discs from the two editions. Other regional versions, title updates and modified XEX files are not supported. With all four discs imported, the game changes discs on its own.

## Adding or replacing discs and DLC

Open the importer again from **Settings → System → Import discs & DLC**. Choose the discs or DLC to add or replace, check the result and confirm. Your other discs, saves and settings stay as they are. DLC can be selected directly or found in a scanned folder; it is not part of the download.

The importer copies your files and never moves or changes them, so keep the originals until the game works. If an import is cancelled or fails, run it again.

On the destination page, choose **New folder**, press **F2** or controller **Y** to create a folder. By default the game goes to `game/disc1`–`game/disc4` and DLC to `game/dlc/`; if you choose another folder, `game-path.txt` remembers it.

<a id="running-on-linux"></a>

## Linux packages

Linux uses Vulkan. Steam Deck and other Linux hardware have had limited testing. To build from source, see [BUILDING.md](BUILDING.md).

### AppImage

Download `LostOdysseyRecomp-linux-x64-v0.9.0.AppImage`, then run:

```bash
chmod +x LostOdysseyRecomp-linux-x64-v0.9.0.AppImage
./LostOdysseyRecomp-linux-x64-v0.9.0.AppImage
```

Import the game in the importer, or start it with a game folder directly:

```bash
./LostOdysseyRecomp-linux-x64-v0.9.0.AppImage --game /path/to/game
```

The AppImage keeps saves and settings in your user folders; see [file locations](#file-locations).

### Flatpak

Install the Freedesktop 26.08 runtime from Flathub once:

```bash
flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak install --user flathub org.freedesktop.Platform//26.08
```

Then install and run the downloaded bundle:

```bash
flatpak --user install --bundle LostOdysseyRecomp-linux-x64-v0.9.0.flatpak
flatpak run io.github.freefrank.LostOdysseyRecomp
```

The importer can read dumps anywhere on your computer, including `/media`, `/run/media` and `/mnt`. To update, download the new bundle and install it the same way.

<a id="macos"></a>

## macOS (Apple Silicon)

You need an Apple Silicon Mac with macOS 15 or later. The app is not notarized, so macOS blocks the first launch.

1. Download `LostOdysseyRecomp-macos-arm64-v0.9.0.dmg` and open it.
2. Drag `LostOdysseyRecomp.app` onto the Applications link, then eject the disk image.
3. Open `LostOdysseyRecomp` from Applications. macOS blocks it the first time. Open **System Settings → Privacy & Security**, scroll to Security and choose **Open Anyway** next to the app (the button appears only after a blocked launch). Choose **Open** when macOS asks again; it may ask for your password. Later launches need no approval.
4. The importer opens when no game is found; see [Importing game data](#automatic-content-import).

To approve the app from Terminal instead, run `xattr -dr com.apple.quarantine /Applications/LostOdysseyRecomp.app`, then open it again.

The first start may offer to download the Metal shaders; see [Shader preparation](#shader-preparation). To update, download the new disk image and replace the app in Applications; your saves and settings are stored outside the app and stay. Only one Mac has run the game so far.

<a id="android"></a>

## Android

You need a 64-bit Android 8.0 or newer device with Vulkan and about 20 GB of free space for the four discs.

1. Download the APK on the device, open it and allow installs from your browser or file manager when Android asks. Open the app once: it creates the folder `Android/data/io.github.freefrank.lostodyssey/files/game/` on the internal storage and on every SD card.
2. Put the game on the device in one of three ways:
   - **Import on the device:** **Game folder → Import disc images…** imports disc images (`.iso`) or extracted discs from the internal storage or an SD card, then starts the game. It needs "All files access", which the page asks for.
   - **Copy from a PC:** import the game with the PC version, connect the device over USB in file-transfer mode and copy `disc1`–`disc4` (and `dlc/`) into that `game` folder, so that `game/disc1/default.xex` exists.
   - **Use another folder:** file managers on the device usually cannot write into `Android/data`, so copy the discs anywhere else with one, then choose **Game folder → Choose folder…** and pick the folder that holds `disc1`. This also needs "All files access".
3. Open the app. On Qualcomm devices the **GPU driver** page appears first, because the phone's own driver makes some menu text invisible: download a Turnip driver there (the page shows the one recommended for your model) and press **Start game**. You can come back later from **CTRL → GPU driver**. If a driver cannot run the game, the game returns to this page and shows why.
4. Accept the shader download with the on-screen **A** button. **B** skips it and compiles the shaders on the device, which takes minutes.

Touch controls appear over the game. **CTRL** opens their size, opacity and layout settings, the saves page, the game folder page and the GPU driver page. A USB or Bluetooth controller hides the touch controls automatically. Settings are changed on the in-game Settings page.

With **Automatic updates** on, the app checks for a new version at startup and offers the APK download. Install the new APK over the old one; saves and settings stay. If you installed an APK you built yourself, uninstall it first. Uninstalling the app deletes your saves and the game data in `Android/data`.

<a id="android-saves"></a>
**Saves** stay inside the app, where file managers cannot reach them. **CTRL → Saves** exports every slot to one ZIP file you choose, for example in Download, and imports a ZIP of save folders: one exported here or from a PC, a ZIP of Xenia's `userNN` folders, or the download from the [RGH save converter](#console-saves). Export your saves before uninstalling the app.

<a id="android-logs"></a>
**Logs** are in `Android/data/io.github.freefrank.lostodyssey/files/logs/`: `runtime-*.log`, `native-stderr.log` and, if the app itself failed, `java-crash-*.txt`. If the game crashes or stays black, open the app once more, then copy these files to a PC over USB and attach them to your report. With adb, `adb logcat -s LostOdyssey` shows the same lines live.

Only one tablet has been tested so far.

## First launch and settings

On Windows, the first-launch page sets the interface language, game language and graphics options; run `LostOdysseyRecomp.exe --setup` to open it again. Linux and macOS start with default settings, which you change on the in-game Settings page. Settings tells you when a change needs a restart.

In Graphics, **Display mode** is Windowed or Fullscreen. On a PC with several monitors or graphics cards, **Display** picks the monitor and **GPU** the graphics card (a GPU change applies after a restart). After you switch the display, the game asks whether to keep it and goes back after 5 seconds without an answer. On Windows, Win+Shift+Left/Right also moves the game to the next monitor. **Aspect ratio** is Auto, where the game fills the window, or 16:9, 21:9 or 4:3, which keep that shape and add black bars when the screen has another shape. In Gameplay, **Vibration** sets the rumble strength. In Audio, **Audio output** chooses Stereo or 5.1 surround; for 5.1, set your speakers to 5.1 or 7.1 in the system sound settings first, otherwise the game stays on stereo. On Windows that option is only in Control Panel → Sound → Playback: select the device, click Configure and choose 5.1 or 7.1 Surround. **Matrix surround** needs no speaker setup: it encodes the 5.1 mix into stereo for an AV receiver's Pro Logic II, Dolby Surround or Neural:X mode, and **Matrix phase** sets the phase of the rear channels (90° by default); while that row is selected, test noise circles the speakers.

The interface language and the game language are separate. USA/Europe discs have English, Japanese, German, French, Spanish and Italian; the Asian set has English, Japanese, Korean, Traditional Chinese and Simplified Chinese. A game language your discs don't have falls back to English.

Press **F1** or **LB+RB** for the Debug Menu; see the [README](../README.md#debug-menu).

### Shader preparation

The first time you start the game with a renderer, it offers to download precompiled shaders for it, with the download size shown.

- **Download (A)** gets them from GitHub and saves several minutes of compiling. **Cancel (B)** stops the download.
- **Skip (B)** compiles the shaders on your PC instead. The game remembers this until the shaders change. Closing the window asks again at the next start.

After an update that changes the shaders, the first start offers the download again. Offline, the game compiles the shaders without asking. Later launches reuse them.

## File locations

| Package | Locations |
|---|---|
| Windows ZIP | Everything beside `LostOdysseyRecomp.exe`: `save/`, `profile/`, `cache/`, `logs/`, `settings.ini`, `game-path.txt`, and imported games in `game/`. |
| Linux AppImage | Saves, profiles, cache and games: `~/.local/share/lost-odyssey-recomp/`. Settings: `~/.config/lost-odyssey-recomp/`. Logs: `~/.local/state/lost-odyssey-recomp/logs/`. |
| Linux Flatpak | Under `~/.var/app/io.github.freefrank.LostOdysseyRecomp/`: saves, profiles, cache and games in `data/` (`/var/data` inside the sandbox); settings in `config/lost-odyssey-recomp/`; logs in `.local/state/lost-odyssey-recomp/logs/`. |
| macOS | Saves, profiles, cache, games and settings: `~/Library/Application Support/LostOdysseyRecomp/`. Logs: `~/Library/Logs/LostOdysseyRecomp/logs/`. |
| Android | Game: `Android/data/io.github.freefrank.lostodyssey/files/game/` (or the folder chosen on the **Game folder** page). Logs: `Android/data/io.github.freefrank.lostodyssey/files/logs/`. Saves and settings stay inside the app; **CTRL → Saves** exports and imports saves. |

Render captures go to `captures/` and mods to `mods/`: beside the program for the Windows ZIP, otherwise captures in the settings folder and mods in the data folder. Downloaded shaders go to `shaders/` in the same place as mods. To manage mods with Mod Organizer 2, see [Mod Organizer 2](wiki/Mod-Organizer-2.md).

On Linux, `XDG_CONFIG_HOME`, `XDG_DATA_HOME` and `XDG_STATE_HOME` move the AppImage folders. A Linux build in a writable folder keeps everything beside the program, like the Windows ZIP. When you start the Windows ZIP with `--game`, saves and settings follow the folder you start from, so always start from the same folder.

<a id="importing-saves"></a>

## Importing saves from Xenia or an Xbox 360

Each save slot is a folder in `save/`, for example `save/user00/save.bin`; see [file locations](#file-locations) for where `save/` is. Close the game and back up `save/` first. The copied saves then appear in the game's load list. On Android, import a ZIP of the save folders with **CTRL → Saves** instead ([details](#android-saves)).

<a id="xenia-saves"></a>

### From Xenia

Xenia saves have the same format and need no conversion.

1. Find Xenia's `content` folder: beside the Xenia program for a portable Xenia, otherwise `Documents\Xenia\content`. Xenia Canary is portable by default, and Xenia Manager installs it that way, so look in the Xenia folder first.
2. In it, open `4D5307FA\00000001`. Xenia Canary puts a 16-digit profile folder in between: `content\<profile ID>\4D5307FA\00000001`.
3. Each `userNN` folder there is one save. Copy the ones you want into `save/`.

A folder with the same name replaces that slot. To keep both, rename the copy to an unused number, such as `user07`.

<a id="console-saves"></a>

### From an Xbox 360 (RGH)

On the console, each save is one file, usually `user00`, in `Content\<profile ID>\4D5307FA\00000001\` on the hard drive. Copy it to your computer, for example over FTP.

1. Open the [save converter](https://freefrank.github.io/LostOdysseyRecomp/) in a browser. It converts on your computer and uploads nothing.
2. Choose the save file, or a ZIP that contains it, and an empty destination slot.
3. Select **Convert save** and download the ZIP.
4. On Windows, extract the ZIP beside `LostOdysseyRecomp.exe`. Elsewhere, copy the `userNN` folder from the ZIP's `save` folder into your `save/`.

Saves cannot be moved back to a console.

## Command-line options

| Option | Effect |
| :--- | :--- |
| `--game <path>` | Use this game: a folder with `default.xex` or `disc1/`, or the `default.xex` file. Skips the importer; exits with an error if no `default.xex` is found. |
| `--install` | Open the importer even when a game is set up, then exit. |
| `--setup` | Run the first-launch setup again, then start the game (Windows; elsewhere it only saves the current settings). |
| `--setup-only` | Like `--setup`, then exit. |
| `--prepare-shaders-only` | Prepare all shaders, then exit without starting the game. |
| `--quiet-kernel` | Leave kernel trace lines out of the log. |

Write `--game <path>` as two arguments; `--game=<path>` and unknown arguments are ignored. The program prints nothing to a console; check the log.

```bash
LostOdysseyRecomp.exe --game "D:\Games\Lost Odyssey"
./LostOdysseyRecomp-linux-x64-v0.9.0.AppImage --game ~/Games/LostOdyssey
flatpak run io.github.freefrank.LostOdysseyRecomp --game ~/Games/LostOdyssey
LostOdysseyRecomp.app/Contents/MacOS/LostOdysseyRecomp --game ~/Games/LostOdyssey
```

Environment variables override the saved settings for one run:

| Variable | Effect |
| :--- | :--- |
| `LO_GRAPHICS_API` | `d3d12` or `vulkan` on Windows. |
| `LO_FPS` | Frame-rate cap from 0 to 1000; `0` means uncapped. |
| `LO_FG_PROVIDER`, `LO_FG_MODE`, `LO_FG_MULTIPLIER`, `LO_FG_TARGET_FPS` | Frame generation: `off`/`dlss`/`fsr`/`xess`; `off`/`fixed`/`dynamic`; 2–6; target FPS ([details](notes/vulkan-fg-fsr4-metalfx.md)). |
| `LO_OPTISCALER_PATH` | Windows: full path to your own `OptiScaler.dll` ([setup](notes/vulkan-fg-fsr4-metalfx.md#optional-optiscaler-loading-on-windows)). |
| `LO_NO_UPDATE` | Any value other than `0` skips the update check. |
| `LO_PROFILE_DIR`, `LO_SHADER_CACHE_DIR`, `LO_MODS_DIR` | Use another profile, shader cache or mods folder. An empty `LO_SHADER_CACHE_DIR` turns the shader cache off. |
| `LO_MODS` | `0` or `false` disables mods. |
| `LO_LOG_FILE` | Write the log to this path, or `0` for no log file. |
| `LO_AUDIO_MUTE`, `LO_CONTROLLER_RUMBLE` | `LO_AUDIO_MUTE=1` mutes audio; `LO_CONTROLLER_RUMBLE=0` turns rumble off. |
| `LO_TRACE_STARTUP` | Windows: `1` adds startup details to the log: graphics adapters, add-on software loaded into the game (overlays, capture tools), and window and swap chain timing. |

### How the game is found

When `--game` is not given:

1. The program reads `game-path.txt`.
2. If that file is absent, it looks for `default.xex` in the data folder's `game/` (per-user packages only).
3. Then it checks `game/`, the program folder and `../game` next to the program.
4. If nothing is found, the importer opens.

## Updating and keeping user data

The game checks GitHub for a new version at startup; turn off **Automatic updates** in the settings if you prefer to check yourself. On Windows and the AppImage it can update itself. Flatpak, macOS and Android packages are updated by installing the new download.

Keep these when updating:

- saves, profiles and `settings.ini`;
- `logs/` and the shader cache;
- `game-path.txt` and the imported game, if they are beside the program.

To update by hand, close the game first, keep a copy of your saves and settings, and keep the old package until the new one works.

## Reporting a startup or rendering failure

Attach the newest `logs/runtime-<timestamp>.log` (on Android, see [Android logs](#android-logs)) and write down the package version, graphics backend, GPU and driver, game edition, disc and scene.

If the game stops during startup, the log shows which step is stuck after 10 seconds. Run once more with `LO_TRACE_STARTUP=1` and attach that log too.

For a rendering problem, open **F1 → Overview → Capture render state**, confirm, then **close F1** so the game keeps rendering. Reopen F1 to see where the archive was saved, look through it and attach it.
