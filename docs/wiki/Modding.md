# Modding LostOdysseyRecomp

These pages describe the mod system as of v0.9.52: mod format v1 and v2 (mod folders with their own overlay files).

## 4x HD texture pack

About 15,400 of the game's textures, upscaled 4x. Windows only, like all mods; it needs v0.9.22 or later. The download is 16.4 GB in four parts and unpacks to 33 GB.

1. Join the [Discord server](https://discord.gg/z2yPct6z2w), open the **#mods** forum and find the **4x HD Texture Pack v1.0** post. Its download link has five files; download all of them into one folder: `LostOdysseyRecomp-HD-Textures-4x-v1.0.7z.001` to `.004`, and `SHA256SUMS.txt`.
2. Optional: check the downloads. Open PowerShell in that folder and run `Get-FileHash *.7z.* -Algorithm SHA256`. Each hash must match the line for that file in `SHA256SUMS.txt`.
3. Install [7-Zip](https://www.7-zip.org/) if you don't have it.
4. Right-click `LostOdysseyRecomp-HD-Textures-4x-v1.0.7z.001` and choose **7-Zip → Extract files...**. As the destination, pick your game folder: the folder that contains `LostOdysseyRecomp.exe`. 7-Zip reads the other three parts by itself.
5. Check that the game folder now contains `mods\hd-textures-4x\mod.ini`.
6. Start the game. Settings → System → Mods lists **4x HD Texture Pack**, turned on.

After step 5 you can delete the four `.7z` files.

| Problem | Fix |
| --- | --- |
| 7-Zip reports a missing volume or an unexpected end of archive. | One part is missing or incomplete. Download all four parts into the same folder, then check them with step 2. |
| The textures look unchanged. | The folder must be `mods\hd-textures-4x\` directly next to `LostOdysseyRecomp.exe`, not `mods\mods\...`. Check that the pack is on in Settings → System → Mods. A pack copied in while the game runs appears after a restart. |
| Short stutters when entering an area for the first time. | The game reads the new textures from disk as they are needed. Keep the game folder on an SSD. |

To remove the pack, delete `mods\hd-textures-4x\`, or turn it off in Settings → System → Mods.

## What works today

| Resource or feature | Status |
| --- | --- |
| Native settings-menu `UI_MAIN_00` atlas | Replacement is wired into `settings/menu_assets.cpp`; preserve the original 512x1024 dimensions and layout. See [Creating mods](Creating-Mods.md#settings-menu-artwork). |
| Texture pages used by native menu fonts | Image replacement is wired at the same decoder; preserve each page's dimensions and glyph positions. Font metrics are unchanged. |
| Textures drawn by the game | Replaced at upload by fingerprint from `.lotex2` files, at the original size or 2x/4x/8x larger (experimental; uncompressed RGBA8 or BC1, BC3, BC4 or BC7 DDS). See [Creating mods](Creating-Mods.md#make-a-texture-pack-experimental) and [Modding API](Modding-API.md#runtime-textures-lotex2). |
| Game text | Translations replace the game's text by key from the JSON files the export writes, including longer text (experimental). A language pack adds a new choice to Settings → System → Game language. See [Creating mods](Creating-Mods.md#translate-the-games-text-experimental). |
| Cutscene videos (movies) | Replace a cutscene picture with an H.264 MP4 named after the original video (`overlay/movies/mv_00v.mp4`). Windows only for now; see [Creating mods](Creating-Mods.md#replace-a-cutscene-video). |
| Models | Character, monster and moving-object models (`SkeletalMesh`) and map pieces (`StaticMesh`, `StaticMeshRPG`), as glTF (`.glb`) edited from the export, keeping the original skeleton and material names; a map's baked lighting follows a new map piece. See [Creating mods](Creating-Mods.md#replace-a-model). Animations are not replaceable. |
| Letters the fonts lack | A language pack can add glyphs to the game's fonts, drawn from a TrueType or OpenType font with `lo_mod.py font-glyphs`, for languages such as Polish, Russian or (with pre-shaped text) Arabic. See [Creating mods](Creating-Mods.md#add-letters-the-fonts-lack). Replacing whole font files or metrics is not supported. |
| Mod folders | One folder per mod: `mods/<id>/mod.ini` with a name, version and author, plus its own `overlay/` files found by name. The order in `mod-list.ini` (next to `settings.ini`), then `priority`, decides between mods. Settings → System → Mods turns mods on or off and changes their order. See [Creating mods](Creating-Mods.md#mod-folders-recommended). |
| External manager overlay | Deterministic top-level `overlay/` paths that a manager orders, and an isolated resolution mode. |
| Exporting original artwork | `LostOdysseyRecomp.exe --export-assets <folder>` writes your game's textures (PNG plus `index.csv` with mod keys and fingerprints), movies and text (JSON per game text file, the starting point for translations). With `--export-kinds models,animations` it also writes 3D models and animation sets as glTF (`.glb`); edited character models can go back into a mod. Also available from an MO2 tool. See [Creating mods](Creating-Mods.md#export-the-original-assets). |
| Overlay archives | A mod folder can hold its `overlay/` files in one `overlay.loarc` archive instead of thousands of loose files, made with `lo_mod.py archive`. See [Creating mods](Creating-Mods.md#ship-a-mod-as-one-archive). |
| Rebuilt game files | Files that replaced models or translations rebuild once, then are kept compressed in `cache/mods`; a window shows **Preparing mods** while this runs. See [Creating mods](Creating-Mods.md#replace-a-model). |
| Mod Organizer 2 | The game plugin and export tool are in the Windows ZIP's `mo2` folder (source: `tools/modding/mo2`); MO2 maps mods onto `mods/`. See [Mod Organizer 2](Mod-Organizer-2.md). |

Mods do not modify `LO.fpi`, FPD archives or other imported game files. Mod packages contain data, not automatically loaded native libraries.

## Supported platforms

Mods are supported on the Windows build only. The Linux, macOS and Android builds read the same `mods/` folder, but mods are untested and unsupported there. A GPU that cannot sample BC-compressed textures skips BC texture replacements and keeps the original texture.

## Start here

- [Creating mods](Creating-Mods.md): texture packs, translations and language packs, settings-menu artwork, and how to install them.
- [Mod Organizer 2](Mod-Organizer-2.md): MO2 setup, the export tool, and which mod wins.
- [Open models in Blender](Open-Models-in-Blender.md): look at exported models and play their animations.
- [Modding API](Modding-API.md): identities, resolution, the file formats and extension points.
- [Asset inventory](Asset-Inventory.md): the read-only four-disc resource catalog and its counting limits.
- [Runtime texture replacement](Runtime-Texture-Replacement.md): what texture replacement covers and what is still open.
- [Validation](Modding-Validation.md): the automated checks and the game and MO2 acceptance steps.

## Installation essentials

The portable layout uses `mods/` next to the executable. Other layouts use the application's data directory plus `mods/`. Set `LO_MODS_DIR` to an absolute path to choose an explicit root. In the game, Settings → System → Mods → **Open mods folder** (Windows) opens it. ZIPs produced by the packer contain a top-level `mods/` folder (`mods/<id>/...` for a mod folder); deploy the contents of that folder into the selected root, without adding another `mods/` level.

With Mod Organizer 2, install packages through MO2 instead; see [Mod Organizer 2](Mod-Organizer-2.md). Restart after installing or removing mods, or after editing `mod-list.ini` by hand. Settings → System → Mods turns mods on or off and changes their order; saving applies the list while the game runs: textures at once, replaced models from the next area you enter (party members after a restart). There is no file watcher. `LO_MODS=0` disables every replacement, including trusted providers.

Use only artwork you may distribute. Do not bundle the original game archives, executable, extraction catalog or unrelated extracted artwork with a mod.
