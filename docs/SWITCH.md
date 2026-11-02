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

- `state/logs/runtime-*.log`: the runtime log, one per session. The newest 10
  sessions are kept.
- `stderr.log`: renderer, Mesa and NVK diagnostics of the current (or last)
  session.
- `crash.log`: written on a CPU exception, with registers and a host
  backtrace. Addresses marked `elf+0x…` resolve with
  `aarch64-none-elf-addr2line -e LostOdysseyRecomp.elf 0x…`.
- `hang.log`: every thread's position when frames stopped (CPU profile on, below).

At the next start, the previous session's `stderr`, `crash`, `hang` and `exit`
files move to `state/logs/` as `stderr-<n>.log`, `crash-<n>.log`, … with the
number of that session's `runtime-<n>.log`, and are kept and removed with it.
The files in the app folder are always the last session's.

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
| `LO_NATIVE_MALLOC` | on | `0` keeps the recompiled pool allocator |
| `LO_SWITCH_OUTPUT_HEIGHT` | off | `540` presents 960x540 and leaves the scaling to the display (deko3d) |
| `LO_SKIP_RESTORE` | on | `0` draws the game's EDRAM restores instead of skipping them (deko3d) |
| `LO_RT32` | off | Set to `1` to use 32-bit formats for the two 10-bit render-target classes; on Switch the 7e3 class uses `R11G11B10_FLOAT`, so its 2-bit alpha is read as 1 |
| `LO_GUEST_AFFINITY` | off | Set to `1` to map Xbox guest thread affinity onto the Switch's three cores |
| `LO_THREAD_PLACEMENT` | off | Set to `1` to log every 30 s (`switch: placement`) each core's idle share and, per thread above 1% CPU, its priority, ideal core, allowed mask, and the cores it was seen on |
| `LO_THREAD_CORES` | off | With `LO_THREAD_PLACEMENT=1`: `name=core:hexmask,...` sets the ideal core and allowed mask of a thread of that name once a report finds it above 50% CPU, for placement experiments |
| `LO_DEKO3D_BARRIER_TRACE` | off | Set to `<n>`: every barrier of one frame in n goes to the log (`barrier trace:`), with what it waits for, its textures and the caller's address relative to `Deko3DBarrierTrace` |
| `LO_SWITCH_BUTTONS` | letter layout | Set to `position` to use the physical positional A/B/X/Y layout |
| `LO_ZPD_MODE` | host queries | Set to `xenia` for made-up occlusion query counts instead of host queries |
| `LO_SKIP_NOOP_DRAWS` | `1` | Set to `0` to draw the draws that write no colour, depth or stencil |
| `LO_XAUDIO_WORKER_PRIORITY` | `0x2D` | Hex priority for Switch XAudio workers; `0` keeps the default `0x3B` |
| `LO_NATIVE_MOVIES` | on | Cutscene video is decoded on the host with FFmpeg (the game's own decoder freezes cutscenes on the Switch); `0` uses the guest decoder alone. Failures fall back to the guest decoder |
| `LO_MOVIE_GUEST_VIDEO_PRIORITY` | `0x3F` | Hex priority for the guest video thread while native movies are active; `0` keeps the game's priority |
| `LO_IO_PROFILE` | off | Set to `1` to log periodic game-file read statistics |
| `LO_LOAD_PROFILE` | off | Set to `1` to log thread dumps during long scene or battle loads |
| `LO_WAIT_PROFILE` | off | Set to `1` to log guest waits of at least 1 ms every 10 seconds |
| `LO_PERF_INTERVAL` | `10` | Seconds between `perf:` lines (below); `0` turns off the `perf:` and `hitch:` lines |
| `LO_PERF_GPU` | `1` | `0`: no GPU timestamps for the `perf:` line (its GPU figures then read `-` unless the Detailed overlay or dynamic resolution measures them) |
| `LO_PERF_HITCH_MS` | 2× target and 2× recent median | A `hitch:` line for every frame longer than this (ms), outside loading screens. By default a frame must be over twice the target and twice the median of the last 64 frames, so a scene that is slow throughout gives no lines |
| `LO_PERF_HITCHES` | `300` | `hitch:` lines per session |
| `LO_BENCH` and `LO_BENCH_*` | off | A benchmark run: see [SWITCH-BENCHMARKS.md](SWITCH-BENCHMARKS.md) (`tools/switch/bench.py` writes these) |
| `LO_AUTO_BUTTONS` | off | Scripted presses: `a@900` at a presented frame, `a@30s` at seconds since launch (letters: `s a b x y`, `u d l r` d-pad, `k` back, `L R` shoulders) |
| `LO_DEBUG_SERVER` | off | `1` (or a port) starts the debug server (below) at this start; `0` keeps it off whatever the settings say |
| `LO_DEBUG_RESTART` | automatic | `1`: the debug server's `restart` works here (a HOME-menu forwarder whose ID starts with `01`); `0`: never |

The runtime log (`state/logs/runtime-*.log`) is buffered on the console and
written at least once a second; warnings and errors are written at once.

### Live log over the network

The log files cannot be read over FTP while the game has them open. With
`LO_NETLOG=1` in env.txt the console also streams the runtime log and stderr
over TCP (port 28780, or `LO_NETLOG=<port>`), starting with the newest
megabyte. On a PC in the same network:

    python tools/switch/netlog.py <console IP> --out netlog.txt

It reconnects by itself, so it can be started before the game.

### Debug server

A TCP port (6543) on the console for a PC on the same network: build and frame
info, the log as it is written, screenshots, controller input, the app
folder's files, and deploying a new NRO. It is off unless turned on, and never
on in a fresh install: `LO_DEBUG_SERVER=1` in env.txt, or `debug_server=1` in
`config/settings.ini` (the settings menu's Debug section), read at the next
start. There is no password: use it on your own network only. At start
`stderr.log` (and the network log) says
`[debug] server listening on <ip>:6543, turned on by env.txt`.

The client is `tools/switch/lo_debug.py`. Give it the console's address with
`--host`, `LO_SWITCH_HOST=<ip>`, or the first line of `out/switch-host.txt`.

| Command | What it does |
|---|---|
| `info` | build, session, frame, map, docked/handheld, CPU/GPU/memory clocks, memory and threads, battery, program ID, address, the last `perf:` line |
| `log [--all] [--save F] [--grep RE] [--seconds N]` | the log and stderr as they are written; `--all` starts with what the console keeps (the session's last 1-2 MiB) |
| `logtext [OUT]`, `lastlog [LOCAL] [--current]`, `logs` | the kept text; the previous session's `runtime-*.log` (`--current`: the running one's kept text, since its file is open); the list |
| `shot [OUT.png] [--ppm]` | the next presented frame, through the same capture as `LO_SCREENSHOT_REQUEST`; a PNG made on the console, or the PPM as saved |
| `press A [B ...] [ms]`, `hold`, `release [buttons]`, `stick L\|R x y [ms]` | buttons and sticks for the game, as from the controller (120 ms presses by default; `A+B` together; sticks -1..1, y up; `stick L 0 0` lets go) |
| `input <serial> <mask> <x> <y> <polls> [lt rt [rx ry]]` | one `LO_TEST_INPUT_FILE` line (what `scenario.py` writes) |
| `get`, `put`, `ls`, `rm`, `mkdir` | files; paths are relative to `sdmc:/switch/LostOdysseyRecomp`, or start with `/` for the card's root (read only: writes stay in the app folder; `put` writes `<path>.part`, then renames it) |
| `env [KEY=VALUE ...] [--unset KEY] [--restart]` | show or change env.txt |
| `crashes [--fetch DIR]` | `crash.log`, `hang.log`, the kept `crash-*.log` and Atmosphère's reports |
| `deploy [NRO] [--no-restart]` | uploads `out/switch/LostOdysseyRecomp.nro`, checks its CRC32, restarts the game and waits for the new session |
| `restart [--force]`, `wait`, `ping`, `help`, `raw CMD ...` | |

Button names are the game's (Xbox 360) buttons, as in `tools/scenario.py`:
`A B X Y LB RB LT RT START BACK UP DOWN LEFT RIGHT LS RS`, with the Switch
names `L R ZL ZR PLUS MINUS` as aliases. Input enters where `LO_TEST_INPUT_FILE`
does (`hid/hid.cpp`), so it reaches the game but not the host settings menu.

`restart` (and `deploy`) start the program again with `appletRestartProgram`.
That only brings the game back from a **HOME-menu forwarder**. Through title
takeover the program is the game that was taken over (an ID starting with
`01`), so restarting would start that game: there `restart` refuses and
`deploy` uploads only; start the game again by hand. A forwarder whose ID
starts with `01` needs `LO_DEBUG_RESTART=1` (or `restart --force`).

A typical loop:

```
tools/switch/lo_debug.py env LO_DEKO3D_GPU_FAST=0 --restart     # an A/B switch, then a fresh start
tools/switch/lo_debug.py log --save out/session.log --grep "perf:|hitch:" &
tools/switch/lo_debug.py press START && sleep 20 && tools/switch/lo_debug.py shot
```

#### Scenarios and regression runs on the console

`tools/scenario.py run <scenario.toml> --switch <ip>` and
`tools/regression.py run --switch <ip>` run the PC scenarios on the console.
For each scenario the client writes the console's env.txt (the console's own
lines kept; the scenario's `buttons`, `[env]`, `LO_FRAME_TIMING`,
`LO_RENDER_TIMING`, `LO_DEBUG_LOG` and the debug server added; a `[settings]`
`frame_rate` becomes `LO_FPS`, other `[settings]` are not applied), uploads the
save slot to `scenario/save/` (`LO_SAVE_DIR`: the player's saves are never
touched), restarts the game, streams its log into the run folder and sends the
steps' input and screenshots (`input`, `shot --ppm`) over the network. Then it
puts env.txt back and restarts again. A `[switch]` table's `buttons` replace
the scenario's frame stamps, which may need retuning for the console's loads.
Console runs and their baseline live in `out/regression/switch/`, apart from
the PC's (`regression.py compare --switch`, `accept --switch`). Without a
forwarder the script asks you to start the game by hand each time.

#### What it costs

Off, nothing is set up: the input and screenshot hooks cost one or two relaxed
atomic loads per poll or frame, and the log is not kept unless the network log
is on. On, the socket service's buffers come out of the same heap as
guest memory: libnx sizes them as `sb_efficiency × (TCP send + receive maximum
+ UDP buffers)`, 4 × 308 KiB = 1.2 MiB with the server's 128 KiB windows (the
network log alone keeps its 2 × 184 KiB = 368 KiB; libnx's default would be
2.2 MiB). The two share one `socketInitialize`, with one socket service session
per thread that can wait in it (listener, 4 connections, the network log's 2).
Each open connection has a thread with a 128 KiB stack, freed when it closes;
the log stream reads the network log's kept text (1-2 MiB, kept while either
runs) instead of a copy of its own.
The server's threads run at priority 0x2B like the network log's (above the
game, mostly asleep); `shot` compresses its PNG below the game's threads.

