# Sky pairs and a depth VS from the map tour — 2026-10-01

Status: mapped in `temporal_scene.h`, checked by `LoTemporalJitterTest --captured-tour-sky` and `--captured-tour-batch2`. Not rechecked in the game, and no player has reported these spots yet.

## How they were found

The renderer logs a "temporal suspect" line for each unmapped scene-camera VS/PS pair that writes the main scene depth under jitter or repeats a jittered depth draw's geometry. Until now these lines arrived only from players who happened to stand in the right place. On 2026-10-01 a diagnostic build visited the game's maps one after another with TAA on:

- Of the 316 native map definitions it reached 264. The others did not resolve on any disc, or had no player to jump from.
- Each map got about 7 s, with the camera turning.
- The run collected 36 suspect pairs. The tour driver and its uncommitted map-jump bridge are kept outside the repository.

`tools/capture_analysis/triage_suspect.py` then reviewed every pair against an HLSL dump of the 28,484 known shaders (`LO_SHADER_HLSL_DIR` at startup preparation):

- the `oPos` matrix and the outputs that carry it;
- PS reads of the clip copy's X/Y, and clip-derived texture coordinates;
- the logged depth companion.

The pairs below passed the review; the rest stay held for the reasons the triage printed.

## Sky pairs: the #67 VS with six more PS

All six are `depth_writer_after_jittered_geometry` suspects of VS `bda41a11626a545c` over the jittered `b030ab4e17a20783` depth with the same world and camera, the #67/#121/#130 class:

- The VS's c7-c10 reach only `oPos` and the `o1` copy (HLSL 435-441).
- Each PS reads only `i1.w`.
- Each PS samples only at mesh UVs; the fetch lines are listed in the triage output.
- The only statement the clip review could not parse is the translator's host debug view (`xeFlags`, `i15`).

| PS | Seen at |
|---|---|
| `e086f5f676c72482` | Numara Palace - Facade, Ghost Town, Armored Vehicle, Ipsilon Mountains - Mountain Hut |
| `e2b89a553d00ef47` | Saman - Main Street, Port of Saman |
| `72bcd05d7ab61ce1` | Experimental Staff Marine Division, Ice Canyon - Glacier Fang, The White Boa - Queen's Room |
| `bbbac6693e441760` | Uhra - Amphitheater of the Sky, Grand Staff - Central Connector (also in the user's v0.7.25 logs) |
| `40496f0784d54689` | The White Boa - Main Deck |
| `1693d368b809e65d` | Aurora-Bound Train - Engine Car |

Each is an exact `SkyMaterialPairs` entry with the #67 motion fallback. The 2026-09-30 telemetry sweep had listed e086, 72bc, 1693, e2b8 and a9e9 as unreviewed partners of this VS; the tour gave runtime evidence for four of them and found 4049 and bbba. `bda4/642665c9452ccafb` (seen on map `gh7_0_scrw`) samples at clip-derived coordinates and stays held.

## Depth VS `b9b8056050a4c194`

This VS draws to the main scene depth with the scene camera at slot 4 (Uhra - Army Sewers, Eastern Gohtza Railroad Track). Its HLSL is the mapped alpha-tested depth VS `8d3c80b318235b22` line for line, except for the vertex stride (12 dwords instead of 10). It joins `8d3c` in the slot-4 group, so its depth now gets the same jitter as the materials drawn over it.

## Validation

- `LoTemporalJitterTest --captured-tour-sky`: 6 × 33,029 checks with the logged banks, max jitter error 0.0001–0.002 px against an old separation of about 0.488 px. It also checks that `b9b8` and `8d3c` map to slot 4.
- The case fails against the previous `temporal_scene.h`; the full run passes (4,298,844 checks).
- Not rechecked in the game.

## Second pass: runtime-only variants (2026-10-01)

Thirty-two of the suspect shaders were not among the known shaders, so the HLSL dump did not contain them. These are mostly the stride and attribute-order variants that the 2026-09-30 sweep held for a runtime capture. A second tour visited only the 22 maps where they had appeared, with `LO_SHADER_DUMP_DIR`. That variable keeps the portable pack and writes the microcode of every new shader. `LoShaderTool` translated the 29 that were found, after renaming each file to the renderer's byte FNV hash.

| VS + PS | Seen at | Depth | Mapping |
|---|---|---|---|
| `24418a5936c2d236` + `d7f3f85d208dc73d` | Snow-Covered Trail | `52e4` | VS-wide slot 7 |
| `f8b1457ed05cacdf` + `e5b735783b09888b` | Astral Square, Numara Palace - Facade | `52e4` | VS-wide slot 7 |
| `cbadff38155833b6` + `311b14004ee00284` | Gohtza - Southernmost Cape | `f7fd` | exact sky pair, no fallback |
| `f964d2661094b1a0` | Experimental Staff Marine Division | (depth writer) | slot 4 with `fe3e` |

The four were reviewed as follows:

- **2441 and f8b1.** In both, c7-c10 reach only `oPos` and the `o4` copy, and their only observed PS reads just `i4.w` and samples at mesh UVs. In telemetry and in the tour each VS appeared with that one PS, so they map VS-wide like `61bc`.
- **cbad.** This is the #102 sky program `db23` with a different vertex component order (`r2.xyz` against `r2.zxy`); the position matrix chain is the same. It gets the same exact-pair policy as `db23`.
- **f964.** Its HLSL is the alpha-tested depth VS `fe3e` line for line, except for the 14-dword vertex stride.

Held from this pass:

- **Clip X/Y reads.** Most of the remaining pairs read the clip copy's X/Y and sample at clip-derived coordinates. Among them are the stride variants `3fbb`, `dd47`, `ef71`, `c189`, `0d90`, `2214`, `9bde`, `da5b`, `c4a2` and `83f8`. Those are screen-space consumers, not this class.
- **`25d2/725f`.** Its `oPos` uses slot 2, which no mapped shader uses yet.
- **`7def` and `8d66`.** Another PS partner had guest control flow the clip review could not follow (an alpha-test kill or a `select`). Both were mapped on 2026-10-02 once the review learned those forms; see [the battle depth writers note](jitter-battle-depth-2026-10-01.md).
- **Not reproduced.** `1c00`, `4cca`, `3305`, `ac32` and `c9ed` were not drawn again, or were logged only on map-transition frames with non-finite cameras.
- **Different camera window.** The slot-3 VS whose camera window is 230/233 are a different camera.

Validation: `--captured-tour-batch2` passes (2441 66,241 checks, max 0.0010 px; f8b1 66,241 checks, max 0.0001 px; cbad 33,029 checks, max 0.0026 px, the same with GCC 16 and clang 22 under ASan/UBSan) and fails against the previous map; the full run passes (4,464,365 checks). Not rechecked in the game.
