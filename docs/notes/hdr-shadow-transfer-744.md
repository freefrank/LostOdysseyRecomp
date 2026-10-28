# HDR extended-scene shadow response (#744)

Based on upstream `f9da0ae9bdd481a5ea4fe6431f3a4724d8261147` (v1.1.0).
Related report: [#744](https://github.com/freefrank/LostOdysseyRecomp/issues/744).

## Proven defect and scope

The presentation shader interprets an identical diffuse display signal
differently depending on whether this frame has an extended HDR companion.
`outputSignal(color, false)` uses sRGB decoding; `outputSignal(color, true)`
previously used `pow(color, 2.2)`. This crushes the sRGB toe only in the second
path. Both occur after the same guest display LUT and user adjustments.

This is a reproducible software defect. It is **not proof of the entire cause
of #744**: that report supplies neither runtime logs nor display/backend/settings
information. In particular, a pure SDR movie frame already takes the sRGB
branch and this change does not brighten it. The report must stay open pending
physical-display confirmation, including its intro-movie symptom.

The SDR surface contract is explicitly sRGB nonlinear on Vulkan. The existing
inactive scRGB fix also relies on the compositor's sRGB response. See
[Khronos VkColorSpaceKHR](https://docs.vulkan.org/refpages/latest/refpages/source/VkColorSpaceKHR.html)
and [Microsoft Advanced Color/scRGB](https://learn.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range).
scRGB 1.0 is nominally 80 nits; PQ carries absolute nits. EDR uses relative
system-white units. Neither a nominal nit conversion nor these tests measures a
display's actual light output.

## Pipeline audit

| Stage | Encoding / behavior | Change |
|---|---|---|
| Game tone map | Exposure, bloom, DoF and fade; gamma-encoded result | None |
| `hdr_scene_shader.h` | Removes the verified SDR saturation and output clamp for FP16 | None |
| Scene AA / TAA / AO | Existing SDR surface and FP16 companion | None |
| Upscaler | Existing SDR SDK inputs, then the existing FP16 highlight gain | None |
| Guest UI / subtitles | Existing scene suffix, before the presentation LUT | None |
| Display LUT (#209/#179) | Once, including the documented sRGB-to-BT.709 ramp | None |
| Range / user curve | Once, after the guest LUT | None |
| Extended output decode | sRGB at/below white; original gamma 2.2 above white | Fixed diffuse transfer |
| Shoulder | Hue-preserving compression of the linear maximum above white | None |
| scRGB / EDR / PQ | Existing white scale, peak ratio, BT.709-to-2020 matrix and ST2084 | None |
| Host menu / opaque overlay | Existing sRGB transfer and white scale | No intended pixel change |
| SDR screenshot preview | Normalize, clip above white, encode sRGB | Correct inverse for diffuse output |

`video.cpp` selects the FP16 companion only when it is available and valid;
otherwise it presents SDR through the HDR output transform. `MakeOutput` already
uses paper-white/80 for scRGB, paper-white in nits for PQ, and 1 for relative
EDR. The peak shoulder does not operate below linear 1. No missing white-scale
factor, duplicate PQ encode or shoulder-induced shadow compression was found
in these audited branches. This does not establish actual display negotiation
or game-frame producer correctness on the reporting machine.

The movie replacement NV12 shader converts limited-range BT.709 YUV to an SDR
RGB signal and invalidates the extended companion. Original guest movies and
real-time cutscenes can have different producers; the fixtures supply synthetic
RGB movie-like colours, not captured game footage or a complete decoder run.
The legacy `DrawSeparatedUi` API has no live game call site in this revision;
its existing SDR alpha/resource-lease fixture is retained. No new claim is made
about that API's unused HDR conversion or translucent PQ blending.

## Why this correction

For a normalized *post-LUT* grey signal, at 203-nit paper-white:

| Signal | Original gamma 2.2 (nits) | sRGB reference (nits) | Original/reference |
|---|---:|---:|---:|
| 1/255 | 0.001031 | 0.061616 | 0.01673 |
| 8/255 | 0.099978 | 0.492928 | 0.20283 |
| 13/255 | 0.290926 | 0.817018 | 0.35608 |
| 32/255 | 2.110754 | 2.932100 | 0.71988 |
| 128/255 | 44.562503 | 43.819682 | 1.01695 |
| 255/255 | 203 | 203 | 1 |

This is a transfer mismatch, not a uniformly low exposure. Increasing global
brightness cannot fix these ratios while preserving black, midtones and white.
Across grey codes 1..32, the old/reference integrated linear luminance is
0.555864 with an identity LUT and 0.274089 with the synthetic quantized guest
LUT. The latter reconstructs the documented #209 ramp; it is not an attached
runtime capture. The old near-black values are compressed, not universally
clamped to zero: this fixture has no extra PQ10 black codes in that range.

Using extended sRGB everywhere would also change highlights: encoded 2 would
decode to 4.953846 rather than 4.594793; encoded 4 to 25.314663 rather than
21.112127. Keep the existing gamma-2.2 excess instead. The selected transfer is
continuous at white, monotonic and anchored at black/white, with no toe lift
or new exposure parameter. It has a small derivative change at white; the
existing two encodings are preserved on their respective sides. The final
highlight mapping and peak calibration pattern are unchanged.

`displayToLinear` selects the base/exponent before its single `pow`, then uses
the linear sRGB toe. The calibration scene uses the same helper. The old SDR
UNORM path still bypasses output decoding. Screenshot previews use sRGB only
after normalization/clipping; they remain SDR previews, not HDR evidence.

## Reproduction

No game files, generated PPC sources, native window, or HDR monitor are needed.
Initialize the pinned dependencies and apply the repository's existing patches
(apply only once on a clean submodule):

```sh
git submodule update --init thirdparty/plume tools/XenosRecomp
git -C thirdparty/plume submodule update --init contrib/volk contrib/Vulkan-Headers contrib/VulkanMemoryAllocator
git -C tools/XenosRecomp submodule update --init thirdparty/fmt thirdparty/xxHash thirdparty/dxc-bin
git -C thirdparty/plume apply ../../tools/patches/plume-lostodyssey.patch
git -C thirdparty/plume apply ../../tools/patches/plume-sdl3.patch
```

Linux needs CMake 3.24+, a C++20 compiler, X11 development headers/libraries,
the Vulkan loader and an ICD. Mesa lavapipe works without a physical GPU. Use
`LO_HDR_GPU_TESTS=OFF` for independent CPU/reference tests only.

```sh
cmake -S tools/tests/hdr_transfer -B out/hdr-transfer -DCMAKE_BUILD_TYPE=Release
cmake --build out/hdr-transfer --parallel 4
export LO_DXC_PATH="$PWD/tools/XenosRecomp/thirdparty/dxc-bin/lib/x64/libdxcompiler.so"
export LD_LIBRARY_PATH="$PWD/tools/XenosRecomp/thirdparty/dxc-bin/lib/x64:${LD_LIBRARY_PATH:-}"
# Optional: select the installed software ICD and validation layer.
export VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.json
export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation
ctest --test-dir out/hdr-transfer --output-on-failure
```

On Windows stage the pinned `dxcompiler.dll`/`dxil.dll` beside the executable
and run CTest with `-C Release`. The standalone fixture defaults to D3D12 there;
`LoPresentationTest --vulkan --hdr-transfer-only` selects Vulkan explicitly.
These Windows commands are provided for reproduction, not claimed as executed.

For unchanged-upstream A/B, create a second build with
`-DLO_HDR_PRESENTATION_SOURCE=/absolute/path/to/upstream/presentation.cpp`.
The same candidate test source then executes the original shader. Set
`LO_HDR_FIXTURE_SDR_DUMP=/absolute/output/prefix` to compare the two raw SDR
readbacks byte for byte. The fixture never reinitializes a live Presentation
when only output parameters change and forgets its short-lived target
framebuffers after GPU completion. The older HDR fixture now does the latter
as well, avoiding CPU-address reuse of a destroyed test target.

Compile all production shader strings and compare fixed-SDR shader binaries:

```sh
python tools/tests/hdr_transfer/compile_shaders.py \
  --dxc tools/XenosRecomp/thirdparty/dxc-bin/bin/x64/dxc-linux \
  --output out/hdr-shaders --baseline /absolute/path/to/upstream/presentation.cpp
```

The script uses the production DXC flags, including HV2021, Vulkan 1.2,
DX layout, and vertex Y inversion. It compiles DXIL as well as SPIR-V;
compiling DXIL on Linux does not execute Direct3D 12.

## Results and limits

Local GCC 13.3, pinned DXC (`dxc-bin` `737ac9f`), Mesa 25.2.8 lavapipe
(LLVM 20.1.2, CPU implementation):

- Independent double CPU reference: **508,314 checks pass**, also with
  ASan/UBSan (LeakSanitizer disabled because of the container).
- Real production presentation shader readbacks: **151 cases, 185,922 RGB
  comparisons pass**. Identical tests against unchanged upstream fail with
  **74,807 channel mismatches**, starting at the extended near-black ramp.
- Cases cover full and dense dark ramps, mixed synthetic movie colours,
  high contrast, values above white, paper-whites 80/203/400, peaks
  400/1000/10000, the quantized guest LUT, range/user curves, inactive scRGB,
  SDR/video on HDR, EDR, packed PQ10, gain reconstruction including highlights,
  Off/FXAA/SMAA transfer isolation, both calibration previews and output
  transform changes. Tolerances: FP16 0.15% or 2e-7 absolute, packed UNORM at
  most 1.1 code values (including implementation rounding).
- Existing HDR output, HDR calibration, separated-UI and SDR presentation
  fixtures pass; **6/6** standalone CTests pass, with no Vulkan validation
  errors. These tests render off screen; output-transform changes do not test
  actual swapchain recreation.
- Two raw RGBA8 SDR outputs are byte-identical before/after (4,096 bytes total).
  Eight fixed-SDR shaders (pixel/compute, DXIL/SPIR-V, with/without the display
  LUT and user curve) are byte-identical. **32 shader compilation checks pass**
  including the A/B variants.
- Review regressions: **49/49 enabled tests pass** with SDL fixtures disabled.
  The Linux-home namespace fixture is excluded after the container rejected
  its namespace setup and the wrapper timed out. The allocator initially hit
  a LeakSanitizer `/proc` failure and passes with only leak detection disabled;
  its ASan/UBSan remain enabled.
- Existing FG/settings contract suite: **36/41 pass**, including all 34 FG,
  upscaler, menu and debug tests. Five unrelated Discord IPC tests cannot create
  their private Unix sockets in this container; the other two Discord tests
  pass. These are not SDK-generated-frame rendering checks.
- The complete production `video.cpp` translation unit compiles with the real
  graphics/SDL interfaces and the existing generated-guest PCH boundary. This
  is not a complete game/runtime link. The existing real-shader Vulkan temporal
  resolve fixture also passes with synthetic inputs; its deliberately
  diagnostic moving-depth case is not asserted as game/TAA acceptance.

No additional fullscreen pass, texture, buffer, descriptor, allocation or
synchronization is introduced in the rendering path. The complete dynamic
SPIR-V pixel shader has 34 image-sample instructions before and after; static
Pow instructions decrease from 21 to 17, while total instruction count grows
from 2946 to 2986 and binary size from 53,080 to 53,868 bytes. These are compiler
counts across all branches, not executed GPU timings or proof of equal speed.

Physical HDR display acceptance, native Windows/D3D12 execution, Android NDK
runtime/APK linkage, Adreno 750 timing/thermal/memory measurements, actual game
3D/CGI/subtitle playback and SDK-generated frames remain unverified. No game
assets or physical GPU/display are present. No claim is made that #744 is
definitively resolved or that FPS improved. On a real system compare the same
intro/battle frame at default range/brightness/gamma, matched paper-white and
peak, HDR off/on, then AA/upscaling/FG and repeated HDR/SDR changes; retain the
runtime log and output-mode information alongside physical-display observations.
