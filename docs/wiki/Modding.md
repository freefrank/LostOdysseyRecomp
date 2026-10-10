# Modding LostOdysseyRecomp

These pages document the API v1 implementation developed in PR #68. A published Wiki page does not mean the feature is included in an existing release. Use a build containing that PR's changes.

## What works today

| Resource or feature | Status |
| --- | --- |
| Native settings-menu `UI_MAIN_00` atlas | Replacement is wired into `settings/menu_assets.cpp`; preserve the original 512x1024 dimensions and layout. |
| Texture pages used by native menu fonts | Image replacement is wired at the same decoder; preserve each page's dimensions and glyph positions. Font metrics are unchanged. |
| Textures drawn by the game | Replaced at upload by fingerprint from `.lotex2` files, at the original size or 2x/4x/8x larger (experimental; uncompressed RGBA8 or BC1/BC4/BC7 DDS). See [Creating mods](Creating-Mods.md) and [Modding API](Modding-API.md). |
| Game text | Translations replace the game's text by key from the JSON files the export writes, including longer text (experimental). A language pack adds a new choice to Settings > System > Game language. See [Creating mods](Creating-Mods.md#1e-translate-the-games-text-experimental). |
| Font files/metrics, models and movies | Resource kinds and provider extension points reserved; no runtime consumers yet. |
| External manager overlay | Implemented deterministic paths and isolated resolution mode. |
| Exporting original artwork | `LostOdysseyRecomp.exe --export-assets <folder>` writes your game's textures (PNG plus `index.csv` with mod keys), movies and text (JSON per game text file, the starting point for translations), also from an MO2 tool. Reference only; see [Creating mods](Creating-Mods.md). |
| Mod Organizer 2 | Game plugin in `tools/modding/mo2`; MO2 maps mods onto `mods/`. See [Mod Organizer 2](Mod-Organizer-2.md). |

Mods do not modify `LO.fpi`, FPD archives or other imported game files. Mod packages contain data, not automatically loaded native libraries.

## Supported platforms

Mods are supported on the Windows build only. The Linux, macOS and Android builds read the same `mods/` folder, but mods are untested and unsupported there. GPUs without BC texture support, which includes most Android devices, skip BC-compressed texture replacements.

## Start here

[Create and install a mod](Creating-Mods.md) explains the manifest-to-PNG-to-ZIP workflow. [API reference](Modding-API.md) defines identities, resolution, the binary image format and extension points. [Asset inventory](Asset-Inventory.md) explains the read-only four-disc resource catalog and its counting limits. [Mod Organizer 2](Mod-Organizer-2.md) covers MO2 setup and which mod wins. [Validation](Modding-Validation.md) separates automated checks from game/MO2 acceptance. [Runtime texture replacement](Runtime-Texture-Replacement.md) records the remaining general GPU work.

## Installation essentials

The portable layout uses `mods/` next to the executable. Other layouts use the application's data directory plus `mods/`. Set `LO_MODS_DIR` to an absolute path to choose an explicit root. ZIPs produced by the packer contain a top-level `mods/` folder; deploy the contents of that folder into the selected root, without adding another `mods/` level.

With Mod Organizer 2, install packages through MO2 instead; see [Mod Organizer 2](Mod-Organizer-2.md). Restart after changing installed mods. Host integrations may call `modding::Reload()`, but there is no file watcher or player-facing reload button in this implementation. `LO_MODS=0` disables every replacement, including trusted providers.

Use only artwork you may distribute. Do not bundle the original game archives, executable, extraction catalog or unrelated extracted artwork with a mod.
