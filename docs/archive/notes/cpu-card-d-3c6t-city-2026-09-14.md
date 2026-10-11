# Card D 3C6T city measurement — published v0.5.11 — 2026-09-14

> **Historical checkpoint.** Version, issue, process and pending-work statements below describe their original date. Recheck current delivery and acceptance in [STATUS](../STATUS.md) and [ROADMAP](../ROADMAP.md).

This record is a process-level 3-physical-core / 6-hardware-thread envelope on the **same published v0.5.11** package as [Card A](cpu-card-a-city-2026-09-14.md). It does **not** implement host pinning, does not change `KeSetAffinityThread`, does not authorize a version bump, whole-game acceptance, Steam Deck validation, or a new GitHub Release. UnleashedRecomp's affinity stub is the same no-pin baseline; do not treat that project as a pinning recipe.

## Identity

| Field | Value |
| --- | --- |
| Package | GitHub Release [v0.5.11](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.11) |
| ZIP SHA-256 | `5de068c4e77c82feb0bbe7cfcf1dacbca3d44aa94bde064f7f59f5ad6944e132` |
| Runtime | `LostOdysseyRecomp.exe` 83,536,384 bytes, SHA-256 `33a460410b7f397187a12ac6984718c1716997936127f94f5dbcb2ef40315283` |
| Source / commit | 0.5.11 / `624729cdb1263b96061b1fa14d4d1c5ba0b50239` |
| Protocol | Hidden 1280×720, Windowed, D3D12, AA3, 60-cap |
| Scene | isolated `user01` city slot; Continue load; never send Down |
| Env | Card A env plus `LO_NO_UPDATE=1` |
| Process affinity | `0x3F` (logical CPUs 0–5 = physical cores 0–2 on this 9800X3D) |
| Host | AMD Ryzen 7 9800X3D 8C/16T, NVIDIA GeForce RTX 5080 |
| Driver | `.omo/cpu-perf/drive-city-3c6t.ps1` (gitignored wrapper; isolated `out/perf-ring/run`) |
| Log | `%TEMP%\lo-city-logs\runtime-639250355668305659.log` |
| Local persist | `.omo/cpu-perf/out/perf-ring/card-d-results.json` (gitignored) |

The measured binary is the published Windows package, not a `cpu-perf` RelWithDebInfo rebuild and not the installed v0.5.13 player executable. Player `settings.ini` hash remained `f41265ad611d2241cc5522f18ea1908156045856b43856730797e62849d9d8a1`. `original_saves_changed=false` and `player_saves_changed=false`.

Observed affinity after `Start-Process` was `0x3F` / logical `[0,1,2,3,4,5]`. Topology on this machine is 8 SMT pairs, all `eclass=0`, single CCD.

## City window

City frames are `draws >= 800`. Classifier `over_budget` was 0%. `phase_end=done`, elapsed 274.1 s. Isolated `run/save` and the player save tree were both unchanged.

| Count | Value |
| --- | ---: |
| City frames | 1370 |
| Mean draws | 953.0 |
| Menu frames | 891 |
| Last swap / last draws | 1380 / 824 |

Fewer city frames than Card A (1836) because this isolated run had **no startup bundle**: metadata snapshot 3 ms, then `startup bundle fallback: bundle missing`. Shader preparation reported **16 logical threads, 15 requested workers** for 28,484 shaders (`std::thread::hardware_concurrency()` is the machine width, not the affinity mask). Bundle publish landed at 215.360 s (28,484 records). Pipeline preparation was 0 recipes / 0 ms. That cold-prepare oversubscription is a boot observation, not a per-frame city fence.

Video: D3D12, RTX 5080, swapchain 1280×720, `gpu_slots=2`, descriptor limit 1800.

## Card A metrics on the 3C6T envelope (city frames only)

| Metric | mean | p95 | max | vs Card A unconstrained |
| --- | ---: | ---: | ---: | --- |
| `fence_wait_ms` | 0.0008 | — | 0.1336 | 0.0007 / 0.1371; 0 frames >1 ms both |
| `nested_flush_ms` | 0 | 0 | 0 | same |
| `gpu_batches` | 1.001 | — | — | 1.0005 |
| `gpu_queue_batches_elapsed_ms` | 0.8162 | — | 1.8092 | 0.8115 / 1.5575; 0 frames >16.67 ms both |
| `draw_ms` | 3.805 | 4.556 | 13.65 | 3.4395 / 3.9024 / 6.3522 |
| `draw_ms_over_16.67` | 0 | — | — | 0 |
| descriptor / upload / arena splits | 0 | — | — | 0 |

City `vertex_ms` 0.376, `bind_ms` 0.784, `record_ms` 1.339, `shader_lookup_ms` 0.052, `pipeline_lookup_ms` 0.031.

## UnleashedRecomp reference (not copied)

[UnleashedRecomp](https://github.com/hedge-dev/UnleashedRecomp) `KeSetAffinityThread` is the same stub (`*lpPreviousAffinity = 2; return 0`). There is no `SetThreadAffinityMask` / `SetProcessAffinityMask` / `pthread_setaffinity`. Steam Deck notes there are Linux + Gamescope GPU-clock guidance, not Xenon 6T pinning. Their renderer is D3D9 HLE, not a Xenos ring; that path is not portable to this UE3/PM4 host.

## Verdict

`exhausted_resource_class = null`. **implement = false** for default host pinning and for writing placement into `KeSetAffinityThread`.

On this published-v0.5.11 city protocol with process affinity `0x3F`:

- Per-frame fence / split / GPU queue stay in the same class as unconstrained Card A.
- `draw_ms` mean rose 3.44 → 3.81 ms; p95 4.56 ms; max 13.65 ms still under 16.67 ms.
- Cold shader prepare requested 15 workers on a 6-thread mask because `hardware_concurrency` ignores process affinity. Do not treat that boot oversubscription as a reason to add another prepare pool (Card C already `implement=false`).
- This host is still a 9800X3D plus RTX 5080. **It is not Steam Deck, not 15 W, not 4C8T Zen2+RDNA2, not 1080p60@15W.**

Keep `KeSetAffinityThread` as a guest-visible stub. Keep topology-aware pinning experimental and default off. Low-spec 60 FPS work stays in quality budget, GPU submit path, and native Linux/Vulkan — not Card D pinning.

This is one Hidden 1280×720 D3D12 city window under a desktop 3C6T process mask. It is not 4K, Vulkan, Deck, whole-game, or player acceptance evidence.
