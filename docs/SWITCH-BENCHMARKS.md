# Switch benchmarks

How to measure the Switch build the same way every time, so a change can be
judged by numbers: the runs, the conditions they need, and the reference
results each optimisation is compared with.

## What a run is

A benchmark run is one game start with a benchmark `env.txt` (`LO_BENCH`). The
game:

1. presses the scenario's buttons (title screen, Continue, the save slot);
2. waits for the scene: the first frame outside a loading screen with at least
   `LO_BENCH_START_DRAWS` draws;
3. holds the scenario's stick from there, settles for `LO_BENCH_SETTLE` frames
   (150), then measures `LO_BENCH_FRAMES` frames;
4. logs one `bench result:` line, takes a screenshot
   (`bench-<name>.ppm` in the app folder) and quits.

Nothing is driven from the PC: the network log only carries the result back.
The scenario files are the PC ones (`tools/scenarios/*.toml`). Console-only
values go in their `[switch]` table (see `tools/switch/bench.py --help`).

The result line has the frame time distribution (mean, p50, p90, p95, p99,
max, standard deviation, 1% low FPS as 1000 / p99, the share of frames over
1.25× the target), hitches, where the frame time went (pacing sleep, present,
render thread idle), GPU busy time, draws and deko3d work per frame (pipeline
and state binds, descriptor writes, barriers, uploads, copies, shader compiles),
every thread's CPU share, the scene size, docked or handheld, the CPU, GPU and
memory clocks, the map, and the build. A `bench env:` line lists every `LO_*`
setting of the run.

## Conditions

Runs are only comparable when these are the same; `bench.py compare` warns when
mode, scene size, target, clocks or any `LO_*` setting differ.

- **The same console mode.** Handheld and docked run at different clocks.
  Handheld runs use the 460 MHz GPU configuration (`[apm]` line in
  `stderr.log`). Do all runs of a comparison in one mode; the reference below
  is handheld.
- **The release `env.txt`** as the base (`bench.py env` starts from
  `tools/switch/release-env.txt`). Dynamic resolution stays off: it changes the
  workload from frame to frame.
- **Warm caches.** The first start after a new build compiles and translates
  shaders and builds pipelines in the background. `bench.py watch` treats the
  first run as a warm-up (`--warmup 1`) and leaves it out.
- **The same settings.ini** (Settings menu): frame rate 30, the same
  antialiasing. The target is in every result (`target_fps`).
- **A committed build.** Results name the build's revision; a `-dirty` one
  cannot be rebuilt later to check a number.
- **The measurement's own cost**, checked once on hardware: the `perf:` line
  keeps GPU timestamps on (two per command buffer submission). One scenario
  with `LO_PERF_GPU=0` in `env.txt` against the same without shows whether
  they cost anything (its GPU figures then read `-`).
- **Three counted runs** per scenario. The report shows their median, minimum
  and maximum. A change smaller than the spread between runs is noise.

## Running

The console needs the network log, so it must be on the same network as the
PC. Copy each scenario's save slot once: `out/drive-city/save/<slot>` from the
PC goes to `sdmc:/switch/LostOdysseyRecomp/bench/save/<slot>`. Runs read their
own save folder (`LO_SAVE_DIR`) and never touch the player's saves.

```bash
python3 tools/switch/bench.py env disc1-ship --out env.txt
```

Copy `env.txt` to `sdmc:/switch/LostOdysseyRecomp/env.txt`, or upload it with
`--ftp <console IP>` when an FTP server runs on the console (ftpd, sys-ftpd;
`--ftp-user`/`--ftp-password` if it asks), which also uploads the save slot.

```bash
python3 tools/switch/bench.py watch <console IP> --runs 3
```

Start the game four times (one warm-up, three counted). Each run quits by
itself after its result. The results, the raw log and a Markdown report go to
`out/bench/<date>/`. Restore the normal `env.txt` when done.

### With the debug server

When the game runs its debug server (`LO_DEBUG_SERVER=1`, see
[SWITCH.md](SWITCH.md), "Debug server"), one command does all of the above:

```bash
python3 tools/switch/bench.py env disc1-ship --debug-server <console IP> --runs 3
```

