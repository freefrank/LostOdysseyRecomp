# Native renderer front-end: ordinary mesh implementation

Current follow-up: [acceptance rework](native-frontend-rework.md). The initial
checkpoint below predates the maintainer-supplied limited D3D12 report;
performance acceptance remains failed and the default remains off.

Date: 2026-09-29. Base: `62fc70588807aadeed88fefd238d7f14c84c8883`.
Branch: `feature/native-renderer-front-end`.
Runtime checkpoint: `2c85a3af290aac13dd658ad33cf39f9a12df47fd`.

**Status: opt-in mixed native/legacy implementation. Complete PM4 removal,
real-game correctness, performance acceptance and release are still open.**
The original [handoff](native-renderer-pm4-removal-handoff.zh-CN.md) and
[historical Uhra results](native-command-uhra-results.json) are preserved.
The earlier off/all comparison does not measure this new front-end.

## Delivered stages and evidence

| Stage | Implemented | Validation / acceptance |
| --- | --- | --- |
| P0, ordinary entry contract | Canonical SDK inspection identifies indexed `823C6860`, non-indexed `827B56B0`, their dirty writers and stream wait. | Static contracts and original-SDK CPU oracle; actual game call frequency is not measured. Shader/recorded-path investigation remains incomplete. |
| P1, shared backend input | `DrawState`, `CaptureLegacyDrawState`, named pipeline/target/viewport/resolve fields, borrowed shader/constants; common renderer has no CP/register reads. | Windows production renderer TU compilation and Linux/Windows CPU contracts pass. No full application link or GPU run. |
| P2, pre-flush ordinary mesh | Full SDK entry overrides, preflight, value-owned ordered mesh delta, dirty acknowledgement, mixed state adapter and counters. | Windows production CP/producer compilation and 128-case original-SDK oracle pass, including predication. Runtime coverage, Uhra A/B and user acceptance are open. |
| P3, stream-coherency subset | Ordinary mesh stream state and fixed coherency wait no longer require PM4 production/decoding. Both routes share `ExecuteWait`. | Expanded original-SDK oracle verifies stream/wait/bool ordering. This is only one part of P3; the remaining list below is not implemented. |
| P4, PM4-free defaults | Not implemented. | Disabling legacy scheduling cannot yet run the supported game path. Default remains off; no release/tag was made. |

Substantive commits (source-transport commits are not feature milestones):

- `3776e861939540aaaa9456b07c5309a13a15c2c7`: explicit draw-state and legacy adapter contracts.
- `e1c4c55fedfff7a571e3415fabb433cd6328917b`: production renderer consumes explicit state; 112 `Reg`/`RegF` accesses removed.
- `6c37a5dc93e1662761686c305de4b467447656c8`: complete ordinary mesh SDK entry interception before flush.
- `2c85a3af290aac13dd658ad33cf39f9a12df47fd`: stream coherency and native-front-end probe integration.

## P0: actual ordinary SDK contracts

Generated source is inspected through `tools/ppc_codegen.py`; it is not edited
or checked into the repository. `native_mesh_hooks.cpp` uses the project's
existing strong `PPC_FUNC` override mechanism and retains the original
`__imp__sub_*` implementations for fallback. The application source glob picks
up this file; no generated-C++-only hook or address patch is required.

| Entry | Inputs used by the original ordinary path | Source and semantics |
| --- | --- | --- |
| `sub_823C6860` | r3 device, r4 primitive, r6 start index, r7 index count; r5 is not consumed in this path | `ppc_recomp.12.cpp`, original line 43478. Index resource at device+12428; flags at resource+0, data at resource+24. Writes base vertex zero. |
| `sub_827B56B0` | r3 device, r4 primitive, r5 first vertex, r6 vertex count | `ppc_recomp.74.cpp`, original line 5875. Auto-index source, first vertex becomes `VGT_INDX_OFFSET`. |
| `sub_823C6468` | Higher-level draw wrapper | `ppc_recomp.12.cpp`, original line 42883. Sets stream/index resources and calls the ordinary entries. Static callers do not prove a measured frame coverage percentage. |
| `sub_823C6740` | Index binding/resource setter | Remains in the guest path, including resource-use/retirement bookkeeping. The new hooks do not bypass it. |

