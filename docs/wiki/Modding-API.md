# Modding API v1

The public source interface is `LostOdysseyRecomp/modding/mod_api.h`; image decoding is in `image_mod.h`. API v1 versions the data contract. It is not a stable binary DLL/plugin ABI.

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

`Initialize(defaultRoot)` snapshots standalone manifests; `LO_MODS_DIR` overrides the root. The game's `--mods-mode <mode>` argument sets `LO_MODS_MODE` for launchers that cannot set environment variables, such as Mod Organizer 2. Use an absolute override for reproducibility. `LO_MODS=0` or `LO_MODS=false` disables all resolution. An invalid nonempty `LO_MODS_MODE` disables resolution and emits a diagnostic.

| `LO_MODS_MODE` | Lookup order before the original asset |
| --- | --- |
| `combined` or unset | Merged overlay, trusted provider, standalone manifest winner. |
| `standalone` | Trusted provider, standalone manifest winner; merged overlay ignored. |
| `overlay` | Merged overlay only; standalone manifests and providers are not consulted. |

`overlay` mode prevents a mod disabled in an external manager from reappearing through a second installation or provider. Normal overlay absence returns no replacement. After a file is selected, invalid image contents fall back to the original; the decoder does not search lower-priority mods for a different payload.

`Reload()` rebuilds the snapshot using the original default root and current environment overrides. Initialization, reload, shutdown and provider changes advance `Generation()`. Consumers must invalidate decoded caches when the generation changes. The native menu cache already does this. Reload host configuration at a safe application boundary; there is no automatic file watcher. Do not mutate process environment concurrently with resolution/reload.

## Standalone manifests

Scan direct, non-hidden mod directories under the root. The `overlay` directory is reserved. A manifest is UTF-8 `key=value` text, at most 1 MiB, with optional BOM. Whole-line `#` and `;` comments are accepted. There are no INI sections, quoting or inline comments.

```ini
api_version=1
id=my-menu
priority=100
enabled=true
# image:<canonical-key>=images/replacement.lotex
```

`api_version=1` is mandatory. ID defaults to the directory name and must contain 1-128 ASCII letters, digits, `.`, `_` or `-`, excluding `.` and `..`. Priority defaults to zero and is a signed 32-bit integer. Enabled defaults to true; accepted values are `true`, `false`, `1`, `0`. Unknown/duplicate/invalid metadata rejects the manifest. Disabled mods do not participate. Enabled duplicate IDs reject the later directory.

Resource kinds are `image`, `font`, `model`, `movie`. Invalid resource lines are diagnosed and skipped. Payload paths are relative to the mod directory and may not be absolute or contain `..`. Containment is checked on the path as written, not after symlink resolution, so symlinked mod folders and a manager's virtual file system work. Metadata is parsed before resources, so a trailing priority or enabled field applies to the whole mod.

Higher priority wins. Equal priority uses the lexically later directory in UTF-8 byte order. Within the same manifest, the last declaration of an identity wins. These priorities do not override a merged overlay.

## Manager overlay paths

`OverlayRelativePath({kind, key})` returns a path relative to the mods root. For images:

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
- Overlay path: `overlay/textures/fp-<16 hex digits>.lotex2`. The fingerprint is the file name, so two manager mods that replace the same image conflict on the same path.
- Standalone manifest line: `texture:<16 hex digits>=textures/<name>.lotex2`.
- Modes and precedence follow the image rules.

All integers are little-endian. The fixed header is 64 bytes.

| Byte offset | Size | Value |
| --- | --- | --- |
| 0 | 8 | ASCII `LOTEX2` followed by CR LF |
| 8 | 4 | Header size: 64 + key length |
| 12 | 4 | Payload type: `1` RGBA8, `2` DDS (reserved) |
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

The game replaces only uploads it can match safely. These are tiled 2D base levels in the formats above, with a shorter side over 16 texels. Render targets, resolved surfaces, movie frames and the controller-prompt atlas are never replaced. Shaders keep seeing the original texture size, so a larger replacement samples like the original at a higher resolution.

## Trusted providers and future consumers

`RegisterProvider(kind, shared_ptr<AssetProvider>)` registers at most one provider per type. It returns false for an invalid kind, null provider or occupied slot. Providers run outside the API mutex and retain shared ownership during callbacks. Exceptions, mismatched identities and non-file results are ignored. Recursive resolution skips providers to avoid recursion. `UnregisterProvider(kind, pointer)` removes only the matching instance; destruction happens outside the mutex. Providers are trusted host extensions and may resolve outside the mods root.

Font/model/movie registration, parsing and path conventions are extension points only. A future consumer must define its payload validator, integrate at an actual load/decode boundary, preserve original fallback, observe generation changes and add acceptance coverage. No TTF, GLB or movie decoder is activated merely by adding a manifest line.

`Root()`, `Mode()`, `Generation()` and `Diagnostics()` support host/tool diagnostics. Diagnostics also go to stderr with the `[mods]` prefix; the snapshot retains at most 256 records. This data interface is not an operating-system sandbox against a process racing filesystem changes.
