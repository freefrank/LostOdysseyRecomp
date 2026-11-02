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

`pgo_hot_files.txt` lists the 55 of 246 `ppc_recomp.N.cpp` files holding 99.5% of the guest samples of a CPU profile
(`tools/switch/switch-cpu-profile.py`). Instrumenting all 96 MB of game code would make training unusable.

## Procedure

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
python3 tools/switch/pgo_train.py scene ship
```

```bash
python3 tools/switch/pgo_train.py fetch --out out/pgo
```

Configure a second folder with `-DLO_SWITCH_PGO=USE -DLO_SWITCH_PGO_DIR=$PWD/out/pgo` and build it. Deploy a normal build
afterwards: the training build is slow, and its speed means nothing.

To check that a profile is not overfitted, fetch after one scene into its own folder, build `USE` from it and judge it on
the other scene, which it never saw.

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