Both accepted entries are void. The hook leaves the entire incoming PPC
context unchanged, including LR/SP/nonvolatile registers. It does not skip UP
save/restore code because UP and special submissions are outside this scope.
Primitive low six bits, 16/32-bit index flags, start-index offset, index count,
DMA size/endian and the SDK's physical-address alias correction are retained.
Counts zero or above 65535 retain the original split/empty behavior.

The device's first five 64-bit dirty masks are at offsets 0/8/16/24/32.
Offset 40 selects additional derived-state preparation. The current device is
validated against `0x83302A38`. Byte device+`0x2ABC` masked by `0x81`, and word
device+`0x33A0`, exclude special/recorded submission modes.

Preflight rejects unsupported shader/derived-state preparation, invalid index
resource ranges, and command-space rollover **before any guest mutation**.
Capacity uses the smaller of device+52 and device+56 and a conservative bound
for both native and original encodings. The ordinary cursor is device+48.
Accepted submission initializes the complete payload, copies it at cursor+4,
acknowledges the five dirty masks, then publishes the final cursor. No original
SDK state-flush helper is invoked on this accepted path. Rejected submission
calls the complete original producer without partial acknowledgement.

## State, transport and ownership

The native producer emits one bounded semantic mesh command containing draw
arguments and changed groups. Values are copied into the original guest-owned
command stream; unchanged state is inherited at the command's execution point.
This keeps ring/indirect-command-buffer ordering, including repeated execution.
There is no hook-time side FIFO and no transient host handle to retire on first
consumption. Indexed geometry remains an execution-time resource read, as in
the original path; command values are not a new snapshot of all resource data.

The tag is `0x80004C4D`. The nine-dword header contains length, wire version,
group bits, initiator/DMA/base-vertex values and a producer revision. Version 2
adds stream state; the decoder still accepts version 1 without that group.
The maximum command is bounded at 2500 dwords. Lengths, masks, groups, source
selection and complete payload bounds are checked before applying any state.

| Group | Guest device byte offset | Compatibility bank | Dirty field size |
| --- | ---: | --- | --- |
| VS constants | 1920 | `0x4000` | 64 fields, 16 dwords each |
| PS constants | 6016 | `0x4400` | 64 fields, 16 dwords each |
| Pipeline | 10548 | `0x2200` | 12 fields, 1 dword |
| Program control | 10528 | `0x2180` | 5 fields, 1 dword |
| Targets | 10368 | `0x2000` | 16 fields, 1 dword |
| Raster | 10444 | `0x2100` | 21 fields, 1 dword |
| Texture/vertex fetch | 1152 | `0x4800` | 32 fields, 6 dwords |
| Point raster | 10596 | `0x2280` | 21 fields, 1 dword |
| Draw/resolve control | 10680 | `0x2300` | 38 fields, 1 dword |
| Polygon offset | 10832 | `0x2380` | 8 fields, 1 dword |
| Bool/loop constants | 10112 | `0x4900` | 1 field, 40 dwords |
| Stream fetch | 10272 | `0x2388` | 6 fields, 4 dwords; owned control from device+10396 |

The CP worker remains the only GPU command-list owner. It consumes deltas in
the original SDK order, updates the common effective state and compatibility
MMIO/register mirror, then calls the same `renderer::Draw(DrawState)`. Native
and legacy writes update named scalar fields through `ExecutionDrawState`.
Legacy draws therefore see preceding native updates, and legacy changes are
not hidden behind a stale native cache. External MMIO writes request atomic
cache invalidation; they do not mutate the worker-owned typed state.

The renderer no longer reads `Reg()`, `RegF()`, `g_commandProcessor`, or implicit
active shaders. Draw, resolve and clear share the same existing backend; no
second copy of `DrawImpl`, shader translator, PSO/texture cache, EDRAM system,
AA/MV/FG path or upload/descriptor/fence owner was introduced.

