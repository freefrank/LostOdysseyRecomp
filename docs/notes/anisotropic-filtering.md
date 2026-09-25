# Live anisotropic filtering

The `AF` branch adds `Off / 2x / 4x / 8x / 16x` to the graphics menu.
Save graphics settings to apply the request on the next renderer frame; no
restart is required. The persisted key is `anisotropic_filtering`. Missing,
invalid and malformed values default to Off. The selected level is clamped down
to the Vulkan device's supported maximum; unsupported anisotropy stays Off.

## Ownership and publication

`gpu/sampler_palette.h` is the production cache, exercised directly by the
regression fixtures. Palette keys describe the guest sampler plus a host-only
eligibility flag, **not** the current AF level. Changing levels therefore does
not consume additional logical slots. Slot zero remains the original dummy
fallback. A new recipe can recycle a slot not used by the current draw; older
draws keep their own descriptor-table versions. An impossible current-draw
capacity request or allocation failure skips the affected draw rather than
sampling an unrelated slot zero.

Every new recipe and AF change builds a complete candidate sampler table. The
active table is replaced only after all sampler and descriptor allocations
succeed. Failure leaves both active samplers and descriptors unchanged. A failed
AF request is logged and is not retried on every texture, draw or frame; changing
the requested setting permits a new attempt.

The renderer reads AF configuration once per renderer frame outside texture
binding. It pins the final table into each GPU batch that binds it. Original,
alpha, mask and motion-vector replay draws use the same pinned table. The
batch's references are released only after its checked fence wait succeeds;
a version shared by two batches survives completion of only one. Both the
samplers **and** descriptor sets stay alive. No update-after-bind flag or queue
idle is required for the hot switch itself.

Vulkan uses independent set-4 sampler tables. D3D12 versions contain both the
set-0 sampler table and its immutable vertex-buffer bank. Host-generated blits
keep separate immutable default tables. The renderer starts a new normal GPU
batch after eight bound versions, bounding D3D12 sampler-heap consumption; this
limit is checked before borrowing draw resources, never in texture binding.

## Scope of the override

Only explicitly linear min/mag guest-uploaded 2D textures in depth-tested pixel
shader geometry are candidates. The override excludes pre-divided screen-space
XY, texture slots also used by the vertex shader, point/mixed/reserved filtering,
explicit base-mip selection, GPU resolves/depth surfaces, and host bloom/temporal
substitutions. This conservative policy intentionally leaves some materials at
their original filtering. It is not a semantic classification of every game UI
or shader; game captures remain the final compatibility check.

LOD bias, guest mip selection, and shader translation are unchanged. In
particular, the existing guest uploader still uploads one available mip level,
not a complete mip chain. AF does not fix that separate limitation and must not
be described as a complete solution to distant-texture shimmer.

## Regression coverage

Standalone CPU tests build on Windows and Linux without game assets:

```sh
cmake -S tools/tests/anisotropic_filtering -B out/af -DCMAKE_BUILD_TYPE=Debug
cmake --build out/af --config Debug --parallel 2
ctest --test-dir out/af -C Debug --output-on-failure
cmake -S tools/tests/settings_menu -B out/settings-menu -DCMAKE_BUILD_TYPE=Release
cmake --build out/settings-menu --config Release --parallel 2
ctest --test-dir out/settings-menu -C Release --output-on-failure
```

They cover 1,000 AF mode changes, stable palette size, per-sampler and descriptor
failure injection, shared-batch lifetime, slot recycling/capacity, protected
sampling, device clamping, actual extracted renderer AF methods, frame-snapshot
reads, replay wiring, and settings-menu navigation/save without restart. Linux
can add `-DLO_AF_SANITIZE=ON` for AddressSanitizer/UndefinedBehaviorSanitizer.
The extracted-method fixture substitutes device/settings services; it is not a
full translation-unit or game build.

An optional Linux lane (`-DLO_AF_VULKAN_TEST=ON`, Debug build) uses pinned patched
Plume, glslang, Mesa Lavapipe and Vulkan validation. It creates real AF samplers
at every supported requested level, records two command buffers using different
versions of the same logical palette slot, submits both, publishes another
version, waits, and checks the GPU readback of wrap versus clamp sampling. This
checks native descriptor isolation; it does not measure AF image quality or
hardware performance. Use the `AF sampler regression` workflow for dependency
setup and the reproducible software-Vulkan lane.

Before release, test repeated Off -> 16x -> Off saves in actual gameplay on
Windows D3D12, Windows/Linux Vulkan and Steam Deck. Compare oblique ground
textures, menu text, shadows, bloom, FSR/DLSS and motion replay; measure CmdProc
and GPU frame time. These hardware/gameplay checks are separate from the
asset-free fixtures.
