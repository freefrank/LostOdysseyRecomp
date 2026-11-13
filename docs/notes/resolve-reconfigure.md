# GPU resolve contents across settings changes

The Android logs from build `b11c884e9a00-dirty` contain two unsupported
`k_24_8_FLOAT` (23) fetches at `0x9c00000`, both immediately after graphics
settings reconfiguration. The first follows the 1080p-to-720p change at
388.194–388.319 seconds in `runtime-1792834032084317.log`. The second follows
the shadow-resolution change at 594.027–594.053 seconds in
`runtime-1792956256339481.log`. Both scenes produce a new depth resolve only
after the settings menu closes, at 391.129 and 597.328 seconds respectively.

`ResolveDepthOnGpu` already supports this fetch through an R32 float resolve.
`ApplyInternalResolution` discarded the resolve map even though the normal
GPU-only path never writes those destination pixels to guest RAM. A frozen
menu backdrop can still read the old destination before the game resolves
again. Falling back to a raw texture conversion cannot recover these pixels.

Keep resolved images, format variants, fetch views, HDR companions and their
original extents until a later guest resolve replaces them. Existing resolve
writers already recreate allocations when their dimensions change. Continue
draining GPU/presentation work and rebuilding EDRAM targets and framebuffers.
Invalidate old frame-plan and frame-generation lineage, retaining only pixels.
This changes neither shaders nor packed-depth interpretation.

Validation uses the extracted production reconfiguration, lookup and FG
invalidation methods with CPU resource stand-ins. The unmodified baseline
fails because the depth destination disappears; the candidate passes 21 checks
covering shadow/internal-size changes, both depth fetch formats, separate color
format variants, HDR/crop retention, original scaling, replacement, failed
drain and the unchanged-settings path. Linux Release and ASan/UBSan pass;
the fixture and full renderer translation unit compile for Android arm64,
API 26, with NDK r28c. The renderer compile substitutes generated guest ABI
declarations, not graphics interfaces. This is not a complete runtime/APK build.

On-device menu/backdrop comparisons and Vulkan validation on Adreno/Mali remain
required. In particular, verify repeated 720p/1080p and shadow-resolution
changes while paused, then resume and confirm fresh depth/color resolves
replace retained surfaces. Retained destinations can temporarily keep their
old allocations; no memory or FPS improvement is claimed.
