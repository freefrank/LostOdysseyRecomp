# Creating and installing image mods

Read [current support](Modding.md) first. Start with the native settings-menu atlas `UI_MAIN_00`; an arbitrary exported Texture2D is not necessarily consumed by the native menu. The catalog lists native font-page candidates, but they are not confirmed `init` consumers and do not change character mapping or font metrics.

## 1. Select an actual resource

Run these commands from a source checkout containing the Mod API. Python 3.10+ is required; Pillow is needed only for PNG conversion. The preferred source is the completed asset-inventory SQLite catalog. The older CSV manifest remains supported for compatibility. The verified `UI_MAIN_00` native atlas is 512x1024 in each of the five language packages.

```sh
python -m pip install Pillow
python tools/modding/lo_mod.py catalog --database catalog.sqlite --runtime-only --limit 100
```

`--database` and the legacy `--manifest` input are mutually exclusive. A database must be schema 1 with `complete=true`; it is opened read-only. By default catalog output is limited to 100 rows and includes the canonical key, dimensions, content SHA-256, consumer label, overlay path and source packages. `--runtime-only` includes the native menu and font-page candidate labels, but `init` accepts only a confirmed consumer by default. Use `--object`, `--package` and `--content-sha256` to narrow a result. The last filter selects a package content variant used to derive the authoring identity and dimensions; the key and LOTEX1 payload do not bind a mod to that SHA at runtime. Do not guess an export index or use a CSV row number as one.

The catalog labels the native settings atlas `UI_MAIN_00` and a native font-page candidate consumer for the `Maru23`, `LocTit1` and `Abc` font owners. Native font references are not indexed as complete runtime support: `init` rejects the font-page candidates by default, and catalog output must not be read as proof that 63 font resources are already replaceable. Other catalog entries are marked `no_runtime_consumer`; `--allow-unwired` is an explicit experimental escape hatch.

Select the package for the language used by the game. `int`, `chi`, `jpn`, `kor` and `sch` are separate localized package namespaces.

## 1b. Export the original artwork (optional)

The game can export its own textures, movies and text from your game data as a starting point. The export is for your reference only: it comes from your copy of the game and must not be redistributed or bundled in a mod.

```sh
LostOdysseyRecomp.exe --export-assets my-export --export-kinds textures,movies --export-filter UI_MAIN
```