The server core (`os/switch/debug_server_core.*`) and the client come from
[SwitchWakerHD](https://github.com/centollOS/SwitchWakerHD)'s debug server and
keep its MPL-2.0 notice. `tools/switch/debug_server_host_test.cpp` runs the
same core on a computer with stand-in game commands, for testing the client
and the scenario runs without a console (build line at its top).

### Performance log

Every session logs a `perf:` line every 10 seconds and a `hitch:` line for each
long frame, with no `env.txt` needed. They use counters the renderer keeps
anyway and are written by a thread of their own, so they cost the game nothing
measurable.

```
perf: fps=27.4 frames=274 ms_p50=33.4 ms_p95=41.0 ms_p99=58.0 ms_max=66.1 target_fps=30 loading=0 hitches=0
      pace_ms=0.4 present_ms=0.9 cp_idle_ms=1.2 gpu_ms=31.2 gpu_pct=85 draws=2900 gpu_draws=2480
      pipeline_binds=310.0 state_binds=220.0 descriptor_writes=950.0 barriers_full=14.0 barriers_other=41.0
      uploads=0.40 copies=7.0 compiles=0 compile_ms=0.0 translations=0 translate_ms=0.0 scene=960x540
      cpu_total_pct=190 threads=Guest_Main:93.0,GPU_CmdProc:78.0,... mode=handheld cpu_mhz=1020 gpu_mhz=461
      mem_mhz=1331 heap_mib=1210/3290 map=...
```

(one line in the log). Frame times are the time between presented frames;
`ms_*` leave loading-screen frames out (`loading` counts them). Per frame:
`pace_ms` is the frame limiter's sleep, `present_ms` the present,
`cp_idle_ms` the time the render thread had nothing to do because the game had
not sent the next commands yet, `gpu_ms` the GPU's busy time. `threads` are the
busiest threads as a share of one core (100 = never waits). Reading it:

- `pace_ms` well above 0: the frame was done early; the scene holds its target.
- `gpu_pct` near 100 with the render thread below about 90%: GPU-bound. Fewer
  barriers, binds and pixels help.
- `GPU_CmdProc` near 100 with `cp_idle_ms` near 0: the render thread is the
  limit (command translation and recording).
- `cp_idle_ms` large and `Guest_Main` (or another guest thread) near 100: the
  game's own code is the limit.

`hitch:` lines give a long frame's time and what happened during it: GPU time,
`compiles` and `compile_ms` (shaders compiled on the console; about 0.1 s
each), `translations`, `uploads` (texture uploads) and idle and present time:

```
hitch: swap=4123 frame_ms=148.2 target_ms=33.3 gpu_ms=34.0 cp_idle_ms=0.3 pace_ms=0.0 present_ms=1.1 draws=2900
       ... compiles=1 compile_ms=104.2 translations=1 translate_ms=3.0 ... (12 of 300)
```

The Detailed performance overlay (Settings → Graphics → Performance overlay)
shows the same split live: `MAIN` and `RENDER` are the game's main thread and
the render thread as a share of one core, then GPU load and clock, and draws per
frame.

### Debug page

Settings → System → Debug (on the Switch only) holds switches for A/B tests.
Each one is applied and saved at once (in `settings.ini`), and every change is
logged with the presented frame it took effect at, so a log or a benchmark run
shows exactly when each setting was on:

```
option: frame=1 deko3d_merge_barriers=0 source=settings
option: frame=1 deko3d_zcull=1 source=env.txt env=LO_DEKO3D_ZCULL saved=0 (env.txt wins over the Debug page)
option: frame=4810 deko3d_merge_barriers=1 source=menu
option: frame=5233 deko3d_depth_compression=1 source=menu applies=next_start
option: frame=6020 performance_overlay=2 source=menu
```

`frame` is the first presented frame (the `swap` of the `hitch:` lines) that
runs with the value. At start the deko3d back end logs one `source=settings` or
`source=env.txt` line per option, and the overlay mode; `applies=next_start`
marks a change that is saved but not running yet.

| Row | Applies | Log key | env.txt |
|---|---|---|---|
| Performance overlay (Off / FPS / Detailed) | at once | `performance_overlay` | |
| deko3d: merge barriers | from the next frame | `deko3d_merge_barriers` | `LO_DEKO3D_MERGE_BARRIERS` |
| deko3d: lean barriers | from the next frame | `deko3d_lean_barriers` | `LO_DEKO3D_LEAN_BARRIERS` |
| deko3d: Zcull | next start | `deko3d_zcull` | `LO_DEKO3D_ZCULL` |
| deko3d: depth compression | next start | `deko3d_depth_compression` | `LO_DEKO3D_DEPTH_COMPRESSION` |
| deko3d: colour compression | next start | `deko3d_color_compression` | `LO_DEKO3D_COLOR_COMPRESSION` |
| deko3d: vertex output trimming | next start | `deko3d_vs_outputs` | `LO_DEKO3D_VS_OUTPUTS` |
| Debug server | next start | `debug_server` | |

The two barrier switches are read for each barrier, so the render thread
changes them between frames. The other deko3d switches are read when the GPU
queue (Zcull), a render target (compression) or a pipeline (vertex outputs) is
made; changing them mid-game would leave old and new targets and pipelines
mixed, so they wait for the next start. The deko3d rows need the deko3d back
end (`LO_SWITCH_BACKEND=deko3d`); the help line under each one says what runs
now and whether env.txt set it. The debug server (issue #10) is not in the
build yet: its row only saves the choice.

env.txt stays the way to script runs: an option set there (`LO_DEKO3D_<NAME>`,
or `LO_DEKO3D_GPU_FAST` for all but colour compression) wins over the Debug
page at start, and its `option:` line says so. Changing that option on the page
later in the session still applies (live rows) and the line says
`overrides_env=<variable>`; a next-start row says `env_wins=<variable>`, since
env.txt will win again. `tools/switch/bench.py` records the options in effect
when a run started measuring (`options` in its JSON), warns when one changed
while it measured, and `compare` warns when the two runs had different options.

Untested on hardware: the page, the barrier switches changed mid-game and the
log lines were only built, not run on a console.

### CPU profile

Put `LO_CPU_PROFILE=1` in env.txt (or create an empty file named
`cpu-profile` in `sdmc:/switch/LostOdysseyRecomp/`) and play: every 30 s `stderr.log` gets a `[cpu profile]` block with the
hottest code of each thread (sampled every 2 ms; costs a little speed). Then,
on the PC, with the `.elf` of the same build:

    python tools/switch/switch-cpu-profile.py stderr.log --elf out/switch/LostOdysseyRecomp.elf

Delete the file to turn sampling off. Per-thread CPU shares are logged every
30 s either way (`switch: ... CPU ...` lines).

The `[cpu profile]` blocks only list each thread's 150 hottest 16-byte code
lines and no callers; a game thread whose time is spread over many functions
(Guest Main in a battle: about half its samples) is not fully covered. For the
whole picture add `LO_CPU_PROFILE_SAMPLES=1`, or a comma-separated list of
thread names (`LO_CPU_PROFILE_SAMPLES=Guest Main,GPU CmdProc`): every sample
of those threads, with the host LR and, for guest threads, the guest call
chain read from the PowerPC back chain on the guest stack, is appended to
`cpu-samples.bin` in the app folder at each report (`[cpu samples]` lines say
how much; about 4 MiB per 30 s for the three busy threads of a battle, written
in about 0.2 s during which nothing is sampled). Delete the file before a run
(it keeps every session; the script reads the last one), then:

    tools/switch/lo_debug.py get cpu-samples.bin out/cpu-samples.bin
    python tools/switch/switch-cpu-profile.py --samples out/cpu-samples.bin --from 130 --to 300 \
        --folded out/folded.txt --elf out/switch/LostOdysseyRecomp.elf

It prints, per thread, self time per function with each one's callers and,
for guest threads, inclusive time per guest function. `--from`/`--to` are
seconds since the sampler started (about the game's start); `--folded` writes
the stacks for `flamegraph.pl`. The chain's first entry is the guest LR, then
each frame's saved LR. A sample in a function's prologue (between its LR save
and its `stwu`), or one taken while the compiled code holds r1 in a register
rather than in the PPCContext, can miss the immediate caller; that is a few
instructions per call, so it shifts little time.

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
  `LO_DEKO3D_EARLY_Z=0` keeps depth tests after the pixel shader on every draw (by default they run before it on draws that
  can't discard or write depth, with the same image and far less shading);
  `LO_DEKO3D_LEAN_SHADERS=1|2` (shader translation without vertex fetch range checks and debug branches; 2 also without
  `precise` and constant index clamps; needs `shaders/deko3d_shaders_lean<n>.lodk`, see SWITCH-BENCHMARKS.md);
  `LO_DEKO3D_VS_OUTPUTS` (vertex shaders store only the outputs their pipeline's pixel shader reads, and none without
  one: what `LO_TRIM_VARYINGS` does on NVK). `LO_DEKO3D_COLOR_COMPRESSION=0` turns off colour target compression
  (on by default, not part of `LO_DEKO3D_GPU_FAST`); compressed targets get memory blocks of their own, since the
  compression state of reused image memory showed as stripes after dynamic resolution resized the targets. Without
  these lines the values saved on the Debug page (Settings → System → Debug, "Debug page" above) apply. The
  runtime log's `video: deko3d GPU options` line and its `option:` lines show what is on. With `LO_RENDER_TIMING=1`, a `deko3d frame work`
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
