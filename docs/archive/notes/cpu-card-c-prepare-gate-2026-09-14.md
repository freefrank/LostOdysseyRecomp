# Card C prepare gate — published v0.5.11 — 2026-09-14

> **Historical checkpoint.** Version, issue, process and pending-work statements below describe their original date. Recheck current delivery and acceptance in [STATUS](../STATUS.md) and [ROADMAP](../ROADMAP.md).

This record is the current Card C gate for bounded Parallel Prepare / Serial Commit. It reuses the **same published v0.5.11** city log as [Card B](cpu-card-b-city-2026-09-14.md) plus a static inventory of existing prepare workers. It does **not** authorize a new worker pool, per-frame Parallel Prepare, a version bump, whole-game acceptance, or a new GitHub Release. No new city drive was launched.

## Identity

| Field | Value |
| --- | --- |
| Package | GitHub Release [v0.5.11](https://github.com/freefrank/LostOdysseyRecomp/releases/tag/v0.5.11) |
| ZIP SHA-256 | `5de068c4e77c82feb0bbe7cfcf1dacbca3d44aa94bde064f7f59f5ad6944e132` |
| Runtime | `LostOdysseyRecomp.exe` 83,536,384 bytes, SHA-256 `33a460410b7f397187a12ac6984718c1716997936127f94f5dbcb2ef40315283` |
| Source / commit | 0.5.11 / `624729cdb1263b96061b1fa14d4d1c5ba0b50239` |
| Evidence | Card B city log `%TEMP%\lo-city-logs\runtime-639250002081609096.log` plus `PrepareKnownShaders` / `PrepareKnownPipelines` / XMA `WorkerMain` |
| Local persist | `.omo/cpu-perf/out/perf-ring/card-c-results.json` (gitignored) |

## Existing prepare (startup, already implemented)

The Card B city process already ran the current startup prepare path. That work is **boot-time**, not a per-frame Parallel Prepare candidate.

| Event | Value |
| --- | ---: |
| Shader startup metadata snapshot | 158 ms |
| Startup bundle elapsed | 2248 ms (56 ms device module creation) |
| Bundle records / modules ready / cached failures | 28547 / 28545 / 2 |
| Source reads / translations / DXC attempts | 0 / 0 / 0 |
| Pipeline recipes / ready / workers | 222 / 222 / 4 |
| Pipeline preparation | 21 ms |

Code contract (current tree, not a new design):

- `PrepareKnownShaders` already parallelizes translation/DXC/cache with `xenos::preparation::WorkerCount`; `LO_SHADER_PREPARE_SERIAL` and `LO_NO_SHADER_PREPARE` exist. Device maps stay on the command-processor thread. Bundle load checks `identity != snapshot()` and falls back if inputs change.
- `PrepareKnownPipelines` already uses up to 4 `std::jthread` workers (`hardware_concurrency-1`, cap 4) or `LO_PIPELINE_PREPARE_SERIAL`. Workers write only their `Job.pipeline`; the CP thread joins, then inserts into `pipelines`.
- XMA `WorkerMain` already exists. This city run did not set `LO_TRACE_XMA`.
- `gpu::taa_collection::CollectionWorker` is an expendable F1 upload thread, not a render prepare path.

Do **not** add a third prepare pool. Startup prepare is already present and already bounded.

## Per-frame remaining work (city)

From the same Card B city window (1846 city frames):

| Candidate | mean ms | Independent prepare? |
| --- | ---: | --- |
| vertex `copy_ms` | 0.0091 | No. Tiny vs queue/copy overhead. Output is the render upload path. |
| `shader_lookup_ms` | 0.051 | No. Cache lookup, not compile. |
| `pipeline_lookup_ms` | 0.030 | No. Cache lookup after startup prepare. |
| vertex stage sum | 0.3616 | Mostly `match_ms` dirty-detect; not a read-only slice with owned output. |
| `draw_ms` | 3.82 | Serial record/submit. Guide forbids pooling `FSceneRenderer::Render`. |
| Card A GPU queue | 0.8115 | Not a CPU prepare task. |

Guide hypotheses (texture-key prehash, vertex-format conversion slices) remain **unauthorized**. This gate does not size those tasks independently; the observed per-frame leftovers are already below a 1-SMT prepare/join budget.

## Data contract (new Parallel Prepare)

| Requirement | Present for a **new** per-frame task |
| --- | --- |
| Snapshot time | no |
| Output ownership | no |
| Join before serial commit | no |
| Cancel / drop stale results | no |
| Serial fallback | no |
| Task size > queue/copy overhead | no |
| Input stable during prepare | no |
| Overlap with existing XMA/shader workers measured | no (`LO_TRACE_XMA` unset; startup prepare already finished before city frames) |

Missing any one of these is a stop. Card B's profile does not lift Card C.

## Verdict

`exhausted_resource_class = null`. **implement = false**.

- Do **not** implement a new Parallel Prepare / Serial Commit path.
- Do **not** add another worker pool on top of shader/pipeline prepare and XMA.
- Do **not** parallelize guest scene traversal, descriptor mutation, or `FSceneRenderer::Render`.
- Card D remains a separate affinity experiment, default off.

This is not 4K, Vulkan, 3C6T-restricted, whole-game, or player acceptance evidence.
