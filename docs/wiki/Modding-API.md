# Modding API

The public source interface is `LostOdysseyRecomp/modding/mod_api.h`; image decoding is in `image_mod.h`. The `api_version` of `mod.ini` (1 or 2) versions the data contract. It is not a stable binary DLL/plugin ABI.

## Resource identity

```text
<normalized package>#<zero-based export_index>:<object>
```

Use the extractor's `package`, `export_index` and `object` fields. CSV row IDs, absolute extraction paths, guest addresses and PNG bytes are not resource identities. Normalize backslashes to slashes, remove redundant `.`/separators and lowercase ASCII package letters. Preserve object case and UTF-8 bytes. Keep the localized package path. Reject absolute paths, drive prefixes, `..`, control characters and reserved identity delimiters. Keys are at most 4096 UTF-8 bytes; export indices are unsigned 32-bit integers.

```cpp
const auto key = modding::MakeManifestKey(package, exportIndex, object);
modding::AssetRequest request{{modding::AssetKind::Image, key}, originalPath};
if (auto image = modding::ReadImageReplacement(request, width, height)) {
    // Consume image->pixels as native 0xAARRGGBB words.
} else {
    // Decode the original asset through the existing loader.
}
```

`originalPath` is context for a trusted provider. `Resolve()` does not open it or automatically decode the original. The consumer owns fallback and format validation.

## Root, modes and precedence

`Initialize(defaultRoot, modList)` snapshots the mods folder: the mod folders and their `mod.ini` files, the files under each `api_version=2` mod's own `overlay/`, the order from `modList` (`mod-list.ini`), and which kind folders of the top-level `overlay/` exist. `LO_MODS_DIR` overrides the root. The game's `--mods-mode <mode>` argument sets `LO_MODS_MODE` for launchers that cannot set environment variables, such as Mod Organizer 2. Use an absolute override for reproducibility. `LO_MODS=0` or `LO_MODS=false` disables all resolution. An invalid nonempty `LO_MODS_MODE` disables resolution and emits a diagnostic.

| `LO_MODS_MODE` | Lookup order before the original asset |
| --- | --- |
| `combined` or unset | Top-level overlay, trusted provider, mod folders in order. |
| `standalone` | Trusted provider, mod folders in order; top-level overlay ignored. |
| `overlay` | Top-level overlay only; mod folders and providers are not consulted. |

The top-level overlay (`mods/overlay/`) holds the files of overlay packages, which a manager such as Mod Organizer 2 merges in its own order. Its files are checked on each request, for the kind folders (`textures`, `images`, `text`, …) that existed at `Initialize`. Files of mod folders are listed once, on a background thread that `Initialize` starts, because listing through Mod Organizer 2's virtual file system costs about 150 µs per file (about 2.5 s for 15,000 textures). A request that comes before the listing is done waits for it; later requests are table lookups, not file-system probes. Text is listed separately, so the game's text never waits for a large texture folder.

`overlay` mode prevents a mod disabled in an external manager from reappearing through a second installation or provider. Normal overlay absence returns no replacement. After a file is selected, invalid contents fall back to the original; the decoder does not search lower-priority mods for a different payload. The same holds for a mod-folder file deleted after `Initialize`.

`Reload()` rebuilds the snapshot using the original default root, the same mod list path and current environment overrides. Initialization, reload, shutdown and provider changes advance `Generation()`. Consumers must invalidate decoded caches when the generation changes. The native menu cache already does this. Reload host configuration at a safe application boundary; there is no automatic file watcher. Do not mutate process environment concurrently with resolution/reload.

## Mod folders

