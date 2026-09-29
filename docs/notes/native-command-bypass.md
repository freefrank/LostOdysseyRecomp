# Native command bypass: first runtime implementation

Next development handoff: [native graphics frontend and PM4 removal (Chinese)](native-renderer-pm4-removal-handoff.zh-CN.md). It carries the Uhra 120 FPS evidence, the proposed earlier mesh/state interception, and the ordering, replay and acceptance contracts for the replacement architecture.

Renderer data-path follow-up: [endian conversion and index-cache reuse](renderer-endian-reuse.md). It removes one index-hit CPU copy but has no live-game performance result and does not replace the historical title CPU comparison below.

The title pair below is retained as historical probe evidence. The current performance decision uses the ordinary Uhra city-plaza comparisons recorded in [native-command-uhra-results.json](native-command-uhra-results.json). The 120 FPS target did not cap the measured rate, making that run more useful for comparing throughput; a separate `LO_GPU_STATS=1` four-run comparison is retained for diagnostic coverage because that instrumentation changes renderer CPU timing.

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

For the current scene workload, use the same driver with `--scene uhra`. It copies the same isolated baseline for each mode and sends the fixed Continue/Uhra input. Ordinary scene entry requires the `xenon_scr.fpd` load and 3,300 completed frames; optional `--scene-stats` also requires the diagnostic heartbeat. Ordinary CPU timing uses `psutil` OS thread snapshots with the denominator from `LO_FRAME_TIMING` completed-frame receipts. `LO_GPU_STATS=1` changes renderer CPU timing and belongs only to the diagnostic comparison. Screenshot capture occurs outside the timed sample. Pass `--fps 120` to run the higher native target.

```powershell
python tools/perf/run_native_title.py --scene uhra --build C:/candidate --baseline C:/baseline --game D:/game/disc1 --output C:/evidence/uhra-off --mode off
python tools/perf/run_native_title.py --scene uhra --build C:/candidate --baseline C:/baseline --game D:/game/disc1 --output C:/evidence/uhra-all --mode all
```

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

## Runtime result: Uhra city-plaza scene

The ordinary scene comparison used the same development executable (SHA-256 `cc04ae26c1a610dd92c6038c02f73b64b268ef65508c250824a8d23b988d3b83`) in four interleaved D3D12 runs: `off-1`, `all-1`, `all-2`, and `off-2`. Its link inputs include the branch objects, but its embedded `0.7.11/45fbeb2-dirty` version stamp is stale; this is not a clean HEAD build claim. Each run used 1280×720, a 60 FPS cap, SR and FG off, 20 seconds, the isolated `issue70-runtime/baseline`, and the Disc 1 game data. The driver entered the same Uhra city plaza through the fixed Continue input. CPU timing came from `psutil` OS thread snapshots divided by `LO_FRAME_TIMING` completed-frame receipts. Screenshots show the same plaza and were captured outside the timed window.

| Interleaved Uhra sample | Original PM4 (`off`) | Native commands (`all`) |
|---|---:|---:|
| Mean CmdProc CPU / frame | 7.551031 ms | 7.157140 ms |
| Mean whole-process CPU / frame | 14.410547 ms | 14.074803 ms |
| CmdProc change | — | -5.216% |
| Whole-process change | — | -2.330% |

All four ordinary runs exited with code 0, were not force-stopped, preserved the baseline, and show the same Uhra plaza without an obvious new defect; animation phases differ. The separate `LO_GPU_STATS=1` diagnostic runs retained the draw-density and command-consumption checks, but that instrumentation changes renderer CPU timing and is not used as the ordinary performance basis.

The ordinary Uhra point estimate is lower with `all`, but the single-run ranges overlap. This is one scene and four samples; the 60 FPS cap does not establish stable throughput gain, GPU time, or formal performance acceptance. No same-input pixel comparison or complete playthrough was performed. The `e2a600b` index-cache reuse change was present in both modes, so this A/B cannot attribute its effect separately. Keep the experimental path default-off and do not expand it on this evidence. Ordinary-run summaries are retained under `out/native-pm4/uhra-lite-{off-1,all-1,all-2,off-2}/`; diagnostic summaries are under `out/native-pm4/uhra-20260929-{off-1,all-1,all-2,off-2}/`.

### 120 FPS target comparison

The second ordinary four-run comparison used the same executable, scene, resolution, backend, SR/FG settings and 20-second duration with a 120 FPS target. The measured rate was about 105 FPS in both modes, so the target did not cap the run. Mean results were:

| 120 FPS target sample | Original PM4 (`off`) | Native commands (`all`) |
|---|---:|---:|
| Mean actual rate | 105.1506 FPS | 104.7048 FPS |
| Mean CmdProc CPU / frame | 7.786641 ms | 7.801491 ms |
| Mean whole-process CPU / frame | 13.974594 ms | 14.501339 ms |
| Mean Guest Main / frame | 2.226368 ms | 2.576964 ms |
| `all` change | — | -0.4240% FPS, +0.1907% CmdProc, +3.7693% process CPU |

All four runs exited normally, preserved the baseline, recorded no runtime errors, and showed the same Uhra plaza view with different NPC animation phases. This higher target did not measure a throughput or CmdProc advantage. The process CPU increase is an observation from this run and is not attributed to a mechanism. The 120 FPS comparison also does not establish formal performance acceptance or a general hardware result. Raw evidence is retained under `out/native-pm4/uhra-120-{off-1,all-1,all-2,off-2}/`.
