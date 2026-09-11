# Assembly profiler gameplay captures — 2026-09-11

Diagnostic sampling of the published v0.5.4 runtime. This is validation evidence, not a performance fix, player acceptance or a new Release. Current status: [STATUS](../STATUS.md#optional-assembly-profiler).

## Conditions

| Item | Value |
|---|---|
| Runtime SHA-256 | `3c3b4073f1b7abbcce38747dc08763335ac019edf560df849177176d0399f949` (published v0.5.4) |
| Profiler SHA-256 | `d6326a19c51e97ee1338364a98f36a7f2cb29aeb537c82f6b100bc3104b9d0f1` |
| Working copies | Isolated; original install save timestamps unchanged |
| Audio / input | `LO_AUDIO_MUTE=1`; `LO_TEST_INPUT_FILE` only |
| PDB | No matching game PDB beside the EXE; all EXE samples unresolved |
| Method | `wall_clock_all_threads_suspend_context`, 15 s, 10 ms interval |

## Session 1 — `out/asm-profiler/play-2026-09-11/`

Loaded user00; walking `xenon_scr.fpd`.

- 15.036 s; 5,092 samples; 0 failed; 38 threads
- Hottest OS CPU TID 52396: 8.11 s; that thread EXE 38.8% + `NtWaitForSingleObject` 36.6% + AMD/D3D12
- All-thread wall share dominated by ntdll waits; EXE about 2.18%
- About 36 fps, 1,700–2,300 draws/frame, frontbuffer 1280×720
- Sampling dipped to 15.8 fps (suspend perturbation)
- Capture SHA-256 `2cd7afff88e8e8dee61bb0aa0326e7e06d0bf2ad69aa8ef7da37dfff96581631`

## Session 2 — `out/asm-profiler/play-2026-09-11-city/`

Loaded second install save user01 (02:15 Lv.10); city walk `xenon_scr.fpd`.

- 15.017 s; 10,736 samples; 0 failed; 44 threads
- Hottest TID 5092: 7.27 s; EXE 35.2% + `NtWaitForSingleObject` 39.8% + amdxc64/D3D12
- After sampling: about 43–44 fps, about 1,913 draws/frame, 1280×720
- Same bottleneck picture as session 1; no new function-level hotspot without PDB
- Capture SHA-256 `f7545829ee7897d8bfbbe0c7303a30b50cf31ecddb70647c08021d76026f7f45`

## Limits

Wall-clock all-thread suspend snapshots include waits. There are no stacks, ETW, cycles or GPU pass timing. Sampling perturbs frame rate. Unresolved EXE RIPs cannot be mapped to functions or guest PCs. This does not authorize a performance change, player acceptance or a new Release.