Mod folders are the direct, non-hidden directories under the root that hold a `mod.ini`. A folder with a `language.ini` is a [language pack](#text-language-packs) instead, never a mod; a `mod.ini` next to it is ignored with a diagnostic. Both kinds sit side by side in the mods root. The `overlay` directory is reserved. A manifest is UTF-8 `key=value` text, at most 1 MiB, with optional BOM. Whole-line `#` and `;` comments are accepted. There are no INI sections, quoting or inline comments.

### Format v2 (`api_version=2`)

```text
mods/<id>/mod.ini
mods/<id>/overlay/textures/fp-<16 hex digits>.lotex2
mods/<id>/overlay/images/key-fnv1a64-<16 hex digits>.lotex
mods/<id>/overlay/text/<member path>.json
```

```ini
api_version=2
id=hd-textures-4x
name=4x HD Texture Pack
version=1.0
author=Someone
description=Upscaled textures for every map.
priority=0
enabled=true
```

- Files under the mod's own `overlay/` are found by name. The names are exactly those of the top-level overlay ([Manager overlay paths](#manager-overlay-paths)) and match regardless of ASCII case. Other files and folders there are ignored.
- Resource lines (`texture:`, `image:`, `text:`, …) still work. Within one mod, a resource line beats the overlay file for the same resource.
- `name`, `version`, `author` and `description` are optional display text for mod managers: one line of UTF-8 each, without control characters, at most 128, 64, 128 and 1024 bytes. Leading and trailing spaces are removed.
- Only `api_version=2` mods are searched for `overlay/` files. Runtimes that know only v1 reject a v2 `mod.ini` as invalid metadata, so an older game never loads such a mod without its files.

### Format v1 (`api_version=1`)

```ini
api_version=1
id=my-menu
priority=100
enabled=true
# image:<canonical-key>=images/replacement.lotex
```

A v1 mod lists every file in a resource line. It may not use the v2 display fields.

### Rules for both versions

`api_version` is mandatory. ID defaults to the directory name and must contain 1-128 ASCII letters, digits, `.`, `_` or `-`, excluding `.` and `..`. Priority defaults to zero and is a signed 32-bit integer. Enabled defaults to true; accepted values are `true`, `false`, `1`, `0`. Unknown/duplicate/invalid metadata rejects the manifest. Disabled mods do not participate. Enabled duplicate IDs reject the lexically later directory.

Resource kinds are `image`, `font`, `model`, `movie`, `texture` and `text`. Invalid resource lines, and lines whose file is missing, are diagnosed and skipped. Payload paths are relative to the mod directory and may not be absolute or contain `..`. Containment is checked on the path as written, not after symlink resolution, so symlinked mod folders and a manager's virtual file system work. Metadata is parsed before resources, so a trailing priority or enabled field applies to the whole mod. Within the same manifest, the last line for a resource wins.

### Order and `mod-list.ini`

The in-game mod manager keeps its order in `mod-list.ini` next to `settings.ini` (in the game folder for the portable Windows layout), outside `mods/`: Mod Organizer 2 virtualizes only `mods/` and would send writes there to its Overwrite folder. The game reads the file at startup. It writes it only when the manager saves.

```ini
# First line = highest priority.
hd-textures-4x=on
my-menu=off
```

- One `<mod id>=on` or `<mod id>=off` line per mod (`true`, `false`, `1`, `0` work too), first line highest. Comments as in `mod.ini`. Bad lines and repeated ids are reported; the first line of an id counts.
- A mod takes part when its `mod.ini` does not say `enabled=false` and the list does not say `off`.
- Listed mods come first, in list order. Mods not in the list follow, by `priority` (higher first) and then folder name (lexically later first), and count as `on`. Without a list this is the v1 order.
- Ids without an installed mod are ignored and stay in the file, so a mod disabled in MO2 (its folder disappears) returns to its place.
- The list orders mod folders only. Language packs have no line in it; players choose one in Settings → System → Game language.

For each resource, the first mod in this order that has it wins, with its resource line or else its overlay file. Examples, all for the texture `fp-…ab` in combined mode:

| Mods | Result |
| --- | --- |
| `hd` (priority 10) and `alt` (priority 5) both have it, no list | `hd` |
| Same, list `alt=on` | `alt`: listed mods come before the others |
| Same, list `hd=off` | `alt` |
| `hd` has `texture:…ab=custom.lotex2` and `overlay/textures/fp-…ab.lotex2` | `hd`'s `custom.lotex2` |
| `mods/overlay/textures/fp-…ab.lotex2` exists as well | the top-level file |

Changes to the list or to the mod folders take effect at the next start (`Initialize` or `Reload`).

### Manager API

```cpp
std::vector<modding::ModInfo> modding::ListMods();   // all mod folders, highest first, active or not
std::filesystem::path modding::ModListPath();        // the mod-list.ini given to Initialize
bool modding::SaveModList(const std::vector<std::pair<std::string, bool>>& order, std::string* error = nullptr);
```

`ModInfo` holds the id (the folder name when the manifest has none or is rejected), the v2 display fields, the folder, priority, `apiVersion` (0 when the manifest is rejected), `manifestEnabled`, `listEnabled`, `active` (contributes files now), `overlayFiles` and `manifestEntries` (counted for active mods), and `problems` (that mod's diagnostics). In `overlay` mode every mod is listed and none is active; with mods disabled the list is empty. `ListMods` waits for the background listing.

Language packs follow the mods, by folder name, as read-only entries: `kind` is `ModKind::LanguagePack` (`ModKind::Mod` for mods), with the pack's `id`, `name` and `base` (the folder name and empty fields when its `language.ini` is rejected) and its `problems`. They are never `active` here and take no part in `SaveModList`; a manager shows them and points to Settings → System → Game language.

`SaveModList` takes the order highest first, `true` for on. It checks the ids, writes a temporary file next to `mod-list.ini` and renames it over the old one. Lines of the old file whose id is not in `order`, and comments, stay right after the id that preceded them. The current snapshot does not change; the new order applies after `Reload()` or a restart. All functions are thread-safe; snapshots are immutable.

## Manager overlay paths

`OverlayRelativePath({kind, key})` returns a path relative to the mods root. A format v2 mod folder uses the same names under `<id>/`. For images:

```text
overlay/images/key-fnv1a64-<hash>.lotex
```

Compute FNV-1a-64 over canonical-key UTF-8 bytes only: offset basis `14695981039346656037`, xor each byte, multiply by `1099511628211` modulo 2^64. Format 16 lowercase hex digits. The type selects the directory, not the hash input. Reserved kinds use `fonts`, `models`, `movies` and `.loasset`; their payload formats are not defined yet.

The filename hash is a lookup convenience, not a cryptographic identity. LOTEX1 also contains the full canonical key, which must exactly match the request. Do not rename a payload to target a different resource.

## LOTEX1 image payload

All integers are little-endian; the fixed header is exactly 24 bytes.

| Byte offset | Size | Value |
| --- | --- | --- |
| 0 | 8 | ASCII `LOTEX1` followed by CR LF |
| 8 | 4 | Width |
| 12 | 4 | Height |
| 16 | 4 | Canonical-key length in UTF-8 bytes |
| 20 | 4 | Pixel format: `1` for RGBA8 |
| 24 | key length | Canonical-key bytes, no terminator |
| Following key | width × height × 4 | Tightly packed RGBA bytes, rows top to bottom |

Dimensions must be nonzero and at most 8192 per axis, with at most 16,777,216 pixels (64 MiB). Exact total size is required; truncated/extra data, unknown formats and identity mismatches fail. `ReadImageReplacement` accepts only `AssetKind::Image` and requires dimensions equal to those supplied by the consumer. Native menu/font atlases require their original extents and layout. File bytes are RGBA; returned native pixel words are `0xAARRGGBB`. Do not write host-endian ARGB words into the payload.

## Runtime textures (LOTEX2)

Textures the game draws are replaced by **fingerprint**, not by key. When the renderer uploads a texture, it computes the XXH3-64 of the base level's blocks in row order after the guest endian swap. This is the `fingerprint` column that `--export-assets` writes to `textures/index.csv`. Several keys can share a fingerprint, because the game cooks the same image into several packages, so one replacement covers all of them.

- Identity: `AssetKind::Texture` (5). The key is the fingerprint as 16 lowercase hex digits.
- Overlay path: `overlay/textures/fp-<16 hex digits>.lotex2`. The fingerprint is the file name, so two overlay packages that replace the same image conflict on the same path in a manager.
- Mod folder: `<id>/overlay/textures/fp-<16 hex digits>.lotex2` (format v2), or a resource line `texture:<16 hex digits>=textures/<name>.lotex2`.
- Modes and precedence follow the image rules.

All integers are little-endian. The fixed header is 64 bytes.

| Byte offset | Size | Value |
| --- | --- | --- |
| 0 | 8 | ASCII `LOTEX2` followed by CR LF |
| 8 | 4 | Header size: 64 + key length |
| 12 | 4 | Payload type: `1` RGBA8, `2` DDS |
| 16 | 8 | Fingerprint; must equal the requested one |
| 24 | 4 | Original Xenos format: `2` G8, `6` A8R8G8B8, `18` DXT1, `19` DXT3, `20` DXT5 |
| 28 | 4 | Original width |
| 32 | 4 | Original height |
| 36 | 4 | Payload width (level 0) |
| 40 | 4 | Payload height (level 0) |
| 44 | 4 | Mip levels in the payload, at least 1 |
| 48 | 8 | Payload size in bytes |
| 56 | 4 | Key length in UTF-8 bytes; may be 0 (the key is informational) |
| 60 | 4 | Reserved, 0 |
| 64 | key length | Key bytes, no terminator |
| Following key | payload size | Payload |

Payload width and height are the original width and height times the same factor 1, 2, 4 or 8, and at most 8192 each. Type 1 stores the mip levels top first. Level *i* is max(1, w >> *i*) × max(1, h >> *i*) RGBA bytes, rows top to bottom, with no padding. The payload size must be exact, and the mip count may not exceed the full chain. With one level the game builds the rest of the chain itself. Channels are always plain RGBA: a G8 original reads the red channel, and the game maps A8R8G8B8 replacements to the original channel order. Any validation failure keeps the original texture.

Type 2 stores a complete DDS file with block-compressed levels:

- Formats: BC1, BC3, BC4 or BC7, all UNORM. The DDS can use the DX10 header (DXGI formats 71, 77, 80, 98) or the legacy FourCC `DXT1`, `DXT5` or `ATI1`/`BC4U`.
- Shape: 2D, one array slice, not a cube map.
- Size: the DDS width, height and mip count must equal the LOTEX2 header's payload width, height and mip count. Level 0 must be a multiple of 4 on both sides.
- Channels follow the original's host texture rather than plain RGBA. A G8 original uses BC4 (one channel). An A8R8G8B8 original stores B, G, R, A, so the packer swaps red and blue before compressing. DXT originals use plain RGBA.
- The game uploads the blocks unchanged. Devices without BC support (some Mali GPUs) keep the original texture.
- Levels the device cannot address as whole blocks are dropped, as for the original uploads.

The game replaces only uploads it can match safely. These are tiled 2D base levels in the formats above, with a shorter side over 16 texels. Render targets, resolved surfaces, movie frames and the controller-prompt atlas are never replaced. Shaders keep seeing the original texture size, so a larger replacement samples like the original at a higher resolution.

Replacement files are read on a background thread. A texture shows the original until its file has been read, usually a few frames later, and then switches to the replacement. `LO_MODS_TEXTURE_SYNC=1` reads each file during the upload instead, so the replacement shows in the first frame but large files on a slow disk stall the game.

## Text (language packs)

Translations replace the game's text file by file, by **key**, in the JSON files `--export-assets` writes (`--export-kinds text`).

- Identity: `AssetKind::Text` (6). The key is the archive member path as `text/index.csv` lists it: lower case with `/`, for example `bin/xenon/loc/int/menu/menu_int.dat`.
- Overlay path: `overlay/text/<member path>.json`, the export's `text/` folder placed under `overlay/`.
- Mod folder: `<id>/overlay/text/<member path>.json` (format v2), or a resource line `text:<member path>=text/<member path>.json`.
- Modes and precedence follow the image rules; one file per member path wins.
- File: a UTF-8 JSON object of strings, `{"<key>": "<text>"}`, with the export's keys. Keys left out keep the original text; keys the game file does not have are counted in the log. Keep the tokens (`{E001}`, `{E10D:0500}`, …) in place. A translation may be longer than the original. Subtitle, credits and engine (Coalesced) lines may not contain line breaks; menu, name and description strings may not contain `{0000}`.
- Keys name the place that shows a string (a text ID such as `id.9104`, a record field, a message index, a line), so places that share one string in the original can be translated differently.

When the game opens an archive or index of a disc or DLC folder, every translated text file of that folder is rebuilt with its translation and served from memory, and the index points the game at the rebuilt copies. DLC copies of a file (for example `namedata.bin` with DLC installed) take the same translation file. Changes need a restart. The log reports `[mods] text: N translated files in <folder> (F failed; K translation keys this copy does not have)`; a file that fails keeps its original text. Keys a copy does not have are normal when a translation made from the DLC copy of a file also applies to the disc copy.

A **language pack** is a folder in the mods root with a `language.ini` and a `text/` folder in the export's `text/` layout (`<folder>/text/<member path>.json`). `language.ini` holds `id=` (letters, digits, `-`, `_`, `.`; compared in lower case), `name=` (UTF-8, up to 64 bytes, shown in Settings) and `base=` (the three-letter language code it translates: `int`, `jpn`, `deu`, `fra`, `spa`, `ita`, `kor`, `chi` or `sch`); `#` and `;` start comments. `LanguagePacks()` lists the packs, in every mode while mods are enabled; a repeated `id` keeps the first folder by name and logs a diagnostic. Settings lists each pack whose base the edition has after the edition's own languages and stores the choice as `game_language_pack=<id>` beside `game_language=<base ID>` in `settings.ini`. The game runs as the base language. Only the selected pack applies: `ListTexts(pack)` gives its files, and each replaces any other translation of the same member path. The log reports `mods: language pack <id> (<name>, base <code>) in <folder>` at startup.

Text has to use characters the game's font for that language has. The English (`int`) fonts cover ASCII and Latin-1 except `¤¥¦§¬¯±µ¶·¸¼½¾Ð×Þð÷þÿ`, plus `Œœ–—―‘’‚“”„…‹›€™←→∞★☆♪`. The credits font lacks `–`.

## Trusted providers and future consumers

`RegisterProvider(kind, shared_ptr<AssetProvider>)` registers at most one provider per type. It returns false for an invalid kind, null provider or occupied slot. Providers run outside the API mutex and retain shared ownership during callbacks. Exceptions, mismatched identities and non-file results are ignored. Recursive resolution skips providers to avoid recursion. `UnregisterProvider(kind, pointer)` removes only the matching instance; destruction happens outside the mutex. Providers are trusted host extensions and may resolve outside the mods root.

Font/model/movie registration, parsing and path conventions are extension points only. A future consumer must define its payload validator, integrate at an actual load/decode boundary, preserve original fallback, observe generation changes and add acceptance coverage. No TTF, GLB or movie decoder is activated merely by adding a manifest line.

`Root()`, `Mode()`, `Generation()`, `Diagnostics()` and `ListMods()` support host/tool diagnostics. Diagnostics also go to stderr with the `[mods]` prefix; the snapshot retains at most 256 records. This data interface is not an operating-system sandbox against a process racing filesystem changes.
