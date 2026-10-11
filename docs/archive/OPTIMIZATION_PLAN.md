# Optimization plan

The macOS measurements and completed statements below are historical notes from
the MikeRavenelle fork at [`b7951cd`](https://github.com/MikeRavenelle/LostOdysseyRecomp/tree/b7951cd5aa0830b5342aeb3efa835f878e227796).
They are not M1 Max re-tests for this integration. See [development status](STATUS.md)
for the current build and validation boundary.

Project-wide list, not only macOS. Ranked by expected gain against risk. Every item
is measured before and after (`LO_FRAME_TIMING=1`, the same scene, the same
resolution); a change that does not show up in the numbers is reverted, as the
depth-borrowing experiment on Metal was.

## Baseline to capture first

- One fixed benchmark per scene type: attract (title), menu open, Uhra city
  (`tools/drive_city.py`), a battle. Report CPU frame time, GPU frame time, draws,
  render passes (Metal) or barriers (D3D12/Vulkan), FPS min/avg.
- Same set on each platform so a change can be judged everywhere at once.
- Current macOS numbers (M1 Pro, 720p): attract 30.0 FPS locked, about 10.4 ms GPU
  per frame, 82 render + 18 blit passes per frame.

## Measurements (2026-09-29, M1 Pro, macOS, Numara city)

- **Above 30 FPS the GPU and the GPU command-processing thread are balanced**:
  at the 120 FPS target in Numara a frame takes ~18.5 ms, of which the GPU is busy
  17.4 ms (`gpu_queue_batches_elapsed_ms`) and the command thread ~16 ms (draws
  11.4 ms). The game's main thread sleeps in `KeDelayExecutionThread` 72% of the
  time. Speeding up only one side barely moves FPS. Command-thread hot spots:
  per-draw constant snapshots (now incremental, 1.87 -> 1.34 ms/frame), index and
  vertex preparation (~3.3 ms), bindings, and (only with `LO_RENDER_TIMING`)
  timing calls (~9%). GPU side: ~220 render passes per frame; vertex/tiling work
  exceeds fragment work, so fewer passes is the lever.
- **Register promotion (below) is not worth it here:** the safe subset
  (`cr/xer/reserved_as_local`, `skip_msr`) gave 53.4 → 54.7 FPS, within noise;
  `non_argument_as_local` crashes at startup (guest access at 0xFFFFFFC4). Reverted.
- **Camera TAA was GPU-bound** (13 FPS; 52 ms of each 75 ms frame waiting for a
  drawable); fixed on Metal by batching the replay draws: 30 FPS (cap). Metal System Trace: ~1,080 render passes per frame with TAA vs ~220
  without; vertex/tiling work 700 vs 270 ms per second. The motion-vector replay
  draws into its own target between scene draws, splitting the scene pass at almost
  every draw. Fixed on Metal by queuing the replay draws and recording them in one
  pass before submits, clears, resolves and MV reads.
- **Without TAA at 30 FPS** the GPU is about half busy (~220 passes per frame).
- **Pass merging has little headroom (Numara, 740k passes instrumented):** passes
  end because the color target changes (39%), a barrier follows (22%), the depth
  texture changes (21%, mostly depth-only passes switching shadow/depth targets) or
  both change (17%). Splits that reopen the same targets after a barrier are only
  2.7%. Keeping depth attached across depth-disabled draws cut GPU time
  17.68 -> 17.43 ms/frame (~1.4%); reverted. The passes follow the game's own
  render-target structure.
- **No first-use stutter on a warm cache:** in three 150 s scenarios the renderer
  created 0 shaders and 8-12 pipelines (~1 ms total). Every frame over 45 ms has
  `cp_idle_ms` close to `frame_ms`: the game was loading (disc-4 file open, 1.75 s;
  travel, 0.7 s) and submitted nothing. The startup prebuild plus Metal's system
  shader cache already cover what binary archives would.

- **Loading (2026-09-29):** the disc 4 switch spent 69% of the main thread in
  `RtlTimeFieldsToTime` (host `timegm` locks and checks the time zone database on
  macOS). Integer calendar arithmetic cut the switch from 1,751 to 708 ms. The rest
  of that load, and the ~0.7 s travel loads, are guest work spread over the loader
  thread, the main thread and the game's job workers (one decodes in
  `sub_82CCA0B0` while the other spin-waits in `sub_82CC3C50`); a native decoder
  would need that routine identified first.