It keeps a copy of the console's `env.txt`, uploads the benchmark one (with
`LO_DEBUG_SERVER=1`, the console's own `LO_DEBUG_RESTART`/`LO_DEBUG_PORT`, and
without `LO_BENCH_EXIT`: the game stays up for the next restart) and the save
slot (`--saves`), each checked by CRC32. Then, for each run (`--warmup`, default
1, plus `--runs`), it restarts the game, reads the session's log from the server
until `bench done`, and moves on. The raw log (`log.txt`), `results.json` and
`report.md` go to `out/bench/<date>/` (`--results-out`). Last, it puts the
console's `env.txt` back (or removes the benchmark one if there was none) and
restarts the game once more without waiting for it; `--keep-env` leaves the
benchmark `env.txt` in place. A Ctrl-C or a failed run still restores it.

Through title takeover the server refuses `restart` (it would start the game
that was taken over). `bench.py` then asks to close the game and start it again
by hand for each run, and waits for the new session.
The port defaults to 6543 (`--debug-server <IP>:<port>` for another one).

Tested only against the host stand-in, `tools/switch/debug_server_host_test.cpp`
(build line at its top; it listens on 127.0.0.1). With `LO_BENCH` in its
folder's `env.txt` it logs made-up `bench result`/`bench env`/`bench done`
lines once its scene "loads", and `takeover` after its other options makes it
refuse `restart`:

```bash
out/debug_server_host_test out/fake-sd 6543 &
python3 tools/switch/bench.py env disc1-ship --debug-server 127.0.0.1 --runs 3 --frames 120 --saves <a folder with user15/>
```

The same scenario at 60 FPS (`--fps 60`, `LO_FPS=60`) shows headroom that the
30 FPS cap hides: at 30, a faster build only shows as more pacing sleep.
Each scenario is measured at both.

### Scenarios

| Scenario | Scene | Starts at |
|---|---|---|
| `disc1-ship` | Disc 1 ship cells, walking (save slot `user15`) | 700 draws |
| `disc2-forest` | Disc 2 Crimson Forest shrine, walking (`user16`) | 700 draws |
| `numara` | City of Numara from the Disc 4 world map (`user17`) | 3,000 draws (Numara has about 3,300) |

Button stamps come from the PC and may need retuning: console loads take
longer. `watch` prints each scripted press with its frame number and time. If a
run reports `status=timeout` (the scene was never reached), move the stamps
later in the scenario's `[switch]` table. Or use `bench.py env --manual`: load
the save by hand and the run starts by itself once the scene is reached.

### Results without the network

The same lines are in the runtime log
(`state/logs/runtime-*.log`; the last 10 sessions are kept):

```bash
python3 tools/switch/bench.py parse runtime-*.log --json out/bench/manual.json
python3 tools/switch/bench.py report out/bench/manual.json --md
```

### Comparing two builds

```bash
python3 tools/switch/bench.py compare out/bench/<baseline>/results.json out/bench/<candidate>/results.json
```

prints a Markdown table of every metric with its change. Changes over 3%
(`--threshold`) are marked as a win or a loss.

## Reference results

The numbers every optimisation is compared with. Add a section per measured
build; keep the older ones.

### Baseline: `cde98786` (2026-10-29)

Switch OLED, handheld, CPU 1020 / GPU 461 / memory 1331 MHz (the 460 MHz GPU
configuration), release `env.txt` (deko3d, `LO_DEKO3D_GPU_FAST=1`, 960x540,
dynamic resolution off, `LO_PIPELINE_PREBUILD=all`) plus the debug server and
network log, settings frame rate 30. One warm-up run, then 3 counted runs;
median (min to max). Raw results: `out/bench/20261029-175212/` on the machine
that ran them.

`new-game-battle`: the first battle's command menu (Kaim, Wohl Highlands),
waiting for input. Reached from a new game with no save: START, New Game, B
(settings screen), then START and BACK skip the opening movie.

| Metric | Value |
|---|---:|
| FPS | 14.36 (14.25 to 14.36) |
| Frame time p50 / p95 / p99 | 66.5 / 87.4 / 102.7 ms |
| 1% low | 9.7 FPS |
| GPU busy | 52.2 ms per frame (75% of the time) |
| Render thread waiting for the game | 28.6 ms per frame |
| Present | 6.8 ms per frame |
| Draws (submitted) | 1,190 (1,220) per frame |
| Pipeline binds / state binds / descriptor writes | 562 / 372 / 1,380 per frame |
| Full barriers / uploads | 17.1 / 0.25 per frame |
| Threads (% of one core) | Guest Main 93, GPU CmdProc 70, XMA Decoder 7 |

Reading: the GPU alone needs 52 ms of a 33 ms frame, so at 960x540 the scene is
GPU-bound first; at a frame of about 66 ms the GPU still idles a quarter of the
time while the render thread waits 29 ms for the game's main thread (93%), so
the game thread is the next limit. Runs agree within 1%.

