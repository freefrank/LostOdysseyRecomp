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


## Where the GPU time goes

The first battle at 540 rows, timed draw by draw (`LO_GPU_DRAW_TIMING_EVERY=300`, `LO_GPU_DRAW_TIMING_GROUP=1`; the group size is new) with the GPU's
fragment and vertex counters added for the test. 38 ms of GPU time, approximately:

| Part | ms |
|---|---|
| Vertices, 1.1M at about 7.3 ns each | 8 |
| Pixels of the scene's geometry | 6 |
| Draw overhead | 2.4 |
| Full-screen and small passes | 15 |
| Resolves (4.3), clears and barriers | 6.6 |

Only about 6 ms is scene pixels, so changes aimed at them did little. The time is spread over many shaders; no single one is large. The fragment
counter lags the timer by a draw, so per-draw pixel counts are approximate. Scene shaders alone shade about 1.5M pixels.

Measured against 38.2-38.8 ms, alternating runs on the first battle:

| Change | GPU ms | Result |
|---|---|---|
| Hardware vertex attributes (all 285 vertex shaders, no storage loads left) | 40.0 | Rejected. Correct image, but uam adds moves and attribute loads: 484 to 493 instructions in the hot shader |
| `LO_RT32=1` | 39.0 | Rejected. Resolves turn into blits into FP16 |
| `LO_RT32=1`, with those resolves kept as copies | 38.3 | No gain. The format isn't the limit |
| Early tests on discarding draws that write neither depth nor stencil | 38.2 | No gain |
| Skipping the game's EDRAM restores | 36.2 | Kept |

**Restores.** After a resolve the Xbox 360 has to draw the image back into EDRAM, and two full-screen draws per battle frame do that. Here the target
still holds the image, unless the clear queued by the same resolve runs, so the clear and the draw both go. A target stays in `COPY_SOURCE` after a
resolve until something writes it, which proves the two images match. The skipped draw must still make its format the owner of the tiles: without
that, the next draw converts another format's pixels over the image, and the frame blows out to white. `LO_SKIP_RESTORE=0` turns it off. Checked on the
merchant ship: on against off differs as much as two runs of one mode. The estimate of 36-43% from hardware attributes in earlier notes was wrong: it
replaced fetched values with a constant, which let the compiler fold real work away.

With the profile retrained on these sources, first battle at 540 rows, two alternated runs of 90 s: 20.98 FPS normal, 22.47 with the profile (+7.1%);
median frame 41.5 to 39.6 ms, p99 73.6 to 70.4 ms, `GPU CmdProc` 72.2 to 62.8%, GPU busy 36.4 ms in both. These 90 s windows read lower than the 120 s
ones above, so compare only within a table. The median frame is now close to GPU busy, and `Guest Main` runs at 84%: both limit the frame.


## Register write-backs

With `argument_as_local`, a recompiled function writes every argument register it changed back to `PPCContext` before each call and
return, and reads them back after each call. In the 50 hottest functions of `Guest Main`, about 14% of instructions touch the context, about 12
per call. `tools/switch/ppc_elide.py` runs on the generated code (`build-switch.sh` calls it) and removes 1.37 million of 2.68 million stores, 51%:

- A register unchanged since it last matched the context needs no store. This assumes nothing about the guest's code.
- A changed register needs no store before a direct call if the callee doesn't read it and the caller doesn't read it after the call, or before a
  return if it isn't a return register and no caller reads it. Liveness is whole-program, over 62,620 functions.
- Indirect calls keep every changed register, and the 143 functions replaced by host code count as reading everything.

First battle at 540 rows, against the original code, alternated: 20.83 to 21.39 FPS (+2.7%), median frame 42.4 to 40.3 ms. During attacks `Guest Main`
goes from 87% to 85.7% of a core with more frames, and the game's render thread from 75% to 71%. The NRO shrinks from 98.0 to 94.7 MB. Calling
`__imp__sub_X` directly, past the weak alias, so GCC can inline guest functions (`--direct-calls`), gains nothing without a profile.

