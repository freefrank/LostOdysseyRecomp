# Native command bypass: first runtime implementation

Source baseline: `2ce27e35428d85abde20afb5b6b2c59052810b13`, with the architecture research from `3a95001`.

## Implemented scope

`LO_NATIVE_COMMANDS=1` (or `all`) enables three SDK interception points. `registers` enables only the bulk register writer. An absent variable, `0`, or an unknown value preserves the original SDK path. This is an experimental opt-in; no settings-menu default changes.

The strong wrapper for `sub_823C1BD8` replaces its dirty-mask PM4 type-0 packet construction with one native bulk-state record. It snapshots the selected source words before publishing the device cursor. Only ordinary registers `0x2000..0x5002` are eligible; scratch/status/coherence and host-private control registers retain their original side effects. The consumer copies contiguous runs into the register bank and guest-endian MMIO mirror without per-word register dispatch.

The generated `NativeIndexedQuad` hook at `0x823CD44C` intercepts the ordinary indexed-UP branch of `sub_823CD050`, after state setup and geometry allocation. It supports six-index triangle lists (primitive 4) and four-index triangle fans (primitive 5). Successful emission jumps to `0x823CD52C`, retaining the original vertex/index copies, resource bookkeeping and deferred device-cursor publication. Both 16-bit and 32-bit index variants retain their DMA fields. The consumer applies the original bin predicate before changing DMA registers and calls the same `ExecuteDraw` implementation used by PM4.

The `NativeAutoFan` hook at `0x827B5984` intercepts the ordinary non-indexed DrawPrimitive producer `sub_827B56B0` and jumps to `0x827B5A68`. It replaces the original three-word predicated draw with a two-word native record while retaining downstream count bookkeeping and cursor publication. Only the four-vertex auto-index triangle fan (`initiator=0x00040085`) is eligible. The consumer updates the draw initiator and deliberately leaves the DMA base/size registers untouched.

The first live run with the corrected hooks identified the title background's raw command as primitive 5, count 4, source 2 (not indexed). The historical renderer log's six indices and `idx=true` describe the host geometry after fan-to-list expansion. The initial implementation's primitive-5/six-input-index selector was corrected; its bulk-only timing result is not reused as final performance evidence.

The historical title-cloud shader pair is counted separately at consumption: VS `81217dc973d5dc31`, PS `cd6adb98ab83bf90`, in renderer-byte-FNV space. Geometry selection alone does not establish a scene identity. The older pre-packed-mip-fix capture is not used as a correct-image baseline.

## Ordering and fallback

The records occupy the existing guest command allocation, in ring/IB execution order. The discriminator is guest-endian and the payload is host-endian. Records contain values, never host pointers, side-table handles or one-shot tokens. Replaying an IB therefore replays the same recorded values. The native tag check bypasses the PM4 packet-type/opcode parser for an accepted record; unrelated packets keep their original parser.

All hooks check mode, live-device identity, submission flags, address arithmetic and available space before changing guest memory. A rejected request runs the original instructions. The register wrapper also checks the capacity required by the original sparse encoding, avoiding a skipped SDK rollover. Insufficient space never triggers a nested flush. Malformed native input fails command processing; it is not replayed after mutation.

The renderer, command-processor thread, GPU queue, fence retirement, upload/descriptor owners, shader snapshots, resolve and swap implementation remain in place. An accepted CPU command is not a GPU-completion receipt. This implementation removes selected PM4 generation/decoding and per-register dispatch, not the entire command processor or Xenos translation layer. It does not remove guest ABI or XEX dependencies.

## Reference