**What limits the scene (540 against 360 rows, build `893ac83c`, same scene and draws):**

| Render height | FPS | Frame p50 | GPU busy | Render thread waiting for the game | Guest Main |
|---|---:|---:|---:|---:|---:|
| 540 | 14.39 | 66.5 ms | 52.0 ms (75%) | 28.4 ms | 93% |
| 360 | 14.62 | 66.6 ms | 31.8 ms (46%) | 31.8 ms | 93% |

At 360 rows the GPU fits inside 33 ms and the frame doesn't change, so this scene is
limited by the game's main thread first. At 540 rows the GPU's 52 ms is the next limit:
it caps the scene near 19 FPS however fast the CPU gets. Pinning the three busiest
threads to their own cores gains under 1%.

**Since the baseline:** build `71aa681b` (CR, XER, reservation and non-argument registers
as locals) reads 15.4–15.5 FPS in the same scene, from logs rather than a benchmark run.

**After non-volatile locals (build `e7318b77` plus the helper fix), short readings, not a benchmark run:**

| Render height | FPS | Frame p50 | GPU busy | Render thread waiting for the game | Guest Main |
|---|---:|---:|---:|---:|---:|
| 540 | about 15.6 | about 64 ms | 53.8 ms | about 3 ms | 73% |
| 504 | about 17 | about 59 ms | 48 ms | 3-12 ms | 79-90% |
| 360 | 17-20 | 50-58 ms | 32 ms | 15-19 ms | 92% |

The main thread's limit moved from about 66 ms to about 52 ms per frame. At 540 and 504
rows the GPU sets the frame: frame time is about GPU busy time plus 11 ms, with no sign of
a vsync step, so each millisecond of GPU time saved shows up until the CPU limit.

Most of those 11 ms are the final scaling pass, which the GPU busy figure leaves out. With
`scaling_quality=0` (bilinear, not bicubic) at 540 rows: 16.6-18.1 FPS, frame 55-59 ms, GPU
busy unchanged at about 53 ms, and the main thread back at 84-91%.

**With argument registers as locals and bilinear scaling, short readings:** about 17.8 FPS
at 540 rows (frame 53-57 ms) and about 19.9 FPS at 360 rows (p50 48.7 ms, against about
18.6 FPS and 52.4 ms before; that older run used bicubic scaling).

The scenarios with saves (`disc1-ship`, `disc2-forest`, `numara`) need their
save slots on the console; not measured yet.

### Milestone 1: `e079c587` (2026-10-30)

Recompiler register locals and non-volatile guest memory (#5), bilinear scaling. Same
console, scene and method as the baseline: one warm-up and 3 counted runs per row.
The 540-row row uses the release `env.txt`.

| Render height | FPS | p50 | p99 | GPU busy | Render thread idle | Guest Main | CmdProc |
|---|---:|---:|---:|---:|---:|---:|---:|
| 540 | 17.94 (17.85-18.01) | 55.8 ms | 69.8 ms | 52.8 ms | 4.8 ms | 81% | 69% |
| 360 | 20.51 (20.29-20.61) | 47.2 ms | 75.0 ms | 32.2 ms | 10.3 ms | 91% | 84% |

Against the baseline, 540 rows gained 25% (14.36 to 17.94 FPS) and the CPU limit moved
from about 66 ms to about 47 ms. At 540 rows the GPU sets the frame. At 360 rows the three
cores are nearly full (282% of 300%). Only this scene has been measured.

Tried without gain in this scene: `LO_RT32=1`, `-O2` for the game code, depth of field off,
motion blur off and Dynamic shadows Low. Bloom off saves about 1 ms and Dynamic shadows off
about 5 ms of GPU time.


## Native bone matrices (`LO_NATIVE_BONES`)

Short A/B in the first battle at 360 rows, same build, 6 `perf:` lines per run (not a full benchmark): 21.7 and 21.78 FPS with
`cpu/guest_bone_math.cpp`, 20.58 and 20.7 with `LO_NATIVE_BONES=0`, so about +5% (2.5 ms per frame). Skinning looks correct
in a battle screenshot. On by default; `LO_NATIVE_BONES=0` keeps the recompiled loops.

Adding native quaternion slerp and normalize (same switch): 22.08 and 22.17 FPS, compared with about 21.74 on the previous
build. This compares two builds on short runs, so treat the +2% as approximate.

## Hot-function ordering: no gain

The 2,374 functions sampled on the three busy threads, placed at the start of `.text` with `--section-ordering-file`, same
code, alternated runs at 360 rows: 22.08 / 21.87 / 22.5 FPS ordered, 22.1 / 22.27 not. Not adopted.

## Native animation key decoding

`sub_822B8D68` and its decoders, under `LO_NATIVE_BONES` (`=2` runs native and recompiled code together and logs differences:
0 of 9.2 million live calls in the first battle). Alternated short runs at 360 rows: 22.78 / 22.97 / 22.88 FPS against
22.18 / 22.22 on the previous build, about +3%. Rotation formats 2, 3 and 5 stay on the recompiled code.

## Reading the profile: `svcSignalProcessWideKey` is not call cost

In the first battle at 360 rows it takes 15.9% of `GPU CmdProc`'s samples and 7.9% of `Guest Main`'s, yet a `--wrap` counter on
`pthread_cond_broadcast` and `pthread_cond_signal` found about 4,100 calls per second across all threads, about 0.25% of a core
at 600 ns each. Those samples are mostly threads off-CPU. Don't rank syscall samples as work. `GPU CmdProc` shares its core
with the XMA and audio threads, and all three cores are nearly full (277% of 300%).

Other candidates ruled out by reading the costs: `CapturedShader::Load`'s compare (about 1.2% of `GPU CmdProc`, and it guards
against the guest changing shader code) and float mode switches (2.3 points of the 26.5% of `Guest Main` checked are in
functions that switch modes at all). `GPU CmdProc`'s real time is flat.

