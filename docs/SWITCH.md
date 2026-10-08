# Nintendo Switch port

[Overview](../README.md) · [Building on PC](BUILDING.md)

An experimental Nintendo Switch (homebrew) build of Lost Odyssey Recomp. It runs
the same recompiled game code as the PC version on the console's Cortex-A57
cores and renders with Vulkan on Mesa's NVK driver for the Tegra X1 GPU. The
Switch layer follows the Sonic Unleashed Switch ports
([NaGaa95/UnleashedRecomp-NX](https://github.com/NaGaa95/UnleashedRecomp-NX),
[ChanseyIsTheBest/UnleashedRecomp-NX](https://github.com/ChanseyIsTheBest/UnleashedRecomp-NX)).

> **Status: bring-up.** The port compiles as Switch code; it has not yet been
> run on a console. Expect crashes and missing features until it has been
> tested. Performance on the Tegra X1 is unknown: Lost Odyssey is a heavy
> Unreal Engine 3 game, so a lower internal resolution will likely be needed.

## What you need

- A Switch that runs custom firmware: Atmosphère with the Homebrew Menu
  (hbmenu). An unpatched first-model (V1) console works.
- Your own Lost Odyssey discs, extracted the way the PC version's importer
  writes them (`disc1` … `disc4`, each with `default.xex`, `LO.fpi` and the
  `*.fpd` archives). The supported editions are the PC version's: Asian
  multilingual or USA/Europe, not mixed.
- The Vulkan shader pack (`portable_vk.lospv`), see [Shaders](#shaders).
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
caches match. Two things go on the SD card:

1. **The shader pack** (game shaders), `shaders/portable_vk.lospv`:
   - from a PC installation that has run with Vulkan: its
     `shaders/portable_vk.lospv`, or
   - the `vulkan` pack listed in `index.json` of the
     [`shader-packs` release](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/shader-packs),
     renamed to `portable_vk.lospv`.
2. **The PC's Vulkan shader cache**, `cache/shaders/`: the runtime's own host
   shaders (presentation, menus, post-processing) are compiled at run time and
   are not in the pack. Run the PC version **with the Vulkan renderer** (graphics
   settings, or `LO_GRAPHICS_API=vulkan`), play past the title screen and into a scene, quit,
   then copy its `cache/shaders` folder (next to `LostOdysseyRecomp.exe`) to
   `sdmc:/switch/LostOdysseyRecomp/cache/shaders`. Game shaders the PC compiled
   that the pack lacks are in there too.

Both must come from the same game edition. A shader missing from both cannot be
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
   docker pull ghcr.io/autorunhq/switch-dev:2026.09.28
   ```
4. Build:
   - Windows (PowerShell):
     ```
     powershell -ExecutionPolicy Bypass -File tools\switch\build-switch.ps1
     ```
   - Linux/macOS:
     ```
     docker run --rm -v "$PWD:/work" -w /work ghcr.io/autorunhq/switch-dev:2026.09.28 \
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

## How the port works

| Area | Switch implementation |
|---|---|
| Toolchain | devkitA64 GCC, libnx (`cmake/toolchains/switch-devkitA64.cmake`); `LO_PLATFORM_SWITCH` in `os/platform.h` |
| Recompiled code | Same XenonRecomp output; GCC replacements for Clang builtins in `ppc_context.h` (`tools/patches/XenonRecomp-switch.patch`) |
| Guest memory | 4 GiB `virtmem` reservation; pages backed on allocation with `svcMapProcessCodeMemory` + `svcMapProcessMemory`; the physical C and E views map the same pages as A (`kernel/guest_address_space_switch.cpp`) |
| Threads | Guest code threads get 4 MiB stacks (libnx defaults to 128 KiB) |
| Graphics | plume Vulkan on statically linked Mesa NVK, `VK_NN_vi_surface` on the libnx window (`tools/patches/plume-switch.patch`, `volk-switch.patch`) |
| Shaders | Prebuilt SPIR-V pack plus the PC's Vulkan shader cache; DXC is not available (`LO_SWITCH_DXC_IDENTITY`) |
| Window, input, audio | devkitPro's SDL 3.4 fork (`thirdparty/SDL-switch`), built without EGL (`tools/patches/sdl-switch-no-egl.patch`) |
| XMA audio | Xenia's FFmpeg fork with `thirdparty/ffmpeg-config/switch-aarch64` |
| Not on Switch | Updater, network downloads, importer, D3D12, DLSS/FSR/XeSS, frame generation, HDR |

## Known gaps

- Not yet run on hardware; the first runs will find problems the compile
  checks cannot.
- No on-console importer: discs are copied from a PC.
- Shaders missing from both the pack and the PC cache cannot be compiled on the
  console.
- The performance work of the Unleashed Switch ports (shader constants in
  uniform buffers, direct calls between recompiled functions, PGO, the
  patched NVK) is not ported yet.
