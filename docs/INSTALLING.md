# Installing Lost Odyssey Recompiled

This guide covers installation of the current Windows packages, including local development builds.

1. Extract the entire Windows release ZIP to a writable folder, outside Program Files.
2. Run **LostOdysseyRecomp.exe**. If game files are missing, the importer opens; select your source.
3. On first launch, choose interface/game language and graphics settings. The game continues after setup and shader preparation.

Historical **v0.1** packages open setup before import and accept only the Asian set; USA/Europe support below is not included in v0.1.

You can also run **InstallGame.exe** separately to import additional discs. Disc 1 is required to start.

The release includes the game executable, importer, shader compiler libraries,
dependency licenses and a SHA256 manifest. Python and Visual Studio are not required.
Windows x64, an AVX-capable CPU and a Direct3D 12 graphics driver are required.
Game files are supplied by the user and are not included in the download.

## Supported sources

- An extracted game folder, or its `default.xex`. Selecting the XEX imports the complete parent folder.
- An XDVDFS ISO, including a padded disc image with its descriptor within the first 512 MiB.
- A GOD/SVOD header, its `.data` folder, or an outer folder containing multiple GOD discs.

The importer searches five folder levels and reads the XEX disc numbers, so directory names
and container ordering do not matter. `$SystemUpdate` is not imported. Other content types,
including DLC/STFS packages, are not installed by this importer.

The importer accepts these audited sets, both with Title ID `4D5307FA`:

| Edition | Version | Media IDs, discs 1–4 |
|---|---|---|
| Asian multilingual | 4 | `39F7D748`, `0EF8CEA8`, `309E3386`, `7B21A91D` |
| USA/Europe (not in v0.1) | 3 | `368DE6DD`, `1888BE4E`, `6DD59D08`, `0C0E80B5` |

Each XEX SHA256 must match the supported build. Discs from different editions cannot be mixed,
either in a single import or when adding to an existing installation. Other builds, title updates
and modified XEX files need separate compatibility work.

Game-language choices follow the installed edition: English, Japanese, German, French, Spanish
and Italian for USA/Europe; English, Japanese, Korean, Traditional Chinese and Simplified Chinese
for the audited Asian set. The settings interface retains its existing five translations.
A saved game-language choice unavailable in the current edition falls back to English.

Discs are copied to `game/disc1` through `game/disc4` by default. You can select an external
game destination; the executable reads `game-path.txt` next to InstallGame.exe.
The original game's disc request automatically selects the
corresponding imported `discN` directory. No manual disc-selection button is required. Keep all
four discs from the same edition under the same parent directory. The original game reloads
the target disc's own index and archives; the importer does not merge them into one rewritten index.
If the target is missing, from another edition or incomplete, the request fails and the current
mount remains selected. Import the required disc with InstallGame.exe. This feature is not in v0.1.
Controlled switching tests do not establish chapter-boundary progression or full-game compatibility.

## Existing data and cancellation

Original game sources are copied, never moved. Existing installed discs are not overwritten.
To add another disc, select that disc specifically. An import is staged in the destination
and published only after all selected discs finish. Cancellation removes this operation's
temporary files. An abrupt power loss may leave a `.import-*` directory; it is not a completed
installation and can be removed once no importer is running.
The same applies to a stale `.import.lock` left by a crash; never remove it while importing.

Saves, profile, logs and shader caches are kept beside the executable. Keep those folders when
updating the program. Startup discovers shaders in indexed and compressed resources, then prepares
them; unrecognized layouts retain a scan fallback. Initial scanning and compilation may take several
minutes. Later starts reuse the generated shader cache and prepare previously recorded graphics
pipelines. Shader coverage remains incomplete. A cold shader cache is intentionally not distributed.

The first-run settings page saves before game initialization, so the selected language works
on that launch. Existing settings skip this page. Use `--setup` to open it again; closing it
without saving exits before the game starts. No PowerShell or CMD launcher is needed.

## From source / CI

See [BUILDING.md](https://github.com/freefrank/LostOdysseyRecomp/blob/main/docs/BUILDING.md)
and [release packaging](https://github.com/freefrank/LostOdysseyRecomp/blob/main/docs/notes/release-packaging.md).
