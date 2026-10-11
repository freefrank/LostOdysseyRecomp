# Card B city measurement — published v0.5.11 — 2026-09-14

> **Historical checkpoint.** Version, issue, process and pending-work statements below describe their original date. Recheck current delivery and acceptance in [STATUS](../STATUS.md) and [ROADMAP](../ROADMAP.md).

This record is the current Card B profile for vertex-cache stages, shader/pipeline lookup and the B1/B2/B3 implementation gates on the **same published v0.5.11** package as [Card A](cpu-card-a-city-2026-09-14.md). It does not authorize runtime changes, SIMD expansion, another hash mix, `poll_wait` expansion, a version bump, whole-game acceptance, or a new GitHub Release. Historical source-0.5.8 `memcmp` 7% render-self and texture hash chain 84 **must not** replace these numbers.

## Identity

| Field | Value |
| --- | --- |
| Package | GitHub Release [v0.5.11](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.11) |
| ZIP | 44,304,683 bytes, SHA-256 `5de068c4e77c82feb0bbe7cfcf1dacbca3d44aa94bde064f7f59f5ad6944e132` |
| Runtime | `LostOdysseyRecomp.exe` 83,536,384 bytes, SHA-256 `33a460410b7f397187a12ac6984718c1716997936127f94f5dbcb2ef40315283` |
| Source / commit | 0.5.11 / `624729cdb1263b96061b1fa14d4d1c5ba0b50239` |
| Protocol | Hidden 1280×720, Windowed, D3D12, AA3, 60-cap |
| Scene | isolated `user01` city slot; Continue load; never send Down (opens 千年之梦) |
| Env | Card A env plus `LO_VERTEX_TIMING=1` |
| Driver | `out/perf-ring/drive-city.ps1` (worktree copy; isolated `run/save`) |
| Log | `%TEMP%\lo-city-logs\runtime-639250002081609096.log` |
| Local persist | `.omo/cpu-perf/out/perf-ring/{identity.json,card-b-results.json,drive-summary.json}` (gitignored) |

The measured binary is the published Windows package, not a `cpu-perf` RelWithDebInfo rebuild. Player `settings.ini` at `D:\Mihoyo\LostOdysseyRecomp-windows-x64` remained 1920×1080 / 120 and was not used.

## City window

City frames are `draws >= 800`. Classifier `over_budget` was 0%. Isolated `run/save` and the player save tree were both unchanged (`original_saves_changed=false`, `player_saves_changed=false`).

| Count | Value |
| --- | ---: |
| City frames | 1846 |
| Mean draws | 965.9 |
| Menu frames | 891 |
| Last swap / last draws | 1920 / 893 |
| `vertex timing` lines | 3622 (1:1 with render timing; 1846 city vertex frames) |

Published v0.5.11 **does** emit `vertex timing frame=` histograms (`find` / `match` / `erase` / `capture` / `copy` / `insert` plus cache size / buckets / rehashes / evictions).

## Vertex stages (city frames only)

| Stage | mean | p50 | p95 | p99 | max |
| --- | ---: | ---: | ---: | ---: | ---: |
| `find_ms` | 0.0693 | 0.0688 | 0.0886 | 0.101 | 0.361 |
| `match_ms` | 0.1535 | 0.1511 | 0.1965 | 0.229 | 0.3405 |
| `erase_ms` | 0.0098 | 0.011 | 0.0182 | 0.0212 | 0.1047 |
| `capture_ms` | 0.0422 | 0.0411 | 0.0553 | 0.0758 | 0.1446 |
| `copy_ms` | 0.0091 | 0.009 | 0.011 | 0.013 | 0.0637 |
| `insert_ms` | 0.0777 | 0.0726 | 0.1199 | 0.1312 | 0.2035 |
| stage sum | 0.3616 | 0.3594 | 0.4297 | 0.4779 | 0.6323 |

`match_ms` is the largest vertex stage (mean 0.1535 ms, 42% of the stage sum). City `vertex_ms` mean 0.476 includes scheduling around the stage timers. Same-run `render timing` city means: `bind_ms` 0.747, `record_ms` 1.34, `draw_ms` 3.82, `shader_lookup_ms` 0.051, `pipeline_lookup_ms` 0.03, `taa_ms` 0.085, `scene_copy_ms` 0.025.

## Vertex cache

| Metric | Value |
| --- | ---: |
| Calls mean | 1014.4 |
| Uploads mean | 288.7 |
| Bytes mean / max | 168594 / 932120 |
| `cache_after` max | 65536 |
| Buckets | 131072 |
| Rehashes sum | 0 |
| Evictions mean / sum | 175.7 / 324386 |

Evictions are vertex-cache cap turnover, **not** texture hash chains.

## Gates

| Card | Gate | Present | Implement |
| --- | --- | --- | --- |
| B1 remaining non-64B `memcmp` | current-package length histogram + call-site proportions | no | **false** |
| B2 texture hash lookup | v0.5.9+ texture chain-length snapshot + lookup still hot after avalanche | no snapshot; lookup not hot | **false** |
| B3 `poll_wait` | new guest spin evidence | no | **false** |

`LO_VERTEX_TIMING` is wall time of vertex-cache find/match/erase/capture/copy/insert. It is **not** a `memcmp` length histogram. `EqualSampleBlock64` already covers 64-byte blocks. City shader/pipeline lookup means are tiny; vertex-cache rehashes stayed 0 at 131072 buckets. Keep `poll_wait` at 32 polls then yield, `GpuPoll` cap 50 µs.

## Verdict

`exhausted_resource_class = null`.

Card B's current profile exists on published v0.5.11 city:

- Do **not** expand SIMD without a length histogram.
- Do **not** write another texture hash mix.
- Do **not** expand `poll_wait` Kind / cap / OS-sleep.
- Card C's prepare gate is now recorded separately; Card D stays blocked on its own affinity experiment, default off.

This is one Hidden 1280×720 D3D12 city window. It is not 4K, Vulkan, 3C6T-restricted, whole-game, or player acceptance evidence.
