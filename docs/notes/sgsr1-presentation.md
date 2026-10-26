# Optional SGSR1 presentation experiment

`LO_SGSR1=1`, with the High-quality scaling filter, selects Qualcomm SGSR1
for Vulkan SDR upscaling. Unset it or use `LO_SGSR1=0` to restore the existing
filter. This is an experiment, not a new default or a measured speedup.

The unchanged Qualcomm algorithm is embedded in `gpu/shader/sgsr1_source.h`,
from SnapdragonStudios/snapdragon-gsr commit
`d926f074bcb9d714e179f1ce0fcb9ee2eeb5074e`,
`sgsr/v1/include/hlsl/sgsr1_mobile.h`. The BSD-3-Clause notice is retained and
the full license is staged in Android and desktop packages.

The adapter uses the existing green-channel mode (one bilinear RGB sample
and, on edges, four green gathers). Each gather tap outside the valid image
is clamped before loading, so padded allocations cannot leak into the image.
The regular fragment and storage-swapchain compute paths both use the filter;
it allocates no image, descriptor or history and adds no pass. SM 6.0 keeps
`half` arithmetic at 32 bits; this does not require native FP16 support or an
Adreno-only Vulkan extension. Shader compile failure retries the normal shader.

Native-size images, reductions, Standard/bilinear, HDR scenes/output and the
calibration/brightness previews keep their existing filters. AA still runs at
input size. Aspect fitting, output gamma/range conversion and host overlays
retain their ordering. Guest UI already composited into a low-resolution
source is scaled with that source, as with bicubic; this does not create native
resolution UI or separate it from the scene. A temporal upscaler's output at
the destination size is not sharpened again.

## Validation

- `python tools/tests/sgsr1_shader_test.py --dxc <dxc>` compiles all eight
  production SPIR-V variants: vertex, pixel, HDR gain and compute, with the
  option off/on. It needs neither a game nor a Vulkan device.
- `presentation.cpp` and `presentation_test.cpp` passed GCC 13 C++20 syntax
  checks with the pinned Plume overlay and SDL/fmt headers. The runtime PCH was
  replaced by standard includes; this is not a full runtime/NDK build.
- The AppImage, Flatpak and Windows package fixtures passed (11 + 7 + 1 tests).
- The existing GPU readback fixture accepts `LoPresentationTest --vulkan
  --sgsr1`, also with `--hdr-only` or `--hdr-calibration-only`. Native identity,
  padding, AA order, flat regions and letterboxing still need execution on a
  real Vulkan implementation. No GPU/Android result is claimed here.

Before accepting the filter, compare identical fixed scenes and resolution,
with warm shader caches, off/on/off/on on Adreno 650/740/750 and at least one
non-Adreno device. Compare the same input resolution, not native rendering
against a smaller image. Record `LO_FRAME_STATS=60`, `LO_RENDER_TIMING=1`,
`LO_RECORD_TIMING=1`, GPU presentation time and memory, both cold and thermally
sustained; also inspect thin UI, moving edges and cropped input boundaries.
CPU present-call p95/p99 is not displayed-frame pacing: collect Android
FrameTimeline/SurfaceFlinger displayed intervals as well. No battery or FPS
benefit has been measured in this environment.
