# Native command bypass: first runtime implementation

Source baseline: `2ce27e35428d85abde20afb5b6b2c59052810b13`, with the architecture research from `3a95001`.

## Implemented scope

`LO_NATIVE_COMMANDS=1` (or `all`) enables two SDK interception points. `registers` enables only the bulk register writer. An absent variable, `0`, or an unknown value preserves the original SDK path. This is an experimental opt-in; no settings-menu default changes.

The strong wrapper for `sub_823C1BD8` replaces its dirty-mask PM4 type-0 packet construction with one native bulk-state record. It snapshots the selected source words before publishing the device cursor. Only ordinary registers `0x2000..0x5002` are eligible; scratch/status/coherence and host-private control registers retain their original side effects. The consumer copies contiguous runs into the register bank and guest-endian MMIO mirror without per-word register dispatch.

The generated `NativeIndexedQuad` hook at `0x823CD44C` intercepts the ordinary indexed-UP branch of `sub_823CD050`, after state setup and geometry allocation. It supports exactly six indices and triangle-list primitive 5. Successful emission jumps to `0x823CD52C`, retaining the original vertex/index copies, resource bookkeeping and deferred device-cursor publication. Both 16-bit and 32-bit index variants retain their DMA fields. The consumer applies the original bin predicate before changing DMA registers and calls the same `ExecuteDraw` implementation used by PM4.

The historical title-cloud shader pair is counted separately at consumption: VS `81217dc973d5dc31`, PS `cd6adb98ab83bf90`, in renderer-byte-FNV space. Geometry selection alone does not establish a scene identity. The older pre-packed-mip-fix capture is not used as a correct-image baseline.

## Ordering and fallback

The records occupy the existing guest command allocation, in ring/IB execution order. The discriminator is guest-endian and the payload is host-endian. Records contain values, never host pointers, side-table handles or one-shot tokens. Replaying an IB therefore replays the same recorded values. The native tag check bypasses the PM4 packet-type/opcode parser for an accepted record; unrelated packets keep their original parser.

Both hooks check mode, live-device identity, submission flags, address arithmetic and available space before changing guest memory. A rejected request runs the original instructions. The register wrapper also checks the capacity required by the original sparse encoding, avoiding a skipped SDK rollover. Insufficient space never triggers a nested flush. Malformed native input fails command processing; it is not replayed after mutation.

The renderer, command-processor thread, GPU queue, fence retirement, upload/descriptor owners, shader snapshots, resolve and swap implementation remain in place. An accepted CPU command is not a GPU-completion receipt. This implementation removes selected PM4 generation/decoding and per-register dispatch, not the entire command processor or Xenos translation layer. It does not remove guest ABI or XEX dependencies.

## Reference

The architectural reference is [reblue at ce0edad](https://github.com/zolaware/reblue/tree/ce0edadbb79007526842dadebf80941bf8ea46ca), particularly `src/gpu/hooks/draw.cpp::DispatchDraw` and `src/gpu/hooks/state.cpp`. Those hooks maintain host render state and directly record backend draws. LO's first step retains the existing ordered stream to interoperate with unconverted producers. No reblue source was copied and its game-specific addresses are not used.

## Build and verification

The canonical PPC generator completed; comparing generated output against the baseline found only `ppc_recomp.12.cpp` changed, with the configured hook and jump. Both new runtime units and the changed generated unit compiled. The isolated development executable then linked after rebuilding current runtime, plume and frame-generation sources; untouched guest units and other dependencies reused local build inputs. This is a development integration build, not a release package.

`LoNativeCommandStreamTest` passed on Linux g++ and Windows clang-cl. Its independent PM4 encoder/decoder compares full register/mirror state for 134 masks, plus snapshot ownership, repeated consumption, bounds, rollover rejection and 16/32-bit indexed-quad fields. These CPU checks do not validate GPU completion, the entire game, or an FPS improvement.

Runtime A/B results are recorded below only after actual execution. A lower number of decoded packets alone is not evidence of lower CPU frame time. The one-second frame logger measures wall intervals; the title probe records OS user+kernel CPU separately and excludes screenshot readback from its sample.

## Reproduce a bounded title probe

After regenerating PPC sources and building this branch, run `tools/perf/run_native_title.py` twice with the same build and baseline. Use separate new outputs and `--mode off` / `--mode all`. The driver requires Windows and installed `psutil`; it installs nothing. It copies settings, profile, saves and shader packs, applies identical 720p/60 FPS/SR-off/FG-off settings, remains muted and hidden, sends no input, and closes only its own process. Baseline metadata is compared afterward. Use `--backend vulkan` for a separate backend comparison.

```powershell
python tools/perf/run_native_title.py --build C:/candidate --baseline D:/installed-game --game D:/installed-game/game/disc1 --output C:/evidence/title-off --mode off
python tools/perf/run_native_title.py --build C:/candidate --baseline D:/installed-game --game D:/installed-game/game/disc1 --output C:/evidence/title-on --mode all
```

`native commands:` log receipts distinguish native register blocks, register words, indexed draws, predicated skips, title shader-pair hits and equivalent PM4 packet/word counts. `native_words` includes the private record headers. No byte reduction is implied for every register mask. CPU measurements use OS thread times; completed-frame boundaries are sampled from one-second receipts, so their ratio is a bounded-window estimate. Separate-process title images can have different cloud animation phases; the probe explicitly does not claim same-input image replay.

## Runtime result

Pending. No same-scene GPU or CPU result is claimed at this implementation checkpoint.
