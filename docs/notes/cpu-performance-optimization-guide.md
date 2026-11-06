# CPU performance optimization guide: 3C6T budget, feasible parallelism and anti-patterns

[简体中文](cpu-performance-optimization-guide.zh-CN.md)

> **Topic reference.** The implementation and measurements below apply only to the version and scope noted for each item. For current delivery and acceptance status, see [project status](../STATUS.md) and [roadmap](../ROADMAP.md).

Date: 2026-09-14  
Branch: `cpu-perf` / experimental branch `cpu-perf-exp`  
Status: **Guide and R3 implementation record.** Card A / Card B / Card C / Card D (the process-level 3C6T envelope) were measured on the published v0.5.11 build (resource-exhaustion class = null; B1/B2/B3, Parallel Prepare and default pinning are not implemented). On 2026-09-15, the R3 wait-path modernisation (`notified_wait.h`, kernel-object condition-variable waits, CommandProcessor write-pointer and shutdown notification, and the 500 µs bounded wait) was implemented on the local `deck` branch. It is uncommitted, the source version was not changed, and nothing was released.
Body text: Simplified Chinese; symbols, paths and function names stay in English.

### Current status clarification (2026-09-15)

On 2026-09-15 the R3 CPU wait-path optimisation was implemented and verified on the local `deck` branch. It is kept as an uncommitted change in development:
- **Wait-path modernisation**: adds `LostOdysseyRecomp/notified_wait.h`. In `kernel/imports.cpp`, the 200 µs polling loops for bounded-timeout waits on Event, Semaphore and Mutant are replaced by condition-variable predicate / deadline waits, keeping the consume, recursion and timeout semantics. In `gpu/command_processor.{h,cpp}`, write-pointer updates and shutdown notification are added, and the original 200 yield loop is replaced by a bounded 500 µs wait, keeping SDL window event pumping.
- **Validation scope and limits**: Windows `LoNotifiedWaitTest` (pre-notification, early wake, deadline timeout, write-pointer wake) and `LoPollWaitTest` passed. The full Windows target build and diff check passed. After the dependency patches were applied, the Linux native build and codegen passed. A 15 W native Vulkan run on a Radeon 8060S (STAPM 15 W / Fast 25 W / Slow 20 W) created and resized a 1280x720 swapchain, started the guest, prepared 28,484 shaders at 60 W (28,482 ready, 2 deterministic failures), and ran for 90 seconds after consuming the start-up bundle with no errors. The target was 60 FPS. The stable early window was about 58.46–58.58 FPS, and the later complex scenes about 37–40 FPS (**not claimed as a locked 60 FPS**). No player visual acceptance. Not committed, pushed or released. See [current status](../STATUS.md) for details.

Companion records:

- [Card A city measurement (2026-09-14; published v0.5.11, no exhausted resources)](../archive/notes/cpu-card-a-city-2026-09-14.md)
- [Card B city measurement (2026-09-14; published v0.5.11, B1/B2/B3 not implemented)](../archive/notes/cpu-card-b-city-2026-09-14.md)
- [Card C prepare gate (2026-09-14; published v0.5.11, Parallel Prepare not implemented)](../archive/notes/cpu-card-c-prepare-gate-2026-09-14.md)
- [Card D 3C6T city measurement (2026-09-14; published v0.5.11, pinning off by default)](../archive/notes/cpu-card-d-3c6t-city-2026-09-14.md)
- [CPU recompiler deep diagnosis (2026-09-13; historical diagnosis)](../archive/notes/cpu-recomp-deep-2026-09-13.md)
- [Full performance analysis report (2026-09-11; v0.5.4 diagnosis)](../archive/notes/perf-complete-analysis.md)
- [GPU ring buffer measured comparison (2026-09-11; diagnosis, not acceptance)](../archive/notes/perf-gpu-ring-compare.md)
- [Vulkan depth-clear performance (2026-09-13; released in v0.5.9)](../archive/notes/vulkan-depth-clear-performance-2026-09-13.md)
- Not in the notes index, but still valid historical evidence: [CPU live profile (2026-09-13; `EqualSampleBlock64`)](../archive/notes/cpu-live-profile-2026-09-13.md)

Evidence grades (used throughout):

| Marker | Meaning |
|---|---|
| **Fact** | The current code contract, checked on `cpu-perf` on 2026-09-14 |
| **Historical evidence** | A measurement for a named version/scene; must not be treated as the current baseline or as an FPS gain |
| **Assumption** | Not yet re-measured with the current binary |
| **Pre-implementation gate** | Conditions that must be met before changing code |

---

## 1. Conclusions and scope

### Goal

Within a target resource budget of at most **3 physical cores and 6 hardware threads**, answer:

1. Why the existing CPU execution can still be slow.
2. Which work is already concurrent, and which waits or redundancy are the real levers.
3. Which preparation stages can be bounded in parallel, and which must never be split.
4. Where the next implementer should start, how to validate it, and when to stop.

