# Issue #12 root cause: garbage-collected materials drawn by a stale scene proxy

**Status (2026-09-09):** root cause established from two exception-time captures on the unmodified
official v0.4.2 executable plus an instrumented diagnostic build; the merged host-side mitigation is
validated below only in that diagnostic build (run-13 reproduces the race in an instrumented build, run-14 shows the mitigation closes it). Current-source production validation remains pending. This note supersedes the open questions in the
[investigation log](issue12-funeral-crash.md) and the [earlier Claude handoff](issue12-claude-handoff.md).
All guest addresses refer to the original XEX image (base `0x82000000`); host tooling lives in
`tools/diagnostics/issue12/` (see [README](../../tools/diagnostics/issue12/README.md)).

## One-paragraph summary

At the funeral flower hand-in the story VM swaps the player pawn's skeletal mesh
(`fpPawn_0 / SkeletalMeshComponent_0`: `pc_050a0_m` → `pc_000a0_m`) and then triggers a full
garbage collection in the same tick sequence. The old per-slot `MaterialInstanceConstant`
objects (`MaterialInstanceConstant_2..12`, Outer = that component) become unreachable and are
freed synchronously by `UObject::CollectGarbage`, because their `Resources[0..1]` are already
NULL, so `UMaterialInstance::BeginDestroy` enqueues no render fence and
`IsReadyForFinishDestroy` returns TRUE at once. The rendering thread, however, is still
executing the *previous* frame's `FDrawSceneCommand`, whose scene still contains the pawn's old
`FSkeletalMeshSceneProxy` (the `RemovePrimitive` command queued by the mesh swap sits behind that
draw in the render command ring). `DrawDynamicElements` (`0x823CB350`) reads
`Material->vtable[+0x124]` (`GetRenderProxy`) through the freed pool block: the allocator's
free-list header (`next`, `1`) is taken as a vtable, the slot read lands in another freed block
and yields 0 (`bctrl` to address 0, "execute address 0"), or garbage (read fault variant seen in
run-07). Nothing in the guest protects this window; on the console the render thread is simply
fast enough to finish the previous frame before the purge. In the recompiled runtime the render
thread lags (~64 KB of unprocessed commands at the fault), so v0.4.2 crashes deterministically.
Builds from the current 0.5.x host tree do not crash only because the timing shifted; the race
is still present.

## Evidence chain

