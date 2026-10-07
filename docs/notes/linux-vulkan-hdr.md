# Linux/Vulkan HDR research — 2026-10-02

This note began as a Linux/Vulkan HDR implementation study. The dated research checkpoint below is retained; the implementation update records the later source state, which ships in v0.7.35 as part of its experimental HDR output. The Windows Vulkan probe is not Linux evidence.

## Implementation update — 2026-10-02

The source, now part of v0.7.35, requests HDR on Vulkan when the active window surface advertises a usable exact format/color-space pair. It distinguishes FP16 extended-linear output from packed ten-bit HDR10/PQ, converts the game's BT.709 scene to BT.2020 and ST2084 in the final PQ pass, and falls back to SDR for unsupported pairs. It rechecks the surface on resize/display changes and rebuilds format-dependent presentation resources. `VK_EXT_hdr_metadata` availability alone never turns HDR on. The shared Graphics page offers a true output HDR comparison pattern, a logarithmic slider and exact numeric entry. Auto can follow a usable display report, but the current Linux Vulkan path does not obtain one and uses a clearly labelled 1000-nit content reference. Linux WSI HDR transport may be accepted when the physical monitor mode cannot be queried, and must remain labelled as such.

The initial HDR scene path still requires anti-aliasing, upscaling and frame generation Off. On WSLg 1.0.73 with Mesa Dozen 26.2.2, the Plume Vulkan path, surface probe and CPU math compiled; the SDL Wayland probe requested HDR, selected BGRA8/sRGB SDR and passed resize. That WSLg surface exposed no HDR presentation transport in this test. Linux full-runtime build, HDR Wayland/Gamescope hardware and AppImage/Flatpak behavior remain pending in the collected evidence. The maintainer separately confirmed on-device HDR validation on 2026-10-02, but did not specify its platform/backend/display scope. The following research sections describe the earlier gaps and proposed checks; they should not be read as an assertion that those gaps remain in source.

## Research checkpoint before implementation

The shared renderer already has FP16 scene and output math, but the Linux/Vulkan presentation path still follows the SDR contract. `plume_vulkan.cpp` enumerates `VkSurfaceFormatKHR` pairs, but does not yet enable the optional `VK_EXT_swapchain_colorspace` path, expose negotiated encoding/display state, or query HDR metadata capabilities through the shared contract. Swap-chain selection picks a format pair and reuses it on resize; display or compositor changes therefore need a fresh capability query and a safe swap-chain/pipeline lifecycle.

The Vulkan video path creates presentation pipelines separately from resize handling, so changing output format or encoding must drain in-flight work and rebuild or invalidate dependent pipelines. The shared `RenderFormat` mapping also needs explicit RGB10A2 and size mappings before a PQ10 path can be considered complete. The current HDR final pass assumes linear output, and creation of the FP16 intermediate pipeline is tied to an FP16 swapchain. That intermediate must remain FP16 independently of a future ten-bit swapchain.

Normal menus/debug UI remain a CPU SDR image composed by the final presentation copy; game UI/fade is replayed with the original game blending into the ExtendedGamma22 FP16 sidecar before final decode. Menu frames need the negotiated output transfer even when `hdrScene` is false. Any new HDR host overlay should blend in linear FP16, never ordinary alpha over PQ. The existing separated-UI test uses encoded-RGB blending with an RGBA8 intermediate, so it is not HDR compositor evidence. FSR Vulkan upscaling and frame generation currently accept 8-bit RGBA/BGRA paths, so the first Vulkan HDR prototype should keep upscaling, anti-aliasing and frame generation Off. Existing Vulkan readback/capture code owns a copy before WSI; that ownership should be preserved while recording the negotiated encoding and decoding packed PQ10 or extended-linear data for SDR previews.

A read-only `vulkaninfo` capture was made on Windows, not Linux. On that machine, an RTX 5080 driver 616.56 advertised FP16 extended-linear sRGB and A2B10G10R10 HDR10 formats, while the integrated AMD adapter advertised SDR-only surface pairs despite exposing `VK_EXT_hdr_metadata`. This is useful evidence that format pairs are adapter/WSI-specific; it is not evidence for Linux, Wayland, X11 or a physical HDR display.

## Vulkan API semantics to preserve