**Remaining state costs are explicit.** The mixed adapter still writes the
compatibility bank/MMIO, and large banks are borrowed from its current values.
The 40 scalar fields preserve the old zero-value MMIO fallback. The existing
VS/PS working copies (4 KiB each) and vertex/index content checks remain.
Producer and applied-group revisions describe ordered deltas, not allocation
identity or proof that unobserved writes cannot occur. DrawState revisions
stay untracked where direct MMIO coverage is incomplete; no new revision-only
GPU upload cache or claimed per-draw 8 KiB conversion elimination is included.

Borrowed DrawWords/shader views are valid only for the synchronous Draw call.
Renderer AA/MV modifications operate on working data, not retained base values.
GPU descriptors/uploads/textures retain existing slot and fence lifetimes.
Allocation generation, content revision and command-buffer lifetime are not
collapsed into a single producer revision.

### Stream coherency and predication

The newly ported stream group preserves these ordered operations:
stream-control register `0x2007`, coherency status `0x0A31`, range
`0x0A2F/0x0A30`, then the original fixed register wait. The exact wait operands
are `(3, 0x0A31, 0, 0x80000000, 8)`. `ExecuteWait` is extracted from the existing
legacy implementation and is used by both paths; its worker-stop checks,
event handling and CPU-resident coherency completion are preserved.
Stream fields precede bool/loop updates even though their wire group was added
last for version compatibility. The original fetch-writer tail values remain.

State, base-vertex updates and stream waits are unpredicated. Only the draw and
its DMA/initiator writes are skipped by the bin mask. Malformed native commands
follow the existing command error path. After state/wait/upload/draw effects,
the consumer never attempts a second legacy draw as a fallback.

## Validation record

Machine-readable details, hashes and raw bounded oracle transcripts are in
[native-renderer-front-end-evidence.json](native-renderer-front-end-evidence.json).

| Check | Result | Evidence |
| --- | --- | --- |
| Explicit-state CPU contract | Linux GCC C++20 and Windows ClangCL pass | Bounds, non-contiguous targets, high ALU banks, zero MMIO fallback, shader/index identity, base/work-copy separation |
| Production renderer TU | Windows ClangCL pass | Run [36540593846](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/36540593846), job 109314785082 |
| Mesh/mixed CPU contract | Linux GCC and Windows ClangCL pass | Random wire deltas, replay, malformed/truncated messages, legacy/native transitions, stream/v1 compatibility |
| Ordinary SDK oracle and production CP/hooks TUs | Pass | Run [36544598596](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/36544598596), job 109327838754 |
| Extended stream-order SDK oracle and CP/hooks TUs | Pass | Run [36546697149](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/36546697149), job 109334695786 |
| Probe parser/environment | Six tests pass on Linux and Windows | Two receipt forms, latched scene marker beyond old tail, partial lines, log replacement, independent counter window, explicit environment injection |
| Windows full application link / real D3D12 / real Vulkan | Not performed for this front-end | Compilation and CPU SDK comparisons do not establish GPU correctness |
| Linux production Vulkan build / real game | Not performed | Linux evidence is limited to portable CPU tests |
| Uhra 120 FPS ordinary A/B, performance benefit, user acceptance | Not performed / not accepted | Local runtime device was offline; no replacement performance numbers were fabricated |

The final original-SDK transcript is:

```text
SDK_ORACLE_PASS cases=128 predicates=pass,skip stream_wait_order=verified original_helpers=1451 native_flush_calls=0 source=canonical_generated_sdk gpu_or_game_validation=false
```

This compares the production hook against the canonical generated original
SDK on synthetic guest memory, including all compatibility register results,
draw counts, index semantics, dirty acknowledgement, rejected shader preflight
without mutation, and stream/wait/bool ordering. It does not simulate shader
variants, arbitrary resources, game input, GPU scheduling or a full playthrough.

Some integration jobs are red **after their compile/test steps passed**: P1
had source publication trouble, P2 staging read a CRLF pathname, and P3 staging
rejected a trailing blank Python EOF line. Source-only publish jobs rechecked
all input/output SHA-256 values and pushed ordinary fast-forward commits.
The sole P3 cleanup normalized that Python EOF with an identical AST; runtime
source is byte-identical to the tested result. Those publish retries are not
additional gameplay or performance tests.

