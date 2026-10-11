# City 60 FPS handoff — 2026-09-12

> **Historical checkpoint.** Version, issue, process and pending-work statements below describe their original date. Recheck current delivery and acceptance in [STATUS](../STATUS.md) and [ROADMAP](../ROADMAP.md).

Diagnostic work, not player acceptance. The current source and release target
are **0.5.6**; the measured executable remains from source version **0.5.4**.
The 0.5.6 release was published on 2026-09-13; the measurements below retain
their original source and validation identity.

Session: `ses_f6c1d11f0ffeuJGmPTH4jjhMvK`. Goal: city exploration 60 FPS via CPU efficiency only (no GPU/quality/resolution fallback). S4 gate: mean + `>16.67 ms` ratio + 1% low on the same Hidden city path.

## User requests (verbatim)

- 获取上一次的开发进度
- 想办法达到fps60。提示，现在fps低完全是因为cpu效率问题。不存在任何硬件性能限制或gpu不足的问题。你可以委派metis去做思考和研究。
- 尽你所能，自主完成目标，不要问我，我只需要结果
- 为什么又停了。继续
- 先commit
- ok,收尾，commit，总结，写一个给别的agent的handoff，今天就到这里了

## What landed on `main` (historical checkpoint)

Ahead of `github/main` by 9 after the wrap-up commit below (was 8 before `frame_timing`).

Earlier GPU-ring (already on `main` before this session): `b91d279` 2-slot ring / descriptor 1800; `ed90fe9` dummy banks / skip constants; `54b8619` GpuPoll 50µs; DrawImpl timers; `e58fb88` city classifier; item 1 `daea69c`+`5025e8b` 128MB halves (regressed 1% low); item 3 locate-only timers.

This session:

| Commit | What |
|---|---|
| `1c6f237` | Vertex cache hits any slot on content match; wrap only when both 128MB halves are full; 1 GiB arena (`kVertexArenaSize = 1024ull << 20`); tests in `tools/tests/render_arena_policy_test.cpp` (35/35) |
| `bae54b6` | `PreparePresent` records COPY_SOURCE on the still-open swap list; `AcquireResolvedSurface` is lookup-only (no second Flush). Fixes city `gpu_batches` 2.005 → ~1.00 and fence 8.97 → ~0.005 |
| `frame_timing` (this wrap-up) | Snapshot under mutex, then skip the 1s `LOG_INFO` pair when `gpu::render_timing::Enabled()`; still logs outside the lock if only `LO_FRAME_TIMING` |

Historical session constraint (2026-09-12): do **not** commit `AGENTS.md`, `ROADMAP.md`, `ROADMAP.zh-CN.md`, `docs/project/items.json` (pre-existing docs drift), `.omo/`, `.stignore`, `nul`, or dirty submodules. The then-current commit included the synchronized roadmaps and Project records; it excluded the pre-existing `AGENTS.md` edit, `.omo/`, `.stignore`, `nul` and dirty submodules. This paragraph records that checkpoint and does not define the current release authorization.

## City harness (reuse, do not invent)

