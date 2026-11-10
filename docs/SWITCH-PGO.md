# Profile-guided optimisation of the Switch build

The recompiled game is machine-translated PowerPC: its branches carry no layout information, so GCC lays blocks out and
allocates registers from static guesses. A profile of the real game on the console tells it which paths run. The method
comes from SwitchWakerHD (`docs/pgo.md`, `docs/pgo-progress.md`), where the game and the runtime together gained up to
7.5% and the game code alone stayed inside the noise, so here the runtime is always included.

**A profile only fits the sources it was made from.** GCC compares a checksum of each function's flow graph and ignores the
profile of a function that changed (`-Wno-coverage-mismatch`). Make it again after a recompile with a different
XenonRecomp or config, or after changing runtime code that matters. A stale profile costs speed on the changed functions,
never correctness.

## The three modes (`cmake -DLO_SWITCH_PGO=...`)

| Mode | What | Use |
|---|---|---|
| `OFF` (default) | a normal build | everything else |
| `GEN` | the hot game files (`tools/switch/pgo_hot_files.txt`), the runtime and the deko3d backend are instrumented | training only, never released |
| `USE` | the same code compiled with the profile in `LO_SWITCH_PGO_DIR` | the optimised build |

This is separate from `LO_PGO_MODE` in `cmake/LoOptimization.cmake`, the Clang profile for PC builds.

`pgo_hot_files.txt` lists the 61 of 246 `ppc_recomp.N.cpp` files holding 99.5% of the guest samples of a CPU profile
(`tools/switch/switch-cpu-profile.py`). Instrumenting all 96 MB of game code would make training unusable.

## Recording a profile by playing (no PC needed)

The training build saves its profile to the SD card on its own, so the debug server and `pgo_train.py` are optional.