## Counters and ordinary A/B

Use `LO_NATIVE_FRONTEND=mesh` to enable this implementation, and `off` (or unset)
to restore the original SDK producers. Keep `LO_NATIVE_COMMANDS=off` when
isolating the new path from the older sparse/quad/fan experiment. Restart the
process after changing these environment options.

Low-cost `native frontend:` receipts identify the new mode and count executed
mesh commands, native/other backend draw calls, skipped predicates, command
words, state values and stream coherency waits. They do not count expanded host
GPU draws. Producer creation counts and CP execution counts are kept separate.
`LO_NATIVE_FRONTEND_STATS=1` additionally records fallback reasons and remaining
PM4 types/opcodes; it is an explicit diagnostic, not the ordinary CPU basis.

`run_native_title.py --native-frontend off|mesh` records the new mode separately
from `--mode`. It rejects an old binary that never acknowledges the new mode,
and a mesh sample that demonstrates no native mesh execution. Native coverage
uses its own receipt swap window rather than pretending it is exactly aligned
with the OS-CPU completed-frame window. Diagnostic switches are
`--frontend-stats`, `--render-timing` and `--scene-stats`; results containing any
of them are labelled diagnostic. Both completed-frame formats are understood,
and the Uhra load marker is latched while reading incrementally.

The [probe instructions](../../tools/perf/README.md#ordinary-mesh-native-front-end)
provide same-candidate 120 FPS off/mesh/mesh/off commands. Preserve game data,
baseline config/profile/save/shaders and previous evidence. Use a newly built
candidate that includes this branch and a unique output directory for each
case. Compare throughput, CmdProc, Guest Main and whole-process CPU together.
No numeric benefit is accepted until that controlled comparison is run.

## Remaining PM4 removal inventory and next entry contracts

| Remaining producer/path | Why it still uses legacy | Required next implementation |
| --- | --- | --- |
| `sub_823C6E18`, shader/linkage preparation | Shader dirty bits 17..20 still reject the full mesh call. It mutates mode/target/program state, loads constants/shaders and may select multiple variants. | Port its guest writeback and shader-variant/resource-use contracts; semantic shader/constant/bin commands must share the execution owner. |
| `sub_823C2200`, derived-state preparation | A nonzero main-mask & device+40 requires conditional/bin-specific state effects not covered by the simple group delta. | Explicit conditional pipeline/target state and correct bin mask/select restoration. |
| Shader preparation helpers | `823C7630` declaration work, `823C7830` initial constants, `827B84E8` variant lookup and `827B8378` variant generation have separate effects. | Inventory safe CPU-only helpers versus PM4 writers before broadening acceptance. Do not mark dirty bits cleared merely to increase counters. |
| UP and unusual draw producers | Temporary device fields, resource copies, split/empty counts and special submission remain original. | Own/copy UP data before its caller may overwrite it; preserve restoration and resource bookkeeping. |
| Recorded/nested command creation and execution | Native value records themselves can replay, but producer qualification excludes unresolved recorded modes; legacy ring/IB scheduling remains. | Establish allocation/generation and inherited/recorded/execution-time semantics for each command path before removing its scheduler. |
| Clear/resolve/copy producers | The shared backend has explicit inputs, but these SDK producers have not all been replaced. | Semantic operations with the current EDRAM/resource invalidation and ordered synchronization. |
| General events, waits, swap and presentation | Only the ordinary mesh stream wait is native. Generic scheduling, interrupts and XE_SWAP retain PM4 producers/dispatch. | Preserve visibility, frame-plan markers, interrupts, pending GPU work and presentation ownership. |
| Resource identity and unobserved writes | Existing content checks remain essential; guest address is not allocation identity. | Track allocation generation/content revision and slot/descriptor epochs before more aggressive reuse. |

Consequently, there is **no PM4-free runtime mode or complete path-coverage
claim** at this checkpoint. Ordinary mesh throughput dominance has not been
measured. The off switch is a working rollback, and default enablement must be
based on new-path correctness and repeatable real-game performance evidence.
Do not delete the legacy consumer or stub ring submission to make counters zero.
