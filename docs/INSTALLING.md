# Installing Lost Odyssey Recomp

This guide describes the current development branch and the next package transition, with Direct3D 12 and Vulkan graphics backends. The importer and updater are built into **LostOdysseyRecomp.exe**; there is no separate `InstallGame.exe` or `LostOdysseyUpdater.exe`.

1. Extract the entire package to a writable folder, outside Program Files. Keep the main executable, validated DXC v1.8.2407 DLL pair and license files together.
2. Run **LostOdysseyRecomp.exe** directly. If game files are missing, the built-in importer opens; select your source and review its recognition result before importing.
3. On first launch, choose interface/game language and graphics settings. The game continues after setup and the separate shader preparation stages.

The built-in importer can also be opened from the game to import additional discs or DLC. Disc 1 is required to start.

The package includes the game executable and built-in importer/updater, the DXC v1.8.2407 DLL pair and dependency licenses. Python and Visual Studio are not required. Windows x64 and an AVX-capable CPU are required. D3D12 is the default graphics path; the development Vulkan path requires a compatible Windows driver and uses the driver-provided Vulkan loader rather than a bundled SDK.
Game files are supplied by the user and are not included in the download.

<a id="automatic-content-import"></a>

## Automatic content import

On first launch of **LostOdysseyRecomp.exe**, choose **Files** to select one or more files, or **Folder** to scan a directory. The same content importer can be reopened from the game when assets are missing. It recognizes supported game discs and Lost Odyssey DLC from their headers and structure; you do not need to choose a disc or DLC mode. Mixed selections are reviewed together before import.

The import order is game-disc transaction, shared game-path save, then DLC transaction. If DLC import fails or is cancelled after discs succeed, the completed discs remain installed and the retry contains only the remaining DLC. A game-path save failure is warned about without rolling back completed imports. DLC-only imports do not change `game-path.txt`.

Disc resource copies and `import-info.json` are published only after their writes,
flushes and closes succeed. A final write error aborts the staged import before
publication; the importer does not reread the complete resource to perform this
check. XDVDFS discovery advances on 2048-byte boundaries and the install phase
reuses the identity-verified reader while retaining a final identity check.

On the destination page, use **New folder**, `F2`, or controller `Y` to create a
folder. A unique default name is supplied and can be edited with the keyboard.
After creation the browser enters and selects the folder, without starting an
import. Name collisions, permission failures and read-only destinations are
reported clearly. The source browser keeps its existing `Y` behavior.

## Supported sources

- An extracted game folder, or its `default.xex`. Selecting the XEX imports the complete parent folder.
- An XDVDFS ISO, including a padded disc image with its descriptor within the first 512 MiB.
- A GOD/SVOD header, its `.data` folder, or an outer folder containing multiple GOD discs.

The importer searches up to eight directory levels and reads the XEX disc numbers, so directory names
and container ordering do not matter within that bounded search. It scans the selected source, shows one review step, and imports only after confirmation. `$SystemUpdate` is not imported. Title updates and other content types are not accepted as game discs.

The importer accepts these audited sets, both with Title ID `4D5307FA`:

| Edition | Version | Media IDs, discs 1–4 |
|---|---|---|
| Asian multilingual | 4 | `39F7D748`, `0EF8CEA8`, `309E3386`, `7B21A91D` |
| USA/Europe | 3 | `368DE6DD`, `1888BE4E`, `6DD59D08`, `0C0E80B5` |

Each XEX must carry the metadata of one audited supported build. Discs from different editions cannot be mixed,
either in a single import or when adding to an existing installation. Other builds, title updates
and modified XEX files need separate compatibility work.

Game-language choices follow the installed edition: English, Japanese, German, French, Spanish
and Italian for USA/Europe; English, Japanese, Korean, Traditional Chinese and Simplified Chinese
for the audited Asian set. The settings interface retains its existing five translations.
A saved game-language choice unavailable in the current edition falls back to English.
The embedded installer UI font is a packed Unifont subset from the pinned SDL source; it is not an
original Lost Odyssey font and does not claim full Unicode coverage or game-style visual fidelity.

Discs are copied to `game/disc1` through `game/disc4` by default. You can select an external
game destination. In portable mode, the executable reads `game-path.txt` beside itself; an empty
or missing file prefers `game` beside the executable and retains a parent `../game` location as a
compatibility fallback. In non-portable Linux mode, the path file is under
`XDG_CONFIG_HOME/lost-odyssey-recomp` (normally `~/.config/lost-odyssey-recomp`), and the default
game data is under `XDG_DATA_HOME/lost-odyssey-recomp/game` (normally
`~/.local/share/lost-odyssey-recomp/game`). Flatpak defaults to `/var/data/game`. For direct startup, an
explicit `--game` directory has priority. An invalid non-empty configuration or explicit path is
reported and does not silently select an older installation.
The original game's disc request automatically selects the
corresponding imported `discN` directory. No manual disc-selection button is required. Keep all
four discs from the same edition under the same parent directory. The original game reloads
the target disc's own index and archives; the importer does not merge them into one rewritten index.
If the target is missing, from another edition or incomplete, the request fails and the current
mount remains selected. Import the required disc with the built-in importer.
Controlled switching tests do not establish chapter-boundary progression or full-game compatibility.

## DLC recognition

Select DLC files directly or include them in a scanned folder. Filenames and extensions do not matter. The importer accepts `CON`, `LIVE` and `PIRS` packages with Lost Odyssey Title ID `4D5307FA`, Marketplace content type `2` and an STFS volume. Nested files with unknown extensions receive a bounded ISO descriptor probe; manually selected files and `.iso` inputs retain the bounded padded-image search.

