# Mod Organizer 2

LostOdysseyRecomp has a Mod Organizer 2 (MO2) game plugin. MO2 maps the mods you enable onto the game's `mods/` folder through its virtual file system, so nothing is copied into the game folder and disabling a mod removes it on the next launch.

## Setup

1. Use MO2 2.5 or newer on Windows. It already includes the `basic_games` plugin.
2. Download [game_lostodysseyrecomp.py](https://github.com/freefrank/LostOdysseyRecomp/blob/main/tools/modding/mo2/game_lostodysseyrecomp.py) and put it in `<MO2>/plugins/basic_games/games/`.
3. Start MO2 and create a new instance. Pick **Lost Odyssey Recomp** and browse to the folder that holds `LostOdysseyRecomp.exe` (the extracted Windows ZIP). If that folder has no `mods/` subfolder yet, start the game once or create the folder.
4. Install mod archives with MO2's install button, enable them, and start the game with MO2's **Run** button. Restart the game after you change mods.

Both package layouts made by `lo_mod.py pack` install as they are. The plugin removes the outer `mods/` folder of those ZIPs, so the archive's contents land in the game's `mods/`.

## Export the original artwork

An optional tool plugin adds **Tools > Export Lost Odyssey assets**.

1. Download [lostodysseyrecomp_export.py](https://github.com/freefrank/LostOdysseyRecomp/blob/main/tools/modding/mo2/lostodysseyrecomp_export.py) and put it in `<MO2>/plugins/` (not in `basic_games`). Restart MO2.
2. With a Lost Odyssey Recomp instance open, choose the tool, pick a new or empty output folder, tick Textures and/or Movies, optionally enter a filter, and press **Start**.
3. When it finishes, open the folder. See [Creating mods](Creating-Mods.md) for the layout and how to start a mod from an exported texture.

The export runs the game program directly in the game folder, outside MO2's virtual file system, and reads your own game data. Keep the result for reference; do not redistribute it.

## Which mod wins

- **Overlay packages** (`--layout overlay`): two mods that replace the same asset ship the same file path. MO2 shows the conflict, and the mod lower in MO2's left pane wins.
- **Standalone packages** (the default layout): each mod has its own folder and `mod.ini`. The `priority` in `mod.ini` decides between them, not MO2's order. An overlay file beats any standalone mod.

To let MO2's order decide everything, open **Modify Executables** in MO2 and add `--mods-mode overlay` to the game's arguments. Standalone packages are then ignored.

## What stays outside MO2

Only `mods/` is virtual. Settings, saves (`profile/`), logs and shader caches stay in the game folder, so MO2 profiles do not separate saves.

## Checking what loaded

Every run writes `logs/runtime-*.log` in the game folder. Its `mods:` line shows the mods folder, the mode, the standalone mods it loaded and whether an `overlay` folder was visible; warnings about broken `mod.ini` files follow it. If an MO2 mod is missing there, make sure the game was started from MO2.

## Other managers

Managers that deploy with hard links or symbolic links (Vortex, or a script on Linux and Steam Deck) work too: put the files under the game's `mods/` folder while the game is closed. Symlinked mod folders are followed. MO2 itself runs only the Windows build.