- **Frame timing above 60 FPS (2026-09-29):** game time follows real time at 30, 60
  and 120 FPS (`game_time_ratio` 1.00), the opening movie and first battle start at
  the same wall-clock times, and a 20 s walk ends at the same spot. Battles are
  GPU-bound near 53 FPS at 720p. `new-game-battle-60fps` guards this.

## Guest CPU (all platforms)

1. **XenonRecomp local-variable options.** *Measured 2026-09-29: ~2%, reverted (see
   Measurements).* `LostOdysseyRecompLib/config/LostOdysseyRecomp.toml`
   has every register-promotion option off (`ctr_as_local`, `xer_as_local`,
   `reserved_as_local`, `cr_as_local`, `non_argument_as_local`,
   `non_volatile_as_local`, `skip_lr`, `skip_msr`). UnleashedRecomp turns most of
   them on: the compiler then keeps those registers in host registers instead of
   loading and storing the context structure around every instruction. Expected to
   be the single largest CPU win. Risk: needs a playthrough-level test for each
   option (setjmp/longjmp and exception paths), so it depends on automated testing.
   Enable one option at a time.
2. **PGO and ThinLTO in release builds.** `cmake/LoOptimization.cmake` already
   supports `LO_LTO_MODE=thin` and `LO_PGO_MODE=generate/use`; release CI uses
   neither. Profile from the automated scenarios, then build with the profile.
3. **Hot guest function overrides.** Profile (Instruments on macOS, perf on Linux,
   the existing `LO_PROFILE_DIAGNOSTIC` build) and replace the top guest routines
   (memcpy/memset, string, math, decompression) with native host implementations,
   as the runtime already does for some kernel calls.
4. **Vector code on AArch64.** simde maps VMX to NEON; check the hot VMX ops in the
   generated code for scalar fallbacks (permute, pack/unpack, `vpkd3d128`,
   dot products) and add direct NEON paths where simde falls back.

## Renderer CPU (all platforms, the real limit above 30 FPS)

1. *Done:* ALU constants are re-read only when a bank changed (generation per bank).
2. Cheaper vertex-content change detection (the memcmp/hash path).

## GPU, all backends

1. **Rect lists without geometry shaders.** Vertex-stage expansion (six vertices
   per rect, fourth corner computed in the vertex shader). Required for correct
   Metal output; also removes the geometry shader stage on D3D12/Vulkan, where GS is
   slow on many GPUs.
2. **Framebuffer switches.** *Measured 2026-09-29: ~1.4% GPU time, reverted (see
   Measurements).* Keeping depth attached when the guest leaves it bound but
   disabled merges few passes; most switches are real target changes.
3. **Barrier batching.** *Measured 2026-09-29: splits that reopen the same targets
   after a barrier are 2.7% of passes; not worth it.*
4. **Menu copy cost.** Menus copy full-screen targets every frame (identified in the
   worklog, not changed). Skip the copy when the source did not change.
5. **Shader and pipeline warm-up.** Keep the pipeline-recipe prebuild; extend the
   portable shader pack to macOS (pre-translated MSL or Metal binary archives) so a
   first launch does not translate about 28,000 shaders.

## Metal specific

- Binary archives (`MTLBinaryArchive`): *not needed for stutter (no first-use
  stutter measured on a warm cache).* Only useful if a shipped archive shortens the
  first launch, and archives are tied to GPU family and OS version.
- Memoryless or `DontCare` load/store actions on transient targets (depth that is
  never read back).
- Argument buffer reuse across draws with identical bindings.
- MetalFX Temporal behind the existing upscaler interface (spatial is done). Design
  and measured gain (720p -> 1440p: ~50 FPS vs 33 native in Numara):
  [METALFX_TEMPORAL_DESIGN.md](METALFX_TEMPORAL_DESIGN.md).

## Frame pacing and audio (all platforms)

- Keep the precise-sleep approach (`os/host_scheduling`) and check Windows/Linux
  pacing with the same oversleep measurement.
- Audio buffer size adaptive to frame-time spikes, so a slow frame does not
  underrun.

## Memory

- Metal shader modules are built lazily; also evict unused translated shaders
  (about 850 MB at the title screen before lazy modules).
- Watch peak memory during long play on 16 GB machines.

## Build and CI

- Separate the PPC library build (slow, rarely changes) from the runtime in CI with
  caching, so macOS and Linux jobs stay fast.
- ccache/sccache for local and CI builds.