`VK_EXT_swapchain_colorspace` enables additional color spaces; select an exact pair returned by `vkGetPhysicalDeviceSurfaceFormatsKHR`. A ten-bit format by itself does not establish HDR. `VK_EXT_hdr_metadata` supplies optional metadata and is not an HDR switch or a replacement for selecting the swap-chain color space. Vulkan distinguishes extended-linear sRGB from PQ BT.2020; transfer functions and gamut conversion remain the application's responsibility. Final PQ encoding must occur after existing scene/UI composition and resizing; any new HDR host overlay must blend in linear FP16.

Wayland's color-management protocol lets the compositor map accepted content to the output; HDR content acceptance is not the same as proof that the current panel is in an HDR mode. Its color-management definitions also contain different reference-white conventions for extended-sRGB and ordinary parameters, so Linux FP16 values must be validated per WSI route rather than assigned one universal absolute-nit rule. Mesa's recent release notes document ongoing Wayland color-management and extended-target-volume work; they do not establish a project-wide minimum Mesa version. Gamescope's shader code explicitly treats scRGB as 80 nits per unit, but its X11/XWayland route must not be assumed equivalent to a native Wayland route. Enumerate the actual surface pairs and negotiated encoding in each environment. A PQ BT.2020 output path would convert the existing BT.709 scene at the final output stage; it would not imply native wide-gamut game assets.

The initial implementation should let the driver/WSI own the surface and avoid a bespoke protocol path. For native Wayland, any later pass-through design must account for the compositor protocol rather than treating metadata support as sufficient.

