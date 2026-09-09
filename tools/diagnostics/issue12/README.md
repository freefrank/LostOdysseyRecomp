# Issue #12 diagnostics: stale scene-proxy materials freed by the garbage collector

These are the tools that established the root cause recorded in
[docs/notes/issue12-root-cause.md](../../../docs/notes/issue12-root-cause.md). They are kept so the
same class of bug (a rendering-thread structure holding a raw pointer to a UObject that the game
thread frees) can be reproduced and captured again. Nothing here is part of the shipped runtime
except `LostOdysseyRecomp/cpu/gc_render_flush.cpp`, which is the mitigation itself.

All scripts assume the private triage layout under `out/issue12-triage/` (ignored by git):

```text
out/issue12-triage/
  runtime/official-v0.4.2/LostOdysseyRecomp.exe   frozen official EXE (SHA-256 13f1294b...)
  runtime/checkpoints/flowers-ten/                native 10/10 save (slot user08) + profile
  runtime/run-01/shader-cache/                    reusable shader cache
  runtime/session.py                              the run harness (start/input/teleport/shot/stop)
  bridge/frozen-current-build/, bridge/inputs.json, bridge/compile.json, bridge/current-link-structure.json
  ../issue7-semantics-fix/build/guest-fixed.lib   the audited guest library
```

## Files

| File | Role |
|---|---|
| `issue12_probe.cpp` | Host overlay TU linked into a diagnostic EXE. Hooks the pooled allocator (`sub_82295B08`/`sub_82298990`) to record 0xE0-byte allocations and UObject purge frees with guest back-traces; validates every `(Material, UseMaterialIndex)` record of every LOD before `FSkeletalMeshSceneProxy::DrawDynamicElements` (`sub_823CB350`) and dumps alloc/free history, FName-resolved names and the owning proxy when a record points at freed memory; logs engine events (`CollectGarbage`, `IncrementalPurgeGarbage`, `UMaterialInstance::BeginDestroy` with resource/fence state, watched fence execution, `SetSkeletalMesh`, reattach contexts, `CreateSceneProxy`, proxy destructor, `FlushRenderingCommands`). Environment: `LO_ISSUE12_PROBE_FILE` (output, enables the probe), `LO_ISSUE12_SKIP_STALE=1` (skip a stale draw instead of crashing), `LO_ISSUE12_RT_DELAY_US=N` (busy-wait N µs per skeletal proxy draw on the rendering thread to widen the race). |
| `gc_render_flush.cpp` | The mitigation (same file as `LostOdysseyRecomp/cpu/gc_render_flush.cpp`). Built with `-DISSUE12_EXTERNAL_GC_HOOKS` it chains into the probe's GC event hooks for validation builds. |
| `build_probe.py` | Compiles the overlay with the frozen bridge compile flags and links it with the frozen host objects and the audited guest library. Variants: `default` (0.5.1 host objects), `nopoll` (drops `cpu/poll_wait.cpp.obj` to mimic v0.4.2 polling), `gcflush` (nopoll + mitigation). Output under `out/issue12-triage/probe-build/bin[-variant]/` with `artifact.json`. |
| `patch_probe_r2.py` | The r1 → r2 patch that added the engine event hooks (kept for provenance; `issue12_probe.cpp` already contains it). |
| `session_probe.py` | Copy of the run harness that forwards the probe environment variables and accepts a diagnostic EXE (`--exe`, `--exe-sha256`). |
| `drive_probe_run.py` | Scripted route from process start to Melvi's hand-in dialogue (title → Continue → Last Saved Game → walk → talk); `--step forward-talk` and `--step final-a` for the last two inputs; `--official` uses `session.py` with the frozen official EXE. |
| `capture_execute_null.py` | v1 read-only external debugger (ctypes): attaches to the official EXE, captures the first execute-null with `LR == 0x823CB53C`, continues the exception unhandled and detaches so the game's crash handler still runs. |
| `capture_execute_null_v2.py` … `_v4.py` | v2 adds every thread's host context, the PPCContext found on its host stack, guest back-chains, FName resolution through `FName::Names` (`0x833690D0`) and the proxy owner/mesh; v3 triggers on any first-chance access violation whose thread has `LR == 0x823CB53C`; v4 adds the owning component's `Materials` arrays, attach flags and the GC callback tables. |

## Reproducing

1. Build the diagnostic EXE: `python build_probe.py nopoll` (or `gcflush`).
2. From `out/issue12-triage/runtime`: `python drive_probe_run.py --run run-NN --exe ../probe-build/bin-nopoll/LostOdysseyRecomp.exe --sha <sha256 from artifact.json> --delay-us 150 --skip-stale 1`, check the screenshot, then `--step final-a`.
3. Read `run-NN/probe.log`: `### STALE MATERIAL` blocks are the use-after-free detections; `EV` lines are the engine timeline.
4. For the official EXE instead: `python drive_probe_run.py --run run-NN --official`, then attach `python capture_execute_null_v4.py --session run-NN/session.json --pid <pid> --output ../debug-probe/capture-run-NN --timeout-seconds 1500`, wait for `armed.json`, and press the final A.

Guest addresses used by the tools (original image): `FName::Names` `0x833690D0`, `GObjObjects` `0x833690F4`, `GIsThreadedRendering` `0x83318040`, purge-required flag `0x83315F40`, render command ring `0x8336A7A4`, `CollectGarbage` `0x8249A568`, `IncrementalPurgeGarbage` `0x822FD0A8`, `FlushRenderingCommands` `0x82485C18`, `UMaterialInstance::BeginDestroy` `0x82702098`, `IsReadyForFinishDestroy` `0x827020F8`, skeletal proxy vtable `0x82002900`, component vtable `0x8200CF08`.
