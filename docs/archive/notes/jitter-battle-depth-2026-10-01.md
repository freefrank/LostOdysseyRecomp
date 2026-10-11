# Battle depth writers and the tour material 8d66 — 2026-10-01

Status: mapped in `temporal_scene.h`, checked by `LoTemporalJitterTest --captured-battle-depth` and `--captured-tour-8d66`. The battle mapping was checked in the game with an A/B run of the opening battle. 8d66 has not been rechecked in the game.

## Battle depth writers 7def and c511

### How they were found

The #117 A/B runs started a new game, skipped the opening movie and played the opening battle (Vulkan, DLAA, Frame rate 120, 1600x900 in a hidden window). Their runtime logs held "temporal suspect" lines of kind `depth_writer` with `camera_slot=8` for three pairs:

| VS + PS | Frame | Camera in the bank |
|---|---|---|
| `7def181705ff29c9` + `7dca56468fd32b5a` | 7373 | full battle view-projection |
| `7def181705ff29c9` + `9b11147dbbe2e170` | 6019 | projection only |
| `c511136caf4421eb` + `c4cba1ceb6ea0825` | 6019 | projection only |

Frame 6019 is the battle's first command frame. The scene camera itself was projection only on that frame, and both pairs were logged there in both runs. The 2026-09-30 telemetry sweep had ranked both VS high: 7def with 1,346 main-depth write records and 7 PS, c511 with 725 records and 2 PS. It had asked for one runtime check that c8-c11 hold the scene camera before mapping them.

### What they broke

An unmapped VS that writes the main scene depth under jitter stops the per-draw motion-vector replay for the whole frame. `LO_MV_LOG=1` in the same opening battle, with the same input script, showed what that cost:

| Build | Motion summaries | Replay ready | Aborted | First failing draw (frames) |
|---|---|---|---|---|
| Before | 63 | 1 | 62 | `7def/9b11` 3,811, `c511/c4cb` 3,321, `7def/7dca` 81 |
| After | 63 | 62 | 0 | none |

Before the mapping, the temporal upscalers got no object motion vectors for the whole battle. The one summary after the mapping that is not ready is the first, while the replay pipeline was still being built. The only suspect line left is `e810/5b11`, which stays held (below).

### Review

- **VS.** Both end with `oPos = P.x*c11 + P.w*c10 + P.z*c9 + P.y*c8`: 7def at HLSL 534-543, c511 at 540-549. P is built from the vertex, the world rows and the c12 eye. c8-c11 reach only `oPos` and one copy, `o2` in 7def and `o4` in c511, so the jitter moves nothing else.
- **Camera.** On each logged draw c8-c11 equal the scene camera. For c511 the only bank is from frame 6019, where that camera is projection only; 7def also has the frame-7373 bank with the full view-projection. A draw whose c8-c11 differ from the scene camera is still rejected with `CameraMismatch` and keeps its constants.
- **PS partners.** Player feedback has nine: 9b11, 0007, 7dca, c99e, 0d1e, 979b and f541 for 7def, and c4cb and fc20 for c511. Each reads only W of the copy and samples at mesh UVs. Five of them kill on alpha (`clip(any(c > r) ? -1 : 1)`, texture alpha times vertex colour), which the clip review used to hold. Only 7dca, 9b11 and c4cb were drawn in these runs. The other six were reviewed statically: their hashes were put on the real 7def and c511 suspect lines and triaged against the HLSL dump of the known shaders.

Both VS join the slot-8 group, which already holds the battle program `1da1ddc75da8e994`. The 2026-09-30 sweep found 7def's chain identical to that program's.

## Tour material 8d66

The map tours logged `8d6658641e3b780d` over the jittered `b030` depth with the same world and camera: with PS `c7956695c6ba859b` at Ice Canyon - Ice Gorge (tour 1), and with PS `9e1cd4d452f7380d` at Frozen Trail (tour 2). Its HLSL 430-447 is the `TireMaterialFf9` chain (world c0-c3, VP c7-c10) and copies the clip to `o1`; both PS read only `i1.w` and sample at mesh UVs. c795 is the only partner in player feedback. It had been held for a `select(c == 0.0, a, b)` statement, which the clip review could not parse. 8d66 joins the slot-7 group.

## Clip review: kills and selects

`audit_ps.py` now reviews two translator forms as reads of their operands: the guest kill `clip(any(a op b) ? -1 : 1)` and the conditional move `select(a op b, x, y)`. They count as clip X/Y reads only when an operand carries those components. Before, every one held its pair, although the HLSL dump of the known shaders contains about 7,300 kills.

Before and after over every tour and battle log, the change moved exactly three pairs from hold to `map_vs_wide`: 7def/9b11, c511/c4cb and 8d66/c795. `tools/tests/test_ps_clip_reads.py` covers both forms and now runs in review-regressions.

## Held

- **`25d2c638dcba4059/725f354714f685ff`** (maps `xzc_0_scrw`, `xzf_0_scrw`). It passes the clip review, but its `oPos` uses slot 2, which no mapped shader uses, and nothing has shown that window to be the scene camera. `tools/shader_analysis/reviews/tour_holds_20261001.json` records it with the new decision `hold`, which keeps `triage_suspect.py` from suggesting it.
- **`e810cfacc107fd3c/5b11f88a8bb293df`** (opening battle). The PS reads the clip copy's X/Y and samples at clip-derived coordinates, a screen-space consumer.
- **Slot-3 depth writers** (041c, 5363, 83b4, 938a, c2a1, d4ec). The log found their camera at c230, the enemy skinning path, not at their `oPos` slot.
- **3305 and ac32.** They were logged only on map-transition frames, with non-finite cameras.

## Validation

- `LoTemporalJitterTest --captured-battle-depth`: 198,151 checks over the three logged banks, 32 phases at 1440p and 4K. The maximum error is 0.0009 px against an old separation of 0.488 px. It also checks that each depth writer gets the same jittered camera as the mapped slot-8 VS 1da1.
- `--captured-tour-8d66`: 2 × 66,241 checks; the maximum error is 0.0003 px.
- Both fail against the previous `temporal_scene.h`. The full run passes (4,795,002 checks), and GCC 16 and clang 22 under ASan/UBSan give the same values.
- In the game: the opening-battle A/B above.
