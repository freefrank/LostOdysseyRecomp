# Uhra City CPU Optimization Results — 2026-09-18

Method, baselines, two delivered optimizations (index conversion cache, BOLT),
and the variance caveats behind every number below.

Companion handoff (environment, build flags, benchmark protocol):
`docs/archive/clang-optimization-and-benchmark-handoff-2026-09-17.md`.
Chinese version of this report: `PERF_CITY_UHRA_RESULTS.zh-CN.md`.

## 1. Methodology (read before quoting numbers)

- Scene: Uhra Residential Area walk, automated via `tools/drive_city.py`
  (menu sequence `s@300,d@600,a@900,a@1200,a@1500,a@1800`, city entry at
  `draws>=800`, 120 s open-loop stick walk, same route both runs).
- Host: Ryzen AI Max+ 395 (Strix Halo), RADV, Bazzite 44, **15 W TDP lock**
  (`ryzenadj -a/-b/-c 15000`), internal resolution controlled via
  `run/settings.ini` (the game reads the run-dir ini, not `~/.config`).
- Metric: per-frame `draw_ms` (CPU wall time of `Renderer::Draw`, summed over
  all draws in the frame, averaged over ~7000 city frames).
- **Variance**: the walk is open-loop, so each run diverges slightly (stuck
  points, camera angle). Repeated identical-binary runs vary **±9 %**
  (12.706 vs 13.784), with all segments (vertex/bind/record) swinging together.
  Baselines are single runs. Treat deltas under ~5 % as directional, not exact.

## 2. Compiler baselines (720p and 1080p, 15 W)

C0 = sandybridge/O2 baseline, C4 = znver2/O2/ThinLTO/PGO (`game.profdata`).

| | 720p C0 | 720p C4 | 1080p C0 | 1080p C4 |
|---|---|---|---|---|
| draws mean | 1759 | 1760 | ~1760 | ~1760 |
| draw_ms | 9.448 | **8.906 (-5.7 %)** | 13.084 | 13.588 |
| vertex_ms | 1.202 | **0.966 (-19.6 %)** | 1.759 | 1.670 |

At 1080p the frame is GPU-bound: `draw_ms` no longer separates CPU builds.
At 720p the CPU is in the critical path and C4 wins across the board.

## 3. PR05 profile (C4, 1080p, 25 s, 24120 `cycles:u` samples, 0 lost)

- Threads: GPU CmdProc **51 %**, two Guest threads 42 %, rest ~7 %.
- In `DrawImpl`: index conversion `Convert<false,1U>` **5.6 %**, vertex-buffer
  content matching (`EqualSampleBlock64` + `memcmp`) ~3.7 %, register snapshot
  copy 1.1 %, `memmove` 2.3 %.
- Pipeline/RenderTarget `unordered_map` lookups ~3.2 % combined,
  `steady_clock::now` ~0.7 %, unresolved vdso (clock) ~4.8 %.

Conclusion: over half the CPU cycles are the unavoidable PM4→Vulkan
translation tax on the CmdProc thread, but ~10 points are redundant per-draw
work (reconversion, revalidation, lookups) that caching can remove.

## 4. Index conversion cache (`renderer.cpp`, `vertex_cache.h`)

Xbox 360 index buffers are big-endian (16/32-bit); Vulkan needs
little-endian 32-bit, so every indexed draw ran a full byteswap+widen loop.
The cache stores the final post-expansion indices keyed by
`(indexBase, count, primitiveType, wide, endian)` and validated against
`SampledContent` source samples (same guarantee as the vertex cache).

- **v1 failed honestly**: cached every indexed draw in an `ankerl` map with a
  mediocre `is_avalanching` hash and erase+re-emplace on content mismatch.
  Result: +13 % (`probe_simd` 13 %, `erase` 4.8 %) — the lookup cost more
  than the 5.6 % conversion it replaced. Lesson: for small buffers a fixed
  lookup can never beat a tight loop.
- **v2**: cache only `count >= 256` (`IndexCache::kMinCount`), dropped
  `is_avalanching`, content mismatch refreshes the entry **in place**
  (no erase/emplace churn). Profile confirms both pathologies gone;
  `Convert` at ~0 %, 1283 entries, zero evictions, 99.98 % hit rate.
- **PGO retrain required**: the new code discarded the old profile for
  `DrawImpl`/`GetVertexBuffer`; retraining (`c4-gen` → city drive →
  `game-new.profdata` → `c4`) recovered codegen with zero PGO warnings.

Results (15 W):

| | 720p | 1080p |
|---|---|---|
| C4 baseline | 8.906 | 13.588 |
| v2 + fresh PGO | 7.978 / 8.721 (mean 8.35) | 12.505 |
| delta | **-6.2 %** | **-8.0 %** |

Unit tests: `tools/tests/vertex_cache_test.cpp` extended with index-key
participation (prim/width/endian), hit/miss on source mutation, and bounded
churn (3.57 M checks, Windows + Linux pass). No save-game modifications in
any benchmark run.

## 5. BOLT (PR09)

- Toolchain: LLVM 22.1.8 `llvm-bolt` extracted to `~/bin/llvm-bolt-22`
  (branch sampling `-j any,u` works on this kernel; `perf2bolt` needs `perf`
  on PATH via a shim).
- Repo switch `LO_BOLT_READY=ON` adds `emit-relocs` + line tables; kept ON so
  every `c4` rebuild stays BOLT-ready (baseline cost ~neutral).
- Profile: 60 s city walk, 452 K branch samples → `bolt-profile.fdata`
  (3236/70532 functions, 4.6 % coverage; the tool suggests 6x more samples).
  `llvm-bolt` with ext-tsp+hfsort+split: taken branches -18.8 %, 104 MB output.

Results (1080p, 15 W):

| binary | draw_ms |
|---|---|
| relocs control (same code, no BOLT) | 13.368 / 14.13 |
| **BOLT** | **12.436 / 12.509 (±0.6 %)** |

-7 – -9 % vs same-binary control. Bonus: run-to-run variance collapsed
from ±5 % to ±0.6 % (n=2, noted not claimed).

## 6. Reproduction

1. 15 W lock: `sudo ryzenadj -a 15000 -b 15000 -c 15000`; verify with
   `ryzenadj --info`.
2. Resolution: edit `run/settings.ini` (`internal_resolution`), confirm
   `effective=WxH` in the log (the `~/.config` copy is ignored).
3. Benchmark: `python3 tools/drive_city.py --variant c4 --deadline 400`
   from `~/perf/lo` under a `systemd-run --user` unit (ssh exits reap
   foreground processes); read `drive-status.json` + `drive-summary-c4.json`.
4. PGO: build `c4-gen`, train with the city drive, `llvm-profdata merge`,
   replace `game.profdata`, rebuild `c4`.
5. BOLT: sample with `perf record -j any,u`, `perf2bolt`, `llvm-bolt`
   with `ext-tsp+hfsort+split`; run from a directory containing the
   `libdxcompiler.so` symlink next to the executable.

Artifacts (remote `freefrank@psvita`): `~/build/lo/c4-bolt/` (BOLT binary),
`~/bolt-profile.fdata`, `~/bin/llvm-bolt-22/`, `~/perf-city-c4*.data`
(raw perf captures for the PR05/v1/v2 profiles quoted above).
