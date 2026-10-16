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

Players manage installed mods in Settings → System → Mods. The page lists each mod with its `name`, `version` and `author`. The confirm button (A, or B if the game's confirm and cancel buttons are swapped) switches the selected mod On or Off, LB / RB move it up or down, and **Save mod list** (or Start) saves the order and applies it while the game runs: texture mods change at once, model mods from the next area you enter. Until then the help line says so. Party members' models, and the letters a language pack adds to the fonts, are loaded once at startup, so a change to them needs a restart; an area the game still holds in memory, such as one you just left, can show the old models until it is loaded again. The selected mod's `description` and folder appear in the help line. A mod with a broken `mod.ini` shows an error there with its first problem. Language packs are listed below the mods; choose one in Settings → System → Game language.

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

The game can export its own textures, movies, text, 3D models and animations from your game data as a starting point. The export is for your reference only: it comes from your copy of the game and must not be redistributed or bundled in a mod.

```sh
LostOdysseyRecomp.exe --export-assets my-export --export-kinds textures,movies --export-filter UI_MAIN
```

The output folder must not exist or must be empty, and it must be outside the game data. Optional arguments:

- `--export-kinds`: `textures`, `movies`, `text`, `models`, `animations`, `fonts` (each font's size, pages and letters as JSON, for [adding letters](#add-letters-the-fonts-lack)), or `fingerprints` (the texture index without PNG files). The default is textures, movies and text; models and animations are not in the default because of their size.
- `--export-filter`: only names containing this text.
- `--export-language`: the text of one language only: `int`, `jpn`, `deu`, `fra`, `spa`, `ita`, `kor`, `chi` or `sch`.
- `--export-language-pack <id>`: only text, written as a language pack to translate (see [make a language pack](#make-a-language-pack-a-new-language)).
- `--export-model-textures`: with `models`, writes each material's colour texture as a PNG file to a `textures` folder beside the models and links it from the `.glb` files.
- `--export-select <file>`: exports only the names listed in a UTF-8 text file, one per line (see [models and animations](#models-and-animations-for-reference)).
- `--export-preview`: with `models` or `animations`, writes a 512×512 PNG picture of each model, or of the character an animation set moves, in place of its `.glb` file; `index.csv` lists the pictures. The MO2 tool shows these.

No window opens; the program prints progress and exits (exit code 1 on a fatal error). With [Mod Organizer 2](Mod-Organizer-2.md#export-the-original-assets), the **Export Lost Odyssey assets** tool does the same from a window; that page has step-by-step instructions.

```text
my-export/textures/<package path>/<object>.<export index>.png
my-export/textures/index.csv        key,file,width,height,format,fingerprint,fingerprint_tiled
my-export/movies/<name>.wmv         original videos, unchanged
my-export/text/<file path>.json     {"key": "text"} for one game text file
my-export/text/index.csv            path,language,format,source,entries,round_trip
my-export/models/<package path>/<object>.<export index>.glb   3D models (glTF)
my-export/models/index.csv          key,file,class,vertices,triangles,bones
my-export/animations/<package path>/<set>.<index>.glb         animation sets (glTF)
my-export/animations/index.csv      key,file,sequences,tracks,frames,model
my-export/fonts/<package path>/<font>.json                   font size, kerning, pages and letters
my-export/export-summary.txt
```

`textures/index.csv` holds the mod key and the fingerprint of every texture, so no catalog is needed.

### Models and animations (for reference)

Models and animations are exported as glTF binaries (`.glb`). Blender opens them with **File → Import → glTF 2.0**; [Open models in Blender](Open-Models-in-Blender.md) has the steps. Models, map pieces included, can be edited and put back with a mod (see [Replace a model](#replace-a-model)); animations are for reference only.

1. Export the models, the animations or both. Add `--export-model-textures` to link each material's colour texture:

   ```sh
   LostOdysseyRecomp.exe --export-assets my-export --export-kinds models,animations --export-model-textures
   ```

   In the [MO2 tool](Mod-Organizer-2.md#export), tick **Models** (with **With textures**) and **Animations**. Both are off by default because they are large.
2. Open `my-export/models/index.csv` or `my-export/animations/index.csv` to find a file by its `key`. A model file holds the highest-detail level with its skeleton, normals, UV sets, vertex colours and material slot names. An animation file holds the character model whose skeleton the set moves (without textures), and every sequence of the set as its own glTF animation; Blender lists them as Actions (use the NLA editor to combine them). Some animation sets, such as weapon props in cutscenes and camera boxes, have no matching model and are skipped; `export-summary.txt` lists them.
3. To export only some of them, first write the list. Run the export with `--export-list` and the same kinds:

   ```sh
   LostOdysseyRecomp.exe --export-assets my-list --export-kinds models,animations --export-list
   ```

   `my-list/list.csv` has a row for each model and animation set, with its `name`. Its `name` rows give the game's own names: a map code (`u36_0`) or character code (`pc_000`, `en_043a0`) in `name`, the language in `language` and the name in `label`. Copy the `name` values you want into a UTF-8 text file, one per line, for example `selected.txt`. Then export into a new folder:

   ```sh
   LostOdysseyRecomp.exe --export-assets my-selection --export-kinds models,animations --export-select selected.txt
   ```

Text covers the menus, item and skill names and descriptions, battle messages, field dialogue, cutscene subtitles, the credits and the engine's own messages, for every language on your discs. Each game file becomes one UTF-8 JSON file with its own path plus `.json`, for example `text/bin/xenon/scr/mes/int/u3b_0_scrw.jmd.json`. Codes that are not text appear as tokens such as `{E001}` (a line break in dialogue) or `{E10D:0500}` (a code with its value); keep them where they are. `source` in `text/index.csv` is `base`, or the DLC whose version the game uses instead of the disc's (`lodlc002`, `lodlc003`). `round_trip` is `ok` when the file was rebuilt from its exported text byte for byte; only those files can take translations. The Thousand Years of Dreams stories and place names in `name_data.xmb` are not exported yet.

## Replace a cutscene video

A mod can replace a cutscene with an upscaled or re-rendered video. This works in the Windows build; other platforms play the original video. The replacement takes the place of the picture only: the game keeps playing the original audio and subtitles. The video is drawn at the game's render resolution, so a 4K video shows its full detail only when Settings → Graphics → Render resolution is as large as your screen.

The video is named after the original. [Export the movies](#export-the-original-assets) to see the names: `my-export/movies/mv_00v.wmv` is replaced by the key `mv_00v`. A key has 1 to 64 characters from `a-z`, `0-9` and `_`.

What the video must be:

- MP4 container with the `moov` box at the front (ffmpeg `-movflags +faststart`). Fragmented MP4, which Topaz Video AI writes, also works.
- H.264 High profile, 8-bit `yuv420p`, progressive, BT.709 limited range, constant frame rate.
- Any frame rate from 24 to 120 fps. The game picks frames by time, so the rate does not have to match the original. Twice the original's rate is easiest when a tool doubles the frames: 60 fps for a 30 fps original, 60000/1001 for 29.97. `ffprobe` on the exported video shows its rate (most are 30 or 29.97).
- 16:9. Use 3840x2160 for desktop packs (H.264 level 5.2) and 1920x1080 for Steam Deck and phones (level 4.2).
- No audio track is needed. An audio track in the file is ignored.
- The same length as the original picture, lined up from the first frame. A video up to 1% longer or shorter is stretched or squeezed evenly to the original's length, which covers a tool that labels 29.97 fps as 30 or drops a few frames. A bigger difference plays as it is, so check the length when a tool changes the frame rate: a video that runs longer or shorter drifts away from the sound.

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

## Replace a model

A mod can replace a 3D model with one you edited: a character, monster or moving object (a `SkeletalMesh`), or a map piece or other fixed object (`StaticMesh`, `StaticMeshRPG`), such as a building or a stretch of ground made more detailed. The new model keeps the original's materials, so it uses the original textures, or those of a [texture pack](#make-a-texture-pack-experimental). A character model also keeps the original's skeleton, so it moves with the game's own animations. Map pieces have more to keep in mind; see [Map pieces](#map-pieces-and-other-static-models).

1. Export the model. In the [MO2 tool](Mod-Organizer-2.md#export), select its row and press **Open model**, or run:

   ```sh
   LostOdysseyRecomp.exe --export-assets my-export --export-kinds models --export-model-textures --export-filter pc_000a0_m
   ```

   The file is `my-export/models/<package path>/<object>.<export index>.glb`. Kaim's is `my-export/models/bin/xenon/chr/pc/pc_000a0_m.xxx/pc_000a0_m.0.glb`.
2. In Blender, choose **File → Import → glTF 2.0** and open the file.
3. Edit the mesh: move, add or delete vertices and faces, change the UV map, paint the weights. Keep these as they are:
   - Character models: the armature and its bone names. Weight the mesh only to bones that are already there. Do not add, rename or delete bones.
   - The material names. Every face needs one of the original materials, such as `MAT_pc_000a0_bodyMT`. New materials do not work. Names that Blender gives a `.001` ending are fine.
   - A UV map.
4. Choose **File → Export → glTF 2.0**, set **Format** to **glTF Binary (.glb)**, and export. The other options can stay as they are. Textures and animations in the file are not used.
5. Put the file in a mod at the same path it was exported to, under `overlay/models/`:

   ```text
   mods/my-models/overlay/models/bin/xenon/chr/pc/pc_000a0_m.xxx/pc_000a0_m.0.glb
   ```

6. Make `mods/my-models/mod.ini` with this text:

   ```ini
   api_version=2
   id=my-models
   name=My Models
   ```

7. Start the game. The newest `logs/runtime-*.log` in the game folder shows the result:
   - `[mods] model bin/xenon/chr/pc/pc_000a0_m.xxx: pc_000a0_m: … vertices` means the model was replaced.
   - `kept original` means it was not, followed by the reason (for example a bone or material the original does not have).

Limits of character models:

- At most 65,535 vertices. The game splits the model into parts of up to 75 bones, and vertices on the seams count twice.
- Up to 4 bone weights per vertex. Extra weights are dropped and the largest 4 are kept.
- The replacement is shown at every distance; the original's lower-detail versions are not used.
- A few models are in more than one package, such as a boss in its battle and event packages. Export and replace each copy.

### Map pieces and other static models

Buildings, ground, walls and other fixed parts of a map are in the map's `_map.xxx` package. A magic car in Uhra, for example, is `my-export/models/bin/xenon/field/u3b_0/u3b_0_map.xxx/StaticMesh_467.61.glb`. The file names do not say what a piece is: export a whole map with `--export-filter u3b_0_map` and open the files in Blender to find the one you want.

The steps are the same as above. In addition:

- Keep the model where the original is. The map's light and shadow were worked out in advance for each vertex of the original. When the game starts, each vertex of your model takes them from the point of the original's surface nearest to it, so a more detailed version of the same shape is lit like the original. Parts far from the original's surface take the light of the nearest original part.
- Faces with a material that is not one of the original's, or with none, take the material of the original surface beneath them. This keeps the materials of a model that an upscaling tool merged or renamed.
- In Blender's glTF export, open **Data → Mesh** and set **Vertex Color** to **Active**. With the default, some models are exported with white vertex colours.
- Where characters can walk and where they stop is set by the map's own collision, which stays as it was. Keep floors and walls where they were, or characters will walk through the new shapes or float above them.
- `[mods] model lighting … follow the new vertices` in the log means the light and shadow were carried over.

Limits of map pieces:

- At most 65,536 vertices per model. Most map pieces have a few hundred to a few thousand, so there is room for many times more detail.
- The first start after a mod is installed or changed, or after a game update that changes how mods are applied, rebuilds the game files it changes: about half a minute to a minute per disc on an 8-core PC for a mod that replaces every map's pieces, longer on fewer cores. Meanwhile the window shows "Preparing mods" with a progress bar. The rebuilt files are kept in the `cache/mods` folder (in the game folder, or next to the shader cache when the game folder cannot be written) and later starts read them from there. For such a mod they take up to about 6 GB per disc. Rebuilt files that no longer fit in the disc's own 4 GB archives go into extra `xenon_mod0.fpd`, `xenon_mod1.fpd`… files there. While you play, the other discs' files are rebuilt in the background, the next disc first, on one low-priority thread that rests half the time (about 6 to 15 minutes per disc on an 8-core PC), so switching discs later only reads them; a switch that comes before that waits with the same progress window. `LO_MODS_PREBUILD=0` turns the background rebuild off (on Android it is off unless `LO_MODS_PREBUILD=1`), and `LO_MODS_PREBUILD_THREADS` gives it more threads. The game keeps the files of your current and your previous set of mods, also while it runs with no mods, so turning a mod off and on again does not rebuild; older sets are removed. Delete `cache/mods` to free the space.

`python tools/modding/lo_mod.py archive` packs `overlay/models/` into the `.loarc` like other files.

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

### Add letters the fonts lack

The game draws text with bitmap fonts: each letter is a small picture on a texture page, white with a dark outline, at one fixed size. A language that needs letters those fonts do not have, such as Polish, Russian or Arabic, can add them to a language pack: `lo_mod.py font-glyphs` draws every letter the pack's text uses and the fonts lack from a TrueType or OpenType font, at each game font's size and in its style, and the game adds them to its fonts while the pack is selected.

1. Export the fonts and their pages: `LostOdysseyRecomp.exe --export-assets my-fonts --export-kinds textures,fonts --export-filter rpfonts`. Besides the page PNGs in `textures/`, `my-fonts/fonts/<package path>/<font>.json` has each font's size, kerning, pages and letters.
2. Translate the pack's text first: the letters drawn are the ones it uses. `--chars` adds more.
3. Install what the command needs: `python -m pip install Pillow fonttools freetype-py uharfbuzz`.
4. Draw the letters with a font whose licence lets you share it, such as an OFL font:

   ```sh
   python tools/modding/lo_mod.py font-glyphs my-export/pt-br --export my-fonts --ttf MyFont-Regular.ttf
   ```

   For each font of the pack's base language (`rpfontscommon_int.xxx`, `rpfontsdream_int.xxx`, `rpfontstitle_int.xxx` for `int`) it writes `my-export/pt-br/fonts/<package path>/<font>.lofont` and prints how many letters it drew and at which size. The size matches the TTF's `H` to the game's `H`, then shrinks until every new letter fits the font's line height; `--scale` makes them larger or smaller, `--fonts Maru23,Meiyo18` limits the fonts. A heavier weight of the TTF reads better at the small sizes. A font without `H`, such as the Japanese `Yume24` (kana and kanji only), is measured by a letter it shares with a font that has one (`口`, `あ`, `ア`, a digit…): its `H` would stand where it does relative to that letter there. Fonts without letters, such as `Icon`, are skipped.
5. Start the game with the pack selected. The log shows `[mods] font: N glyph files in M packages from language pack pt-br` and one `[mods] font <package>: <font>: … glyphs` line per font; the first start after a change rebuilds the font packages with the other mod files. The game reads its fonts once, at startup, so changed glyph files need a restart; saving the Mods page does not apply them.

`language-clean` copies the pack's `fonts/` folder with the text. A letter the game's font already has keeps the game's own glyph unless `fonts/` draws it again.

The game draws every line from left to right and does not join letters. Arabic, Persian and other right-to-left scripts work when the text in the pack is already shaped and in display order: each letter as its presentation form (U+FB50–U+FEFF, which `font-glyphs` draws by shaping the letter with its neighbours) and each line reversed, with codes such as `{E001}` kept in place. Lines the game wraps on its own, such as long subtitles, wrap from the wrong end; break them by hand.

### Change a language the game already has

1. Export the text: `LostOdysseyRecomp.exe --export-assets my-export --export-kinds text --export-language int` (or tick Text in the MO2 tool). Without `--export-language` you get every language on your discs.
2. Copy the files you change into the mods folder at the same path under `overlay/`, for example `my-export/text/bin/xenon/loc/int/menu/menu_int.dat.json` to `mods/overlay/text/bin/xenon/loc/int/menu/menu_int.dat.json`. To share the change, put the files in a [mod folder](#mod-folders-recommended) instead, under `mods/<id>/overlay/text/...` next to its `mod.ini`. A v1 mod lists each file in its `mod.ini`: `text:bin/xenon/loc/int/menu/menu_int.dat=text/bin/xenon/loc/int/menu/menu_int.dat.json`.
3. Change the values as in step 3 above. Entries you leave out keep the original text.
4. Start the game with the language of the files you changed (English for `int`). The log names the translated files: `[mods] text: K translated files in ...`. Restart after changing text.

### For both

Translated text can be longer than the original. Menus and message boxes do not grow, so long lines may need a manual break. Use only characters the game's font has (see [Modding API](Modding-API.md#text-language-packs)); a language pack can [add the letters it lacks](#add-letters-the-fonts-lack). The Thousand Years of Dreams stories and the place names in `name_data.xmb` are not exported yet.

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

Use `LO_MODS_MODE=standalone` to let the mod folder win over the top-level overlay while testing it. Restart, open the native settings menu in the chosen language, and compare an obvious artwork change. Set `enabled=false` in that mod's `mod.ini` and restart to verify restoration. Removing the mod folder also uninstalls it.

To inspect an extracted payload:

```sh
python tools/modding/lo_mod.py inspect path/to/replacement.lotex
```

This validates the file structure and prints its embedded identity and expected overlay path; it does not prove that a game draw uses that resource. For troubleshooting, check `[mods]` diagnostics, the selected language, original dimensions, root placement and whether the requested resource has an active consumer.
