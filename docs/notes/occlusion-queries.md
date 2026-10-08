# Host GPU occlusion queries (#118)

Status 2026-10-02: merged as PR #122 (`c0163a8`) and part of v0.7.35 (tag
`v0.7.35`); per-owner Fast answers (see "Who owns a query record") are
unreleased. Direct3D 12 and Vulkan; Metal keeps the old fake counts. Checked in
the frozen Uhra save and at the #118 spot on the world map on an RTX 5080.

## Why the flare showed through terrain

The game measures how much of the sun is visible with GPU occlusion queries.
The command processor used to answer every `EVENT_WRITE_ZPD` with a made-up,
growing count (`LO_ZPD_MODE=grow`), so every query read as fully visible and
the flare never faded behind geometry. This has been the case since the first
release; it is not a regression.

## What the guest does with a query

D3D issues a ZPD event for the BEGIN record (slot + 0x20), the query's draws,
then a ZPD event for the END record (the 64-byte slot base). Each record is
`xe_gpu_depth_sample_counts`: eight little-endian dwords, ZPass at +16/+20.

`sub_823CF3F0` (D3D `GetData`, type 9) returns "not ready" while the GPU fence
recorded in the query object (`query+24`) has not passed, kicking the command
buffer when the query is still in the open segment. After the fence it sums
`END.ZPassA + END.ZPassB - BEGIN.ZPassA - BEGIN.ZPassB` over the query's records
(`query+28`, count at `query+148`) and still reports "not ready" while the last
END record holds the sentinel `0xFFFFFEED` in both ZPass halves. Its caller
`sub_823CF390` loops without a timeout. UE3 (`sub_823CE960`) clears a
primitive's visibility bit when the result is zero.

In Uhra the game issues about 256 queries per frame and reads them in the same
frame, right after issuing them (main render flow: depth prepass, query
results, base pass).

## Who owns a query record (v0.7.35 flash, #118)

The renderer's query pool `sub_823CCCE8` (pool pointer at `0x83235AB8`) hands
out query objects in request order, `pool[index++]` with the index at
`pool+0x20` back at 0 every frame. Each object keeps the record slot it got at
creation (`query+28`, a `0xA0000000`-based address of the 64-byte slot), so a
slot belongs to whichever object asks first in that frame. Three call sites
allocate:

| Return address | Caller | Queries |
|---|---|---|
| `0x823CC2D0` | `sub_823CBF68` (primitive occlusion pass) | single primitives |
| `0x823CD744` | `sub_823CD718` (batched boxes, 8 vertices each) | groups of primitives |
| `0x823D5688` | `sub_823D5430` (sun flare) | one per frame, after all others |

