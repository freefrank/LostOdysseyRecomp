# Creating and installing image mods

Read [current support](Modding.md) first. Start with the native settings-menu atlas `UI_MAIN_00`; an arbitrary exported Texture2D is not necessarily consumed by the native menu. Native font texture pages may also be replaced, but this does not change character mapping or font metrics.

## 1. Select an actual resource

Run these commands from a source checkout containing PR #68. Python 3.10+ is required; Pillow is needed only for PNG conversion.

```sh
python -m pip install Pillow
python tools/modding/lo_mod.py catalog --manifest manifest.csv --object UI_MAIN_00
```

The catalog reads `status=exported`, `cls=Texture2D` rows and prints canonical keys, dimensions and preview paths. It ignores failed/package/font rows, de-duplicates identical identities across discs and rejects conflicting dimensions. Do not guess an export index or use the CSV row number as one.

Select the package for the language used by the game. `int`, `chi`, `jpn`, `kor` and `sch` are separate localized package namespaces.

## 2. Prepare artwork and a specification

Copy your edited PNG into an authoring directory, for example `my-menu/art/UI_MAIN_00.png`. Keep the original image dimensions, alpha channel and atlas layout. The native menu's verified `UI_MAIN_00` layout is 512x1024; larger atlases are rejected by this consumer.

Generate the identity from your own catalog rather than copying an example index:

```sh
python tools/modding/lo_mod.py init --manifest manifest.csv --object UI_MAIN_00 --package bin/xenon/loc/int/menu/rpmenurescommon_int.xxx --image art/UI_MAIN_00.png --id my-menu --output my-menu/mod.json
```

`--image` is relative to the new specification, not the shell's working directory. `init` requires exactly one distinct matching identity and refuses to overwrite an existing specification. A zero-match result is a reason to inspect the catalog, not to invent another key.

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