Two limits: an indirect callee is taken to keep to the ABI, and a value returned in a non-return register and passed straight to an indirect
call isn't tracked.

## More GPU and CPU trials

First battle at 540 rows, alternated runs, against about 36.3 ms of GPU time:

| Change | Result |
|---|---|
| Lean shaders level 3: no lower clamp for signed targets, which the game didn't draw to in these runs | GPU -0.55 ms, kept. Needs the level 3 pack; a draw to such a target logs a warning |
| `LO_SWITCH_OUTPUT_HEIGHT=540`: present 960x540 and let the display scale it | GPU -0.75 ms, kept as an option: the picture hasn't been compared yet, and the host menus would be 540 too |
| Native pool allocator fast paths | At most 1%, inside the noise. Removed: 2 unexplained stops in about 55 runs with it, none in about 50 before it |
| Maxwell's tiled cache, which deko3d never enables | No gain, removed |
| All shaders capped at 32 registers | 36.3 to 38 ms, removed. The hot vertex shader needs spills: 484 to 595 instructions |
| Real clears for D3D `Clear()` draws | No gain, removed. They're thin edge strips, not whole targets |
| Render target lookup memo | No gain, removed. `GPU CmdProc` no longer limits the frame |
| `-fno-rounding-math` | No gain, removed |
| Worker #10 sleeping 2 ms, not 1 | Median frame worse, more audio `starved`, removed |
| Copying only the constants each shader reads | No gain, removed. Street, 4 runs: 26.21 FPS and 27.5 ms of `GPU CmdProc` with it, 26.45 and 27.6 without, with 68% of banks sparse |
| Maxwell's alpha test, not one in every shader | GPU 35.30 to 35.12 ms, near noise, removed |
| Dual issue | GPU 35.12 to 34.65 ms, but 2 crashes in about 25 minutes (`dkQueueAcquireImage`, 2359-0001), none on level 3 in hours. Removed |
| Branchless gamma decode | Same size or larger, and every path runs on every pixel, removed |

The battle has two limits. In the command menu the GPU sets the frame (38 ms). During attacks `GPU CmdProc` idles 6-17 ms a frame waiting for the game,
so `Guest Main` sets it, and its profile is flat: about 18% UnrealScript, 27% skeletal update, 12% scene update, 9% particles. Long frames around 1.4 s at the first
attack, and 0.6 s at the end of the battle, are one-off stalls. Note that `lo_debug.py shot` writes a PPM to the SD card and stalls the game about 4.5 s.

All three cores are 92-96% busy during attacks. The two threads at priority 0x2D, `82cc4d98` and `82cc4e60`, are the XAudio workers: about 12% of a core each.
They meet at a spin barrier (`sub_82CC3C50`). Confining them to the render threads' cores while `Guest Main` had core 0 alone took them to 74% each and the game
to 16 FPS.

## Milestone 4 (`8549a9f5` with the changes below, full benchmark, 1 warm-up + 3 counted runs)

The same scene as Milestone 3, with the profile-guided build retrained on this code and `LO_DEKO3D_LEAN_SHADERS=3`. Stock handheld clocks, CPU 1020 / GPU 460 / memory 1331 MHz.

| First battle | Baseline | Milestone 3, lean 2 | Milestone 4 |
|---|---:|---:|---:|
| FPS at 540 rows | 14.36 | 23.51 | **26.46** |
| Median frame at 540 rows | 66.5 ms | 40.5 ms | 36.9 ms |
| Frame p99 at 540 rows | | 52.3 ms | 48.9 ms |
| 1% low at 540 rows | | 19.13 | 20.46 |
| GPU busy at 540 rows | 52 ms | 38.2 ms | 36.95 ms |
| FPS at 360 rows | 14.6 | 25.49 (lean off) | **29.27** |
| Median frame at 360 rows | | | 33.4 ms |
| 1% low at 360 rows | | 18.44 (lean off) | 19.20 |
| GPU busy at 360 rows | | | 23.9 ms |

