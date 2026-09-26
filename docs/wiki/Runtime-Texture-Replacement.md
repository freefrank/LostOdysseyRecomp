# Runtime texture replacement investigation

## Result

The practical interception point for high-resolution texture mods is the renderer's guest-texture upload path, not the guest filesystem.

Lost Odyssey opens the FPD archives (for example `xenon_loc.fpd`) and reads package byte ranges selected by `LO.fpi`. The guest does not open `rpmenurescommon_int.xxx` as an independent host file. Replacing a Texture2D inside an FPD read would therefore require rebuilding the UE3 package and preserving or rewriting the archive/index lengths.

`Renderer::GetTexture()` already performs the expensive format boundary we need:

1. read the Xenos fetch constant;
2. locate guest physical texture memory;
3. untile Xenos blocks;
4. apply endian conversion;
5. build a linear staging image;
6. allocate the host D3D12/Vulkan texture;
7. upload the staging bytes.

A mod replacement should be selected between steps 5 and 6. This keeps guest package parsing unchanged and permits a replacement to have a larger host extent than the original texture.

## Resource identity versus runtime identity

The extractor manifest's canonical identity remains:

```
<normalized package>#<export_index>:<object>
```

Example:

```
bin/xenon/loc/int/menu/rpmenurescommon_int.xxx#21:Icon_Page_0
```

That is the user/mod-manager identity. It is stable across extraction output directory changes and, for shared content, across discs.

The renderer cannot reconstruct that package/object name from a Xenos texture fetch. Runtime matching therefore needs a second identity: a content fingerprint emitted by the extractor.

Recommended fingerprint input is the exact linear block stream that `Renderer::GetTexture()` produces immediately after untile/endian conversion, plus the source format and logical extent. Do not use the guest physical address: streaming and allocation make it session-dependent.

Recommended v1 record:

```json
{
  "runtime_fingerprint": "fnv1a64:<hex>",
  "runtime_layout": "xenos-linear-blocks-v1"
}
```

The extractor and runtime must share one small fingerprint implementation and test vectors.

## Why not hash the exported PNG

The renderer normally uploads BC1/BC3 blocks directly and lets the GPU sample/decompress them. Hashing decoded PNG pixels would force runtime BC decompression only to identify a texture. Fingerprinting the linear block stream is cheaper and exactly matches data already produced by the upload path.

## Replacement payload

Do not decode PNG/JPEG on the renderer hot path.

A mod manager should compile source artwork at install/import time into a small runtime payload. Suggested `.lotex` v1 header:

```text
magic       "LOTX"
version     u32 = 1
format      u32
width       u32
height      u32
row_pitch   u32
data_size   u64
data        ...
```

v1 should support RGBA8 first. BC1/BC3 can be added when the manager has a deterministic encoder. The runtime validates dimensions, pitch, data size and a conservative maximum allocation before upload.

This separates authoring formats (PNG, DDS, etc.) from the runtime ABI.

## Renderer integration

Pseudo-flow inside `Renderer::GetTexture()` after the original staging buffer is built:

```cpp
const auto fingerprint = modding::FingerprintTexture(
    format, width, height, staging.data(), staging.size());

if (auto replacement = modding::ResolveTexture(fingerprint)) {
    // validated .lotex payload
    uploadWidth = replacement->width;
    uploadHeight = replacement->height;
    uploadFormat = replacement->format;
    uploadBytes = replacement->pixels;
} else {
    uploadWidth = originalWidth;
    uploadHeight = originalHeight;
    uploadFormat = originalHostFormat;
    uploadBytes = staging;
}
```

The cache key must continue to represent the guest fetch. The cached `HostTexture` additionally needs the selected mod generation/fingerprint so enabling, disabling or changing a mod invalidates the host upload.

## High-resolution behavior

Normalized texture coordinates naturally benefit from a larger host texture. However, replacement acceptance must explicitly verify:

- descriptor creation uses replacement physical width/height;
- shaders relying on `GetDimensions` do not regress;
- block-padded dimensions remain correct;
- cube/3D textures are rejected in v1 unless explicitly supported;
- render targets and resolved surfaces are never considered mod candidates;
- packed-mip guest textures identify from the selected source mip consistently;
- texture revalidation does not evict/reupload a replacement every frame.

v1 should initially limit replacement to ordinary 2D guest uploads.

## Confirmed first validation target

Issue #40 identified:

```
bin/xenon/loc/int/menu/rpmenurescommon_int.xxx#21:Icon_Page_0
256x128 BC3
```

as the controller icon atlas. This is an ideal smoke test because a deliberately obvious replacement can be verified visually. The historical HUD A prompt is separately confirmed on screen, but it is not yet proven that the draw binds this atlas. Therefore success criteria should distinguish:

1. runtime fingerprint hit and replacement upload;
2. a draw sampling the replacement;
3. actual HUD prompt coverage.

## Required next implementation pieces

1. Add the shared texture fingerprint helper.
2. Extend the extractor manifest with `runtime_fingerprint` and `runtime_layout`.
3. Add a validated `.lotex` reader to the mod runtime.
4. Resolve fingerprints to canonical mod entries during mod initialization.
5. Insert replacement selection in `Renderer::GetTexture()`.
6. Add a diagnostic-only log for the first replacement hit.
7. Validate `Icon_Page_0`, then a non-UI Texture2D before generalizing the ABI.

The filesystem/archive layer should remain unchanged for texture mods. It may still be useful later for whole-file resources such as movies.