| # | Evidence | Where |
|---|---|---|
| 1 | Fault site: `sub_823CB350` = `FSkeletalMeshSceneProxy::DrawDynamicElements(PDI, View, DPGIndex)`; `r22=[proxy+0x134]` = per-LOD `TArray<FSectionElementInfo{Material, UseMaterialIndex}>`; call `Material->vt[+0x124]`. | `out/issue12-triage/translation/crash-analysis/`, generated `ppc_recomp.12.cpp` |
| 2 | Proxy vtable `0x82002900`, constructed by `sub_82559980` from `sub_8255B620` (component vtable `0x8200CF08` slot `+0x1B4`, CreateSceneProxy); materials copied from `Component->GetMaterial(i)` (`sub_8258A958`: component `Materials` at `+624`, else `SkeletalMesh->Materials` at `+88`). | static lens (workflow journal), verified against `image_disc1.bin` |
| 3 | Freed object identity (official EXE, run-09/run-10 debugger captures): `MaterialInstanceConstant MaterialInstanceConstant_8`, Outer `SkeletalMeshComponent SkeletalMeshComponent_0` (Outer chain `fpPawn_0` → `PersistentLevel`), Parent `Material SHADER_P0N0P0P0_PO_0_1N_CC__0_3_____` in package `chrpcMaterial`; `ObjectFlags = 0x0000400200138020` = `RF_Transient | RF_Unreachable | RF_BeginDestroyed | RF_FinishDestroyed | RF_InitializedProps | debug bits`; `Resources[0..1]` (+0xC8/+0xCC) = NULL. All 13 records of both LODs point at `MaterialInstanceConstant_2..12` in the same state. | `out/issue12-triage/debug-probe/capture-run09/capture.json`, `capture-run10/capture.json` (`v2`/`v4` sections) |
| 4 | At the fault the component is alive and attached (`+0x50 = 0x80000000`, no deferred-reattach bit), its `SkeletalMesh` is already `pc_000a0_m` and its `Materials` array holds 22 fresh MICs (`MaterialInstanceConstant_281..302`); the proxy being drawn still carries mesh `pc_050a0_m` and the old MICs. | run-10 `v4` section |
| 5 | Thread roles at the fault: crash thread = UE3 rendering thread (`RenderingThreadMain 0x824856A0` → `FDrawSceneCommand::Execute 0x825B2AC8` → `FSceneRenderer::Render 0x825B2A20` → `0x823BAB50` → `0x823D0500` → `DrawDynamicElements`); game thread = inside `UObject::CollectGarbage` (`sub_8249A568`, after its purge) waiting in `FlushRenderingCommands` (`sub_82485AF8` → `sub_82485C18` → fence wait `sub_82322478`). Render ring `0x8336A7A4`: ~`0xFC30` bytes queued behind the draw being executed. | run-09/run-10 `threads` and `render_ring` |
| 5b | GC trigger: `UWorld::Tick` (`sub_82299818`, called as `GWorld->Tick(LEVELTICK_All, dt)` from `UGameEngine::Tick` → `sub_82290DD8`) ends with a deferred full-GC request check: byte `0x83318748` set by `sub_825B32E8(delay)` (18 call sites; the scene-change/world-update function `sub_8231EE68` called from `UGameEngine::Tick` passes delay 0) → `sub_82820478` thunk → `CollectGarbage(RF_Native, bPerformFullPurge=TRUE)` at `0x8229A5B0`. Of the 11 direct callers of `CollectGarbage`, only the 60-second periodic purge (`sub_82523870`) passes FALSE; every scene change is a synchronous full purge. A static call-graph search (2,342 functions, depth 12) finds no rendering flush anywhere before this call in the tick. | opus static lens, verified addresses |
| 6 | `UObject::CollectGarbage` (`sub_8249A568`) order: pre-GC callbacks (`0x8237CC40` async-loading flush, `0x825B51D8`) → mark (`sub_82499518`) → `BeginDestroy` on every unreachable object → `IncrementalPurgeGarbage(FALSE)` (`sub_822FD0A8`) → **only then** `sub_82485AF8` (flush + deferred RHI release). There is no rendering flush before the purge. | generated `ppc_recomp.25.cpp`, listing in `out/issue12-triage/notes` |
| 7 | `IncrementalPurgeGarbage` phase 1 spins on `Object->vt[+36]` (`IsReadyForFinishDestroy`) per unreachable object, then `ConditionalFinishDestroy` (`sub_823F89F8`); phase 2 calls the deleting destructor (`vt[+8](1)`). | `ppc_recomp.*` listing of `sub_822FD0A8` |
| 8 | `UMaterialInstance::BeginDestroy` (`sub_82702098`) begins a fence (`sub_822D6BB0`) only for non-NULL `Resources[i]` (+200/+204); `IsReadyForFinishDestroy` (`sub_827020F8`) checks `Resources[i]->fence(+148) == 0`. With `Resources` NULL both are no-ops. | listings of `0x82702098`, `0x827020F8` |
| 9 | Instrumented run-11 (host hooks, no delay): `SetSkeletalMesh(comp=SkeletalMeshComponent_0, pc_000a0_m)` from the story VM (`0x829FDBE0` dispatcher chain) at swap 4866 → old proxy `074d44c0` destroyed on the render thread at swap 4867 → `CollectGarbage(fullpurge=1)` at swap 4868 from `sub_82299818` → `MIC_BeginDestroy` for `MaterialInstanceConstant_2..12` with `res0=res1=NULL, fence=0` → freed. No crash because the render thread had already processed the removal. | `out/issue12-triage/runtime/run-11/probe.log` |
| 10 | Instrumented run-12 (same build, uniform 150 µs delay per skeletal proxy draw): still no reproduction; the old proxy was removed 36 ms after the swap and the purge came 93 ms after it. The window is ~100 ms, so a per-frame slowdown of a few ms cannot reach it. | `run-12/probe.log` |
| 11 | Instrumented run-13 (same build, 20 ms delay per skeletal proxy draw armed for 3 s at the hand-in `SetSkeletalMesh`): deterministic reproduction. `SetSkeletalMesh` at t=185.854 → `CollectGarbage(fullpurge=1)` at 185.933 → `MIC_BeginDestroy _2.._12` (`res0=res1=NULL`) → purge frees them at 185.944 → **19.5 ms later** the rendering thread draws the old proxy `0455cf80` with all 13 records pointing at the freed blocks (`### STALE MATERIAL`, draw skipped by the probe) → the proxy's `RemovePrimitive` is only processed at 191.07, inside the game thread's post-purge flush. | `run-13/probe.log` |

## Why the recompiled runtime differs from the console

The guest logic is identical; only the relative speed of the two threads differs. UE3 lets the
game thread run one frame ahead of the rendering thread. The window between "old proxy still in
the scene on the rendering thread" and "old materials freed on the game thread" is the time the
rendering thread needs to finish the previous frame's draw and execute the queued
`RemovePrimitive`. On the 360 that is a few milliseconds; in the recompiled runtime the render
thread pays host translation and D3D12 costs per draw (1,200–1,800 draws per frame at this
scene), and at the hand-in it also has to bring up the new character's materials, so it was
still inside the previous frame when the purge ran (run-09/run-10). The 0.5.x host tree
(faster startup shader path, poll-wait changes) happened to move the render thread ahead of the
purge in runs 05/06/11; that is luck, not a fix.