Review the detected package names, content IDs and game discs together, then confirm the single import. Restart **LostOdysseyRecomp.exe** after importing.

All discs share `game/dlc/<content-id>/`. Selecting an existing `game/disc1` through `game/disc4` directory also uses this shared location. Keep the extracted files and their hidden metadata together. DLC import leaves `game-path.txt`, source packages, saves, profiles and settings unchanged.

An existing installation with the same content ID and expected size is recognized without copying it again. A conflicting package is reported and left unchanged. Importing stages the selected packages before publication; cancellation removes this operation's temporary data. The importer checks structure, path and I/O results, without verifying Microsoft signatures or doing a full source hash scan. Other games, title updates, SVOD DLC and arbitrary loose DLC folders are unsupported.

Three real DLC packages have been imported and read at runtime through their headers, complete indexes and payloads in 24 total reads without a crash; imported files and isolated user data remained unchanged. Rewards, dungeon gameplay and broader edition compatibility still need verification. DLC files are not included in the program download.

## Existing data and cancellation

Original game sources are copied, never moved. Existing installed discs are not overwritten.
To add another disc, select that disc specifically. An import is staged in the destination
and published only after all selected discs finish. Cancellation removes this operation's
temporary files. An abrupt power loss may leave a `.import-*` directory; it is not a completed
installation and can be removed once no importer is running.
The same applies to a stale `.import.lock` left by a crash; never remove it while importing.

Saves, profile, logs and shader caches are kept beside the executable. Keep those folders when
updating the program. Shader preparation reports resource discovery, cache validation, compilation
and pipeline preparation as separate stages. Initial scanning and compilation may take several
minutes; later starts reuse valid shader caches and prepare previously recorded graphics pipelines.
Shader coverage remains incomplete, and a cold shader cache is intentionally not distributed.

The first-run settings page saves before game initialization, so the selected language works
on that launch. Existing settings skip this page. Use `--setup` to open it again; closing it
without saving exits before the game starts. The in-game Settings entry offers a controlled Restart
choice for settings that need a new process. No PowerShell or CMD launcher is needed.

Formal packages built from the current branch can check GitHub for a newer matching release at
startup; automatic updates can be disabled and failed or offline checks must not block launching.
Development packages skip automatic update checks and preserve user data, saves, settings and caches.
The historical v0.4.2 package predates this updater flow. Do not copy a development package over a
published installation without retaining those folders.

## Running on Linux (first-playable)

This section describes running the native Linux unbundled executable.

The v0.6.1 release includes native Linux x64 packaging. Download the AppImage from the [v0.6.1 GitHub Release](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.6.1), or build the native ELF locally following [BUILDING.md](BUILDING.md). The AppImage update path, native Linux GPU coverage and Steam Deck experience still require the validation described below.

### Flatpak source manifest

`packaging/linux/io.github.freefrank.LostOdysseyRecomp.json` is a source-build Flatpak manifest; it is not a published Flatpak package. The Flatpak importer defaults to `/var/data` for game files. Its SDL built-in file browser deliberately receives the required real host-path permissions (`host`, `/media`, `/run/media`, and `/mnt`) rather than using a document portal, so select a different writable destination when `/var/data` is unsuitable.

The verified first-playable path is WSL2 Manjaro using Mesa Dozen's Vulkan-on-D3D12 layer. Native Linux NVIDIA/Mesa ICD paths have not been tested; this result does not establish general Linux GPU compatibility.

### Linux requirements

- The compiled `LostOdysseyRecomp` executable
- `libdxcompiler.so` located beside the executable (copied automatically by the CMake build)
- Extracted game disc files (Disc 1 required to boot)
- Host Vulkan drivers and Mesa (or another compatible Vulkan ICD)
- Static-linked SDL2 (already built into the binary)

### Graphics backend

Linux runs through Vulkan only. Direct3D 12 is Windows-only and is unavailable on Linux.

### First-run configuration

The embedded Files/Folder importer has native source handling and cross-platform POSIX lock compatibility, but the interactive graphical window remains Windows-only in current builds. On Linux, tell the game where your files are located using the `--game` command-line argument. For a portable ELF, you can instead create `game-path.txt` beside the executable; non-portable mode reads its path file from `XDG_CONFIG_HOME/lost-odyssey-recomp` and defaults to `XDG_DATA_HOME/lost-odyssey-recomp/game`.

### Launching the game

Run the executable directly from your terminal. `--game` accepts the install root (a folder containing `disc1`), `disc1` itself, or `default.xex`:

```bash
./LostOdysseyRecomp --game /path/to/game
```

Run this command with the ELF directory as the current working directory. When `--game` is
explicit, the executable does not change to its own directory, so relative `save/`, `profile/`,
`cache/` and `logs/` paths use the launch CWD.

If you are running in WSL and accessing your existing Windows game dump:

```bash
./LostOdysseyRecomp --game /mnt/d/Mihoyo/LostOdysseyRecomp-windows-x64/game
```

In portable mode, you can also place a `game-path.txt` file next to the binary with your game path, or place an extracted disc folder at `game` adjacent to the executable, then launch:

```bash
./LostOdysseyRecomp
```

## From source / CI

See [BUILDING.md](https://github.com/freefrank/LostOdysseyRecomp/blob/main/docs/BUILDING.md)
and [release packaging](https://github.com/freefrank/LostOdysseyRecomp/blob/main/docs/notes/release-packaging.md).