The architectural reference is [reblue at ce0edad](https://github.com/zolaware/reblue/tree/ce0edadbb79007526842dadebf80941bf8ea46ca), particularly `src/gpu/hooks/draw.cpp::DispatchDraw` and `src/gpu/hooks/state.cpp`. Those hooks maintain host render state and directly record backend draws. LO's first step retains the existing ordered stream to interoperate with unconverted producers. No reblue source was copied and its game-specific addresses are not used.

## Build and verification

The canonical PPC generator completed; comparing generated output against the baseline found only `ppc_recomp.12.cpp` and `ppc_recomp.74.cpp` changed, with the two configured hooks and jumps. The new runtime units and both changed generated units compiled. The isolated development executable then linked after rebuilding current runtime, plume and frame-generation sources; untouched guest units and other dependencies reused local build inputs. This is a development integration build, not a release package.

`LoNativeCommandStreamTest` initially passed on Linux g++ and Windows clang-cl. After the raw topology and auto-fan correction, the affected fixture passed on Windows clang-cl; the final revision was not rerun on Linux. Its independent PM4 encoder/decoder compares full register/mirror state for 134 masks, plus snapshot ownership, repeated consumption, bounds, rollover rejection and 16/32-bit indexed-quad fields. These CPU checks do not validate GPU completion, the entire game, or an FPS improvement.

Runtime A/B results are recorded below only after actual execution. A lower number of decoded packets alone is not evidence of lower CPU frame time. The one-second frame logger measures wall intervals; the title probe records OS user+kernel CPU separately and excludes screenshot readback from its sample.

## Reproduce a bounded title probe

After regenerating PPC sources and building this branch, run `tools/perf/run_native_title.py` twice with the same build and baseline. Use separate new outputs and `--mode off` / `--mode all`. The driver requires Windows and installed `psutil`; it installs nothing. It copies settings, profile, saves and shader packs, applies identical 720p/60 FPS/SR-off/FG-off settings, remains muted and hidden, sends no input, and closes only its own process. Baseline metadata is compared afterward. Use `--backend vulkan` for a separate backend comparison.

```powershell
python tools/perf/run_native_title.py --build C:/candidate --baseline D:/installed-game --game D:/installed-game/game/disc1 --output C:/evidence/title-off --mode off
python tools/perf/run_native_title.py --build C:/candidate --baseline D:/installed-game --game D:/installed-game/game/disc1 --output C:/evidence/title-on --mode all
```

`native commands:` log receipts distinguish native register blocks, register words, indexed draws, auto-index fans, predicated skips, title shader-pair hits and equivalent PM4 packet/word counts. `native_words` includes the private record headers. No byte reduction is implied for every register mask. CPU measurements use OS thread times; completed-frame boundaries are sampled from one-second receipts, so their ratio is a bounded-window estimate. Separate-process title images can have different cloud animation phases; the probe explicitly does not claim same-input image replay.

## Runtime result

The final same-executable D3D12 title pair completed normally, but **the performance go gate did not pass**. Each run used 720p, a 60 FPS cap, SR/FG off, shader prebuild skipped, hidden/muted execution and no input. Screenshot readback was outside the CPU sample. Both processes exited 0 after normal owner cleanup, without forced termination; baseline metadata was unchanged and neither runtime log contained an error-severity record.

| Final bounded sample | Original PM4 (`off`) | Native commands (`all`) |
|---|---:|---:|
| Logged completed frames | 906 | 905 |
| Sample wall time | 15.126 s | 15.137 s |
| CmdProc OS CPU | 1062.500 ms | 1312.500 ms |
| CmdProc CPU / logged frame | 1.172737 ms | 1.450276 ms |
| Whole-process OS CPU | 2468.750 ms | 2890.625 ms |

The CmdProc ratio was **23.7% higher** with the bypass in this pair. It is an approximate windowed ratio, not a per-frame latency distribution or a general hardware result. The whole-process total also rose. No measured GPU-duration result exists. The original PM4 route remains the default, and this evidence does not authorize expansion or default enablement.

The final enabled receipt at swap 1560 consumed 355,197 native register blocks, 22,537 indexed quads and 1,486 auto fans; all 1,486 observed auto fans matched the selected title shader pair. In the last 120-swap interval, the paths displaced 440 PM4 packets per frame. This is actual command-consumption evidence, not a speedup. Native records used 2188 dwords per frame versus 1684 for the displaced PM4 representation, an increase of 29.9%. Extra traffic is a possible cost, not an established cause of the CPU regression.

The final screenshots show the title text and cloud background on both paths without an obvious new defect. They were taken in separate processes at different animation phases; no identical-input image, exact-pixel, resize, fault-injection or complete resource-lifetime qualification is claimed. Normal exit and bounded rendering are the synchronization/lifetime evidence available here.

Raw local evidence is retained under `out/native-pm4/title-final-off/` and `out/native-pm4/title-final-on/`. The [sanitized result record](native-command-bypass-results.json) retains the numbers and limits. An earlier startup attempt exhausted its warmup bound during shader prebuild and exited normally. The older bulk-only pair predates the corrected draw hooks and is not used to assert a final gain. The retained final summaries have unique CmdProc/process CPU values; unnamed or duplicated thread labels may overwrite each other, so their thread dictionary must not be summed. The driver now aggregates future duplicate labels without altering these retained measurements.
