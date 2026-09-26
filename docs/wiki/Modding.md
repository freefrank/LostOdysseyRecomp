# Modding LostOdysseyRecomp

These pages document the API v1 implementation developed in PR #68. A published Wiki page does not mean the feature is included in an existing release. Use a build containing that PR's changes.

## What works today

| Resource or feature | Status |
| --- | --- |
| Native settings-menu `UI_MAIN_00` atlas | Replacement is wired into `settings/menu_assets.cpp`; preserve the original 512x1024 dimensions and layout. |
| Texture pages used by native menu fonts | Image replacement is wired at the same decoder; preserve each page's dimensions and glyph positions. Font metrics are unchanged. |
| Arbitrary textures drawn by the guest game | Not wired into the GPU upload path. Packaging a manifest row does not make that texture replaceable in-game. |
| Font files/metrics, models and movies | Resource kinds and provider extension points reserved; no runtime consumers yet. |
| External manager overlay | Implemented deterministic paths and isolated resolution mode. |
| Turnkey MO2 game support | No bundled MO2 game plugin. Real MO2/USVFS acceptance remains required. |

Mods do not modify `LO.fpi`, FPD archives or other imported game files. Mod packages contain data, not automatically loaded native libraries.

## Start here

[Create and install a mod](Creating-Mods.md) explains the manifest-to-PNG-to-ZIP workflow. [API reference](Modding-API.md) defines identities, resolution, the binary image format and extension points. [Mod Organizer 2](Mod-Organizer-2.md) describes the external-manager contract and its current limitations. [Validation](Modding-Validation.md) separates automated checks from game/MO2 acceptance. [Runtime texture replacement](Runtime-Texture-Replacement.md) records the remaining general GPU work.

## Installation essentials

The portable layout uses `mods/` next to the executable. Other layouts use the application's data directory plus `mods/`. Set `LO_MODS_DIR` to an absolute path to choose an explicit root. ZIPs produced by the packer contain a top-level `mods/` folder; deploy the contents of that folder into the selected root, without adding another `mods/` level.

Restart after changing installed mods. Host integrations may call `modding::Reload()`, but there is no file watcher or player-facing reload button in this implementation. `LO_MODS=0` disables every replacement, including trusted providers.

Use only artwork you may distribute. Do not bundle the original game archives, executable, extraction catalog or unrelated extracted artwork with a mod.