1. Build it: `tools\switch\build-switch.ps1 -Pgo GEN` (or `PGO=GEN bash tools/switch/build-switch.sh` in the image).
   The NRO lands in `out\switch\pgo-GEN\`.
2. Copy that NRO to `sdmc:/switch/LostOdysseyRecomp/` in place of the normal one, and delete
   `sdmc:/switch/LostOdysseyRecomp/pgo/` if an older profile is there.
3. Play. Spend most of the time where speed matters: battles, busy towns, the field. Menus and loading screens add
   little. The build is slow; that is expected.
4. Quit from the game's own menu, which writes the last counters. Closing from the HOME menu loses up to one save
   interval and nothing else.
5. Copy every `.gcda` file from `sdmc:/switch/LostOdysseyRecomp/pgo/` into `out\pgo\` (flat, no subfolders).
6. Build `tools\switch\build-switch.ps1 -Pgo USE`; the optimised NRO lands in `out\switch\pgo-USE\`.
   Put the normal or the USE NRO back on the SD card.

The counters are written every `LO_PGO_SAVE_SECONDS` seconds (env.txt, default `60`; `0` writes only on quit) and on
quit. Each write adds to the files on the SD card, so several sessions build one profile; delete the folder to start
over. `pgo/pgo.log` lists every session and write. A write of the whole profile takes a moment and can show as a
short hitch in the training build; that does not matter for the profile.

## Procedure with the debug server

Use a build folder per mode. All commands run inside the `switch-dev` image, with `ninja -j 4` (`cmake --build ... -j 4`).

```bash
cmake -S . -B out/build/switch-pgo-gen -G Ninja -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/toolchains/switch-devkitA64.cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_SCAN_FOR_MODULES=OFF -DDEVKITPRO=/opt/devkitpro -DLO_SWITCH_NVK_LIBRARY=/opt/devkitpro/portlibs/switch/lib/libvulkan.a -DLO_SWITCH_PGO=GEN
```

Build the `LostOdysseyRecomp` target, deploy its NRO, then on the machine that reaches the console:

```bash
python3 tools/switch/pgo_train.py clean
```

```bash
python3 tools/switch/pgo_train.py scene battle
```

```bash
python3 tools/switch/pgo_train.py scene town
```

```bash
python3 tools/switch/pgo_train.py fetch --out out/pgo
```

`scene town` loads the console's last save, so make that a busy town. Add `--here` to train where the game already stands.

Configure a second folder with `-DLO_SWITCH_PGO=USE -DLO_SWITCH_PGO_DIR=$PWD/out/pgo` and build it. Deploy a normal build
afterwards: the training build is slow, and its speed means nothing.

To check that a profile is not overfitted, fetch after one scene into its own folder, build `USE` from it and judge it on
the other scene, which it never saw.

## Results

First battle, 960x540, handheld at CPU 1020 / GPU 460 / memory 1331 MHz, lean shaders level 2. Each row is a scripted
120 s of the battle, attacking, on freshly started builds, two runs per build unless noted.

| Build | FPS | p50 ms | p99 ms | GPU ms | Main % | CmdProc % |
|---|---|---|---|---|---|---|
| Normal | 22.11 | 42.4 | 74.6 | 38.9 | 86.3 | 75.8 |
| Profile from the ship only (battle never seen) | 23.16 | 40.9 | 68.6 | 39.1 | 86.4 | 63.8 |
| Profile from ship and battle | 23.37 | 40.9 | 68.6 | 39.1 | 86.0 | 64.4 |
| Same, with `-freorder-functions` | 23.27 | 40.9 | 70.5 | 39.1 | 86.3 | 63.8 |

- **+5.7% FPS at 540 rows**, and +4.8% on a scene the profile never saw, so it isn't overfitted.
- **Render thread time per frame is 20% lower** (3.43 to 2.76 % of a core per FPS). Main improved about 6%.
- **At 360 rows, one run each:** 23.27 to 25.22 FPS (+8.4%), which is closer to the CPU's gain, since the GPU limits 540 at 39 ms.
- **The merchant ship** holds 30 FPS at 540 with the full profile, p99 34.5 ms.
- With the ship profile the code shrank 0.13%, with 1,603 cold parts split out, against the 29% growth of GCC's default options.
- `-freorder-functions` does group the hot functions, but gains nothing here, so the USE flags stay as in SwitchWakerHD.

The GPU now sets the limit at 540: 39 ms per frame allows 25.6 FPS at best.

Retrained on the final code, with the register write-backs removed and lean shaders level 3, the gain is **+6.4%**: 22.21 to 23.63 FPS, median frame 40.2 to 37.7 ms, GPU unchanged at 35.1 ms. The full result is Milestone 4 in `docs/SWITCH-BENCHMARKS.md`.

## Building a release

The profile is only read while compiling, and its gain is in the machine code of the NRO. Players need nothing extra, and the profile itself doesn't ship. It isn't in the repository either, so a default build doesn't get this gain: whoever makes the release retrains and builds it, about 30 minutes in all.

1. **Freeze the code.** Commit everything first, and run `tools/switch/build-switch.sh`, which leaves `LostOdysseyRecompLib/ppc/` ready. The profile only fits the code it was trained on.
2. **Build `GEN`,** about 9 minutes:

   ```bash
   docker run --rm -v "$PWD:/work" -w /work -e PGO=GEN ghcr.io/autorunhq/switch-dev:2026.10.05 bash tools/switch/build-switch.sh
   ```

   Deploy it:

   ```bash
   python3 tools/switch/lo_debug.py deploy out/switch/pgo-GEN/LostOdysseyRecomp.nro
   ```

3. **Train,** about 10 minutes. This restarts the game, so nobody can play meanwhile:

   ```bash
   python3 tools/switch/pgo_train.py clean
   ```

   ```bash
   python3 tools/switch/pgo_train.py scene town --dwell 180
   ```

   ```bash
   python3 tools/switch/pgo_train.py scene battle --dwell 180
   ```

   ```bash
   python3 tools/switch/pgo_train.py fetch --out out/pgo
   ```

4. **Build `USE`,** about 8 minutes. The result is in `out/switch/pgo-USE/`:

   ```bash
   docker run --rm -v "$PWD:/work" -w /work -e PGO=USE ghcr.io/autorunhq/switch-dev:2026.10.05 bash tools/switch/build-switch.sh
   ```

5. **Test that NRO on the console,** then publish it. The training build must never ship.
6. **Keep the matching `.elf`** from `out/switch/pgo-USE/`: crash addresses from that release only resolve against it.

The level 3 shader pack is separate from the NRO, and `tools/switch/release-env.txt` asks for it: a release must include `shaders/deko3d_shaders_lean3.lodk`, or the console compiles every shader itself. It's built with `LO_DEKO3D_LEAN_SHADERS=3 deko3d_shader_tool cache|variants`, then `pack_dkcache.py`.

A code change after step 3 is safe, but the functions it touches lose their gain. A small hotfix can ship without retraining. Retrain after anything that touches hot code.

## Traps (all from SwitchWakerHD)

- **No value profiling** (`-fno-profile-values`). libgcov's value profiling reads `tpidr_el0`, which Horizon leaves 0: the
  first indirect call faults at a tiny address. After changing compile flags, delete the objects.
- **Profile names come from the object's relative path** (`-fprofile-prefix-path` is the build folder): any folder finds a
  profile made by another, if CMake compiled both.
- **GCC's default `-fprofile-use` options inflate the code**, by 29% in SwitchWakerHD (unrolling, peeling, tail duplication).
  They are off here, and block partitioning is on. `LO_SWITCH_PGO_USE_FLAGS` appends more.
- **Precompiled header**: the runtime's flags are per target, not per file, or the header would not match.
- Compare speed at the stock clocks only, and train at any.

## Hidden symbols

SwitchWakerHD also gained from hidden visibility, because its `-fPIC` build loaded extern symbols through the GOT at each
of 15,000 indirect-call sites. That does not apply here: this build is `-fPIE`, a hot game object (`ppc_recomp.0`) has no
GOT relocations at all, and the renderer has only a few on its hot paths (109 for `g_commandProcessor`, 19 for `g_memory`).
