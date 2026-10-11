# Asset inventory

This page describes the metadata catalog that mod developers can build from the game data. The inventory is read-only: it does not launch the game or modify imported disc data, and it runs with the native host toolchain without WSL or Docker. Reports contain metadata, hashes and classifications only; they do not redistribute original textures, models, audio, scripts or other payloads.

You do not need the inventory to make a mod. The game can export its own textures, movies and text with `--export-assets` (see [Creating mods](Creating-Mods.md#export-the-original-assets)), and that export already lists each texture's mod key and fingerprint. The inventory is for counting and classifying everything on the four discs, and it can also feed the older native-menu workflow.

## Usage

You need Python 3.10 or newer and a C++20 compiler (`clang-cl` on Windows, `c++` elsewhere). Build the native decoder, then scan a game root containing `disc1` through `disc4`. Keep build and report directories outside the game root; an ignored `out/` directory is recommended:

```powershell
python -B tools/asset_inventory/build.py --output out/asset-inventory-build
python -B tools/asset_inventory/inventory.py scan `
  --game "D:\LostOdyssey\game" `
  --output "out/asset-inventory" `
  --decoder "out/asset-inventory-build/asset_decoder.exe"
```

The default requires all four discs for a complete census. Use `--allow-partial` only for an explicitly bounded partial scan; a partial result is not a complete inventory. The scan writes `catalog.sqlite`, `summary.json`, `REPORT.md`, category CSV files (large categories may use `.csv.gz`) and failure records. Keep the full database and CSV files local; do not commit them.

## Completed snapshot

The following figures are from the complete four-disc scan at source commit `fe89910300b92a59006924987e8586ba157cc20c`. Package parsing was finalized at `2026-10-02T19:58:52.608051Z` UTC. An additional 1,062 non-zero-fourth-byte CPX signatures were checked at `2026-10-02T20:01:33Z`; all were classified as non-UE3 with zero errors.

| Item | Count | Scope |
| --- | ---: | --- |
| FPI files | 53,103 | File occurrences across four discs |
| Directory records | 2,318 | FPI directory records |
| Virtual paths | 16,904 | Distinct paths |
| Complete entry contents | 16,203 | Unique complete-payload SHA-256 hashes |
| UE3 package disc entries | 25,687 | Package file occurrences |
| UE3 package contents | 9,134 | Unique complete package contents |
| Package exports | 1,624,436 | Unique exports by complete package content and export index, including components and default templates |
| Models | 51,862 | `StaticMeshRPG` 50,898; `StaticMesh` 349; `SkeletalMesh` 615 |
| Textures | 29,457 | Texture-class exports; the width, height and format of every `Texture2D`, `LightMapTexture2D` and `ShadowMapTexture2D` were parsed |
| Materials | 31,635 | UE3 material exports |
| Fonts | 73 | UE3 font exports |
| Animations | 12,981 | `AnimSequence` 10,912; `AnimSet` 1,207; `FaceFXAnimSet` 862 |
| Animation graphs | 1,878 | Animation graphs, counted separately |
| Particle systems | 4,161 | `ParticleSystem`; module objects are not counted again |
| UI class objects | 1,213 | UI export classes |
| UI usage candidates | 4,372 | Usage hint that overlaps with textures and other categories; includes 215 default templates |
| DDS | 51 unique / 204 entries | Standalone texture files |
| XWV audio | 958 unique / 1,527 entries | Audio streams |
| XSE audio | 1,520 unique / 6,476 entries | Audio banks |
| Video | 66 unique / 72 entries | WMV files |
| JMD files | 1,734 unique / 8,924 entries | Message and dialogue files, counted by extension |
| Subtitles | 1,118 unique / 4,548 entries | Subtitle files |
| BIN files | 703 unique / 2,860 entries | Event scripts and other binary data, counted by extension |

`StaticMeshRPG` is a game-specific resource class; its export count is confirmed, but mesh geometry decoding is not. Reading the class definition confirms that `SkeletalMesh_VC` has `super_ref` pointing to `fpPawn`, so it belongs to `scene_actors`. There are 1,933 object names that do not satisfy the current mod key grammar; they retain `key_error`, which is separate from package parse failure.

After updating the decoder, revalidate registered UE3 candidates and regenerate a report in a new directory:

```powershell
python -B tools/asset_inventory/inventory.py reparse `
  --db "out/asset-inventory/catalog.sqlite" `
  --decoder "out/asset-inventory-build/asset_decoder.exe"
python -B tools/asset_inventory/inventory.py report `
  --db "out/asset-inventory/catalog.sqlite" `
  --output "out/asset-inventory-report-2"
```

Query model, texture or UI candidates from the SQLite catalog:

```powershell
python -B tools/asset_inventory/inventory.py query --db "out/asset-inventory/catalog.sqlite" --category models --limit 30
python -B tools/asset_inventory/inventory.py query --db "out/asset-inventory/catalog.sqlite" --class-name Texture2D --limit 50
python -B tools/asset_inventory/inventory.py query --db "out/asset-inventory/catalog.sqlite" --search UI_MAIN_00 --ui
```

## Mod candidates

The inventory database can be passed directly to the mod tools. `tools/modding/lo_mod.py catalog` and `init` accept `--database catalog.sqlite`, mutually exclusive with the legacy `--manifest manifest.csv` input and with `--export-index` (the `textures/index.csv` of `--export-assets`). The database must be schema 1 with `complete=true` and is opened read-only. Results are limited to 100 by default and include the canonical key, dimensions, complete-content SHA-256, consumer label, overlay path and source packages. `--runtime-only` includes native menu and font-page candidate labels, but `init` accepts only a confirmed consumer by default. Use `--object`, `--package` and `--content-sha256` to narrow candidates; the last option selects the package-content variant used to derive the authoring identity and dimensions, while the key and the LOTEX1 payload do not bind runtime resolution to that SHA.

```powershell
python tools/modding/lo_mod.py catalog --database catalog.sqlite --runtime-only --limit 100
python tools/modding/lo_mod.py init --database catalog.sqlite --object UI_MAIN_00 --package bin/xenon/loc/int/menu/rpmenurescommon_int.xxx --image art.png --id example --output mod.json
```

The confirmed consumer accepted by `init` is the native settings atlas `UI_MAIN_00` (verified in five languages at 512x1024, with four-disc sources for each). The inventory supplies keys, dimensions and provenance metadata only; it does not export original artwork, so mod authors must provide their own PNG. The `Maru23`, `LocTit1` and `Abc` owners are labeled only as native font-page candidates; native font references are not indexed as complete runtime support, and `init` rejects these candidates by default. The 63 font resources must not be described as already replaceable. Other rows are marked `no_runtime_consumer`; `--allow-unwired` is required for an explicitly experimental specification. `pack` converts the JSON specification into LOTEX1 ZIPs with `standalone` and `overlay` layouts.

The catalog path skips `key_error`, `property_error`, missing dimensions and non-`Texture2D` rows; the legacy CSV manifest path still filters by `status=exported`. This connects the catalog to the existing asset provider, manifest and overlay contracts; it does not register a runtime provider or make the game load SQLite. Scanning covers the complete disc set passed to the tool. `sources` is auxiliary provenance evidence and does not change runtime resolution. The native settings-menu loader tries `gameRoot/disc1` first and falls back to `gameRoot`.

## Explicit FMV export

`inventory.py scan` remains metadata-only. To explicitly export indexed FMV, use `export_fmv.py` with a new output directory; raw ASF/WMV streams are preserved without transcoding:

```powershell
python -B tools/asset_inventory/export_fmv.py `
  --db out/asset-inventory/catalog.sqlite `
  --output out/asset-fmv `
  --decoder out/asset-inventory-build/asset_decoder.exe `
  --ffprobe ffprobe
```

Pass `--game <folder>` when the game root has moved. The exporter verifies copied raw streams with SHA-256, decodes CPX through the native decoder, and de-duplicates decoded output by SHA-256. It writes `movies/`, `manifest.json` and `manifest.csv` with source metadata and occurrence records; the JSON also includes `ffprobe` stream data. Unknown or damaged input fails without repair or fallback. `ffprobe` and hash verification do not establish full-duration decoding or in-game playback. This is an offline file exporter; the `AssetKind::Movie` API has no runtime consumer, so FMV replacement is not connected to the game. (`--export-assets` also copies the game's movies; see [Creating mods](Creating-Mods.md#export-the-original-assets).)

## Categories and counting

Categories are based on UE3 export classes and include models, scene actors, world geometry, textures, fonts, materials, animations, effects, audio, UI objects, sequences, script objects, shader caches, levels/worlds and default templates. Movies, sound streams, subtitles, camera data, event scripts and localization data are recorded separately as file-level kinds.

The report distinguishes disc file occurrences, distinct virtual paths, complete payloads, exports, and path/content/export variants. Imports are dependency references and are not counted as assets. Different complete package contents sharing a mod key are retained as variants; select them using disc, path and SHA-256.

The UI label is an additional usage hint from export classes or path/name heuristics; it does not prove runtime replacement support. What the game can replace today:

- The native settings-menu `UI_MAIN_00` atlas and the texture pages used by native menu fonts, as images by key (LOTEX1).
- The textures the game draws, by fingerprint (LOTEX2). This path works from the `--export-assets` export, not from this catalog.

See [Modding](Modding.md) for the full list. The catalog confirms only `UI_MAIN_00` as an `init` consumer; font pages remain candidates. Models, font metrics, movies and other resources still need a runtime consumer. Names containing characters unsupported by the current API retain `key_error` and an empty `mod_key`; the tool does not guess a key.

An FPD offset identifies the original archive extent and must not be confused with an unpacked UE3 object offset. `Default__*` templates are kept in `default_templates`; `Model` and `Terrain` are `world_geometry`; `StaticMeshRPG` is classified as models, while `SkeletalMesh_VC` is classified as `scene_actors`.

Cite the figures together with the source commit, disc set and completion time of the scan. A finished inventory does not mean a player can replace the resource, that it works on every platform, or that the game has been played through.
