# Creating and installing mods

Read [current support](Modding.md) first. This page covers three kinds of mod:

| I want to... | Go to |
| --- | --- |
| Repaint or upscale the textures the game draws | [Make a texture pack](#make-a-texture-pack-experimental) |
| Translate the game's text, or add a language to Settings | [Translate the game's text](#translate-the-games-text-experimental) |
| Change the artwork of the settings menu | [Settings-menu artwork](#settings-menu-artwork) |

Install mods as [mod folders](#mod-folders-recommended) in the game's `mods` folder. Textures and text start from [the export of your own game data](#export-the-original-assets).

The tools are Python scripts in `tools/modding` of the [LostOdysseyRecomp repository](https://github.com/freefrank/LostOdysseyRecomp); get them by cloning or downloading the repository, and run the commands below from its root. Python 3.10 or newer is required; Pillow (`python -m pip install Pillow`) is needed for PNG files. Mods work on Windows only (see [Modding](Modding.md)).

## Mod folders (recommended)

Ship a mod as one folder with its own `overlay/`:

```text
mods/<id>/mod.ini
mods/<id>/overlay/textures/fp-<fingerprint>.lotex2
mods/<id>/overlay/images/key-fnv1a64-<16 hex digits>.lotex
mods/<id>/overlay/text/<member path>.json
```

```ini
api_version=2
id=my-textures
name=My Texture Pack
version=1.0
author=Your name
description=One line about the mod.
```

The game finds the files under `overlay/` by name, so `mod.ini` needs no file list. `name`, `version`, `author` and `description` are optional. `texture-pack --layout standalone` and `pack` (its default layout) write this layout. A translation that adds a new language is a language pack instead, a folder with a `language.ini` ([make a language pack](#make-a-language-pack-a-new-language)); both kinds of folder sit side by side in `mods/`.

Zip it with `mods/` at the top of the archive: `mods/<id>/mod.ini`, `mods/<id>/overlay/...`. Players extract the archive into the game folder; Mod Organizer 2 installs it as it is. Mod folders never conflict with each other. When two of them replace the same file, the in-game mod order decides, then `priority` in `mod.ini` (see [Modding API](Modding-API.md#order-and-mod-listini)). Changes take effect after a restart.

Players manage installed mods in Settings → System → Mods. The page lists each mod with its `name`, `version` and `author`. The confirm button (A, or B if the game's confirm and cancel buttons are swapped) switches the selected mod On or Off, LB / RB move it up or down, and **Save mod list** (or Start) saves the order and offers a restart. The selected mod's `description` and folder appear in the help line. A mod with a broken `mod.ini` shows an error there with its first problem. Language packs are listed below the mods; choose one in Settings → System → Game language.

To turn a top-level `overlay/` folder (an overlay pack) into a mod folder without packing it again:

```sh
python tools/modding/lo_mod.py overlay-to-mod mods --id my-textures --name "My Texture Pack" --version 1.0 --dry-run
python tools/modding/lo_mod.py overlay-to-mod mods --id my-textures --name "My Texture Pack" --version 1.0
```

This moves `mods/overlay/textures` (and `images`, `text` and the other kind folders) into `mods/my-textures/overlay/` and writes `mod.ini`. Folders are renamed, not copied, so even a large pack moves at once; across drives the command fails instead of copying. It refuses when `mods/my-textures` exists, and `--dry-run` only prints what it would do. For a mod already installed in Mod Organizer 2, pass that mod's folder (the one that holds `overlay/`).

## Ship a mod as one archive

A mod with thousands of files under `overlay/` (a full texture pack) is slow to copy, unzip and install. `overlay.loarc` holds all of them in one file that sits in the mod folder; the game reads the files from it as if they were loose. Only `api_version=2` mod folders can use it.

1. Build the mod folder as usual: `mod.ini` plus `overlay/`.
2. Run, with the mod folder first and a new folder second:

   ```sh
   python tools/modding/lo_mod.py archive mods/my-textures packed/my-textures
   ```

   The command copies `mod.ini` and every other file outside `overlay/`, and writes `overlay.loarc` into the new folder with every overlay file the game loads. Files it does not load (wrong name, wrong folder) are skipped with a warning. Two files that end up under the same name (`FP-…` and `fp-…`) are an error and nothing is written. The new folder must not exist or must be empty; `--force` writes into a non-empty one.
3. The command re-reads the archive and compares every file with its source. `--quick` checks only the archive's header and file list, which is much faster for a mod of tens of gigabytes.
4. Install `packed/my-textures` like any mod folder: `mods/my-textures/mod.ini` and `mods/my-textures/overlay.loarc`. Do not leave the `overlay/` folder in it unless you want loose files to override the archive.

Things to know:

- `mod.ini` always stays a loose file; the game reads it before it opens the archive.
- A loose file in the same mod's `overlay/` folder wins over the archive entry with the same name. Between different mods the usual order and `priority` still decide.
- One archive can be larger than 4 GB, so it cannot sit on a FAT32 drive.
- To look inside an archive: `python tools/modding/lo_mod.py extract mods/my-textures/overlay.loarc out --list` prints `name` and `size` of every file. Without `--list` it writes the files to `out/overlay/...` (and refuses to overwrite existing files). Extracting gives back exactly the files that were archived.

### Archive format

All numbers are little endian. The header is 64 bytes:

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 8 | magic `LOARC1\r\n` |
| 8 | 4 | header size, 64 |
| 12 | 4 | entry count |
| 16 | 8 | index offset |
| 24 | 8 | index size in bytes (at most 64 MiB) |
| 32 | 4 | kind mask: bit `1 << kind` for each kind present (Image 1, Font 2, Model 3, Movie 4, Texture 5, Text 6) |
| 36 | 28 | zero |

Each file's data starts at a multiple of 4096 (at least 4096) and ends before the index offset; data ranges do not overlap. The index follows the last file's data. It holds one record per file, sorted by name bytes, names unique: `u64` offset, `u64` size, `u32` name length (1 to 1100), then the UTF-8 name. A name is the file's path inside the mod folder with `/` separators and lower case: `overlay/textures/fp-<16 hex>.lotex2`, `overlay/images/key-fnv1a64-<16 hex>.lotex`, `overlay/<fonts|models>/key-fnv1a64-<16 hex>.loasset`, `overlay/movies/<name>.mp4` (`<name>` is 1 to 64 of `a-z`, `0-9`, `_`), or `overlay/text/<path>.json`.

## Export the original assets

The game can export its own textures, movies and text from your game data as a starting point. The export is for your reference only: it comes from your copy of the game and must not be redistributed or bundled in a mod.

```sh
LostOdysseyRecomp.exe --export-assets my-export --export-kinds textures,movies --export-filter UI_MAIN
```

The output folder must not exist or must be empty, and it must be outside the game data. Optional arguments:

- `--export-kinds`: `textures`, `movies`, `text`, or `fingerprints` (the texture index without PNG files). The default is textures, movies and text.
- `--export-filter`: only names containing this text.
- `--export-language`: the text of one language only: `int`, `jpn`, `deu`, `fra`, `spa`, `ita`, `kor`, `chi` or `sch`.
- `--export-language-pack <id>`: only text, written as a language pack to translate (see [make a language pack](#make-a-language-pack-a-new-language)).

No window opens; the program prints progress and exits (exit code 1 on a fatal error). With [Mod Organizer 2](Mod-Organizer-2.md#export-the-original-assets), the **Export Lost Odyssey assets** tool does the same from a window; that page has step-by-step instructions.

```text
my-export/textures/<package path>/<object>.<export index>.png
my-export/textures/index.csv        key,file,width,height,format,fingerprint,fingerprint_tiled
my-export/movies/<name>.wmv         original videos, unchanged
my-export/text/<file path>.json     {"key": "text"} for one game text file
my-export/text/index.csv            path,language,format,source,entries,round_trip
my-export/export-summary.txt
```

`textures/index.csv` holds the mod key and the fingerprint of every texture, so no catalog is needed.

Text covers the menus, item and skill names and descriptions, battle messages, field dialogue, cutscene subtitles, the credits and the engine's own messages, for every language on your discs. Each game file becomes one UTF-8 JSON file with its own path plus `.json`, for example `text/bin/xenon/scr/mes/int/u3b_0_scrw.jmd.json`. Codes that are not text appear as tokens such as `{E001}` (a line break in dialogue) or `{E10D:0500}` (a code with its value); keep them where they are. `source` in `text/index.csv` is `base`, or the DLC whose version the game uses instead of the disc's (`lodlc002`, `lodlc003`). `round_trip` is `ok` when the file was rebuilt from its exported text byte for byte; only those files can take translations. The Thousand Years of Dreams stories and place names in `name_data.xmb` are not exported yet.

## Replace a cutscene video

A mod can replace a cutscene with an upscaled or re-rendered video. This works in the Windows build; other platforms play the original video. The replacement takes the place of the picture only: the game keeps playing the original audio and subtitles.

The video is named after the original. [Export the movies](#export-the-original-assets) to see the names: `my-export/movies/mv_00v.wmv` is replaced by the key `mv_00v`. A key has 1 to 64 characters from `a-z`, `0-9` and `_`.

What the video must be:

- MP4 container with the `moov` box at the front (ffmpeg `-movflags +faststart`).
- H.264 High profile, 8-bit `yuv420p`, progressive, BT.709 limited range, constant frame rate.
- Any frame rate from 24 to 120 fps. The game picks frames by time, so the rate does not have to match the original. Twice the original's rate is easiest when a tool doubles the frames: 60 fps for a 30 fps original, 60000/1001 for 29.97. `ffprobe` on the exported video shows its rate (most are 30 or 29.97).
- 16:9. Use 3840x2160 for desktop packs (H.264 level 5.2) and 1920x1080 for Steam Deck and phones (level 4.2).
- No audio track is needed. An audio track in the file is ignored.
- The same length as the original, lined up from the first frame. If a tool changes the frame rate, check that the length did not change: a video that runs longer or shorter drifts away from the sound.

Example ffmpeg command:

```sh
ffmpeg -i mv_00v_upscaled.mov -c:v libx264 -profile:v high -pix_fmt yuv420p -r 60000/1001 -crf 18 -preset slow -colorspace bt709 -color_primaries bt709 -color_trc bt709 -color_range tv -an -movflags +faststart mv_00v.mp4
```

Install it as a mod:

1. Make the folder `mods/my-cutscenes/overlay/movies/` in the game folder and put `mv_00v.mp4` in it.
2. Make `mods/my-cutscenes/mod.ini` with this text:

   ```ini
   api_version=2
   id=my-cutscenes
   name=My Cutscenes
   ```

3. Start the game and play the cutscene.

Instead of the `overlay/movies/` folder, a `mod.ini` line can point at the file anywhere in the mod folder (`api_version=1` also works):

```ini
movie:mv_00v=movies/mv_00v.mp4
```

`python tools/modding/lo_mod.py archive` packs `overlay/movies/*.mp4` into the `.loarc` like other files. It checks each video before packing: if `ffprobe` is installed it must be H.264, `yuv420p` and progressive, or the file is skipped with a message. It warns when the frame rate is outside 24-120 fps, the picture is not 16:9, or `moov` is after the video data.

## Make a texture pack (experimental)

A texture pack replaces the textures the game draws, at the original size or 2x, 4x or 8x larger. The steps are: export the textures, repaint or upscale the PNG files, pack them, and put the pack in `mods/`. This works on Windows only.

1. [Export the textures](#export-the-original-assets) (`--export-kinds textures`; add `--export-filter` for a sample first).
2. Repaint the PNG files yourself, or upscale them in bulk ([next section](#upscale-exported-textures-optional)).
3. [Pack them](#pack-textures-for-the-game). Point `--output` at the game's `mods` folder to try the pack right away, or at a work folder and zip it as `mods/<id>/...` to share (see [mod folders](#mod-folders-recommended)).
4. Start the game. A texture shows the original for a few frames until its replacement has been read. With Settings → System → Debug log on (or `LO_DEBUG_LOG=1`), the run log has one `[mods] texture <fingerprint> replaced` line for each texture it replaced; failures are always logged.

### Upscale exported textures (optional)

`tools/modding/texture_prep.py` upscales an export in bulk with AI upscaling models. Run it with a Python that has PyTorch, spandrel and Pillow (ComfyUI's Python works); it runs on a CUDA (NVIDIA) GPU by default, and `--device` selects another device. Download the models it names into one folder:

- `1x_DEDXT.pth` (removes DXT block artifacts)
- `4x-PBRify_RPLKSRd_V3.pth` (material textures)
- `4x-Normal-RG0-BC1.pth` (normal maps)
- `1x-BC1-smooth2.pth` and `4xNomos2_realplksr_dysample.safetensors` (UI and effects)

All of them are on [OpenModelDB](https://openmodeldb.info).

```sh
python tools/modding/texture_prep.py --export my-export --output my-upscale --dry-run
python tools/modding/texture_prep.py --export my-export --output my-upscale --models <model folder> --review 8
```

Each texture is sorted into a class: color, normal, data, ui, vfx, or skipped (light and shadow maps, engine icons, tiny images). `plan.csv` lists the class of every texture.

- **Color, UI and effects:** artifact removal, then the 4x model, then a color fix so the result scales back down to the original.
- **Normal maps:** the model upscales only X and Y. The large-scale slopes are kept, half of the added detail is used, and Z is rebuilt.
- **Data maps (specular, masks):** plain resampling only.
- **Edges:** tiling textures wrap at the edges, so no seams appear.
- **Alpha:** upscaled separately; cutouts stay hard.

Results go to `my-upscale/textures/` with the same file names, plus `index.csv` (new and original sizes) and `review/<class>.png`, which shows before and after crops. Runs resume where they stopped. Use `--classes`, `--filter` and `--limit` for a sample first. `--write-config` writes the defaults (models per class, scale, maximum size) for editing; pass the edited file back with `--config`.

### Pack textures for the game

`lo_mod.py texture-pack` turns PNGs into `.lotex2` files for runtime texture replacement. Textures are matched by fingerprint (the `fingerprint` column of the export's `index.csv`), not by key, so one file covers every package that cooks the same image. Replacements can be the original size or 2x, 4x or 8x larger (see [Modding API](Modding-API.md#runtime-textures-lotex2)).

```sh
# Mod folder (recommended): <output>/<id>/mod.ini (api_version=2) plus <id>/overlay/textures/
python tools/modding/lo_mod.py texture-pack --index my-export/textures/index.csv --images my-upscale/textures --images-index my-upscale/index.csv --output mods --layout standalone --id my-textures --name "My Texture Pack" --version 1.0

# Overlay layout (the default), for a manager: files go to <output>/overlay/textures/fp-<fingerprint>.lotex2
python tools/modding/lo_mod.py texture-pack --index my-export/textures/index.csv --images my-upscale/textures --images-index my-upscale/index.csv --output mods
```

- `--name`, `--version`, `--author` and `--description` go into the mod folder's new `mod.ini`. Packing into an existing mod folder keeps its `mod.ini`, which must say `api_version=2`.
- `--images-index` maps keys to PNG files (default: `index.csv` in the `--images` folder).
- `--filter <text>` keeps keys that contain the text. `--fingerprints <log.csv>` keeps only fingerprints a game run logged.
- `--payload dds` (recommended) compresses the textures to BC1, BC4 or BC7 and always writes the full mip chain. It needs Microsoft's `texconv` (Windows; download it from the [DirectXTex releases](https://github.com/microsoft/DirectXTex/releases)). Pass `--texconv <path>` or put it on `PATH`. DXT1 originals become BC1, DXT3 and DXT5 originals BC7, A8R8G8B8 originals BC7 (red and blue are swapped first, as the game stores B, G, R, A) and G8 originals BC4. `--bc7-all` uses BC7 for DXT1 originals too, which looks better but is twice the size. Textures whose size is not a multiple of 4 are skipped.
- `--mips` writes the whole mip chain with the default `--payload rgba8`; by default one level is written and the game builds the rest.
- Existing files are never overwritten, so you can add to an existing mods folder.
- `--test tint` (red up, green and blue down) and `--test nearest4` (plain 4x enlargement, which must look unchanged in game) transform the original exported PNGs, to check in game that replacement works.
- `lo_mod.py inspect <file>` prints and validates a `.lotex2` header.

The default `--payload rgba8` stores uncompressed pixels, so 4x packs are large (a 2048x2048 texture is 16 MiB) and use as much video memory. DDS payloads are about 4 to 8 times smaller on disk and in video memory than RGBA8 (and a full mip chain adds only a third), so a 4x upscale of the textures used in one play session shrinks from about 1.9 GB to about 340 MB. Levels larger than about 72 MiB (above 4096x4096) are skipped for RGBA8.

## Translate the game's text (experimental)

A translation is either a new language that players pick in Settings (a language pack) or a change to a language the game already has. This works on Windows only.

### Make a language pack (a new language)

1. Export a pack to translate: `LostOdysseyRecomp.exe --export-assets my-export --export-language-pack pt-br` (or type `pt-br` in **Language pack** in the [MO2 tool](Mod-Organizer-2.md#make-a-language-pack)). Use a short id of letters, digits, `-`, `_` or `.`; no two packs may use the same one. You get the folder `my-export/pt-br/` with the English text of your discs in `text/` (581 files in our export) and a `language.ini`, plus `my-export/original/`, an untouched copy for step 6; keep it next to the pack. English (`int`) is the same in the Asia and USA/Europe editions, and its fonts have every Latin-1 letter; add `--export-language jpn` (or another code) to start from another language. Export from a game with the DLC installed if you have it: without it, the DLC dungeon text is the discs' unfinished version (the exporter warns about this).
2. Open `my-export/pt-br/language.ini` in a text editor and change `name=pt-br` to the name Settings should show, for example `name=Português (Brasil)`. Save it as UTF-8. Leave `id` and `base` as they are.
3. Translate the values in the files in `my-export/pt-br/text/` and keep the keys. Keep tokens such as `{E001}` (a line break in dialogue), `{E10E}`…`{E10F}` (a speaker name) and `{E10D:0500}` (a code with its value), as well as `%s`, `%d` and `$500$` (icons), where they are. Lines you do not change stay English.
4. Try it: copy the `pt-br` folder into the game's `mods` folder, so the file is `mods/pt-br/language.ini`.
5. Start the game, open **Settings → System → Game language**, pick your language, choose **Save settings** and restart.
6. Before you share the pack, clean it: `python tools/modding/lo_mod.py language-clean my-export/pt-br` (or **Clean language pack...** in the [MO2 tool](Mod-Organizer-2.md#make-a-language-pack)). It copies only the lines you changed to `my-export/share/pt-br`. Share that folder as a ZIP of the folder itself; players install it like step 4, or with Mod Organizer 2 like any other mod. The lines you did not change are the game's own text, which must not be shared, and from a game without the DLC they would put the discs' old lines over the DLC's for players who have it.

The log shows `mods: language pack pt-br (Português (Brasil), base int)` at startup, then `[mods] text: N translation files (M from language pack pt-br)` and `[mods] text: K translated files in ...`, where K counts the game files your changes went into. If `language.ini` has a mistake, the log has a `mods:` warning that names the file.

The game runs as the base language underneath: voices, artwork with words in it and everything you have not translated stay in that language (English for `int`). While the pack is selected, its files replace the same files from other text mods. Players who pick another language do not see the pack.

### Change a language the game already has

1. Export the text: `LostOdysseyRecomp.exe --export-assets my-export --export-kinds text --export-language int` (or tick Text in the MO2 tool). Without `--export-language` you get every language on your discs.
2. Copy the files you change into the mods folder at the same path under `overlay/`, for example `my-export/text/bin/xenon/loc/int/menu/menu_int.dat.json` to `mods/overlay/text/bin/xenon/loc/int/menu/menu_int.dat.json`. To share the change, put the files in a [mod folder](#mod-folders-recommended) instead, under `mods/<id>/overlay/text/...` next to its `mod.ini`. A v1 mod lists each file in its `mod.ini`: `text:bin/xenon/loc/int/menu/menu_int.dat=text/bin/xenon/loc/int/menu/menu_int.dat.json`.
3. Change the values as in step 3 above. Entries you leave out keep the original text.
4. Start the game with the language of the files you changed (English for `int`). The log names the translated files: `[mods] text: K translated files in ...`. Restart after changing text.

### For both

Translated text can be longer than the original. Menus and message boxes do not grow, so long lines may need a manual break. Use only characters the game's font has (see [Modding API](Modding-API.md#text-language-packs)); languages that need other letters, such as Polish or Russian, are not possible yet. The Thousand Years of Dreams stories and the place names in `name_data.xmb` are not exported yet.

## Settings-menu artwork

The native settings menu draws its own artwork (the atlas `UI_MAIN_00`, 512x1024 in each of the five language packages) and the texture pages of its fonts. These are not game textures, so a texture pack does not change them; they have their own mod format (LOTEX1). The menu atlas is the confirmed case. The font pages are wired in the runtime but not confirmed as `init` consumers, and neither changes character mapping or font metrics.

Select the package for the language used by the game. `int`, `chi`, `jpn`, `kor` and `sch` are separate localized package namespaces.

### 1. Select the resource

The simplest start is the export: `LostOdysseyRecomp.exe --export-assets my-export --export-kinds textures --export-filter UI_MAIN`. Select one texture and copy its PNG into a new mod:

```sh
python tools/modding/lo_mod.py init --export-index my-export/textures/index.csv --object UI_MAIN_00 --package bin/xenon/loc/int/menu/rpmenurescommon_int.xxx --id my-menu --output my-menu/mod.json
```

This writes `mod.json` and copies the PNG to `my-menu/art/<object>.png` (use `--image` to pick another path inside the mod folder). It never overwrites an existing file. `init` accepts only confirmed consumers, such as `UI_MAIN_00`, unless you pass `--allow-unwired` (experimental). Edit the copied PNG, keep its dimensions, alpha channel and atlas layout, and continue with step 3. Other exported textures are not settings-menu artwork: replace them with a [texture pack](#make-a-texture-pack-experimental).

Instead of the export, `catalog` and `init` can read the completed [asset inventory](Asset-Inventory.md) SQLite catalog, or the older CSV manifest:

```sh
python -m pip install Pillow
python tools/modding/lo_mod.py catalog --database catalog.sqlite --runtime-only --limit 100
```

`--database`, `--manifest` and `--export-index` are mutually exclusive. A database must be schema 1 with `complete=true`; it is opened read-only. By default catalog output is limited to 100 rows and includes the canonical key, dimensions, content SHA-256, consumer label, overlay path and source packages. `--runtime-only` includes the native menu and font-page candidate labels, but `init` accepts only a confirmed consumer by default. Use `--object`, `--package` and `--content-sha256` to narrow a result. The last filter selects a package content variant used to derive the authoring identity and dimensions; the key and LOTEX1 payload do not bind a mod to that SHA at runtime. Do not guess an export index or use a CSV row number as one.

The catalog labels the native settings atlas `UI_MAIN_00` and a native font-page candidate consumer for the `Maru23`, `LocTit1` and `Abc` font owners. Native font references are not indexed as complete runtime support: `init` rejects the font-page candidates by default, and catalog output must not be read as proof that 63 font resources are already replaceable. Other catalog entries are marked `no_runtime_consumer`; `--allow-unwired` is an explicit experimental escape hatch.

### 2. Prepare artwork and a specification

Copy your own edited PNG into an authoring directory, for example `my-menu/art/UI_MAIN_00.png`. The inventory contains metadata and dimensions only; it does not export artwork. Keep the original image dimensions, alpha channel and atlas layout. The native menu's verified `UI_MAIN_00` layout is 512x1024; larger atlases are rejected by this consumer.

If you did not start from the export, generate the specification from your own catalog rather than copying an example index:

```sh
python tools/modding/lo_mod.py init --database catalog.sqlite --object UI_MAIN_00 --package bin/xenon/loc/int/menu/rpmenurescommon_int.xxx --image art/UI_MAIN_00.png --id my-menu --output my-menu/mod.json
```

`--image` is relative to the new specification, not the shell's working directory. `init` requires exactly one identity and one content variant, and refuses to overwrite an existing specification. A zero-match result requires checking the object/package/key filters; multiple content variants require `--content-sha256` to select one. The same `--database`/`--manifest`/`--export-index`, filter, and `--allow-unwired` rules apply to every form of `init`.

The generated `mod.json` contains `api_version`, `id`, `priority` and an `images` array; you may add `name`, `version`, `author` and `description`. Each image has `key`, `source`, `width` and `height`. Add more image records using keys and original dimensions from the catalog. Sources must stay inside the specification directory; absolute paths, `..` and escaping symlinks are rejected.

### 3. Build an installable ZIP

Mod folder package (default):

```sh
python tools/modding/lo_mod.py pack my-menu/mod.json --output my-menu.zip --name "My menu art" --version 1.0
```

External-manager package:

```sh
python tools/modding/lo_mod.py pack my-menu/mod.json --layout overlay --output my-menu-overlay.zip
```

The tool compiles non-animated PNGs into identity-bearing LOTEX1 files. The game does not decode mod PNG files directly. Incorrect dimensions, duplicate identities and invalid input fail the build. Existing output ZIPs are never overwritten; select a new name or deliberately remove the previous build.

Mod folder ZIP layout:

```text
mods/my-menu/mod.ini
mods/my-menu/overlay/images/key-fnv1a64-<16 lowercase hex digits>.lotex
```

The generated `mod.ini` has `api_version=2`, `id`, the optional `name`, `version`, `author` and `description`, `priority` and `enabled=true`. The metadata comes from the same fields in `mod.json` or from `--name`, `--version`, `--author` and `--description`, which win. `mod.json` is an authoring input; the runtime reads `mod.ini`, not JSON.

Overlay ZIP layout:

```text
mods/overlay/images/key-fnv1a64-<16 lowercase hex digits>.lotex
```

No shared manifest is included in an overlay ZIP. Competing mods deliberately provide the same destination path for the same identity. See [external managers](Mod-Organizer-2.md).

### 4. Install and verify

For a portable installation, extract the ZIP beside the executable so that `mods/my-menu/mod.ini` is under the runtime's mod root. For another layout, copy the ZIP's `mods/` contents to `LO_MODS_DIR` or the application's selected mods directory.

Use `LO_MODS_MODE=standalone` to ignore the top-level overlay while testing a mod folder. Restart, open the native settings menu in the chosen language, and compare an obvious artwork change. Set `enabled=false` in that mod's `mod.ini` and restart to verify restoration. Removing the mod folder also uninstalls it.

To inspect an extracted payload:

```sh
python tools/modding/lo_mod.py inspect path/to/replacement.lotex
```

This validates the file structure and prints its embedded identity and expected overlay path; it does not prove that a game draw uses that resource. For troubleshooting, check `[mods]` diagnostics, the selected language, original dimensions, root placement and whether the requested resource has an active consumer.