### Non-goals

- Do not reproduce the Xbox 360 Xenon scheduler.
- Do not build a six-worker or fiber scheduler.
- Do not split unsynchronised guest functions across threads.
- Do not pin by default.
- Do not present historical millisecond figures as the frame time that can be recovered now.
- This round does not implement anything, change the version, or run a full-game regression.

### Provisional 3C6T definition

**The whole game process** (guest logic threads + host GPU, audio, VSync, interrupt and shader preparation) shares:

- at most 3 physical cores;
- at most 6 hardware threads (including SMT siblings).

This is a **resource budget / scheduling envelope**. It is not "the process may create only 6 software threads", and it is not "six equivalent workers must be started". Before running any real CPU quota experiment, first confirm that the test machine can provide 3 complete physical cores and their SMT pairs. Pinning a process to 6 logical CPUs does not give 3 complete physical cores.

### One-sentence conclusion

Static recompilation already maps UE3's Game / Render / Async threads, and the host GPU and audio threads, to host `std::thread`s. The current lever is first **reducing waits and redundancy**, and only second, **Parallel Prepare / Serial Commit** for preparation stages that are proven independent. Adding more workers inside a 3C6T budget will most likely make them contend for the same lock and the same fence as the existing submission path.

---

## 2. Current execution model: already multi-threaded, not single-threaded JIT

### 2.1 Static recompilation, not an interpreter

`default.xex` is AOT-compiled into about 62,800 C++ functions. There is no guest CPU interpreter and no JIT scheduler. `ExCreateThread` maps 1:1 through `GuestThreadHandle` to a host `std::thread` (`LostOdysseyRecomp/cpu/guest_thread.cpp`). The guest stack is `STACK_SIZE = 0x100000` (1 MiB).

`GuestThread::Start` takes `procMask` from `flags >> 24`, derives `cpuNumber` with `7 - countl_zero(procMask)` (writes 0 if the mask is 0), and writes it to PCR `+0x10C`. This is only the **guest-visible hardware thread number**. It is not a host affinity.

### 2.2 Affinity stub

`KeSetAffinityThread` in `LostOdysseyRecomp/kernel/imports.cpp` (about lines 775–780) is a stub. It writes the previous affinity as `2` and returns `0`. There is no `SetThreadAffinityMask` / `pthread_setaffinity_np`.

Therefore:

- Filling in the stub **does not equal** restoring Xenon performance.
- The PCR number and the host placement are two separate layers.
- The existing threads are already running. They just drift under the OS scheduler.

### 2.3 Who runs, and who waits for whom

