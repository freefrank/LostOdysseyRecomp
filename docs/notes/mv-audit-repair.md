# MV audit repair on the geometric replay baseline

Runtime base: `c605eb4d4126d93d6033c8cb510d232218f98d1b` (mv).
Audit supplied by the user: `mv-review-9d7b778.md`.

The branch advanced during the repair. This patch preserves the real translated
VS/previous-pose replay already delivered in c605eb4; it does not restore the
removed fullscreen camera-only producer or overwrite that intervening work.

## Additional corrections

- Explicitly requested MV that fails readiness, frame, epoch, allocation, extent,
  or texture validation now rejects history. Only a null MV request selects the
  established camera-only baseline. `HistoryOwner::Reused()` agrees with that policy.
- A reset frame no longer passes an unused previous invalid jitter to stable TAA.
  A failed frame with jitter outside the supported interval cannot poison recovery.
- Renderer capture no longer copies all 4096 VS bytes into a temporary and then
  again into the arena. It saves the proven 64-byte jitter matrix, copies the FULL
  bank once, and restores the original matrix bits in the snapshot. No fixed bone
  window is assumed, and the uploaded guest bank is not mutated. At most 4432
  snapshot/scratch-copy bytes per accepted jittered draw replace 8400 bytes in the
  prior implementation, excluding unchanged GPU upload/shared preparation work.
- Relative constant addressing is reported as relative addressing, not as proof
  that a shader performs skinning. The weighted-palette GPU fixture still executes
  actual synthetic skinning arithmetic through the translated program.
- Optional GPU timers measure replay draws, the validity pass, TAA and display
  reconstruction. Readback/reuse occurs only at the existing completion serial;
  replay query blocks are sealed at EVERY renderer submission, including aborts
  and mid-frame flushes. Query pools are bounded and reused. Missing/stale results
  increment `unavailable`, not a fabricated zero-time result.
- CPU tracking wall time, actual snapshot/scratch bytes, unique/duplicate counts,
  mask batch allocations and in-flight counts are exposed. TAA constant-buffer
  map failures now return failure instead of dereferencing a null mapping.

The default is still `LO_MV_ENABLE=0`: no tracking, MV target allocation, variant
compilation, replay or consumption. Both `LO_MV_LOG` and `LO_MV_TIMING` default off.

## Verification performed locally

Linux software Vulkan: Mesa llvmpipe with the pinned Plume patch and DXC.

- 38 CPU lifecycle/reference checks; ASan/UBSan passed. The 10,000-draw steady-state
  tracker test still performs zero heap allocations. Matrix restoration tests check
  the last legal slot, full-bank preservation, exact bits and copy-byte accounting.
- 771 GPU/shader assertions passed. This is an assertion count, not 771 game scenes.
  The actual translated rigid AND weighted-palette programs traverse all 32 jitter
  phases; geometric motion remains zero for stationary geometry. Both raw-history
  and stable-grid TAA are tested against independently calculated ramp samples.
- Production HistoryOwner tests distinguish absent MV from invalid MV, test all
  readiness/identity failures, valid MV acceptance and recovery after a bad jitter.
- Timer tests cover default-off allocation, delayed serial completion, bounded
  in-flight pools, explicit unavailable samples and reuse across query generations.
- Existing real replay/alpha-discard/late-duplicate/Z-motion/occlusion checks and
  the 1200-frame resource reuse + resize test still pass.
- Existing `temporal_aa_test.cpp` GPU regression passes, including its original
  synthetic Halton coverage fixtures. Distinct-depth silhouette outputs remain
  diagnostics, not unearned convergence assertions.
- Actual renderer.cpp compiles against patched Plume using the documented
  compile-only guest-ABI boundary. This is NOT a full-game link or execution.

The delivery CI additionally builds on Windows/MSVC and compiles DXIL/SPIR-V.
Its Windows job is compile-only for GPU work; inspect the actual run result.
Linux CI executes Vulkan with validation layers and CPU sanitizers.

## Test / benchmark controls

Use the same newly built binary, TAA mode, save, camera, resolution and warmed
shader caches. Restart the process after changing environment variables.

| Configuration | ENABLE | REPLAY | CONSUME |
|---|---:|---:|---:|
| A: established TAA baseline | 0 | - | - |
| B: tracking only | 1 | 0 | 0 |
| C: tracking + geometry replay | 1 | 1 | 0 |
| D: geometry MV consumed by TAA | 1 | 1 | 1 |

`LO_MV_LOG=1` logs every 120 renderer frames. `LO_MV_TIMING=1` enables optional
CPU clocks/GPU timestamp queries, including TAA timing in baseline A. Instrumented
runs have overhead: benchmark normal frame times separately with timing off.
`mv timestamp` totals/counts are lifetime completed queue intervals, not all work
for the current renderer frame. Subtract two observations to obtain interval
averages; a nonzero unavailable delta means the interval is incomplete. Replay
intervals exclude target clearing and unrelated guest rendering; validity intervals
include their recorded transitions. They must not be labelled whole-frame GPU time.
Use existing `LO_RENDER_TIMING=1` for renderer queue batches and fence wait metrics.

`LO_MV_DEBUG=1` visualizes the experimental consumed field. Magenta rejects remain
important evidence; a normal color screenshot alone does not validate motion.

## Acceptance boundary

No real-game 4K FPS, Steam Deck power-budget result, hardware D3D12 image or real
Bell/character capture was measured here. This patch does not claim that 4K60 is
restored or that the full game has reliable motion coverage.

The c605eb4 limitations remain: no recovered stable guest Object ID, repeated mesh
instances rejected, no retained previous VTF texture deformation, and incomplete
non-depth-writing transparency/particle reactive coverage. The opt-in path must
not become default-on until those game-content cases are validated/addressed.
DLSS/FSR/XeSS integration is outside this repair.
