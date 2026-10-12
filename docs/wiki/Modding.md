# Modding LostOdysseyRecomp

These pages describe the mod system on the main branch: mod format v1 and v2 (mod folders with their own overlay files). The newest release may not include the latest changes.

## 4x HD texture pack

About 15,400 of the game's textures, upscaled 4x. Windows only, like all mods; it needs v0.9.22 or later. The download is 16.4 GB in four parts and unpacks to 33 GB.

1. Open the [download page](https://fuckingfast.net/qgrm5ikgre7z) and download all five files into one folder: `LostOdysseyRecomp-HD-Textures-4x-v1.0.7z.001` to `.004`, and `SHA256SUMS.txt`.
2. Optional: check the downloads. Open PowerShell in that folder and run `Get-FileHash *.7z.* -Algorithm SHA256`. Each hash must match the line for that file in `SHA256SUMS.txt`.
3. Install [7-Zip](https://www.7-zip.org/) if you don't have it.
4. Right-click `LostOdysseyRecomp-HD-Textures-4x-v1.0.7z.001` and choose **7-Zip → Extract files...**. As the destination, pick your game folder: the folder that contains `LostOdysseyRecomp.exe`. 7-Zip reads the other three parts by itself.
5. Check that the game folder now contains `mods\hd-textures-4x\mod.ini`.
6. Start the game. Settings → System → Mods lists **4x HD Texture Pack**, turned on.

After step 5 you can delete the four `.7z` files.

| Problem | Fix |
| --- | --- |
| 7-Zip reports a missing volume or an unexpected end of archive. | One part is missing or incomplete. Download all four parts into the same folder, then check them with step 2. |
| The textures look unchanged. | The folder must be `mods\hd-textures-4x\` directly next to `LostOdysseyRecomp.exe`, not `mods\mods\...`. Check that the pack is on in Settings → System → Mods, then restart the game. |
| Short stutters when entering an area for the first time. | The game reads the new textures from disk as they are needed. Keep the game folder on an SSD. |

To remove the pack, delete `mods\hd-textures-4x\`, or turn it off in Settings → System → Mods.

## What works today

| Resource or feature | Status |
| --- | --- |
| Native settings-menu `UI_MAIN_00` atlas | Replacement is wired into `settings/menu_assets.cpp`; preserve the original 512x1024 dimensions and layout. See [Creating mods](Creating-Mods.md#settings-menu-artwork). |
| Texture pages used by native menu fonts | Image replacement is wired at the same decoder; preserve each page's dimensions and glyph positions. Font metrics are unchanged. |
| Textures drawn by the game | Replaced at upload by fingerprint from `.lotex2` files, at the original size or 2x/4x/8x larger (experimental; uncompressed RGBA8 or BC1, BC3, BC4 or BC7 DDS). See [Creating mods](Creating-Mods.md#make-a-texture-pack-experimental) and [Modding API](Modding-API.md#runtime-textures-lotex2). |
| Game text | Translations replace the game's text by key from the JSON files the export writes, including longer text (experimental). A language pack adds a new choice to Settings → System → Game language. See [Creating mods](Creating-Mods.md#translate-the-games-text-experimental). |
| Font files/metrics, models and movies | Resource kinds and provider extension points reserved; no runtime consumers yet. |
| Mod folders | One folder per mod: `mods/<id>/mod.ini` with a name, version and author, plus its own `overlay/` files found by name. The order in `mod-list.ini` (next to `settings.ini`), then `priority`, decides between mods. Settings → System → Mods turns mods on or off and changes their order. See [Creating mods](Creating-Mods.md#mod-folders-recommended). |
| External manager overlay | Deterministic top-level `overlay/` paths that a manager orders, and an isolated resolution mode. |
| Exporting original artwork | `LostOdysseyRecomp.exe --export-assets <folder>` writes your game's textures (PNG plus `index.csv` with mod keys and fingerprints), movies and text (JSON per game text file, the starting point for translations), also from an MO2 tool. Reference only; see [Creating mods](Creating-Mods.md#export-the-original-assets). |
| Mod Organizer 2 | Game plugin in `tools/modding/mo2`; MO2 maps mods onto `mods/`. See [Mod Organizer 2](Mod-Organizer-2.md). |

Mods do not modify `LO.fpi`, FPD archives or other imported game files. Mod packages contain data, not automatically loaded native libraries.

## Supported platforms

Mods are supported on the Windows build only. The Linux, macOS and Android builds read the same `mods/` folder, but mods are untested and unsupported there. A GPU that cannot sample BC-compressed textures skips BC texture replacements and keeps the original texture.

## Start here

- [Creating mods](Creating-Mods.md): texture packs, translations and language packs, settings-menu artwork, and how to install them.
- [Mod Organizer 2](Mod-Organizer-2.md): MO2 setup, the export tool, and which mod wins.
- [Modding API](Modding-API.md): identities, resolution, the file formats and extension points.
- [Asset inventory](Asset-Inventory.md): the read-only four-disc resource catalog and its counting limits.
- [Runtime texture replacement](Runtime-Texture-Replacement.md): what texture replacement covers and what is still open.
- [Validation](Modding-Validation.md): the automated checks and the game and MO2 acceptance steps.

## Installation essentials

The portable layout uses `mods/` next to the executable. Other layouts use the application's data directory plus `mods/`. Set `LO_MODS_DIR` to an absolute path to choose an explicit root. In the game, Settings → System → Mods → **Open mods folder** (Windows) opens it. ZIPs produced by the packer contain a top-level `mods/` folder (`mods/<id>/...` for a mod folder); deploy the contents of that folder into the selected root, without adding another `mods/` level.

With Mod Organizer 2, install packages through MO2 instead; see [Mod Organizer 2](Mod-Organizer-2.md). Restart after changing installed mods or `mod-list.ini`. Settings → System → Mods turns mods on or off and changes their order; it saves `mod-list.ini`, which applies at the next start. Host integrations may call `modding::Reload()`, but there is no file watcher or player-facing reload button in this implementation. `LO_MODS=0` disables every replacement, including trusted providers.

Use only artwork you may distribute. Do not bundle the original game archives, executable, extraction catalog or unrelated extracted artwork with a mod.