- **540 rows is limited by the GPU:** 36.95 ms allows 27 FPS at best, and it's busy 98% of the frame. Reaching 30 needs 3.6 ms more off the GPU.
- **360 rows is limited by the CPU:** the GPU needs only 24 ms, and the median frame is 33.4 ms, just over the 33.3 ms of 30 FPS.
- **This route is lighter than a battle with attacks.** The scripted attack runs below gave 23.6 FPS at 540 rows on the same build, so they remain the hard case.

Profile-guided against normal build on the final code, scripted attacks, 540 rows, two runs each:

| Build | FPS | p50 ms | p99 ms | GPU ms | Main % | CmdProc % |
|---|---|---|---|---|---|---|
| Normal | 22.21 | 40.2 | 71.4 | 35.1 | 82.1 | 74.4 |
| Profile from ship and battle, retrained | 23.63 | 37.7 | 66.7 | 35.1 | 82.4 | 63.3 |

**Guest Main on its own core** (`LO_MAIN_CORE=0`) gave +2% in the attack runs, 21.45 to 21.87 FPS, p99 about 78 to 70 ms, but it is **off by default**. In the intro video one frame took 114.6 s: the XAudio workers ran at 100% each, `Guest Main` got no CPU, and `GPU CmdProc` waited. A pinned thread can't leave a core where a higher-priority thread spins. The `starved` count that rose from about 270 to 550-870 was probably the first sign. The Milestone 4 numbers above were taken with it on, so a default build may read about 2% lower.

**The cause of those stalls was the guest spinlock.** The 360 can't preempt a spinlock's holder, but Horizon can, and a yield only lets threads of the same or higher priority run. The stall-watch dump showed both XAudio workers in `KfAcquireSpinLock` on one lock for over 5 s, with `Guest Main` runnable and getting no CPU. `KfAcquireSpinLock` now sleeps after 32 tries, 20 µs and then 1 ms after 128. With the fix and pinning on, about 10 minutes of the same play had no stall over 0.72 s and 291 `starved`. XAudio stayed at 12-13% of a core each, so this removes the stalls, not that cost. Pinning stays off by default until a longer test.

**Guest memory mapping** now retries at another address. A crash log showed `guest memory commit failed`, after which a guest thread block came back null and the game wrote through it. An allocation that still fails now stops with a clear error. No crash in about 14 game starts since the fix, too few to prove it at the earlier rate of about 1 in 50.

## Milestone 5 (2026-11-05, b64 = `6e3e2bee` against b29 = `f624c098`, 1 warm-up + 3 counted runs)

First battle (`new-game-battle`), stock handheld clocks (CPU 1020 / GPU 461 / memory 1331 MHz), the Milestone 4 env without `LO_ZPD_MODE=xenia` (real occlusion queries, the default now), with `LO_MAIN_CORE=0`. Both builds profile-guided (`out/pgo-street5` for b64).

| First battle | b29 (morning) | b64 | |
|---|---:|---:|---|
| FPS at 540 rows | 28.30 | 28.39 | GPU-bound: 34.9 ms of GPU a frame in both |
| p99 frame at 540 rows | 40.7 ms | 41.1 ms | |
| CPU, all threads, at 540 rows | 237% | 218% | |
| FPS at 360 rows | 29.86 | **29.96** | the 30 FPS cap |
| p99 frame at 360 rows | 39.1 ms | **34.5 ms** | |
| 1% low at 360 rows | 25.55 | **28.98** | |
| CPU, all threads, at 360 rows | 235% | 216% | |

At 360 rows one b64 run never reached the battle (the scripted inputs; a timeout, not a measurement): two counted runs there.

