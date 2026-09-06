# Installing Lost Odyssey Recompiled

1. Extract the entire Windows release ZIP to a writable folder, outside Program Files.
2. Run **LostOdysseyRecomp.exe**. On first launch, choose interface/game language and graphics settings.
3. If game files are missing, the importer opens. Select your source; the game continues after import.

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

Only the tested Asian multilingual XEX set is accepted: Title ID `4D5307FA`, version 4,
with Media IDs `39F7D748`, `0EF8CEA8`, `309E3386`, `7B21A91D` for discs 1–4.
The importer also verifies each XEX SHA256 against the supported build.
Other regions, title updates and modified XEX files need separate compatibility work.

Discs are copied to `game/disc1` through `game/disc4` by default. You can select an external
game destination; the executable reads `game-path.txt` next to InstallGame.exe.
Importing all four discs does not imply that every later-disc scene or automatic disc switch
has been tested in this experimental runtime.

## Existing data and cancellation

Original game sources are copied, never moved. Existing installed discs are not overwritten.
To add another disc, select that disc specifically. An import is staged in the destination
and published only after all selected discs finish. Cancellation removes this operation's
temporary files. An abrupt power loss may leave a `.import-*` directory; it is not a completed
installation and can be removed once no importer is running.
The same applies to a stale `.import.lock` left by a crash; never remove it while importing.

Saves, profile, logs and shader caches are kept beside the executable. Keep those folders when
updating the program. First startup reads shaders using a built-in location index and prepares them;
unrecognized resource layouts fall back to scanning the affected files.
later starts reuse the generated cache. A cold shader cache is intentionally not distributed.

The first-run settings page saves before game initialization, so the selected language works
on that launch. Existing settings skip this page. Use `--setup` to open it again; closing it
without saving exits before the game starts. No PowerShell or CMD launcher is needed.

## From source / CI

See [BUILDING.md](https://github.com/freefrank/LostOdysseyRecomp/blob/main/docs/BUILDING.md)
and [release packaging](https://github.com/freefrank/LostOdysseyRecomp/blob/main/docs/notes/release-packaging.md).