The sun flare draws a 4-vertex quad around the sun inside its query and reads
the result at once (`sub_823CF390(query, 0x83302ADC, 1)`). v0.7.35 answered
Fast queries with the last count of the same record slot. While the camera
stood still the sun kept its slot (pool index 433 in the #118 save); while
sailing the number of primitive queries before it changed, the sun inherited a
primitive's slot and read that primitive's count, and the flare flashed
through the cliff for a frame. The trackers' `fallback` counter stayed 0, so no
unmeasured query was involved.

The hook on `sub_823CCCE8` now reports each allocation's owner, its return
address and its ordinal among that call site's allocations since the pool
index was 0 (`OwnerKey`), and Fast answers are kept per owner. The sun always
has owner (`0x823D5688`, 0). Primitive owners still shift among themselves
when the visible set changes, which only matters for magnitudes, since Fast
never culls. A record without a reported owner keys on its slot as before.

## Implementation

- Plume (`tools/patches/plume-lostodyssey.patch`):
  `RenderDevice::createOcclusionQueryPool`,
  `RenderCommandList::beginOcclusionQuery` / `endOcclusionQuery` and the
  `occlusionQueries` / `occlusionQueryPrecise` capabilities. D3D12 uses an
  `OCCLUSION` query heap and resolves each query into a readback buffer when it
  ends. Vulkan uses a `VK_QUERY_TYPE_OCCLUSION` pool, begins the render pass
  before the query, uses `PRECISE` when the device supports it, and reads results
  with availability. The defaults return no pool, so Metal compiles unchanged and
  the renderer falls back to the fake counts.
- Renderer: each GPU slot owns four 1024-query pools (4096 measured draws per
  batch; v0.8.53 had one pool of 1024, which the White Boa's Queen's Room
  filled). Pools used since their last reset are reset when the slot's command
  list opens. A guest draw inside a guest query is wrapped in its own host query;
  the results are read when the slot's fence has completed (`RecycleSlot`), from
  the pools in use only: reading a pool costs time for every query in it, used
  or not (RADV on a Radeon 8060S: 110–290 µs per frame for one 4096-query pool,
  30–60 µs for the pools in use).
- `gpu/occlusion_queries.h` keeps the bookkeeping, tested by
  `tools/tests/occlusion_queries_test.cpp` (review-regressions):
  - Host samples are converted with the target's guest/host size ratio times the
    guest MSAA sample count (host targets are single-sampled). Any passing host
    sample keeps at least 1.
  - A query with a draw that returned before its host query, no draws, an END
    without BEGIN, an unavailable result or a failed batch reads as visible
    (`0x10000`), as before.
  - A BEGIN for a slot whose previous query is still pending drops the old
    result: the record belongs to the new query.

## Modes (`LO_ZPD_MODE`)

| Value | END record | Culling | Uhra cost (D3D12, 2560x1440, uncapped, ABBA) |
|---|---|---|---|
| unset / `host` (default) | Fast, Strict while most queried objects read zero (below) | Only in Strict stretches | as `fast` in Uhra (it stays Fast) |
| `fast` (the default before adaptive) | Written at the event with the last measured count of the same record; 1 when that was zero or unknown | Never: no query reads zero | 128.8 FPS vs 130.3 fake (about 1%) |
| `strict` | Written once the host GPU has counted it; the guest waits | With exact counts | 111.2 FPS vs 131.7 fake (about 15%) |
| `grow`, `xenia`, `begin0`, `none` | Old fake counts, no host queries | Never | baseline |

Fast follows Xenia's default `fast` mode: counts used as magnitudes, such as
flare visibility, follow the real ones a frame or two late, while culling is
unchanged from the fake counts, so a stale zero can never hide an object.

### Adaptive (default since 2026-10-08)

Fast never culls, so the game draws every object its occlusion pass tests, also
those the console would skip. In most places that costs little (Uhra: 15% of
about 350 queries read zero). In a room seen from its doorway it dominates: the
White Boa's Queen's Room spawn issues about 950 queries per frame with 80% zero,
and its "You wanna go back?" conversation camera in the Guest Area about 1140
with 81% zero. Strict there draws 60–65% fewer objects (3411 → 1206 and 4599 →
1847 draws), and the render thread, which is the limit in these views, gets that
much faster. Where few objects are hidden, Strict's mid-frame wait for the host
GPU costs more than it saves.

`gpu::occlusion::AdaptiveMode` decides once per guest frame from the queries
completed over 32-frame windows; frames that complete no queries (menus,
loading screens, fades) do not count. Fast switches to Strict after two windows
in a row in which at least 50% of the queries read zero with at least 256 zeros
per frame; Strict switches back after two windows below 20% zero (with Strict
the game tests hidden objects in batches, so the same room reads about 35%
zero). After leaving Strict, Fast holds for 240 frames, doubling (up to 16x)
each time Strict lasted under four windows. Queries left waiting by Strict keep
waiting after a switch until their batch completes; the command processor is
woken for a waiting guest only while such a record exists.

Radeon 8060S (psvita), native Linux Vulkan, 1280x720 output, FSR native AA,
120 FPS target, average frame time and draws per view, two runs each:

| View | Fast | Adaptive | Strict |
|---|---|---|---|
| Uhra boot view (15% zero) | 10.2 / 10.1 ms, 2142 draws | 9.8 / 9.7 ms, stays Fast | 11.7 ms, 1973 draws |
| Queen's Room spawn (80%) | 10.0 / 10.7 ms, 3411 draws | 8.6 / 8.6 ms (at the cap), 1206 draws | 8.6 ms, 1206 draws |
| Queen's Room, inside (5%) | 9.0 / 9.2 ms, 2110 draws | 8.8 / 9.0 ms, stays Fast | 10.4 ms, 2033 draws |
| Queen's Room stairs (44%) | 9.6 / 9.6 ms, 2986 draws | 9.6 / 9.6 ms, stays Fast | 9.2 ms, 1924 draws |
| Guest Area conversation (81%) | 14.1 / 13.9 ms, 4599 draws | 10.2 / 10.0 ms, 1847 draws | 10.3 ms, 1847 draws |

Strict needs the command processor to complete records while the guest polls:
the `GetData` hook (`debug/query_trace.cpp`) flags a waiting guest, and the
command processor flushes the open batch and waits for its fence when it is idle
or about to block in `WAIT_REG_MEM`. Because the game reads its queries in the
same frame, every frame then waits for the host GPU to finish its depth prepass
and queries (1.6 ms per frame in Uhra). Submitting the work before the first
query earlier did not shorten that wait and lowered the rate further (about
104 FPS), so it was removed. Any failure (device loss, failed submit or wait,
shutdown) writes the waiting records as visible, so a guest never waits forever.

## Validation so far

- Vulkan and D3D12, Uhra, 60 FPS cap: no errors; about 4.6% of queries measured
  zero in strict mode and 14.6% with the default (more primitives are drawn when
  nothing is culled).
- Screenshots at swap 1800, default and strict against `grow` on D3D12: only
  animated characters, swaying foliage, the animated sign and their shadows
  differ; no static object disappears.
- The query proxy draws have no pixel shader. NVIDIA counts their samples; Xenia
  binds an empty pixel shader for such draws because some drivers do not, which
  this change does not do yet.
- #118 world-map flare, Vulkan, 1600x900: the user saved at the reported spot
  (slot 08, Twilight Ocean, disc 4), where the sun sits behind a cliff next to
  the Nautilus. Loading it with `LO_ZPD_MODE=grow` drew the sun disc and a warm
  glare over the cliff in all three screenshots taken 3–6 s after loading; the
  default mode showed the dark cliff with no sun and no glare in all three.
  Strict mode was not run there.
- #118 follow-up (2026-10-02), AMD Radeon 8060S (Strix Halo) on Linux:
  Windows build through Proton (vkd3d-proton D3D12 on RADV, not the native
  AMD D3D12 driver the reporter uses), slot 08, sailing with the left stick
  for 23 s and a screenshot about every 2 frames. v0.7.35: single-frame sun
  and glare flashes through the cliff at swaps 1175, 1179 and 1184 (+28 mean
  luminance); with owners: none, and the brightness curve otherwise matches.
  A static camera showed no flash with either build, which is why the first
  check missed it. The same sail on the RTX 5080 (D3D12) flashed once at swap
  974 with the #122 build and not with owners, so the bug is not
  vendor-specific. Native Vulkan (RADV) with a static camera was also clean.
- Not checked: native AMD D3D12 driver, Intel GPUs, exclusive fullscreen. The disc 1 jail scene and the disc 3 train fight, which Xenia's
  "Disable Occlusion Queries" patch for this game mentions, have not been run
  in strict mode.
