# Mod Organizer 2

LostOdysseyRecomp has a Mod Organizer 2 (MO2) game plugin. MO2 maps the mods you enable onto the game's `mods/` folder through its virtual file system, so nothing is copied into the game folder and disabling a mod removes it on the next launch.

## Setup

1. Use MO2 2.5 or newer on Windows. It already includes the `basic_games` plugin.
2. Download [game_lostodysseyrecomp.py](https://github.com/freefrank/LostOdysseyRecomp/blob/main/tools/modding/mo2/game_lostodysseyrecomp.py) and put it in `<MO2>/plugins/basic_games/games/`.
3. Start MO2 and create a new instance. Pick **Lost Odyssey Recomp** and browse to the folder that holds `LostOdysseyRecomp.exe` (the extracted Windows ZIP). If that folder has no `mods/` subfolder yet, start the game once or create the folder.
4. Install mod archives with MO2's install button, enable them, and start the game with MO2's **Run** button. Restart the game after you change mods.

Mod folders (`mods/<id>/mod.ini` with `mods/<id>/overlay/...`), older `mod.ini` packages, overlay packages (a top-level `overlay/` folder) and language packs (a folder with a `language.ini`) install as they are. The plugin removes the outer `mods/` folder of those ZIPs, so the archive's contents land in the game's `mods/`. An archive whose top holds `mod.ini` or `language.ini` itself (the inside of such a folder) is marked as invalid: put it in a folder named after the mod in MO2's manual installer. Download the plugin again if MO2 says a mod folder or language pack has no valid game data.

## Export the original assets

The export tool copies the game's own textures, movies and text out of your game data. Use them as the starting point for a mod: textures to repaint or upscale, text to translate.

### Install the tool (once)

1. Open [lostodysseyrecomp_export.py](https://github.com/freefrank/LostOdysseyRecomp/blob/main/tools/modding/mo2/lostodysseyrecomp_export.py) and press **Download raw file** (the download icon above the code).
2. Put the file in the `plugins` folder of your MO2 folder, for example `C:\Modding\MO2\plugins\`. Not in `plugins\basic_games\`.
3. Restart MO2.

### Export

1. Start MO2 with your Lost Odyssey Recomp instance.
2. In the menu bar, choose **Tools > Tool Plugins > Export Lost Odyssey assets**.
3. Check the window:
   - **Output folder**: where the files go. The default is a new `lost-odyssey-export` folder in your MO2 instance folder. It must be a new or empty folder, outside the game folder. A full export needs about 7 GB of free space.
   - **Export**: tick what you need.
     - **Textures**: every texture as a PNG image (about 5.3 GB).
     - **Movies**: the CG movies as the original WMV files (about 1.7 GB).
     - **Text**: all game text as JSON files, for translations (about 33 MB).
   - **Filter** (optional): export only files whose path contains this text, for example `UI_MAIN` (the settings menu artwork). Leave it empty to export everything.
   - **Text language**: the language of the exported text. **All languages** exports every language on your discs.
   - **Language pack**: leave it empty here. See [Make a language pack](#make-a-language-pack).
4. Press **Start**. The bar shows the progress. A full export takes about a minute on an SSD.
5. When it is done, MO2 asks whether to open the folder. Press **Yes**.

If it fails, the window shows the reason:

| Message | What to do |
| --- | --- |
| `output folder ... is not empty` | Choose a new or empty folder. |
| `output folder ... is inside the game data` | Choose a folder outside the game folder. |
| `no game data` | The game is not set up yet. Start it once from MO2 and finish the first-start setup that imports your discs. |
| `Could not start LostOdysseyRecomp.exe` | The instance's game folder is not the folder that has `LostOdysseyRecomp.exe`. Fix it in MO2's instance settings. |

### What you get

```text
lost-odyssey-export/
  textures/            PNG images; index.csv lists each texture's mod key and fingerprint
  movies/              the CG movies (.wmv)
  text/                one .json file per game text file; index.csv lists them
  export-summary.txt   how many files were exported and what was skipped
```

Next steps:

- Repaint or upscale textures: [Creating mods](Creating-Mods.md), sections 1b to 1d.
- Translate text: [Creating mods](Creating-Mods.md#1e-translate-the-games-text-experimental), section 1e.

The tool reads the game folder directly, not through MO2, so it exports the original game files, not your installed mods. With the DLC installed, files the DLC changes come from the DLC.

The files come from your own copy of the game. Use them to make your mods, but do not share them or put them in a mod.

### Make a language pack

To translate the game into a new language:

1. Open **Tools > Tool Plugins > Export Lost Odyssey assets** as above. If the window has no **Language pack** box, download the tool again (version 1.1.0 has it).
2. In **Language pack**, type a short id for the language, for example `pt-br`. Letters, digits, `-`, `_` and `.` only.
3. Leave **Text language** on **All languages** to translate from English, or pick the language to start from.
4. Press **Start**. Only text is exported (a few seconds), whatever **Export** says.
5. The output folder now holds a `pt-br` folder (`language.ini` and the text in `text/`) and an `original` folder. Follow [Creating mods](Creating-Mods.md#make-a-language-pack-a-new-language) from step 2 to name, translate and try it.
6. To share it, open the tool again, press **Clean language pack...** and choose the `pt-br` folder. The lines you translated are copied to `share/pt-br` in the output folder; zip that folder and share the ZIP. **Clean** needs the `original` folder beside `pt-br`.

## Which mod wins

- **Mod folders** (the default layout of `lo_mod.py`): each mod has its own folder and `mod.ini`, so MO2 shows no conflict between them. The game's own mod order decides which one wins for the same file (`mod-list.ini` next to `settings.ini`, outside MO2's virtual folder), then the `priority` in `mod.ini`. Disabling a mod in MO2 removes its folder, so it stops loading; it keeps its place in the game's order for when you enable it again.
- **Overlay packages** (`--layout overlay`): two mods that replace the same asset ship the same file path. MO2 shows the conflict, and the mod lower in MO2's left pane wins. An overlay file beats every mod folder.
- **Language packs**: MO2's order does not matter. Only the pack picked in **Settings > System > Game language** is used.

To let MO2's order decide everything, use overlay packages, open **Modify Executables** in MO2 and add `--mods-mode overlay` to the game's arguments. Mod folders are then ignored; language packs still show in Settings.

## What stays outside MO2

Only `mods/` is virtual. Settings, the game's mod order (`mod-list.ini`), saves (`profile/`), logs and shader caches stay in the game folder, so MO2 profiles do not separate them.

## Checking what loaded

Every run writes `logs/runtime-*.log` in the game folder. Its `mods:` line shows the mods folder, the mode, the mod folders it loaded (highest priority first) and whether an `overlay` folder was visible; warnings about broken `mod.ini` or `mod-list.ini` files follow it. If an MO2 mod is missing there, make sure the game was started from MO2.

## Other managers

Managers that deploy with hard links or symbolic links (Vortex, or a script on Linux and Steam Deck) work too: put the files under the game's `mods/` folder while the game is closed. Symlinked mod folders are followed. MO2 itself runs only the Windows build.
