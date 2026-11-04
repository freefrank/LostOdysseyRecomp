# Optional SGSR1 presentation filter

Settings → Graphics → SGSR1 → On, Save, and restart selects Qualcomm SGSR1
for Vulkan SDR upscaling. Use the High scaling filter and a render resolution
below the output resolution, for example 720p rendering to a 1080p surface.
Off, Save, and restart restores the existing filter. The persisted key is
`sgsr1=0/1`; the legacy `LO_SGSR1=1` developer override remains supported.
Only requested Vulkan pipelines include the algorithm, retaining ordinary
shader bytecode and startup work with the option off. It remains opt-in and
does not establish a measured speedup.

The unchanged Qualcomm algorithm is embedded in `gpu/shader/sgsr1_source.h`,
from SnapdragonGameStudios/snapdragon-gsr commit
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
- On November 4, 2026, the integrated fork's complete Android ARM64 runtime
  compiled with pinned NDK 28.2.13676358, and its signed APK passed 82 JVM tests
  and release lint (warnings remain, no errors). This is compilation evidence,
  not a physical Android or performance result for this PR branch.
- The integrated fork passes seven production Vulkan readback modes on Mesa lavapipe 25.2.8:
  native filtering, the explicit setting API, storage/compute output, the
  legacy environment override, HDR, calibration and HDR transfer. Checks cover
  native identity, valid-extent padding, AA order, flat regions, reduction and
  letterboxing. The transfer fixture checks 151 cases / 185,922 RGB values.
  The SGSR1 review branch passes the first six; its older base predates the
  extended HDR-transfer CLI mode, which the standalone harness registers only
  when that mode exists. No failure is silently converted into a passing test.
- The standalone Linux fixture needs no game input. Install X11 development
  headers, the Vulkan loader and a software ICD, initialize submodules and
  apply the tracked Plume base and SDL3 overlays, then run:

```sh
cmake -S tools/tests/presentation -B out/verify/presentation -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build out/verify/presentation --parallel 3
ctest --test-dir out/verify/presentation --output-on-failure
```

`LoPresentationTest --vulkan --sgsr1-setting --storage` exercises the new
setting and compute output together. `--sgsr1` retains the developer override.
The fixture replaces only the game-dependent PCH with standard headers and
uses the production presentation, DXC wrapper, backend and readback bodies.
`LO_PRESENTATION_DEPENDENCY_ROOT` may point at another checkout containing the
same prepared pinned dependencies. The dedicated workflow uploads its test log.

Before accepting the filter, compare identical fixed scenes and resolution,
with warm shader caches, off/on/off/on on Adreno 650/740/750 and at least one
non-Adreno device. Compare the same input resolution, not native rendering
against a smaller image. Record `LO_FRAME_STATS=60`, `LO_RENDER_TIMING=1`,
`LO_RECORD_TIMING=1`, GPU presentation time and memory, both cold and thermally
sustained; also inspect thin UI, moving edges and cropped input boundaries.
CPU present-call p95/p99 is not displayed-frame pacing: collect Android
FrameTimeline/SurfaceFlinger displayed intervals as well. No battery or FPS
benefit has been measured in this environment.
