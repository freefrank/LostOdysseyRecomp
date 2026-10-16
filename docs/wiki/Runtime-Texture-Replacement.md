# Runtime texture replacement

Mods can replace the textures the game draws. This page says what is in the game today and what is still open. To make a texture pack, follow [Creating mods](Creating-Mods.md#make-a-texture-pack-experimental); the file format is in the [Modding API](Modding-API.md#runtime-textures-lotex2). The feature is experimental and supported on Windows only.

## What the game does

- **Replacement by fingerprint.** When the renderer uploads a texture, it computes a fingerprint (XXH3-64 of the base level's blocks). If a mod has a `.lotex2` file for that fingerprint, the replacement is uploaded instead. The export (`--export-assets`) writes the fingerprint of every texture to `textures/index.csv`, so no catalog is needed.
- **Which textures.** Tiled 2D base levels in the formats G8, A8R8G8B8, DXT1, DXT3 and DXT5, with a shorter side over 16 texels. Render targets, resolved surfaces, movie frames and the controller-prompt atlas are never replaced.
- **Sizes and formats.** The original size or 2x, 4x or 8x larger, at most 8192 pixels per side, as uncompressed RGBA8 or as block-compressed DDS (BC1, BC3, BC4 or BC7). Shaders keep seeing the original texture size, so a larger replacement samples like the original at a higher resolution.
- **Loading.** Files are read on a background thread. A texture shows the original until its file has been read, usually a few frames later. `LO_MODS_TEXTURE_SYNC=1` reads each file during the upload instead.
- **Fallbacks.** A file that fails validation, or a texture the game uploads in a form the renderer cannot match safely, keeps the original texture. Failures are logged.

In a test over 86 disc-1 field maps and 19 battles, 98.1% of the eligible uploads matched an exported texture. Almost all of the ones that did not match were on two maps of the Experimental Staff Remains.

## Still open

- **Video memory.** There is no video memory budget yet, so a large pack can use a lot of video memory, especially as RGBA8. Prefer DDS payloads, which are about 4 to 8 times smaller.
- **GPUs without BC support.** DDS replacements are skipped there and the originals stay (some Mali GPUs). RGBA8 replacements still load but need much more memory.
- **Shimmer.** Where the game samples only the top level of a texture, a larger replacement can shimmer when it is shown smaller.
- **Other resources.** Models and movies have their own formats, and language packs add letters to fonts (see [Modding](Modding.md)); a texture pack does not change them. Fonts of the native menu are limited to their texture pages.