The output folder must not exist or must be empty. `--export-kinds` (`textures`, `movies`, `text`, or `fingerprints` for the texture index without PNG files; default: textures, movies and text) and `--export-filter` (only names containing the text) are optional. No window opens; the program prints progress and exits (exit code 1 on a fatal error). With [Mod Organizer 2](Mod-Organizer-2.md#export-the-original-assets), the **Export Lost Odyssey assets** tool does the same from a window; that page has step-by-step instructions.

```text
my-export/textures/<package path>/<object>.<export index>.png
my-export/textures/index.csv        key,file,width,height,format,fingerprint,fingerprint_tiled
my-export/movies/<name>.wmv         original videos, unchanged
my-export/text/<file path>.json     {"key": "text"} for one game text file
my-export/text/index.csv            path,language,format,source,entries,round_trip
my-export/export-summary.txt
```

Text covers the menus, item and skill names and descriptions, battle messages, field dialogue, cutscene subtitles, the credits and the engine's own messages, for every language on your discs. Each game file becomes one UTF-8 JSON file with its own path plus `.json`, for example `text/bin/xenon/scr/mes/int/u3b_0_scrw.jmd.json`. Codes that are not text appear as tokens such as `{E001}` (a line break in dialogue) or `{E10D:0500}` (a code with its value); keep them where they are. `source` in `index.csv` is `base`, or the DLC whose version the game uses instead of the disc's (`lodlc002`, `lodlc003`). `round_trip` is `ok` when the file was rebuilt from its exported text byte for byte; only those files can take translations. See [1e](#1e-translate-the-games-text-experimental) for putting translations back into the game. The Thousand Years of Dreams stories and place names in `name_data.xmb` are not exported yet.

`index.csv` holds the mod key of every texture, so no catalog is needed. Select one texture and copy its PNG into a new mod:

```sh
python tools/modding/lo_mod.py init --export-index my-export/textures/index.csv --object UI_MAIN_00 --package bin/xenon/loc/int/menu/rpmenurescommon_int.xxx --id my-menu --output my-menu/mod.json
```

This writes `mod.json` and copies the PNG to `my-menu/art/<object>.png` (use `--image` to pick another path inside the mod folder). It never overwrites an existing file, and it follows the same consumer rule as `--database`: only confirmed consumers such as `UI_MAIN_00` unless you pass `--allow-unwired`. Edit the copied PNG, then continue with step 3. Today only the native settings-menu atlas and font pages are replaceable in-game; other exported textures are reference material until the general texture path exists.

## 1c. Upscale exported textures (optional, preparation)

`tools/modding/texture_prep.py` upscales an export in bulk with AI upscaling models. Run it with a Python that has PyTorch, spandrel and Pillow (ComfyUI's Python works). Download the models it names into one folder:

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

Pack the results with `texture-pack` (next section) to use them in game.

## 1d. Pack textures for the game (experimental)

`lo_mod.py texture-pack` turns PNGs into `.lotex2` files for runtime texture replacement. Textures are matched by fingerprint (the `fingerprint` column of the export's `index.csv`), not by key, so one file covers every package that cooks the same image. Replacements can be the original size or 2x, 4x or 8x larger (see [Modding API](Modding-API#runtime-textures-lotex2)).

```sh
# Overlay layout: files go to <output>/overlay/textures/fp-<fingerprint>.lotex2
python tools/modding/lo_mod.py texture-pack --index my-export/textures/index.csv --images my-upscale/textures --images-index my-upscale/index.csv --output mods

# Standalone layout: <output>/<id>/mod.ini with texture: lines plus <id>/textures/
python tools/modding/lo_mod.py texture-pack --index my-export/textures/index.csv --images my-upscale/textures --images-index my-upscale/index.csv --output mods --layout standalone --id my-textures
```

- `--images-index` maps keys to PNG files (default: `index.csv` in the `--images` folder).
- `--filter <text>` keeps keys that contain the text. `--fingerprints <log.csv>` keeps only fingerprints a game run logged.
- `--payload dds` (recommended) compresses the textures to BC1, BC4 or BC7 and always writes the full mip chain. It needs Microsoft's `texconv` (Windows; download it from the [DirectXTex releases](https://github.com/microsoft/DirectXTex/releases)). Pass `--texconv <path>` or put it on `PATH`. DXT1 originals become BC1, DXT3 and DXT5 originals BC7, A8R8G8B8 originals BC7 (red and blue are swapped first, as the game stores B, G, R, A) and G8 originals BC4. `--bc7-all` uses BC7 for DXT1 originals too, which looks better but is twice the size. Textures whose size is not a multiple of 4 are skipped.
- `--mips` writes the whole mip chain with the default `--payload rgba8`; by default one level is written and the game builds the rest.
- Existing files are never overwritten, so you can add to an existing mods folder.
- `--test tint` (red up, green and blue down) and `--test nearest4` (plain 4x enlargement, which must look unchanged in game) transform the original exported PNGs, to check in game that replacement works.
- `lo_mod.py inspect <file>` prints and validates a `.lotex2` header.

The default `--payload rgba8` stores uncompressed pixels, so 4x packs are large (a 2048x2048 texture is 16 MiB) and use as much video memory. DDS payloads are about 4 to 8 times smaller on disk and in video memory than RGBA8 (and a full mip chain adds only a third), so a 4x upscale of the textures used in one play session shrinks from about 1.9 GB to about 340 MB. Levels larger than about 72 MiB (above 4096x4096) are skipped for RGBA8. With Settings > System > Debug log on (or `LO_DEBUG_LOG=1`), the run log has one `[mods] texture <fingerprint> replaced` line for each texture it replaced; failures are always logged.

## 1e. Translate the game's text (experimental)

1. Export the text of your discs: `LostOdysseyRecomp.exe --export-assets my-export --export-kinds text` (or tick Text in the MO2 tool). Translate from the English files (`int` in the paths): they are the same in the Asia and USA/Europe editions, and their fonts have every Latin-1 letter.
2. Copy the files you translate into the mods folder at the same path under `overlay/`, for example `my-export/text/bin/xenon/loc/int/menu/menu_int.dat.json` to `mods/overlay/text/bin/xenon/loc/int/menu/menu_int.dat.json`. With Mod Organizer 2, make a mod whose folder holds `overlay/text/...`. A standalone mod lists each file in its `mod.ini` instead: `text:bin/xenon/loc/int/menu/menu_int.dat=text/bin/xenon/loc/int/menu/menu_int.dat.json`.
3. Translate the values and keep the keys. Keep tokens such as `{E001}` (a line break in dialogue), `{E10E}`…`{E10F}` (a speaker name) and `{E10D:0500}` (a code with its value), as well as `%s`, `%d` and `$500$` (icons), where they are. Leave out entries you do not translate: they stay in the original language.
4. Start the game with the same language setting as the files you translated (English for `int`). The log names the translated files: `[mods] text: 574 translated files in ...`. Restart after changing text.

Translated text can be longer than the original. Menus and message boxes do not grow, so long lines may need a manual break. The Thousand Years of Dreams stories and the place names in `name_data.xmb` are not exported yet, and a translation cannot yet be offered as its own language in Settings. See [Modding API](Modding-API#text-language-packs) for the file rules.

## 2. Prepare artwork and a specification

Copy your own edited PNG into an authoring directory, for example `my-menu/art/UI_MAIN_00.png`. The inventory contains metadata and dimensions only; it does not export artwork. Keep the original image dimensions, alpha channel and atlas layout. The native menu's verified `UI_MAIN_00` layout is 512x1024; larger atlases are rejected by this consumer.

Generate the identity from your own catalog rather than copying an example index:

```sh
python tools/modding/lo_mod.py init --database catalog.sqlite --object UI_MAIN_00 --package bin/xenon/loc/int/menu/rpmenurescommon_int.xxx --image art/UI_MAIN_00.png --id my-menu --output my-menu/mod.json
```

`--image` is relative to the new specification, not the shell's working directory. `init` requires exactly one identity and one content variant, and refuses to overwrite an existing specification. A zero-match result requires checking the object/package/key filters; multiple content variants require `--content-sha256` to select one. The same `--database`/`--manifest`, filter, and `--allow-unwired` rules apply to `init`.

The generated `mod.json` contains `api_version`, `id`, `priority` and an `images` array. Each image has `key`, `source`, `width` and `height`. Add more image records using keys and original dimensions from the catalog. Sources must stay inside the specification directory; absolute paths, `..` and escaping symlinks are rejected.

## 3. Build an installable ZIP

Standalone package:

```sh
python tools/modding/lo_mod.py pack my-menu/mod.json --output my-menu-standalone.zip
```

External-manager package:

```sh
python tools/modding/lo_mod.py pack my-menu/mod.json --layout overlay --output my-menu-overlay.zip
```

The tool compiles non-animated PNGs into identity-bearing LOTEX1 files. The game does not decode mod PNG files directly. Incorrect dimensions, duplicate identities and invalid input fail the build. Existing output ZIPs are never overwritten; select a new name or deliberately remove the previous build.

Standalone ZIP layout:

```text
mods/my-menu/mod.ini
mods/my-menu/images/key-fnv1a64-<16 lowercase hex digits>.lotex
```

The generated `mod.ini` includes `api_version=1`, `id`, `priority`, `enabled=true` and one `image:<canonical-key>=<relative-file>` entry per image. `mod.json` is an authoring input; the runtime reads `mod.ini`, not JSON.

Overlay ZIP layout:

```text
mods/overlay/images/key-fnv1a64-<16 lowercase hex digits>.lotex
```

No shared manifest is included in an overlay ZIP. Competing mods deliberately provide the same destination path for the same identity. See [external managers](Mod-Organizer-2.md).

## 4. Install and verify

For a portable standalone installation, extract the ZIP beside the executable so that `mods/my-menu/mod.ini` is under the runtime's mod root. For another layout, copy the ZIP's `mods/` contents to `LO_MODS_DIR` or the application's selected mods directory.

Use `LO_MODS_MODE=standalone` to ignore merged overlays during a standalone test. Restart, open the native settings menu in the chosen language, and compare an obvious artwork change. Set `enabled=false` in that mod's manifest and restart to verify restoration. Removing the mod folder also uninstalls it.

To inspect an extracted payload:

```sh
python tools/modding/lo_mod.py inspect path/to/replacement.lotex
```

This validates the file structure and prints its embedded identity and expected overlay path; it does not prove that a game draw uses that resource. For troubleshooting, check `[mods]` diagnostics, the selected language, original dimensions, root placement and whether the requested resource has an active consumer.
