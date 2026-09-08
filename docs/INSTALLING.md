# Installing Lost Odyssey Recompiled

This guide covers Windows packages built from the current development branch. The latest published v0.4.2 package is the historical D3D12 baseline and does not include all of the development features described here, including the updater and optional Vulkan path.

1. Extract the entire package to a writable folder, outside Program Files. Keep the executable, importer, updater helper, validated DXC v1.8.2407 DLL pair and license files together.
2. Run **LostOdysseyRecomp.exe** directly. If game files are missing, the importer opens; select your source and review its recognition result before importing.
3. On first launch, choose interface/game language and graphics settings. The game continues after setup and the separate shader preparation stages.

Historical **v0.1** packages open setup before import and accept only the Asian set; USA/Europe support below is not included in v0.1.

You can also run **InstallGame.exe** separately to import additional discs or DLC. Disc 1 is required to start.

The current branch package includes the game executable, importer, updater helper, validated DXC v1.8.2407 DLL pair, dependency licenses and a SHA256 manifest. Python and Visual Studio are not required. Windows x64 and an AVX-capable CPU are required. D3D12 is the default graphics path; the development Vulkan path requires a compatible Windows driver and uses the driver-provided Vulkan loader rather than a bundled SDK.
Game files are supplied by the user and are not included in the download.

<a id="automatic-content-import"></a>

## Automatic content import

Run **InstallGame.exe**, then choose **Files** to select one or more files, or **Folder** to scan a directory. The importer recognizes supported game discs and Lost Odyssey DLC from their headers and structure; you do not need to choose a disc or DLC mode. Mixed selections are reviewed together before import.

The import order is game-disc transaction, shared game-path save, then DLC transaction. If DLC import fails or is cancelled after discs succeed, the completed discs remain installed and the retry contains only the remaining DLC. A game-path save failure is warned about without rolling back completed imports. DLC-only imports do not change `game-path.txt`.

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
| USA/Europe (not in v0.1) | 3 | `368DE6DD`, `1888BE4E`, `6DD59D08`, `0C0E80B5` |

Each XEX SHA256 must match one of the audited supported builds. Discs from different editions cannot be mixed,
either in a single import or when adding to an existing installation. Other builds, title updates
and modified XEX files need separate compatibility work.

Game-language choices follow the installed edition: English, Japanese, German, French, Spanish
and Italian for USA/Europe; English, Japanese, Korean, Traditional Chinese and Simplified Chinese
for the audited Asian set. The settings interface retains its existing five translations.
A saved game-language choice unavailable in the current edition falls back to English.

Discs are copied to `game/disc1` through `game/disc4` by default. You can select an external
game destination; the executable reads `game-path.txt` beside the executable. For direct startup,
an explicit `--game` directory has priority. Otherwise a valid non-empty `game-path.txt` locates
the configured game; an empty or missing file defaults to `../game` relative to the executable and
can discover the adjacent `game` resources. An invalid non-empty configuration or explicit path is
reported and does not silently select an older installation.
The original game's disc request automatically selects the
corresponding imported `discN` directory. No manual disc-selection button is required. Keep all
four discs from the same edition under the same parent directory. The original game reloads
the target disc's own index and archives; the importer does not merge them into one rewritten index.
If the target is missing, from another edition or incomplete, the request fails and the current
mount remains selected. Import the required disc with InstallGame.exe. This feature is not in v0.1.
Controlled switching tests do not establish chapter-boundary progression or full-game compatibility.

## DLC recognition

Select DLC files directly or include them in a scanned folder. Filenames and extensions do not matter. The importer accepts `CON`, `LIVE` and `PIRS` packages with Lost Odyssey Title ID `4D5307FA`, Marketplace content type `2` and an STFS volume. Nested files with unknown extensions receive a bounded ISO descriptor probe; manually selected files and `.iso` inputs retain the bounded padded-image search.

Review the detected package names, content IDs and game discs together, then confirm the single import. Restart **LostOdysseyRecomp.exe** after importing.

All discs share `game/dlc/<content-id>/`. Selecting an existing `game/disc1` through `game/disc4` directory also uses this shared location. Keep the extracted files and their hidden metadata together. DLC import leaves `game-path.txt`, source packages, saves, profiles and settings unchanged.

An identical, intact installation is recognized without copying it again. A conflicting or damaged package with the same ID is reported and left unchanged. Importing stages the selected packages before publication; cancellation removes this operation's temporary data. The importer checks structure and file integrity, without verifying Microsoft signatures. Other games, title updates, SVOD DLC and arbitrary loose DLC folders are unsupported.

The import-to-runtime file-reading path has synthetic fixture coverage. Real DLC rewards, areas and edition compatibility still need verification with actual content; DLC files are not included in the program download.

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

## From source / CI

See [BUILDING.md](https://github.com/freefrank/LostOdysseyRecomp/blob/main/docs/BUILDING.md)
and [release packaging](https://github.com/freefrank/LostOdysseyRecomp/blob/main/docs/notes/release-packaging.md).
