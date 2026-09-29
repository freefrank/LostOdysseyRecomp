# Native front-end acceptance rework

Date: 2026-09-29. Starting source: `6ad3b89947bbf9d439e48bf6f3f7662c7d4f3cdc`.
Branch: `fix/native-frontend-rework`. Default native frontend remains off.

## Reported acceptance baseline

The maintainer supplied a bounded D3D12 Uhra report for EXE SHA-256
`95137572029759400c83095568ddf2dc388d5947f53415042cd2fc9c897cd03c`.
Four ordinary off/mesh samples did not establish a stable benefit. A separate
same-view stationary old/new-off pair observed -16.1132% completed-frame rate
and +20.3198% CmdProc CPU per logged frame. Shader-dirty preparation rejected
87.17765% of ordinary hook calls in the separate diagnostic window. These are
reported observations, not new measurements made by this rework. The shared
backend remains active in off mode; off is not a rollback of DrawState.

## P1: bulk legacy reads and inactive shader work

`DrawWords::Copy` previously called the general, bounded `Read` view for every
word in a legacy ALU bank. The new bulk loop checks bounds and fallback policy
once, reads native words directly, and reads/swaps BE MMIO only for zero lanes.
Tail zero-fill, unaligned BE input, live MMIO updates, full 256-vec4 banks and
private jitter working copies are retained. No dirty/revision-only cache is
introduced and no necessary endian conversion is silently omitted.

The CP adapter now supplies shader bindings only for draw modes 4/5, and PS
only for mode 4. Resolve/non-drawing modes do not request either byte identity;
depth-only does not request the inactive pixel identity. Loaded shader snapshots
are not cleared. This removes avoidable front-end work, not the shader compiler
or the shared backend.

The portable differential fixture covers native/legacy views, empty/partial/
extended banks, unaligned mirrors, high constants, zero lanes and direct MMIO
mutation. The isolated Clang O2 benchmark compares identical updates/checksums
through the previous general-Read loop and the new bulk loop. The measured
legacy-bank medians improved in this container; native memcpy is unchanged in
algorithm and shows ordinary timing variation. Results are in
[native-frontend-rework-copy-results.json](native-frontend-rework-copy-results.json).
They do not isolate the entire reported off-mode regression or establish game
performance. No new game, GPU or full application build is claimed here.

## P2: actual execution and fresh sample-boundary evidence

A mesh probe now requires an increase in `native_draws`, not merely
`mesh_commands` (which includes predicated skips). It records native draw
fraction and calls per receipt swap using that independent counter window.

For Uhra probes, `LO_NATIVE_PROBE_SCENE=1` requests one scene receipt per 120
swaps. It contains a serialised map observation from the existing game-thread
map reader. No scene memory is read on the CP thread; unavailable or older-than-
2-second map snapshots are unavailable. A historical asset-open marker is no
longer sufficient. Sample boundaries require fresh, matching map ID/package
and advancing observation serials; observed changes, unavailability and stale
receipts fail the sample. This is sampled evidence and cannot exclude a scene
transition shorter than the observation cadence.

Both boundary screenshots are requested outside the CPU window; the first
readback has a two-second settling interval. Screenshot success is checked by
the runtime completion receipt, not an early-created file. Stationary Continue
input is the default; `--movement fixed-swaps` retains the old exploratory route
and marks it unsuitable for cross-build input matching. `--expected-map-id N`
can pin an independently verified save's map. Without that value, matching map
receipts still require manual Uhra screenshot review and do not certify a
particular plaza viewpoint or pixel equality.

## Remaining P1 and validation boundary

Shader-dirty/derived preparation is still rejected by the entry hook. Removing
that guard is unsafe: original shader selection, initial constants, guest
writeback and conditional ordering must be preserved before porting the tail.
No increased shader-transition coverage is claimed. Remote Desktop Commander
had no online device during this rework, so the locked candidate, private SDK
source and raw machine results were not reopened or overwritten. The supplied
negative performance record remains the acceptance baseline. Retest the changed
paths with a new properly identified build; do not reuse old partial object
directories as proof of candidate provenance, or rerun for documentation alone.