## Early depth test: the largest GPU gain so far

Every translated pixel shader contains the alpha test's `discard`, in a branch decided at run time, so the GPU tested depth
after shading on every draw and shaded hidden pixels. Each fragment shader now has a second `DkShader` on the same code with
`early_fragment_tests` set in its program header, and a draw uses it when no discard or depth write is possible: alpha test
off, no guest kill, no depth export. Early and late tests only differ when a pixel is discarded, so the image is the same.

Short runs in the first battle at 540 rows: 21.78 / 21.65 FPS, 18.05 with `LO_DEKO3D_EARLY_Z=0`, about +20%. GPU busy 41.5 ms
against 53 ms. 47 of the first 64 pixel shaders can't discard. Checking 64 shaders took about 25 ms in total.

## Milestone 2 (`9fb1b237`, full benchmark, 1 warm-up + 3 counted runs)

| First battle | Baseline | Milestone 1 | Milestone 2 |
|---|---:|---:|---:|
| FPS at 540 rows | 14.36 | 17.94 | **21.63** |
| Median frame at 540 rows | 66.5 ms | 55.8 ms | 44.4 ms |
| 1% low at 540 rows | | 14.32 | 15.90 |
| GPU busy at 540 rows | 52 ms | 52.8 ms | 41.7 ms |
| FPS at 360 rows | 14.6 | 20.51 | **22.88** |
| 1% low at 360 rows | | 13.34 | 16.76 |

540 rows is now within about 6% of the 360-row CPU limit, so the GPU and the CPU cap the frame about equally. Longest frame at
540 rows: 91 ms against 81.2 ms, from one hitch in the runs, so it needs watching.


## The 30 FPS analysis

Short runs in the first battle. Limits at 540 rows: GPU busy 41 ms, `Guest Main` and `GPU CmdProc` about 38 ms busy each.

- Memory clock: 20% more bandwidth saves 3.6% of GPU time, so the GPU is limited by shader cycles, not bandwidth.
- GPU busy is 27 ms at 360 rows and 41.7 ms at 540: about 26 ms of pixels, the rest mostly vertices. 3.2 million indices per
  frame, about 2.8 million of them through three skinned vertex shaders of about 1,000 instructions each.
- Fourth core: the process may only use cores 0x7, and moving a thread to core 3 fails with 0xe201.
- Zcull: no effect (40.9 ms on, 41.0 off). deko3d drops it when the depth target changes, and the shadow pass splits the main
  pass in two, so the second half (25 of 39 ms) has none. deko3d has no save and restore for it.
- `LO_DEKO3D_LEAN_SHADERS=2` (`plume_deko3d_spirv.cpp`): the hot vertex shaders lose 30-34% of their code; `precise` is the
  largest part, as it stops uam fusing multiplies and adds. GPU busy 41.1 to 37.4 ms; FPS 21.8 to 22.4, now limited by the CPU.
  It needs a pack built with the same setting, `shaders/deko3d_shaders_lean2.lodk`:
  `LO_DEKO3D_LEAN_SHADERS=2 deko3d_shader_tool cache|variants`, then `pack_dkcache.py`. Level 2 changes float rounding in
  position math, so depth-equal passes need watching; no flicker in screenshots of this battle. Off by default.