Guest side (UE3's existing concurrency, mapped 1:1):

| Role | Entry | Produces / consumes |
|---|---|---|
| Game | Main tick | Game state; submits scene work to Render |
| Render | `sub_824856A0` `RenderingThreadMain` → `sub_825B2A20` `FSceneRenderer::Render` | Packs draws; consumes Game's scene; submits command buffers to the host GPU worker |
| Async / worker | `sub_82290D28` | Package / resource streaming load |

Host side (the process shares one 3C6T budget):

| Role | Entry | Produces / consumes |
|---|---|---|
| GPU worker | `CommandProcessor::WorkerMain` (`command_processor.cpp` about line 394) | Parses the ring and generates command lists |
| VSync | `VsyncMain` (about line 455; `GuestThreadContext(2)`) | Frame synchronisation |
| GPU interrupt | `InterruptMain` | Interrupt / completion notification |
| APU | `DriverMain` (`audio.cpp`) | Audio driver |
| XMA | `WorkerMain` (`xma.cpp` about line 524) | XMA decoding (already an independent preparation thread) |
| Shader prep | Asynchronous preparation worker | Compile preparation at start-up or on first encounter with a shader |

Key dependencies:

1. The Game → Render submission order is a UE3 contract and must not be reordered.
2. Render and the GPU worker meet at the descriptor set, the upload ring, and the two GPU slots.
3. After `Flush()` submits, it **does not wait**. The real blocking happens when `RecycleSlot` reuses a slot that the GPU is about to occupy.
4. Audio and shader prep already use part of the budget. Adding unbounded workers would squeeze Game and Render.

### 2.4 Current GPU submission contract (fact)

`Renderer` uses `kGpuSlots = 2` (`renderer.cpp`, `render_arena_policy.h`).

- `Flush()` (about lines 1582–1598): `executeCommandLists`, then `gpuSlot++`. It **does not wait on the fence**.
- `RecycleSlot` (about line 1505): `waitForCommandFence`, then reclaims `queryResults`, retired textures / framebuffers, `textureSetCache.Clear`, `temporalHistory.ReleaseCompletedThrough`, sets `sceneAABusy = false`, and resets `uploadOffset` / `setPoolUsed`.
- `Begin()` calls `RecycleSlot` for the current `gpuSlot`.
- `Upload()` performs a mid-way `Flush` + `Begin` when the ring is full.
- `DrawImpl` still performs `Flush` + `Begin` on `poolsFull`, `ringLow`, or arena wrap.

Descriptor limits (`render_batch_policy.h`, **fact, not the historical 500**):

- D3D12: `kD3D12DescriptorLimit = 1800`
- Vulkan: `kVulkanDescriptorLimit = 2048`
- Lower bound: `kMin = 500`
- Unused 3D / cube banks use a static dummy set. The limit is consumed by unique 2D combinations.

The historical note's `kMaxSetsPerKind = 500` and "`Flush` immediately calls `waitForCommandFence`" are the **source contract of v0.5.4 and earlier**. They must not be cited as the current code.

The D3D12 fence wait is `WaitForSingleObjectEx(..., INFINITE)`. Vulkan is `vkWaitForFences(..., UINT64_MAX)` followed by `vkResetFences`. The two slots already allow "the previous slot executes on the GPU while the current slot is recorded on the CPU". **It cannot be claimed that the fence wait can be fully eliminated.** A slot from two frames earlier must still wait for the GPU to finish before its descriptors, upload space and queries can be reclaimed.

---

## 3. Evidence ledger and unknowns

### 3.1 Already landed: do not redo as if not done

| Item | Status | Evidence |
|---|---|---|
| Two-slot command / descriptor ring | Present in current code | `kGpuSlots = 2`; `Flush` does not wait |
| Default descriptor limits 1800 / 2048 | Present in current code | `render_batch_policy.h` |
| Dummy 3D / cube bank | Present in current code | Only unique 2D entries consume the limit |
| `EqualSampleBlock64` | Released in v0.5.8 | `geometry_prepare.h` SSE2 4×16; [cpu-live-profile](../archive/notes/cpu-live-profile-2026-09-13.md) |
| Texture-key avalanche mixing | Released in v0.5.9 | `gpu/texture_key.h`: `>>16 * 0x7FEB352D >>15 * 0x846CA68B >>16` |
| Shader identity cache, Plume bind suppression, per-slot descriptor reuse | Shipped in the v0.5.9 candidate | [vulkan-depth-clear](../archive/notes/vulkan-depth-clear-performance-2026-09-13.md) |
| `poll_wait` adaptive back-off | Present in current code | Only `Query` / `SharedValue` / `GpuPoll`; yields after 32 attempts; GpuPoll cap 50 µs |
| `getenv` caching for Query / `LO_QUERY_TRACE` | Released in v0.5.8 | Same as the live profile follow-up commit |

### 3.2 Historical measurements (must not be used as the current baseline)

**v0.5.4 city, 1280×720** ([perf-complete-analysis.md](../archive/notes/perf-complete-analysis.md), released package):

- About 31–44 fps, 1800–2300 draws, `draw_ms` 21.19, `fence_wait` 15.40 ms **nested inside `tDraw`**.
- Source at the time: `Flush` waited for the fence immediately after submitting. This fragment is out of date.
- Title screen: about 60 fps, 114 draws, 2 batches, `fence_wait` 4.34 ms.

**Two-slot diagnosis, not acceptance** ([perf-gpu-ring-compare.md](../archive/notes/perf-gpu-ring-compare.md), same city scene vs v0.5.4):

- RelWithDebInfo, two slots, limit=1800: FPS 57.7, batches 2.00, `descriptor_splits` 0, `fence_wait` 1.67 ms (range 0.001–11.72).
- This is a diagnostic comparison. It is not acceptance, and it is not the baseline for the current `cpu-perf` binary.

**source-0.5.8 WPR** ([cpu-recomp-deep-2026-09-13.md](../archive/notes/cpu-recomp-deep-2026-09-13.md), commit `6e6f11cf`, PID 38988, 4K internal / 1920 window / AA3 / D3D12 / 8060S, package about 40 W):

- Heartbeat 31.9121 FPS, 2284–2302 draws.
- Render TID accounts for 41.86% of samples. Guest render 24.0%, almost all in `sub_824856A0` (of which 79.93% in `FSceneRenderer::Render`).
- `memcmp` **766 / 10848 = 7.0612% of Render-thread self samples, not 7% of the frame**: vertex 376, `UploadUnchanged` 226, shader identity 132, texture descriptor 32.
- Texture cache snapshot: 386 keys, 33 buckets, longest chain 84. The hash hotspot is about 2.93% of render self. Evidence directory `out/cpu-recomp-deep-20260913/`.
- **The chain length of 84 occurred before the avalanche mixing was released.** After v0.5.9, "fixing the hash" must not be written as unfinished work. The remaining question is: **does the current package still have long chains, and does lookup still take measurable CPU?**

**`EqualSampleBlock64` live** ([cpu-live-profile-2026-09-13.md](../archive/notes/cpu-live-profile-2026-09-13.md); isolated 9.45–14.99×; fixed user00 40 W 4K):

- Mean FPS 46.787 → 47.074 (+0.61%).
- Vertex-match 2.279 → 0.488 ms.
- GPU queue still about 20.7 ms, so the local CPU speed-up **did not** bring 4K to 60.
- The same candidate at 1080p had a mean of 59.981. Released in v0.5.8.

**Register SIMD rejected** ([vulkan-depth-clear-performance-2026-09-13.md](../archive/notes/vulkan-depth-clear-performance-2026-09-13.md)):

- Const path 1.531 → 2.157 ms, FPS 42.72, rolled back.
- 4K Vulkan 60 W user01 static: depth-clear coalescing raised mean FPS from 7.496 to 43.476. This is a **GPU** gain and cannot be counted in the CPU multi-threading story.
- Released in v0.5.9 (`d26ee8b`, CI `34778434518`).

### 3.3 Current unknowns (pre-implementation gates)

1. The **current `cpu-perf` / published v0.5.11 package** in the city scene: `fence_wait`, `descriptor_splits`, and the number of mid-frame `Flush` calls. The two slots may already overlap most of the waits. Without new measurement, "add another slot" cannot be ruled out.
2. The **real hash chain length and lookup CPU** after avalanche mixing.
3. The length distribution of the remaining `memcmp`: 64 B already uses SIMD. Are other lengths still hot?
4. Whether the 3C6T test machine can actually provide 3 physical cores with SMT pairs (P/E cores, CCDs and 3D V-Cache can all break "pin to 6 logical CPUs").
5. For any new parallel preparation task: input snapshot, output ownership, cancellation / invalidation, and how its budget stacks with the existing XMA and shader workers.

---

## 4. Correct use of 3C6T

Xbox 360 Xenon is 3 in-order physical cores × SMT = 6 hardware threads. The two hardware threads on the same core share L1. ATG's experience is that two heavy threads bound to the same core often gain only +10–20%, not ×2.

In this project, "imitating 3C6T" allows only three things:

1. **Treat 3C6T as a stress envelope.** Optimisations must still be a net gain with the process limited to 3 physical cores / 6 hardware threads. An optimisation must not depend on "the machine has 16 threads" to become faster. If the default scheduling already uses more cores, the optimisation must not rely on that.
2. **Leave cores for existing threads.** Game, Render, the GPU worker and audio must at least be able to run. New parallel preparation threads count against the budget. The pool must not grow without limit.
3. **Optional, switchable topology-aware placement experiment.** Place the existing threads on suitable physical cores / SMT siblings. This is an experiment layer, not a correctness fix, and it is off by default.

Explicitly not:

- Not six software workers simulating six Xenon hardware threads.
- Not splitting guest functions by PPC address.
- Not "six `std::thread`s in total for the process". The software thread count is already above six. The limit is on **hardware threads occupied at the same time**.

Modern host differences:

- Intel P/E: pinning Render / GPU workers to E-cores increases tail latency.
- AMD multi-CCD / 3D V-Cache: cache sharing across CCDs is poor. Hot threads should stay on the V-Cache CCD where possible.
- Handheld 4C8T / 8C16T: 3C6T is still a **budget**, not a requirement to turn off the extra cores. But acceptance scenarios should include one comparison that is **limited to 6 hardware threads**.

Keep OS scheduling by default. Affinity is only an experiment behind a switch.

---

## 5. Optimisation priorities and route table

The order is fixed. Do not jump to "add threads" before the previous item has a current measurement.

| Priority | Direction | Lever type | Meaning under 3C6T | Status |
|---|---|---|---|---|
| A | Measure remaining fence / mid-frame split | Waiting | Adds no threads | City on v0.5.11 measured; no exhausted resources |
| B | Remaining redundant CPU (non-64 B `memcmp`, hash lookup, duplicate identity) | Single-thread overhead | Adds no threads | 64 B SIMD and avalanche **done**; B1/B2/B3 not implemented |
| C | Bounded Parallel Prepare / Serial Commit | Parallel preparation | At most one more SMT sibling, and only where independence is provable | Start-up prepare already exists; new Parallel Prepare not implemented |
| D | Optional topology-aware affinity | Placement | Only moves existing threads | Process-level 3C6T city measured (affinity 0x3F); **default pinning not implemented** |

Amdahl warning: the historical main cause in the city scene was serial waiting on the submission path, not "not using all 6 threads". While the fence still dominates, adding workers only makes more threads block in `RecycleSlot` / descriptor allocation.

---

## 6. Candidate implementation cards

### Card A — Remaining fence / split measurement (do first; almost no code change)

**Problem and evidence**  
The v0.5.4 city `fence_wait` of 15.40 ms came from "`Flush` waits immediately". The current `Flush` no longer waits. The two-slot + 1800/2048 limit diagnosis brought the mean `fence_wait` in the same city scene down to 1.67 ms, but the range still reaches 11.72 ms. That was not acceptance, and it is not the v0.5.11 / `cpu-perf` baseline.

**Current measurement (fact, 2026-09-14)**: published v0.5.11 Hidden 1280×720 D3D12 AA3 60-cap city `user01`, 1836 city frames. `fence_wait_ms` mean 0.0007, max 0.1371 (0 frames >1 ms). `nested_flush_ms` all 0. Descriptor / upload / arena splits all 0 (D3D12 limit 1800). `gpu_batches` mean 1.0005. GPU queue mean 0.8115 ms, max 1.5575 (0 frames >16.67 ms). Resource-exhaustion class = null. The historical 15.40 ms and the two-slot diagnosis of 1.67 ms **must not** replace this baseline. See [Card A city measurement](../archive/notes/cpu-card-a-city-2026-09-14.md).

**Code entry points**  
`LostOdysseyRecomp/gpu/renderer.cpp`: `Flush`, `RecycleSlot`, `Begin`, `DrawImpl`, `Upload`.  
`LostOdysseyRecomp/gpu/render_batch_policy.h`.  
Timing: `LO_RENDER_TIMING`, `LO_FRAME_TIMING`, `LO_GPU_STATS`.  
City scene driver: `out/perf-ring/drive-city.ps1` (if still available; if not, record the gate and do not invent a command).  
Tests: `LoRenderBatchPolicyTest`, render-timing fixture.

**Change boundary**  
This card is **measurement only** by default. A separate proposal is allowed only when the current package still shows "several blocking `Flush` + `RecycleSlot` per frame" and can identify **which resource class** is exhausted (descriptor set, upload ring, query, or arena wrap). The proposal may then raise that class's limit, defer reclamation, or add a third slot. A general "add another ring" that covers all three lifetimes is forbidden.

**Data / resource contract**  
Slot N's descriptors, upload space, queries, `textureSetCache` and temporal history cannot be reused before the GPU finishes N. `RecycleSlot` is the meeting point of this contract. Adding slots increases resident VRAM and CPU-mapped memory.

**3C6T budget**  
The measurement itself uses no extra hardware thread. If a slot is added, CPU recording still runs on the Render / GPU worker, so there is no new parallelism.

**Validation**  
Fix the version, scene, resolution, API and power limit. Repeat after warm-up. Look at: mean and tail FPS, `fence_wait` mean and p95, `descriptor_splits`, the number of `Flush` per frame, and GPU queue time. A local drop in `fence_wait` while GPU queue is still >16.6 ms must not be declared a "CPU optimisation success".

**Rollback**  
Measurement changes no code. If slot or limit is changed later: keep the original `kGpuSlots=2` and 1800/2048 as defaults, and roll back through a switch or compile-time constant.

**Open conditions**  
The city comparison on the current package is complete (published v0.5.11). No descriptor / upload / arena / fence resource is exhausted. "Add slot / raise limit again" is marked low priority. All implementation sub-items of A remain at the gate. The next step is Card B's current profile, not changing the GPU slots.

---

### Card B1 — Remaining `memcmp` / dirty checks (non-64-byte blocks)

**Problem and evidence**  
source-0.5.8: `memcmp` was 7.0612% of Render **self samples**, not 7% of the frame. Of that, vertex 376, `UploadUnchanged` 226, shader identity 132, texture descriptor 32. v0.5.8 already uses `EqualSampleBlock64` for 64-byte blocks. The isolated speed-up was large, but at 4K the whole frame gained only +0.61%, because the GPU queue was still about 20.7 ms. Register SIMD was slower on constant upload and was rolled back.

**Code entry points**  
`LostOdysseyRecomp/gpu/geometry_prepare.h`: `EqualSampleBlock64`; `SampledContent` Visit (≤8192 full compare, otherwise the first and last 512 bytes plus 64 64 B windows).  
Other call sites still using `memcmp` must be listed from the current profile before implementation; do not guess them in the guide.  
Tests: `geometry_prepare_test.cpp`, `LoVertexCacheTest`, `shader_identity_test.cpp`.  
Environment variable: `LO_VERTEX_TIMING`.

**Change boundary**  
Only touch comparisons that are **proven hot and whose length is not 64 B**. Do not redo register SIMD. Do not treat "hand-written SIMD is always faster" as the default.

**Data contract**  
The comparison must keep the same equality semantics as the current `memcmp`, including early exit. Output is still submitted serially by the Render thread.

**3C6T budget**  
Single-thread overhead reduction. It uses no new hardware thread. When it is a net gain, Recycle waiting should fall indirectly, but this is not parallelism.

**Validation**  
First look at the path's own time (for example vertex-match ms), then the whole frame. Change one variable: only the comparison implementation. If the isolated path gets faster but the whole frame does not change, record it as "not a frame-rate bottleneck" and stop. Do not widen the SIMD scope.

**Rollback**  
Keep the `memcmp` path as fallback. The rejection of register SIMD remains in force.

**Current measurement (fact, 2026-09-14)**: the same published v0.5.11 city package with `LO_VERTEX_TIMING=1`, 1846 city frames. `match_ms` mean 0.1535 (42% of the stage sum of 0.3616). `LO_VERTEX_TIMING` is the vertex-cache wall clock, **not** a `memcmp` length histogram. The histogram is still missing. See [Card B city measurement](../archive/notes/cpu-card-b-city-2026-09-14.md).

**Open conditions**  
A length histogram and call-site share for the current package. Without a histogram, SIMD must not be expanded. **Not implemented** this round.

---

### Card B2 — Texture hash lookup (only if still hot after avalanche)

**Problem and evidence**  
source-0.5.8: longest chain 84, hash hotspot about 2.93% of render self. v0.5.9 added avalanche mixing. In current code, `TextureKeyHash` is in `gpu/texture_key.h`. The historical chain length **cannot** be used again as a defect list.

**Code entry points**  
`gpu/texture_key.h`, the texture cache implementation, `LoTextureKeyTest`, `LoTextureDescriptorCacheTest`.

**Change boundary**  
First measure chain length and lookup time on the current package. Change the bucket count or key layout only if the longest chain and lookup CPU are still significant. Do not "write another mix".

**Data contract**  
Key equality semantics do not change. If concurrent lookup exists, it must still follow the existing lock / lock-free convention. Do not introduce cross-thread writes to the cache.

**3C6T budget**  
Lookup still runs on the Render / GPU worker. Do not create a separate worker for hashing.

**Validation**  
Chain-length distribution + this function's self time + whole frame. Improving only the distribution is not success.

**Rollback**  
Keep the existing avalanche.

**Current measurement (fact, 2026-09-14)**: city `shader_lookup_ms` mean 0.051, `pipeline_lookup_ms` 0.03. Vertex cache `rehashes` total 0 (131072 buckets). Evictions are vertex-cache capacity turnover, not a texture hash chain. There is no texture chain-length snapshot. Lookup is not hot after avalanche. See [Card B city measurement](../archive/notes/cpu-card-b-city-2026-09-14.md).

**Open conditions**  
A texture cache chain-length snapshot from v0.5.9 or later. Without a snapshot, and if lookup is not hot, **do not implement**, and do not write another mix.

---

### Card B3 — `poll_wait` (do not expand by default)

**Problem and evidence**  
`LostOdysseyRecomp/cpu/poll_wait.h` covers only known guest loops: `Query`, `SharedValue`, `GpuPoll`. It yields after 32 polls. The `GpuPoll` delay cap `kGpuPollDelayCapUs = 50`. Coarse `Sleep` damages frame pacing.

**Code entry points**  
`poll_wait.h` and the existing call sites. Tests: `LoPollWaitTest`, `LoFramePacerTest`.

**Change boundary**  
Without new evidence of guest busy-waiting, do not expand the Kind set, do not lengthen the cap, and do not switch to OS sleep.

**Data contract**  
Guest-visible query / shared value semantics do not change. A yield must not become "continue when the result is not yet ready".

**3C6T budget**  
Busy-waiting wastes an SMT sibling. Adaptive yield **reduces** the budget waste. It is not adding threads.

**Validation**  
The existing `LoPollWaitTest`. At runtime, use `LO_QUERY_TRACE` (already cached) to count polls, without taking foreground focus.

**Rollback**  
Keep 32 / 50 µs.

**Current measurement (fact, 2026-09-14)**: this city run did not set `LO_QUERY_TRACE`, and there is no new guest spin evidence. Keep 32 / 50 µs. See [Card B city measurement](../archive/notes/cpu-card-b-city-2026-09-14.md).

**Open conditions**  
New evidence of a guest spin call site. Without it, do nothing. **Not implemented** this round.

---

### Card C — Bounded Parallel Prepare / Serial Commit

**Problem and evidence**  
Only work whose **input is read-only, whose output has a clear owner, and whose submission is still serialised by the Render / GPU worker** can run in parallel. Examples that already exist: asynchronous shader preparation and XMA `WorkerMain`. No evidence supports putting `FSceneRenderer::Render`, guest scene traversal, or mutable descriptor state into a thread pool.

**Code entry points**  
Depends on the specific task. Candidate directions (all **assumptions**, not authorised): pre-hashing read-only texture keys, and vertex-format conversion slices with independent output buffers. A measurement is needed first showing that "the task is large enough and its input is stable during execution".

**Change boundary**

- Prepare: a worker writes only to its own output buffer.
- Commit: the original Render / GPU worker submits in the original order.
- Workers must not touch guest memory, TLS/PCR, or UE3 render mutable state.
- New pool size cap: under 3C6T, **at most one more hardware thread**. Prefer reusing the existing shader / XMA threads over building a third pool.

**Data contract**  
Must specify: the snapshot time, who owns the output, who joins before Commit, how stale results are discarded on task cancellation, and how to fall back to serial prepare on failure. Missing any of these = not implementable.

**3C6T budget**  
Game + Render + GPU worker + audio already fill or nearly fill 3C6T. A new worker by default shares a physical core's SMT sibling with some existing light thread. If measurement shows that the preparation task is smaller than its queueing / copy overhead, cancel the candidate.

**Validation**  
Data races (existing unit tests, and TSAN or repeated application-level playback where needed), submission order, stale results never committed, and tail frames under the 3C6T limit. Higher mean FPS with worse p95 = failure.

**Rollback**  
Return to serial prepare through a compile-time or runtime switch. Off by default.

**Current measurement (fact, 2026-09-14)**: reuses the Card B city logs. There is no new city run. The start-up shader bundle took 2248 ms (device module 56 ms). Pipeline prepare 21 ms / 4 workers / 222 recipes. In the city scene, per-frame `copy_ms` 0.0091, `shader_lookup_ms` 0.051 and `pipeline_lookup_ms` 0.03 are smaller than the queueing / copy overhead. The new Parallel Prepare lacks the snapshot, ownership, join, cancellation and serial fallback. See [Card C prepare gate](../archive/notes/cpu-card-c-prepare-gate-2026-09-14.md).

**Open conditions**  
Independent task size, input stability, and how it stacks with the existing asynchronous work. All are gates. **Do not implement** the new Parallel Prepare this round. Do not add a third preparation pool. Card D still defaults to off.

---

### Card D — Optional topology-aware affinity (experiment layer)

**Problem and evidence**  
`KeSetAffinityThread` is still a stub. PCR `+0x10C` already writes the guest cpuNumber. Modern topology differs from Xenon. There is no evidence that default pinning would be faster.

**Code entry points**  
`LostOdysseyRecomp/kernel/imports.cpp`, `KeSetAffinityThread`; the start path in `guest_thread.cpp`. Correctness (the affinity the guest reads) and performance pinning must be kept separate.

**Change boundary**

- Correctness: if the game depends on the previous affinity return value, a stub that always writes `2` may already be wrong. That is a semantic issue. Fix it separately, not bundled with performance.
- Performance: an optional mapping table, off by default. It must identify P/E cores, CCDs and SMT siblings. If identification fails, fall back to OS scheduling, and do not pin blindly.

**Data contract**  
Does not change the number of threads created. It only changes the placement of existing threads. The guest PCR number does not need to equal the host CPU id.

**3C6T budget**  
When experimenting, limit the process to 3 physical cores / 6 hardware threads, and ensure Game and Render do not share one SMT pair (they are two heavy loads). Whether the GPU worker shares with Render or not must be measured, and is not decided in this guide.

**Validation**  
Same scene, with the switch on and off as a control. Record the topology and which threads are pinned where. On machines without topology information, the fallback must be used and must not be noticeably slower.

**Rollback**  
Off by default. Turning the switch off restores OS scheduling.

**Current measurement (fact, 2026-09-14)**: published v0.5.11 Hidden 1280×720 D3D12 city, process affinity `0x3F` (9800X3D cores 0–2 / logical 0–5), 1370 city frames. `fence_wait` mean 0.0008, `gpu_queue` 0.816, `draw_ms` 3.805 / p95 4.556 / max 13.65, `over_budget=0%`, all splits 0. On a cold start with the bundle missing, prepare still asks for 15 workers based on `hardware_concurrency=16`. UnleashedRecomp's `KeSetAffinityThread` is also a stub, with no host pinning. See [Card D 3C6T city measurement](../archive/notes/cpu-card-d-3c6t-city-2026-09-14.md).

**Open conditions**  
The local topology can be read. The 3C6T envelope acting on the whole process has been measured. Default pinning is still not implemented. This is not Steam Deck, 15 W, 4C8T or 1080p60@15W.

---

## 7. Approaches not to adopt, and limits of applicability

| Approach | Why not |
|---|---|
| Six workers / fibers to simulate Xenon's 6 threads | Xenon's 6 threads are hardware SMT, not a software pool. The process already has Game / Render / Async / GPU / audio / VSync. Building six more pools would break ordering and saturate the budget. |
| Splitting unsynchronised guest functions across threads | Breaks PPC weak memory, `lwarx`/`stwcx`, UE3's Game → Render order, and TLS / PCR / the 1 MiB guest stack. Static recompilation has no interpreter to insert fibers into. |
| "Filling in `KeSetAffinityThread` restores 360 performance" | A stub only affects **where existing threads are**. Historical bottlenecks were fences, comparisons and hashing, not missing pinning. |
| Default pinning | P/E, CCD and handheld hardware would be pinned wrongly. No topology = no pinning. |
| Claiming a multi-buffer ring eliminates all fence waits | Two slots already exist. The GPU must still finish before a slot can be reclaimed. The historical 15 ms figure includes the old contract. |
| Treating `memcmp`'s 7% of render samples as 7% of the frame | The denominator is Render self samples. On 4K, vertex SIMD yielded only +0.61% FPS. |
| Assuming hand-written SIMD beats the runtime library by default | Register SIMD is already rejected (const 1.531→2.157 ms). |
| Writing source-0.5.8's chain length of 84 as a current defect | Avalanche was released in v0.5.9. |
| Expanding `poll_wait` or switching to coarse Sleep | No new evidence; damages frame pacing. |
| Parallelising `FSceneRenderer::Render` or mutable render state | No synchronisation evidence. |
| Using a 16-core desktop result to prove the 3C6T approach | Acceptance must include a budget-limited comparison. |

---

## 8. Staged validation and stop criteria

### Reusable tools (do not invent commands)

- Unit tests: `LoPollWaitTest`, `LoRenderBatchPolicyTest`, `LoTextureDescriptorCacheTest`, `LoTextureKeyTest`, `LoVertexCacheTest`, `geometry_prepare_test.cpp`, `shader_identity_test.cpp`, `LoFramePacerTest`, render-timing fixture. For the suite description, see `tools/tests/README.md`.
- Runtime environment variables: `LO_RENDER_TIMING`, `LO_FRAME_TIMING`, `LO_VERTEX_TIMING`, `LO_GPU_STATS`.
- City scene: `out/perf-ring/drive-city.ps1` (use if present; if absent, record the gate).
- Historical WPR: `out/cpu-recomp-deep-20260913/` (source-0.5.8, not a current baseline).

Agent runtime tests must run in the background, muted by default, and must not take foreground focus. When foreground interaction is needed, record it as a gate and wait for the user to schedule it.

### Experiment discipline

1. Fix the binary version, scene, resolution, graphics API and power limit.
2. Repeat after warm-up. Report mean and tail frames, not just mean FPS.
3. **Single variable.** Do not turn on SIMD, a new worker and affinity at the same time.
4. Look at both the target bottleneck and the whole frame. A local gain is not overall success.
5. Correctness gates: picture, submission order, and stale async results. Related unit tests stay green.
6. 3C6T control: at least one run with the process limited to 6 hardware threads. If the machine cannot provide a 3-physical-core SMT pair, label the experiment invalid, and do not present desktop 16-thread results as 3C6T conclusions.

### Stop / negative gain

Stop the candidate and roll back if any of these occur:

- The target timing drops, but p95 frame time rises.
- GPU queue still dominates, so further CPU cuts give no whole-frame gain (see 4K +0.61%).
- New waits, locks or copies outweigh the preparation task itself.
- Data races, out-of-order submission, or resources reused before completion.
- Slower under 3C6T, and faster only on wide machines.

The guide stage does not run the full-game regression. Future optimisation validation is kept separate from this documentation acceptance.

### Acceptance of this document (this round)

- All nine sections are present; each card has all its fields.
- Historical numbers carry version, scene and denominator.
- Feasible and infeasible items are actionable. Gates are not written as decided plans.
- No change to runtime, version or ROADMAP.

---

## 9. Starting checklist for the next implementer

1. **Do the Card A measurement first.** Do not add threads. Use the binary you are optimising (state whether it is a local `cpu-perf` build or the published v0.5.11 package). Turn on `LO_RENDER_TIMING` / `LO_GPU_STATS` on a fixed city scene, and record `Flush` count per frame, `fence_wait`, `descriptor_splits`, and GPU queue. On 2026-09-14 one measurement was completed on the **published v0.5.11** city package. See [Card A city measurement](../archive/notes/cpu-card-a-city-2026-09-14.md).
2. If `fence_wait` is already close to the low single-digit milliseconds in the diagnostic comparison, and `descriptor_splits = 0`: mark "add slot / raise limit again" as low priority, and move to Card B's current profile. Card A's measurement meets this condition (`fence_wait` mean 0.0007 ms, all splits 0). The current Card B profile is recorded: B1 still lacks a length histogram, so SIMD is not expanded; B2 lookup is not hot and there is no texture chain-length snapshot; B3 has no new guest spin. See [Card B city measurement](../archive/notes/cpu-card-b-city-2026-09-14.md).
3. If mid-frame `Flush` still occurs: identify whether the exhausted resource is descriptor, upload or arena, and then propose it separately. A general "multi-buffer" is forbidden.
4. For any parallel preparation: write the data contract first (snapshot, ownership, join, cancellation), then the code. Off by default. Under 3C6T, at most one more hardware thread.
5. Affinity stays experimental and off by default. The process-level 3C6T city envelope has been measured. See [Card D](../archive/notes/cpu-card-d-3c6t-city-2026-09-14.md). Do not write pinning into `KeSetAffinityThread`. If fixing the stub's return value is a correctness issue, commit it separately and do not mix it with performance. This is not a Steam Deck / 15 W acceptance.
6. Without new authorisation, do not change the source version, and do not put this guide into release notes.

**The first task is measurement, not implementation.**