Busy street, heaviest spot (about 2,030 draws, 540 rows, the user's save), the same build pairs alternated: 25.5 FPS (b29) to **28.2-28.5 FPS** (b63/b64), +11%. Its three cores are all about 98% busy, so CPU taken off any thread turns into frames; skipping the game's audio callback entirely (`LO_AUDIO_SKIP_GUEST=1`) gave 29.9 FPS there. The changes, each measured alone, are listed in PR 28: GPU self-waits (`b72ce058`, +3.5%), no allocations per occlusion read (`299ac550`, +1.9%), natives for the D3D constant copy, bone upload and color conversion (+1.0%, +1.4%), constant slots for pack shaders (`4dc21837`), one XAudio mixing worker (`bee3d482`, +1.5%), and the native reverb generated by `tools/switch/ppc2native.py` (`07545829`, `6e3e2bee`, +1.5%).

## Heap growth in the first battle: the occlusion history's pending ring (2026-11-11)

PR 37 (issue 3) saw newlib's heap in use (`mallinfo().uordblks`) grow by about 2.7 KB/s, 10 MB/h, in the first battle with
every build, and by 0.1 KB/s in Uhra's station square, with deko3d's own blocks flat. The heap census
(`-DLO_SWITCH_HEAP_CENSUS=ON`, `LO_HEAP_CENSUS=1`, docs/SWITCH.md) found where: `gpu/occlusion_history.h`.

Setup: `new-game-battle` without the title menu's UP (no save on the console), left in the battle's command menu,
census report every 120 s, handheld, stock clocks, 540 rows, the console's release env.txt, LostOdysseyNX `2c6efaed`
with the census and PR 36's keep-awake (for runs over 18 minutes). The battle runs from about 120 s.

| Battle, seconds since start | 360 | 601 | 962 | 1,323 | 1,683 | 2,044 | 2,164 |
|---|---:|---:|---:|---:|---:|---:|---:|
| uordblks growth, KB/s (previous 120 s) | 2.82 | 2.42 | 1.92 | 1.47 | 1.18 | 0.89 | 0.77 |
| of which 128-255 byte chunks, KB/s | 2.36 | 2.05 | 1.76 | 1.40 | 1.18 | 0.89 | 0.79 |

After the first 6 minutes all of the growth is 128-255 byte chunks (+2.87 MB of +3.43 MB from 240 to 2,164 s; the
rest is the network log's history string, below). The sampled call stacks put them in `occlusion_history::Read`: the
`more` vector of `HistoryAnswers`' pending reads. The 32,768 slots of that ring each kept a vector for the keys of a
query read for more than two objects, and `clear()` keeps a vector's capacity, so every slot that ever overflowed
kept the largest buffer it had needed (8 keys, a 144-byte chunk, in this battle). The rate falls off as more of the
slots already hold one: about exp(-t / 27 min), which leaves about 1.3 MB to come after 36 minutes. A cache filling,
not a leak without end.
But its size is set by the worst query each slot ever saw: a scene with more objects per query raises it for the
rest of the session. Uhra's square was not measured again (the console's saves were gone).

Fix (PR with the census): the keys after the first two go to one pool for all slots, as a list per slot given back when the
count arrives or the slot is taken over. The pool holds the keys pending now; reads still do not allocate once it has
grown to that. Same battle, census build with the fix:

| uordblks | 361 → 842 s | 842 → 1,323 s | 128-255 byte chunks, 361 → 1,323 s |
|---|---:|---:|---:|
| before | +1,396 KB | +797 KB | +1,701 KB |
| with the fix | +286 KB | **-0.5 KB** | **+0** |

The 286 KB is one step at 721 s, the network log's history (`os/switch/netlog.cpp`) doubling its string from 256 to
512 KiB; it stops at 2 MiB and only runs with `LO_NETLOG`, which bench env files turn on.

The census itself, on: about 3,800 frees, 3,400 memaligns, 280 reallocs and 190 mallocs a second in the battle, one
sample in 32 KiB (36,000 in 26 minutes, 2,300 held at a time, 394 call stacks); 28.2 FPS in the battle with it on
(not an A/B).
