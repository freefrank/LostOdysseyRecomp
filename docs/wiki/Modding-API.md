# LostOdysseyRecomp Modding API

The runtime now has a versioned virtual asset-overlay contract for mod managers. v1 targets extracted image replacement first and reserves the same API for fonts, models and movies.

## Why the manifest CSV is the source of truth

The extractor manifest already identifies resources with stable structural fields:

`package + export_index + object`

For example, the extracted texture row for `DefaultTexture` contains package `bin\\xenon\\sys\\engineresources.xxx`, export index `8`, object `DefaultTexture`, dimensions and format metadata, plus a convenience `image_path`. The mod API deliberately does not key on `image_path`, because extraction folders may be regenerated or reorganized.

Canonical v1 key:

```
<normalized package>#<export_index>:<object>
```

Example:

```
bin/xenon/sys/engineresources.xxx#8:DefaultTexture
```

For localized assets the package path already includes language, for example:

```
bin/xenon/loc/chi/menu/tutorial_25_chi.xxx#3:TUTORIAL_25_04
```

## Package layout

```text
LostOdysseyRecomp/
  mods/
    hd-ui/
      mod.ini
      images/
        default_texture.png
```

Mods never overwrite imported game files.

## mod.ini

```ini
id=hd-ui
priority=100

image:bin/xenon/sys/engineresources.xxx#8:DefaultTexture=images/default_texture.png
```

Higher `priority` wins when multiple standalone mods replace the same asset. External managers that provide a merged overlay, such as Mod Organizer 2, resolve conflicts before the runtime sees the files; their final virtual filesystem view is authoritative.

Reserved future kinds use the same syntax:

```ini
font:<canonical-key>=fonts/replacement.ttf
model:<canonical-key>=models/replacement.glb
movie:<canonical-key>=movies/replacement.mkv
```

Font/model/movie entries are parsed and reserved by API v1, but their runtime consumers are not wired yet.

## Runtime contract

Before an extracted image is opened, the image loader creates:

```cpp
modding::AssetRequest request{
    .id = {
        modding::AssetKind::Image,
        modding::MakeManifestKey(package, exportIndex, object)
    },
    .originalPath = extractedImagePath
};

auto replacement = modding::Resolve(request);
const auto& path = replacement ? replacement->path : request.originalPath;
```

If no enabled mod provides the key, loading falls through to the original resource.

## Mod-manager contract

Mod Organizer 2 is a first-class target. See [Mod Organizer 2 compatibility](Mod-Organizer-2.md). MO2-compatible packages should converge on `mods/overlay/` so identical resources map to identical virtual paths and MO2 can expose conflicts normally. The built-in per-mod `priority=` mechanism is retained for standalone installs and must not override a manager-resolved overlay.


A manager only needs to install/remove mod directories, enable/disable them, assign deterministic priorities, and detect conflicts by `AssetKind + canonical key`.

The manager should read the extractor `manifest.csv` to present searchable resource metadata such as disc, package, language, object, width, height and format. The numeric CSV `id` is useful for UI/database indexing but should not be the public mod key because a future extractor pass may reorder rows.

Recommended manager workflow:

1. Import `manifest.csv`.
2. Filter `status=exported` and `cls=Texture2D` for current image mods.
3. Build canonical keys from `package + export_index + object`.
4. Let the user preview the corresponding `image_path`.
5. Write replacement entries to `mod.ini`.
6. Warn on duplicate enabled keys and resolve them through priority.

## Current CSV observations

The supplied manifest has 4,052 data rows. It contains exported `Texture2D` image rows, `Font` rows that reference texture-page exports, language-specific packages, and a small set of package parse failures. Failed rows should remain visible to tooling for diagnostics but are not replacement targets until extraction succeeds.

## Extension points

`AssetProvider` allows future runtime backends to own resolution for a resource type without changing manager packages. Planned uses:

- Font: replace font metadata/glyph pages or native font files once the font loader is understood.
- Model: replace decoded model payloads after model extraction/loading is implemented.
- Movie: redirect movie open/decode to replacement media.
- Image: current first implementation target.

API compatibility is versioned by `modding::kModApiVersion`.