The inspected [Mesa WSI source at commit 7215c4e](https://chromium.googlesource.com/external/gitlab.freedesktop.org/mesa/mesa/+/7215c4ef5fba349dbd5debb6c8fd336f1e3fef48/src/vulkan/wsi/wsi_common_wayland.c) uses parametric extended-linear descriptions rather than the protocol's dedicated Windows-scRGB constructor. This is a reason to validate reference-white behavior, not evidence that all Linux FP16 output is wrong. The [Gamescope WSI layer](https://github.com/ValveSoftware/gamescope/blob/master/layer/VkLayer_FROG_gamescope_wsi.cpp) has separate routing conditions for its HDR pairs; do not force `SDL_VIDEODRIVER=wayland` across all Gamescope configurations.

References: [VK_EXT_swapchain_colorspace](https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_swapchain_colorspace.html), [VK_EXT_hdr_metadata](https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_hdr_metadata.html), [VkColorSpaceKHR](https://docs.vulkan.org/refpages/latest/refpages/source/VkColorSpaceKHR.html), the [Wayland color-management protocol](https://github.com/wayland-mirror/wayland-protocols/blob/main/staging/color-management/color-management-v1.xml), [Wayland color management documentation](https://wayland.freedesktop.org/docs/book/Color.html), [Mesa 25.1 release notes](https://docs.mesa3d.org/relnotes/25.1.0.html), [Mesa 26.0 release notes](https://docs.mesa3d.org/relnotes/26.0.0.html), and [gamescope colorimetry](https://github.com/ValveSoftware/gamescope/blob/master/src/shaders/colorimetry.h).

## Repository map

| Area | Current source boundary |
| --- | --- |
| WSI pair negotiation and resize | [`thirdparty/plume/plume_vulkan.cpp`](../../thirdparty/plume/plume_vulkan.cpp) |
| Shared output mode and encoding types | [`thirdparty/plume/plume_render_interface_types.h`](../../thirdparty/plume/plume_render_interface_types.h) |
| Final transfer/output pipeline | [`LostOdysseyRecomp/gpu/presentation.cpp`](../../LostOdysseyRecomp/gpu/presentation.cpp) |
| Video pipelines and capture/readback | [`LostOdysseyRecomp/gpu/video.cpp`](../../LostOdysseyRecomp/gpu/video.cpp) |
| SDL Vulkan surface integration | [`thirdparty/SDL`](../../thirdparty/SDL) |
| Linux packaging and runtime shader compiler | [`tools/package_appimage.py`](../../tools/package_appimage.py), [`LostOdysseyRecomp/gpu/shader/dxc_compiler.cpp`](../../LostOdysseyRecomp/gpu/shader/dxc_compiler.cpp) |

SDL2 already [creates the Vulkan surface](https://wiki.libsdl.org/SDL2/SDL_Vulkan_CreateSurface) through its backend; an SDL3 migration is not a prerequisite for this work. Bundled SDL is 2.30.12, and the Linux build enables its Plume integration. AppImage excludes host Wayland libraries, and the HDR sidecar uses runtime shader compilation through `libdxcompiler.so`; test both dependencies from the actual package. The release workflow and AppImage/Flatpak packaging need separate Wayland/runtime validation, and no blanket SDL, Mesa, driver or compositor minimum should be promised before those checks.

Update, 2026-10-07: the SDL2 statements in the paragraph above are history. The runtime now uses SDL 3.4.18 (PR [#289](https://github.com/freefrank/LostOdysseyRecomp/pull/289), merged to `main` as `e59bfa24`, not yet in a release; latest release v0.8.53). Plume's SDL Vulkan surface path goes through SDL3 with [`tools/patches/plume-sdl3.patch`](../../tools/patches/plume-sdl3.patch), applied after `plume-lostodyssey.patch` and before `plume-macos.patch` and `plume-android.patch`, and on Linux the window now uses native Wayland where it is available (SDL2 used X11/XWayland) and requests high pixel density. No HDR run and no run of the AppImage or Flatpak packages is recorded for the migration, so the validation this note asks for is unchanged; see the [status record](../STATUS.md).

## Proposed implementation sequence

1. Add diagnostics before enabling output: backend, WSI/compositor, driver, surface formats, exact format/color-space pairs, enabled extensions, requested encoding, negotiated encoding and known/unknown display state. Keep unsafe/unknown encodings and unsupported pairs on SDR. A valid HDR content transport may remain active when physical display state is unknown, but must report that state separately and never claim physical HDR activation.
2. Extend the shared capability contract with encoding, display state and dynamic re-query. Keep FP16 extended-linear and PQ10 distinct; do not infer HDR from bit depth or FP16 alone.
3. Make swap-chain resize, display changes and video/presentation pipeline recreation transactional. Drain or fence in-flight work, rebuild format-dependent pipelines, and preserve the existing SDR fallback on any failure.
4. Prototype FP16 extended-linear output only under an explicitly supported Vulkan WSI route, such as a tested Gamescope HDR route. Keep AA, upscaling and frame generation Off while validating scene values, UI composition, readback and fallback.
5. Add native Wayland PQ10 only after the format, gamut and ST2084 transfer path is independently verified. Preserve the FP16 scene internally, convert to the negotiated output at the final pass, and keep metadata/protocol handling separate from scene math. The priority order is PQ10 for the formal native route, a separately verified FP16 route, then the existing SDR fallback for unsupported pairs.
6. Remove production gates only after the matrix below passes on the target environments. Ordinary X11 with SDR-only surface pairs remains a required negative case; an SDR panel behind an HDR-capable compositor is a separate mapping case.

## Hardware and compositor acceptance matrix

| Environment | Required checks before acceptance |
| --- | --- |
| Linux Wayland + Mesa GPU + HDR monitor | Exact pair enumeration, FP16/PQ choice, metadata/protocol behavior, scene highlights above reference white, UI, SDR preview, resize, minimize/restore and monitor changes. |
| Linux Wayland + NVIDIA GPU + HDR monitor | Same checks with driver-specific pairs and HDR toggle changes; no assumption that the Mesa route or pair list applies. |
| Linux Gamescope HDR output | Verify the gamescope-supported pair and encoding, fullscreen/windowed behavior, compositor HDR toggle, readback and SDR fallback. |
| Ordinary X11 desktop | Confirm safe SDR fallback and no false HDR activation when only SDR pairs are exposed. |
| SDR-only surface pairs | Confirm stable SDR output, no false HDR activation or metadata misuse, and recovery when HDR pairs become available. |
| HDR-capable compositor with SDR or unknown physical output | Confirm valid compositor mapping and distinguish HDR content transport from physical output state. Report known SDR or unknown state accurately rather than claiming HDR display activation. |
| Cross-environment regression | GPU validation, scene values above white, UI lease, screenshot/capture decode, packaged AppImage and Flatpak behavior, mixed DPI, resize, minimize/restore and long-run stability. |

No native Linux Vulkan, Wayland, X11, Gamescope, AppImage or Flatpak HDR result is recorded in this worktree's collected evidence yet. The maintainer separately confirmed on-device HDR validation on 2026-10-02 without a platform/backend/display breakdown; physical-display scope remains separate from build, format-negotiation and offline math evidence.