## Multiple waits: a signal wakes only the threads waiting for that object

`kernel::wait::Multiple` kept every multiple wait on one global condition variable, so each guest signal woke them all: five idle
workers ran at least 400 times a second doing nothing. Each waiter now registers on its own objects. Alternated short runs at 360
rows: 23.6 / 23.72 FPS, 23.17 / 22.93 before, about +2.8%; median frame 42.5 ms against 43.9. No hang files in the runs.
The 15.9% and 7.9% of samples in `svcSignalProcessWideKey` were partly this: the signal call was cheap, the woken threads were not.

## Reader-writer locks: every unlock was a kernel call

newlib's `pthread_rwlock_unlock` always broadcasts a condition variable, a kernel call on libnx even with no waiter, and SDL takes a
read lock in nearly every call that checks an object. Controller polling alone was 11.2% of `Guest Main`, about 5 ms per frame.
`os/switch/rwlock_switch.c` replaces `pthread_rwlock_*` at link time and only calls the kernel for a real waiter. Alternated short
runs at 360 rows: 25.47 / 25.18 FPS against about 23.6, about +7.5%; median frame about 40 ms against 42.2.
A `--wrap` of `pthread_cond_broadcast` misses these calls, as newlib reaches `__libc_cond_broadcast` directly.

## Milestone 3 (`571e52d0`, full benchmark, 1 warm-up + 3 counted runs)

| First battle | Baseline | Milestone 2 | Milestone 3 | With `LO_DEKO3D_LEAN_SHADERS=2` |
|---|---:|---:|---:|---:|
| FPS at 540 rows | 14.36 | 21.63 | **22.30** | **23.51** |
| Median frame at 540 rows | 66.5 ms | 44.4 ms | 44.0 ms | 40.5 ms |
| Frame p99 at 540 rows | | 62.9 ms | 52.8 ms | 52.3 ms |
| 1% low at 540 rows | | 15.90 | 18.95 | 19.13 |
| GPU busy at 540 rows | 52 ms | 41.7 ms | 42.1 ms | 38.2 ms |
| FPS at 360 rows | 14.6 | 22.88 | **25.49** | |
| 1% low at 360 rows | | 16.76 | 18.44 | |

The CPU limit moved from about 23 to about 25.5 FPS, so 540 rows is limited by the GPU again.


## A second scene: the merchant ship

Save at `Barco_mercante_saman` (interior, about 790 draws), 540 rows, handheld, `LO_DEKO3D_LEAN_SHADERS=2`, short reading standing and
walking: 30.0 FPS held, p99 about 34.5 ms, GPU busy 23-25 ms, no hitches. The first battle (about 1,250 draws, 3.2 million indices)
remains the demanding scene.

Handheld memory stays at the system's clock: the game never selects 1600 MHz itself (the short test above removed in favour of
sys-clk for players who want it). Docked already runs at 1600 MHz.


## Controller polling and polling threads

Controller polling was still 4.04% of `Guest Main` after the reader-writer lock fix, as the game polls twice a frame and each reading is
dozens of SDL calls. A reading now stands for 8 ms and prompt tracking runs every 100 ms. XMA sleeps 2 ms instead of mostly 250 us (an
enabled context counted as work), and the audio driver's queue-full wait is 2.5 ms instead of 1 ms. Alternated short runs at 360 rows:
26.05 / 25.92 FPS against 25.47 / 25.33, about +2.6%; median frame 38.2 ms against 39.7. Audio: no drops or errors, `starved` in the same
range.

More vertex shader cuts look small: removing `gl_PointSize` saves about 5% of the hot shaders' code and needs a variant for point draws;
taking the vertex index directly and dropping `floor` on bone indices saves nothing after uam's optimiser.


## Profile-guided build

GCC's profile-guided build, set up as in SwitchWakerHD: game hot files, runtime and deko3d backend ([SWITCH-PGO.md](SWITCH-PGO.md)). First battle at
540 rows, two alternated runs each: 22.11 FPS normal, 23.16 with a profile that never saw the battle (+4.8%), 23.37 with the full profile
(+5.7%), median frame 42.4 to 40.9 ms, p99 74.6 to 68.6 ms. `GPU CmdProc` falls from 75.8 to 64.4% of a core with more frames, 20% less per frame.
At 360 rows: 23.27 to 25.22 FPS (+8.4%, one run each). These scripted runs include the attacks, so they read lower than the command-menu
short runs above. GPU busy stays at 39 ms, which now limits 540 rows to 25.6 FPS at best, so hardware vertex attributes (#20) come next.
