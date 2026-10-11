# Card A city measurement — published v0.5.11 — 2026-09-14

> **Historical checkpoint.** Version, issue, process and pending-work statements below describe their original date. Recheck current delivery and acceptance in [STATUS](../STATUS.md) and [ROADMAP](../ROADMAP.md).

This record is the current Card A baseline for fence / mid-frame Flush / descriptor splits / GPU queue on the **published v0.5.11** package. It does not authorize runtime changes, a version bump, whole-game acceptance, or a new GitHub Release. Historical v0.5.4 city `fence_wait` 15.40 ms and the two-slot diagnostic 1.67 ms **must not** replace these numbers.

## Identity

| Field | Value |
| --- | --- |
| Package | GitHub Release [v0.5.11](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.11) |
| ZIP | 44,304,683 bytes, SHA-256 `5de068c4e77c82feb0bbe7cfcf1dacbca3d44aa94bde064f7f59f5ad6944e132` |
| Runtime | `LostOdysseyRecomp.exe` 83,536,384 bytes, SHA-256 `33a460410b7f397187a12ac6984718c1716997936127f94f5dbcb2ef40315283` |
| Source / commit | 0.5.11 / `624729cdb1263b96061b1fa14d4d1c5ba0b50239` |
| Protocol | Hidden 1280×720, Windowed, D3D12, AA3, 60-cap |
| Scene | isolated `user01` city slot; Continue load; never send Down (opens 千年之梦) |
| Env | `LO_BACKGROUND=1`, `LO_AUDIO_MUTE=1`, `LO_RENDER_TIMING=1`, `LO_GPU_STATS=1`, `LO_FRAME_TIMING=1` |
| Driver | `out/perf-ring/drive-city.ps1` (worktree copy; isolated `run/save`) |
| Log | `%TEMP%\lo-city-logs\runtime-639249974507450803.log` |
| Local persist | `.omo/cpu-perf/out/perf-ring/{identity.json,card-a-results.json,drive-summary.json}` (gitignored) |

The measured binary is the published Windows package, not a `cpu-perf` RelWithDebInfo rebuild and not the installed v0.5.10 player executable. Player `settings.ini` at `D:\Mihoyo\LostOdysseyRecomp-windows-x64` remained 1920×1080 / 120 and was not used.

## City window

City frames are `draws >= 800`. Classifier `over_budget` was 0%. Isolated `run/save` and the player save tree were both unchanged (`original_saves_changed=false`, `player_saves_changed=false`).

| Count | Value |
| --- | ---: |
| City frames | 1836 |
| Mean draws | 965.6 |
| Menu frames | 896 |
| Last swap / last draws | 1917 / 875 |

## Card A metrics (city frames only)

| Metric | mean | p50 | p95 | p99 | max | notes |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `fence_wait_ms` | 0.0007 | 0.0006 | 0.0008 | 0.001 | 0.1371 | 1 frame >0.01 ms; 0 frames >1 ms |
| `nested_flush_ms` | 0 | 0 | 0 | 0 | 0 | no mid-frame Flush+Begin cost |
| `gpu_batches` | 1.0005 | 1 | 1 | 1 | 3 | almost always one Flush/frame |
| `gpu_queue_batches_elapsed_ms` | 0.8115 | 0.8174 | 0.8343 | 0.8507 | 1.5575 | n=1835; 0 frames >16.67 ms |
| `draw_ms` | 3.4395 | — | 3.9024 | 4.2053 | 6.3522 | 0 frames over 16.67 ms |

Descriptor / upload / arena splits were **all 0** on 1836 city batch lines. D3D12 `limit=1800`.

City-frame component means from the same `render timing` lines: `vertex_ms` 0.331, `bind_ms` 0.677, `record_ms` 1.210, `rt_acquire_ms` 0.071, `taa_ms` 0.080, `shader_lookup_ms` 0.046, `pipeline_lookup_ms` 0.029, `scene_copy_ms` 0.025. Descriptor cache hits / misses means: 297 / 128.

`LO_VERTEX_TIMING` was **not** set on this run. A later same-package city run with `LO_VERTEX_TIMING=1` is recorded in [Card B city measurement](cpu-card-b-city-2026-09-14.md).

## Verdict

`exhausted_resource_class = null`.

On this published-v0.5.11 city protocol:

- `RecycleSlot` is not sleeping on the hot path.
- Mid-frame Flush is not costing measurable nested time.
- Descriptor, upload and arena limits are not exhausted.
- GPU queue is not the 1280×720 city bottleneck.

Guide §9 step 2 applies: mark add-slot / raise-limit as **low priority**. Do not add GPU slots, do not raise 1800/2048, do not add rings from this measurement. Card B's current profile is now recorded separately; Cards C/D stay gated on their own data-contract measurement.

This is one Hidden 1280×720 D3D12 city window. It is not 4K, Vulkan, 3C6T-restricted, whole-game, or player acceptance evidence.