- Scripts `out/perf-ring/drive-city.ps1` and `out/perf-ring/analyze-city-comparison.py` are version-controlled reproduction tools; their run outputs remain ignored. Hidden, `LO_BACKGROUND=1`, `LO_AUDIO_MUTE=1`, `LO_RENDER_TIMING=1`, `LO_FRAME_TIMING=1`, `LO_GPU_STATS=1`, `LO_AUTO_BUTTONS=s@120,a@240,a@360,a@480,a@700,a@900`, `LO_AUTO_STICK=0,28000,1400,2800`. Isolated `user01`. 28s `city_hold`. city = draws ≥ 800. `original_saves_changed` vs `D:\Games\LostOdysseyRecomp-windows-x64\save`.
- Game: `D:\Games\LostOdysseyRecomp-windows-x64\game\disc1` (`out/perf-ring/run/game-path.txt`).
- Copy RelWithDebInfo exe+pdb from `out/build/windows-clang/LostOdysseyRecomp/` into `out/perf-ring/run/` after each link. ninja then fails copying missing `tools/XenosRecomp/thirdparty/dxc-bin/bin/x64/dxcompiler.dll` — ignore, `run/` already has the DLL.
- Build: git-bash must use `cmd.exe //c "tools\\build_runtime.bat LostOdysseyRecomp"` (`/c` becomes `C:\`).
- Classifier: `tools/perf/classify-city-timing.ps1` (budget 16.67, no fence mean / 1% low — compute from log rows).
- Heartbeat line: `heartbeat: swap #N X.X fps, D draws/frame, ... last file 'xenon_scr.fpd'`.
- Present line: `sample_valid=true|false frame_ms=N|unknown ...`.
- Script now: FileStream last-64KB tail; **no tail during `city_hold`**; shot marks 200–1000 + one city-entry shot (not 1800/2200). `last_swap` in `drive-status.json` stays stale after hold starts.

## Previous pre-SIMD Hidden city numbers

Log `out/perf-ring/run/logs/runtime-1789233141663996.log` (frame_timing skip, 1 GiB arena, present-merge). 58.4s, `original_saves_changed=false`, city 1743 / menu 851.

| Metric | Value |
|---|---|
| fence_wait mean | 0.006 |
| gpu_batches | 1.001 |
| nested_flush max | 0 |
| splits / arena_splits | 0 |
| draw_ms | 10.264 / p99 13.144 / max 48.983 |
| draw >16.67 | 4/1743 = 0.23% (3 vertex, 1 bind) |
| heartbeat xenon_scr n=29 | min 42.1 mean 58.0 max 59.8 |
| stable present 1% low | 51.09 fps (max frame 427.9 ms) |
| all-city 1% low | 44.04 fps |

Typical stable: draw ~9.4–10.3 ms, present p50 ~16.70 (60 cap). `>16.67` on present is mostly pacer 16.67–17.3, not a mean miss. S4 fails on **1% low / hitches**.

Four draw over-budget frames: load-in bind/vertex (1279/1335/1466) plus mid-hold **vertex 40.83 ms** at frame 1983 (present 1984 = 54.53 ms).

428 ms stall at present 2548: `flush_ms=413.25`, draw of previous frame 9.59 ms. Swap thread blocked inside `PreparePresent`+`gpu::Flush` (per-frame `LOG_INFO`). Log lived on the Syncthing tree. Same class as earlier 205/263 ms stalls. **Historical hypothesis: possible measurement artifact from log placement; the later redirected-log runs did not reproduce this 200–400 ms class, but that does not prove Syncthing was the root cause.**

## Current bounded-cache result

The measured vertex-cache diagnosis found a 41.8241 ms insertion during one
262,144-to-524,288 bucket rehash; all 495 endian copies in that frame took
0.0254 ms. The bounded cache reserves 65,536 metadata entries and evicts from
at most 16 rotating candidates when full. `LoVertexCacheTest` passed 3,569,548
checks once. Three single Hidden Uhra captures used the same 1280x720 D3D12,
AA=3, 60-cap, muted background setup; the final same-EXE run moved only driver
input and screenshot-request files to TEMP. The final all-city sample had
1,790 frames at 59.651 FPS mean, 45.989 FPS 1% low and 43.0117 ms worst
accepted-present. The fixed 1600–2800 window reached 59.918 FPS mean and
54.495 FPS 1% low, with draw max 13.162 ms, vertex max 2.2981 ms and zero
rehashes. The final executable is
`out/perf-ring/run/LostOdysseyRecomp.exe`, SHA-256
`9D9460248FEB72AC7239ABD40AC1DA6619847F176CF4AA38AD6F725C6923852B`, with
the matching PDB alongside it. Evidence: `out/perf-ring/vertex-stage/REPORT.md`,
`comparison.json` and `identity.json`.

The implementation source is recorded in code commit
`ae287f2a43a73c6f6bea61c40822c37f40afe052`. The measured executable was built
from the same runtime source before that commit; recording the commit did not
trigger a rebuild or rerun, so the executable hash and performance figures are
unchanged.

The bounded run's 412.9283 ms previous-swap post-present sample fell to
0.4193 ms after moving the polled controls to TEMP; this does not establish a
Syncthing filesystem or scheduler root cause. The requested mean threshold is
met on this route, but this remains near-60 route evidence, not a locked 60 FPS
result, strict S4 pass, whole-game validation or player acceptance.

## Historical SIMD Hidden city numbers

The earlier SIMD-only build was local and unpublished. Its EXE SHA-256 is
`AFCC4BE89B42C033FB4041185E35F33CDDFCF7832FEACE2F6FA8328F10BFA7C2`.
The run used the same Hidden city harness with `LO_LOG_FILE` redirected to
`%TEMP%\lo-city-logs`; it completed in 57.5 s with 1,801 city frames, 884 menu
frames and `original_saves_changed=false`. Settings remained 1280x720,
window_mode=0, backend=0, AA=3 and frame_rate=60. The city summary was draw 8.203 ms
(p95 10.473, p99 11.547, max 49.265), vertex 1.467 ms, fence 0.003 ms,
`gpu_batches` 1.001, and four draw frames over 16.67 ms. The stable 1600–2800
comparison was draw mean 7.838 ms and vertex mean 1.396 ms, versus the same-harness
no-rebuild baseline's 7.766 ms and 1.436 ms. Present mean was 16.779 ms, with
51.457% over 16.67 ms and a 36.539 fps 1% low; the baseline was 16.831 ms,
50.458% and 31.044 fps. Both windows contain 1,201 frames. This is a small stable vertex change, not a
measured removal of the hitch: the SIMD run still reached vertex 41.736 ms and
had a 47.467 ms `flush_ms` stall. Separately, its worst accepted-present
interval was 779.572 ms during load-in, with 778.866 ms in `between_ms`, so that
interval was not a flush. Neither run reproduced the earlier 200–400 ms flush
class. The primary runtime log writes were moved out of Syncthing; other run
inputs, screenshots and status files remained on the tree. This does not
establish the cause of the remaining stalls.

The implementation moves `CopyDwordsSwapped` into `gpu/geometry_prepare.h`,
uses SSSE3 for four dwords at a time for endian modes 1/2/3, retains scalar
tail/fallback behavior and uses `memcpy` for endian 0. The focused fixture
passed 16,685,865 checks, including unaligned offsets and inaccessible-page
tail guards (`out/perf-ring/simd-copy/geometry_prepare_test.log`). This is implementation and diagnostic evidence only; S4 and player acceptance remain separate. The v0.5.6 publication does not change this bounded diagnostic scope. The current source and published release are version
0.5.6; the measured build remains source version 0.5.4. This work is absent
from the published v0.5.4 package. Full raw evidence is in
`out/perf-ring/city-simd-comparison.json` and the two runtime logs named there.

Ring baseline (docs `perf-gpu-ring-compare.md`, pre-item1): heartbeat 57.7 (49–60), fence 1.67, gpu_queue 3.91, gpu_batches 2.00. Present-merge beats that fence; 1% low is still hitch-limited.

## Next attacks (do these first)

1. Extend bounded-cache coverage to additional scenes and longer sessions, measuring eviction churn and accepted-present variability with the same controls and save-integrity checks. The current evidence is one Hidden Uhra route per configuration.
2. Investigate remaining short present variability and the 43.0117 ms all-city worst interval. Keep previous-swap, post-present and CP-idle values as diagnostic components; do not add them to frame time twice or treat the TEMP comparison as proof of a Syncthing root cause.
3. Do not reattribute the resolved vertex hitch to `CopyDwordsSwapped`: the measured cause was one 41.8241 ms map rehash insertion. Keep `LO_VERTEX_TIMING` opt-in and disabled by default. Do not raise `kGpuSlots`, add a wrap wait, or change the GPU arena/offset/slot/wait policy without new evidence.

The requested mean threshold is met on this route, but the strict present ratio,
remaining short stalls and limited scene scope leave strict S4, whole-game
validation and player acceptance open.

## Constraints

- CPU-only. No GPU upscale, resolution drop, quality drop, or hardware-upgrade story.
- Hidden `LO_BACKGROUND=1`. Never steal focus. Report `original_saves_changed`; non-zero invalidates the run.
- Current source and release target: version 0.5.6. The measured source-0.5.4 executable and evidence are retained; this version increment did not trigger a rebuild or rerun. The measurements remain historical diagnostic evidence for the release preparation.
- Do not treat menu FPS or mean-only as S4. Item 3 timers are locate-only (~0.39 ms), not savings.
- `tools/benchmark_city.ps1` does not exist; use `out/perf-ring/drive-city.ps1`.