Host divergence candidates were reviewed and excluded as causes: the fence counter uses the
`lwarx/stwcx` CAS emulation (cannot lose a count), the guest fence wait re-reads memory each
iteration, `poll_wait` only adds latency, the render command ring is FIFO, and no host override
sits on the material/GC path. The PPC translation of the fault sequence and of the allocator was
verified instruction by instruction in the earlier reports.

## Mitigation

`LostOdysseyRecomp/cpu/gc_render_flush.cpp` hooks the two guest purge entry points and calls the
guest `FlushRenderingCommands` (`sub_82485C18`) first, only while `GIsThreadedRendering`
(`0x83318040`) is set and (for the incremental entry) only when a purge is pending
(`0x83315F40`). This restores the invariant that later UE3 versions added to
`UObject::CollectGarbage` for exactly this class of bug: no UObject is freed while the rendering
thread may still be drawing the previous frame. It does not change what the game frees or when
the proxy is rebuilt; it only orders the purge after the in-flight frame. Opt out with
`LO_GC_RENDER_FLUSH=0`.

Cost: the hook can add a rendering-thread drain at full-GC entry and another at the internal purge
entry; incremental purge may drain on each pending tick. Run-14 measured a 0.77 s wait under the
artificial delay. Normal current-source production cost remains unmeasured.

## Validation

- run-13 (`bin-nopoll`, armed 20 ms delay): 5 stale-material detections on proxy `0455cf80`
  (the pre-swap proxy of `SkeletalMeshComponent_0`), every record freed 19.3–19.6 ms before the
  draw, freeing caller chain `IncrementalPurgeGarbage` ← `CollectGarbage` ← `UWorld::Tick`
  (`lr=0x8229A898` in the purge loop). Without the probe's skip this is the Issue #12 crash.
- run-14 (`bin-gcflush`, identical armed delay): the hook's flush before `CollectGarbage`
  (`FlushRenderingCommands_enter` at t=182.994, exit at 183.763) waited for the rendering thread
  to finish the frame in flight and to process the old proxy's removal (`ProxyDestructor 074b44c0`
  at 183.753) **before** the purge freed `MaterialInstanceConstant_2..12` (BeginDestroy at
  183.767, purge 183.768–183.792). Result: 0 stale-material detections, no crash, the cutscene
  ("All right, Mr. Kaim, please gather the branches for the torch.") rendered normally. The
  runtime log shows the hook firing only at real collections (`gc render flush before
  CollectGarbage/IncrementalPurgeGarbage`, 8 lines in a 4-minute run).
- Not yet done: a production build of the current tree with `gc_render_flush.cpp` compiled in
  (the validation linked the file with the frozen 0.5.1 host objects), a run on the official
  v0.4.2 timing without the artificial delay, and a broader gameplay pass to confirm the extra
  flush has no visible cost (it only runs when a purge actually happens).

## How to look for similar bugs

The class is "render-thread structure holds a raw pointer to a UObject that the game thread can
free without a fence". Symptoms: execute-null or read faults on the rendering thread inside
`DrawDynamicElements`/`DrawStaticElements` right after scene changes (mesh/material swaps, level
transitions), with the object bytes showing the pool free-list header. Tools kept for this:

- `tools/diagnostics/issue12/issue12_probe.cpp`: host hooks that record 0xE0 allocations, purge
  frees, GC/BeginDestroy/fence/reattach events, validate every proxy material before
  `DrawDynamicElements`, optionally skip a stale draw (`LO_ISSUE12_SKIP_STALE=1`) and optionally
  slow the rendering thread (`LO_ISSUE12_RT_DELAY_US`) to widen races.
- `tools/diagnostics/issue12/capture_execute_null_v4.py`: read-only external debugger that,
  at the first matching access violation, dumps every thread's guest back-chain, resolves FNames
  through `FName::Names` (`0x833690D0`), and reads the owning component's material arrays.
- `tools/diagnostics/issue12/build_probe.py` and `drive_probe_run.py`: build/link recipe and
  the scripted route to the hand-in (Continue → Last Saved Game → walk → talk).

## Run index

| Run | Executable | Result |
|---|---|---|
| run-03/04 | official v0.4.2 | crash (execute-null), first object capture |
| run-05 | 0.5.1 host + probe r1 | no crash, no stale material |
| run-06 | same, `poll_wait` removed | no crash |
| run-07 | official v0.4.2 | crash, read-AV variant at the same call |
| run-09/10 | official v0.4.2 + debugger v3/v4 | crash captured with names, threads, component state |
| run-11 | probe r2, no delay | no crash; full event timeline of swap → GC |
| run-12 | probe r2, uniform 150 µs render delay | no crash, no stale material (window not reached) |
| run-13 | probe r3, 20 ms delay armed at the mesh swap | use-after-free reproduced: 5 stale draws of the old proxy, materials freed 19.5 ms earlier |
| run-14 | probe r3 + `gc_render_flush`, same armed delay | no stale material, no crash; flush before GC waited 0.77 s for the in-flight frame and the old proxy's removal |
