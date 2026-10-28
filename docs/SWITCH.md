# Nintendo Switch port

[Overview](../README.md) · [Building on PC](BUILDING.md)

An experimental Nintendo Switch (homebrew) build of Lost Odyssey Recomp. It runs
the same recompiled game code as the PC version on the console's Cortex-A57
cores and renders with Vulkan on Mesa's NVK driver for the Tegra X1 GPU, or with
deko3d, the Switch's own graphics API (`LO_SWITCH_BACKEND=deko3d`, what releases
use; see "deko3d back end" below). Players: see [INSTALL-SWITCH.md](INSTALL-SWITCH.md). The
Switch layer follows the Sonic Unleashed Switch ports
([NaGaa95/UnleashedRecomp-NX](https://github.com/NaGaa95/UnleashedRecomp-NX),
[ChanseyIsTheBest/UnleashedRecomp-NX](https://github.com/ChanseyIsTheBest/UnleashedRecomp-NX)).

> **Status: early.** The game runs on a console. The GPU is the limit: with
> deko3d at stock clocks in handheld mode, battles and lighter areas hold
> 30 fps and heavier field areas run at about 20–25 fps. Expect crashes and
> missing features.

The Switch build keeps the Xbox 360 image and its own effects (bloom, depth of
field, motion blur, shadows). The scene defaults to 540p in handheld mode and
720p when docked; `LO_SWITCH_RENDER_HEIGHT` can force either mode. The PC
additions are off and hidden in the Graphics menu: anti-aliasing, ambient
occlusion, anisotropic filtering, render and shadow resolution, upscalers,
frame generation and HDR.

## What you need

- A Switch that runs custom firmware: Atmosphère with the Homebrew Menu
  (hbmenu). An unpatched first-model (V1) console works.
- Your own Lost Odyssey discs, extracted the way the PC version's importer
  writes them (`disc1` … `disc4`, each with `default.xex`, `LO.fpi` and the
  `*.fpd` archives). The supported editions are the PC version's: Asian
  multilingual or USA/Europe, not mixed.
- The Switch shader pack (`portable_vk.lospv`, built on a PC), see
  [Shaders](#shaders).
- To build: a PC with Docker. Windows: Docker Desktop with the WSL 2 backend,
  plus Git.

## SD card layout

```
sdmc:/switch/LostOdysseyRecomp/
    LostOdysseyRecomp.nro
    game/
        disc1/   default.xex, LO.fpi, LO.fpd, xenon_*.fpd
        disc2/
        disc3/
        disc4/
    shaders/
        portable_vk.lospv
    cache/
        shaders/      copied from a PC Vulkan run, see Shaders
```

Settings, saves, caches and logs are created in the same folder. Disc 1 is
required to start; the game switches discs by itself when the others are
present. The game data is about 21 GB: use an exFAT card with enough space.

## Starting the game

Start the Homebrew Menu **through title takeover**: hold **R** while launching
any installed game, then pick Lost Odyssey Recomp. The port reserves a 4 GiB
guest address space and backs it with console memory, which needs the full
application memory pool and the process-memory system calls. Started from
the Album applet it stops with a message that says so.

## Shaders

The console has no shader compiler (DXC). It renders from shaders compiled on
a PC, which it can use as they are: SPIR-V is the same for every Vulkan
platform, and the port reports the PC's pinned compiler (`dxc-1.8`) so PC-made
caches match.

- **Host shaders** (presentation, menus, blits, SMAA, TAA, AO) are built into
  the NRO as SPIR-V: `LostOdysseyRecomp/os/switch/builtin_spirv.inc`, made by
  `tools/switch/gen-builtin-spirv.py` (Linux x64, pinned DXC). Re-run it after
  changing one of those shaders. A host shader the table lacks is written to
  `state/logs/missing-shaders/` on the SD card; copy it unchanged into
  `tools/switch/extra-builtin-shaders/` and re-run the script.
- **Rect lists** are drawn with the precompiled geometry shader rather than
  the PC default (a vertex-shader variant compiled per game shader), which
  would need DXC. `LO_RECT_LIST_GS=0` switches back.
- **Game shaders** come from the shader pack, `shaders/portable_vk.lospv`.
  The Switch reads shader constants from uniform buffers, which the hardware
  keeps in its constant banks, so it needs its own pack: the PC `vulkan` pack
  from the [`shader-packs` release](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/shader-packs)
  does not match and is rejected. Build it on a Windows or Linux PC from the
  same source as the NRO:
  1. Build the PC runtime and the `LoShaderPackTool` target
     ([BUILDING.md](BUILDING.md)).
  2. Run
     ```
     python tools/shader_pack/build_packs.py --renderer switch --game <your disc1 folder>
     ```
     It prepares every game shader with the PC runtime in the background
     (about 6 minutes) and checks the result.
  3. Copy `out/shader-pack-build/switch/portable_vk.lospv` to
     `sdmc:/switch/LostOdysseyRecomp/shaders/portable_vk.lospv`.
- Optional: a PC's Vulkan shader cache (`cache/shaders/`) from a run with
  `LO_VK_UBO_CONSTANTS=1`, copied to `sdmc:/switch/LostOdysseyRecomp/cache/shaders`,
  adds game shaders the PC compiled that the pack lacks. A cache from an
  ordinary PC run does not match.

The pack and any PC cache must come from the same game edition. A shader missing from all of them cannot be
compiled on the console: the renderer logs it (`shader pack: …`, shader
compile failures in the log) and that draw does not render.

## Building

The build runs inside the `ghcr.io/autorunhq/switch-dev` Docker image, which
contains devkitA64, libnx, CMake, Ninja, a host Clang and a static build of
mesa-switch (NVK). Nothing else is installed on the PC.

1. Clone this repository with its submodules.
2. Copy your Disc 1 `default.xex` to `LostOdysseyRecompLib/private/disc1/`.
3. Pull the image once (a few GB):
   ```
   docker pull ghcr.io/autorunhq/switch-dev:2026.10.05
   ```
4. Build:
   - Windows (PowerShell):
     ```
     powershell -ExecutionPolicy Bypass -File tools\switch\build-switch.ps1
     ```
   - Linux/macOS:
     ```
     docker run --rm -v "$PWD:/work" -w /work ghcr.io/autorunhq/switch-dev:2026.10.05 \
         bash tools/switch/build-switch.sh
     ```

The script applies the dependency patches, builds the host tools, dumps the
Disc 1 image, recompiles the PowerPC code when `LostOdysseyRecompLib/ppc/` is
empty, then cross-compiles and packages `out/switch/LostOdysseyRecomp.nro`.
Keep `out/switch/LostOdysseyRecomp.elf` with it: crash addresses resolve
against it.

The recompiled game code is about 256 MB of C++. A full build takes a long
time and roughly 1.5 GB of memory per compile job; if Docker runs out of
memory, lower the job count (`-Jobs 4`, or `JOBS=4`) or give Docker Desktop
more memory (Settings → Resources).

Options (`tools/switch/build-switch.sh` environment):

| Variable | Default | Effect |
|---|---|---|
| `JOBS` | CPU count | Parallel compile jobs |
| `BUILD_TYPE` | `Release` | `RelWithDebInfo` for symbols in the `.elf` |
| `CLEAN` | `0` | `1` deletes `out/build/switch` first |
| `NVK_LIBRARY` | searched | Path of mesa-switch's `libvulkan.a` |

CMake options (`cmake/LoSwitch.cmake`): `LO_SWITCH_RECOMP_O2`,
`LO_SWITCH_LTO`, `LO_SWITCH_DATA_ROOT`, `LO_SWITCH_APP_TITLE`.

### Without a console toolchain

`tools/switch/check-toolchain.cmake` compiles the Switch code paths with a stock
`aarch64-linux-gnu` GCC and the libnx headers (no link), for checking changes on
a machine without devkitPro. See the comment at the top of that file.

## Logs and crashes

In `sdmc:/switch/LostOdysseyRecomp/`:

- `state/logs/runtime-*.log`: the runtime log.
- `stderr.log`: renderer, Mesa and NVK diagnostics (`stderr.previous.log` is
  the run before).
- `crash.log`: written on a CPU exception, with registers and a host
  backtrace. Addresses marked `elf+0x…` resolve with
  `aarch64-none-elf-addr2line -e LostOdysseyRecomp.elf 0x…`.

### Diagnostics switches (env.txt)

The PC build reads its `LO_*` diagnostic switches from the environment. On the
console put them in `sdmc:/switch/LostOdysseyRecomp/env.txt`, one `KEY=VALUE`
per line (`#` starts a comment); `stderr.log` lists the ones applied. Useful
ones: `LO_CPU_PROFILE=1` (below), `LO_FRAME_TIMING=1` (one line per second:
command flush, waits, present), `LO_RENDER_TIMING=1` (per-frame timing with GPU
timestamps), `LO_GPU_DRAW_TIMING_EVERY=300` (GPU time of every group of four
draws, every 300th frame), `LO_KERNEL_TRACE=1` (guest kernel calls such as file
reads; off by default on the console).

Switch-specific options and diagnostics:

| Variable | Default | Effect |
|---|---|---|
| `LO_SWITCH_BACKEND` | NVK | `deko3d` renders with deko3d instead of Mesa NVK (experimental; see "deko3d back end" below) |
| `LO_SWITCH_RENDER_HEIGHT` | automatic | `540` or `720` forces the scene height; otherwise handheld uses 540 and docked uses 720 |
| `LO_DYNAMIC_RESOLUTION` | off | `1`: the scene height follows the GPU's time per frame, in steps of 36 rows from the configured height (540 or 720) down to `LO_DYNAMIC_RESOLUTION_MIN` (default 80% of it: 432 or 576); the final pass scales it to the output as usual. The runtime log's `dynamic resolution:` lines show each change |
| `LO_DYNAMIC_RESOLUTION_MIN` | 80% | The lowest scene height dynamic resolution uses (rows; 360 at the least) |
| `LO_DYNAMIC_RESOLUTION_STEP` | `36` | Rows per dynamic resolution step (a multiple of 9, so 16:9 widths stay whole; `90` gives 540, 450, 360) |
| `LO_TRIM_VARYINGS` | `1` | Keeps only vertex outputs used by the paired fragment shader; set to `0` to disable the Switch Vulkan optimisation |
| `LO_RT32` | off | Set to `1` to use 32-bit formats for the two 10-bit render-target classes; on Switch the 7e3 class uses `R11G11B10_FLOAT`, so its 2-bit alpha is read as 1 |
| `LO_GUEST_AFFINITY` | off | Set to `1` to map Xbox guest thread affinity onto the Switch's three cores |
| `LO_SWITCH_BUTTONS` | letter layout | Set to `position` to use the physical positional A/B/X/Y layout |
| `LO_ZPD_MODE` | host queries | Set to `xenia` for made-up occlusion query counts instead of host queries |
| `LO_SKIP_NOOP_DRAWS` | `1` | Set to `0` to draw the draws that write no colour, depth or stencil |
| `LO_XAUDIO_WORKER_PRIORITY` | `0x2D` | Hex priority for Switch XAudio workers; `0` keeps the default `0x3B` |
| `LO_NATIVE_MOVIES` | on | Cutscene video is decoded on the host with FFmpeg (the game's own decoder freezes cutscenes on the Switch); `0` uses the guest decoder alone. Failures fall back to the guest decoder |
| `LO_MOVIE_GUEST_VIDEO_PRIORITY` | `0x3F` | Hex priority for the guest video thread while native movies are active; `0` keeps the game's priority |
| `LO_IO_PROFILE` | off | Set to `1` to log periodic game-file read statistics |
| `LO_LOAD_PROFILE` | off | Set to `1` to log thread dumps during long scene or battle loads |
| `LO_WAIT_PROFILE` | off | Set to `1` to log guest waits of at least 1 ms every 10 seconds |

The runtime log (`state/logs/runtime-*.log`) is buffered on the console and
written at least once a second; warnings and errors are written at once.

### Live log over the network

The log files cannot be read over FTP while the game has them open. With
`LO_NETLOG=1` in env.txt the console also streams the runtime log and stderr
over TCP (port 28780, or `LO_NETLOG=<port>`), starting with the newest
megabyte. On a PC in the same network:

    python tools/switch/netlog.py <console IP> --out netlog.txt

It reconnects by itself, so it can be started before the game.

### CPU profile

Put `LO_CPU_PROFILE=1` in env.txt (or create an empty file named
`cpu-profile` in `sdmc:/switch/LostOdysseyRecomp/`) and play: every 30 s `stderr.log` gets a `[cpu profile]` block with the
hottest code of each thread (sampled every 2 ms; costs a little speed). Then,
on the PC, with the `.elf` of the same build:

    python tools/switch/switch-cpu-profile.py stderr.log --elf out/switch/LostOdysseyRecomp.elf

Delete the file to turn sampling off. Per-thread CPU shares are logged every
30 s either way (`switch: ... CPU ...` lines).

## How the port works

| Area | Switch implementation |
|---|---|
| Toolchain | devkitA64 GCC, libnx (`cmake/toolchains/switch-devkitA64.cmake`); `LO_PLATFORM_SWITCH` in `os/platform.h` |
| Recompiled code | Same XenonRecomp output; GCC replacements for Clang builtins in `ppc_context.h` (`tools/patches/XenonRecomp-switch.patch`) |
| Guest memory | 4 GiB `virtmem` reservation; pages backed on allocation with `svcMapProcessCodeMemory` + `svcMapProcessMemory`; the physical C and E views map the same pages as A (`kernel/guest_address_space_switch.cpp`) |
| Threads | Guest code threads get 4 MiB stacks (libnx defaults to 128 KiB) |
| Graphics | plume Vulkan on statically linked Mesa NVK, `VK_NN_vi_surface` on the libnx window (`tools/patches/plume-switch.patch`, `volk-switch.patch`) |
| Shaders | Prebuilt SPIR-V pack plus the PC's Vulkan shader cache; DXC is not available (`LO_SWITCH_DXC_IDENTITY`). Guest shaders read their constants from dynamic uniform buffers in descriptor set 5 (`gpu/shader/common_hlsl.h`, `cache::UboConstants()`), which NVK keeps in the Maxwell constant banks |
| Window, input, audio | devkitPro's SDL 3.4 fork (`thirdparty/SDL-switch`), built without EGL (`tools/patches/sdl-switch-no-egl.patch`) |
| XMA audio | Xenia's FFmpeg fork with `thirdparty/ffmpeg-config/switch-aarch64` |
| Graphics settings | `settings::Validate()` selects 540p handheld or 720p docked (override with `LO_SWITCH_RENDER_HEIGHT`), 1x shadow maps and no AA, AO, anisotropic filtering, upscaling, frame generation or HDR; `GraphicsRowHidden()` hides those rows |
| Not on Switch | Updater, network downloads, importer, D3D12, DLSS/FSR/XeSS, frame generation, HDR |

## deko3d back end (experimental)

`LO_SWITCH_BACKEND=deko3d` in `env.txt` renders through deko3d, the Switch's own GPU API, instead of Mesa NVK. It is
built when devkitPro's deko3d library is installed (`dkp-pacman -S deko3d`; the CMake log says
`LostOdysseyRecomp: deko3d back end`). NVK stays the default.

- Back end: `thirdparty/plume-deko3d`, plume's deko3d back end from sa2-nx/banjo-nx, with LostOdysseyRecomp's changes:
  shaders are taken as SPIR-V, descriptors live in two device-wide heaps, and texture handles come from the bound
  descriptor sets.
- Shaders: the Vulkan SPIR-V is translated to the GLSL that uam (deko3d's shader compiler, `thirdparty/uam`) takes,
  per pipeline layout (`plume_deko3d_spirv.cpp`): texture and sampler pairs become combined handles (the sampler of a
  Xenos texture fetch is chosen by `XeShared.xeSamplerIndex`), vertex arena pointers become storage buffer reads,
  specialization constants become macros, and the vertex stage's y is flipped back.
- DKSH cache: `shaders/deko3d_shaders.lodk` holds the game's shaders compiled on the PC; whatever is missing is
  compiled on the console (about 0.1 s each) and kept in `cache/deko3d_runtime.dkcache`. To build the pack from the
  Vulkan pack:

  ```
  python tools/deko3d/extract_spirv.py portable_vk.lospv spv
  ls spv/*.spv | deko3d_shader_tool cache deko3d.dkcache
  python tools/deko3d/recipe_variants.py spv pipelines_corpus.bin pipelines_vk12_1.bin > variants.txt
  deko3d_shader_tool variants variants.dkcache < variants.txt
  python tools/deko3d/pack_dkcache.py deko3d_shaders.lodk deko3d.dkcache variants.dkcache
  ```

  Texture-info specialization runs on deko3d as on NVK: a pipeline gets variants whose texture fetches have their
  format words as constants, so the per-fetch sign and swizzle decoding folds away (about half the code of the
  heaviest pixel shaders). The `variants` step pre-builds those of the recorded recipes, and the vertex shaders of
  every recipe with only the outputs its pixel shader reads (`LO_DEKO3D_VS_OUTPUTS`); any other variant is compiled
  on the console in the background and kept in the runtime cache. `LO_TEXINFO_SPEC=0` turns specialization off.

  `deko3d_shader_tool` is `tools/deko3d/deko3d_shader_tool.cpp` built for the PC with
  `thirdparty/plume-deko3d/plume_deko3d_spirv.cpp`, SPIRV-Cross and uam's `uamlib`. The pack only matches a game
  edition whose Vulkan pack it was made from.
- GPU switches (`env.txt`, all off by default; `LO_DEKO3D_GPU_FAST=1` turns them all on and each `LO_DEKO3D_<NAME>=0/1`
  overrides it): `LO_DEKO3D_DEPTH_COMPRESSION` (compressed depth targets), `LO_DEKO3D_ZCULL` (hierarchical depth
  culling; another port saw depth-biased decals dropped with it), `LO_DEKO3D_MERGE_BARRIERS` (back-to-back barriers
  become one), `LO_DEKO3D_LEAN_BARRIERS` (no texture-cache invalidation for a target that is only written next),
  `LO_DEKO3D_VS_OUTPUTS` (vertex shaders store only the outputs their pipeline's pixel shader reads, and none without
  one: what `LO_TRIM_VARYINGS` does on NVK). `LO_DEKO3D_COLOR_COMPRESSION=0` turns off colour target compression
  (on by default, not part of `LO_DEKO3D_GPU_FAST`); compressed targets get memory blocks of their own, since the
  compression state of reused image memory showed as stripes after dynamic resolution resized the targets. The
  runtime log's `video: deko3d GPU options` line shows what is on. With `LO_RENDER_TIMING=1`, a `deko3d frame work`
  line every 150 frames gives the draws, binds, barriers and copies per frame.
- Not on deko3d yet: host occlusion queries (`LO_ZPD_MODE` uses made-up counts), LSFG frame generation, the driver
  pipeline cache (not needed: no pipeline compiles).

## Known gaps

- Slow: the opening battle remains well below 30 FPS in handheld mode. The
  540p handheld default and Switch Vulkan shader optimisations are intended to
  reduce GPU cost, but no complete hardware playthrough or performance
  acceptance has been recorded yet.
- No on-console importer: discs are copied from a PC.
- Shaders missing from both the pack and the PC cache cannot be compiled on the
  console.
- The other performance work of the Unleashed Switch ports (direct calls
  between recompiled functions, PGO, the patched NVK) is not ported yet.
