# Render pass merging phase 0: tablet measurements

[简体中文](render-pass-merging-phase0-2026-10-10.zh-CN.md)

- Date: 2026-10-10, device time 17:24–17:53 (about 29 minutes, 25 launches).
- Device: Lenovo TB321FU (Adreno 750), Turnip `v26.0.0 R8` (KIMCHI build, Vulkan 1.4.335), loaded with adrenotools. The maintainer's phone was not touched.
- Code: v0.9.22 (apk-v100 tree) plus the diagnostic patch `apply_diag_q.py`, APK `lo-0.9.22-dev-v100-diagq.apk`.
- Scene: Uhra residential area, after an automatic load, standing still. Native 2560x1600 (profile `default`), plus a separate set at internal 1080.
- Thermal state: as the maintainer asked, the device was not warmed up separately and did not wait for cooling. Runs were back to back. The first run (Q01) started from a cold device (thermal control status 0, body 35 °C). From the second run on, the status was 3 (body 50→55 °C). At the end, Q01 was re-run as Q14 as a sandwich check.
- Raw data, scripts and per-run records are in `apk-v100\out\perf\`, labelled `Q*`. The table is in the section at the end of `PERF_LOG.md`.

---

## 0. Conclusions

### Question 1: Can the main view get faster with GMEM? No, not even after merging passes

- **With GMEM forced, the main view is slower in every pass structure** (native resolution, hot state, per-frame timing per pass):

  | Main view | sysmem (Turnip's current choice) | Forced GMEM | Change |
  |---|---|---|---|
  | Current, 78 passes (Q08 → Q11) | 29.4 ms | 76.7 ms | +161% |
  | After KEEP_COLOR=1 merge, 12 passes (Q07 → Q12) | 29.2 ms | 41.9 ms | +44% |
  | Same, internal 1080 (Q20/Q23 → Q22) | 16.3–17.8 ms | 30.7 ms | +75–90% |

- **The large passes lose on their own.** There are only 3 of these passes, and the load and store of a single pass is a small part of their time. So the loss is not caused by load/store.
  - Z-prepass (1090 operations, depth only): 4.17 → 5.81 ms (×1.39).
  - Main colour pass (620 draws): 7.72 → 10.06 ms (×1.30).
  - Second colour pass (359 draws): 4.36 → 6.15 ms (×1.41).
  - At 1080 it is worse, ×1.7–2.2.
- **The only place GMEM wins** is the lighting pass produced by the KEEP_COLOR merge (71 draws, FP16 blending): at native resolution 5.21 → 4.90 ms (−6%). But at 1080 it also loses (2.52 → 3.17 ms).
- **Why it loses:** u_trace shows that the main view's FP16 + D32S8 attachments need 22 bins at 2560x1636 (14 bins with colour only). Each bin replays the draws that fall inside it. Thousands of draws times 22 bins adds more cost than the bandwidth GMEM saves. When resolution drops, the bandwidth saved shrinks with pixel count, but the replay cost stays the same. So at 1080 it loses more.
- **Best-case estimate** (derivation in §2): merge the main view into one pass and assume all loads and stores are removed except one. GMEM would still need about 33 ms, about 13% slower than sysmem's 29 ms. If each pass is picked individually as whichever of sysmem or GMEM is faster, the saving is at most about 0.3 ms, which is 1% of the main view.
- **Conclusion:** on Adreno 750 with Turnip, merging passes brings no gain from being able to use GMEM, let alone a multiple. The 25–30 ms of the main view is the real shading and bandwidth cost under sysmem: FP16 colour, D32S8 depth, about 2300 operations. Fixed per-pass overhead is not the main cause.

### Question 2: Why is KEEP_COLOR 9% slower on Adreno? It does not reproduce on the tablet, where it is essentially even

- **On the tablet (Adreno 750, Turnip 26.0.0), KEEP_COLOR=1 is not slower.** The most reliable evidence is two ABBA sets, in the first two rows of the table:

  | Comparison (all hot, status 3) | KEEP_COLOR=0 FPS | KEEP_COLOR=1 FPS | Difference |
  |---|---|---|---|
  | Forced sysmem, native, ABBA (Q05/Q08 vs Q06/Q07) | 20.8, 20.2 | 20.6, 20.5 | +0.2% |
  | autotune, internal 1080, ABBA (Q19/Q24 vs Q20/Q23) | 26.4, 26.0 | 26.4, 26.0 | 0% |
  | sysmem + nolrz, native (Q09/Q10, adjacent runs) | 20.1 | 20.3 | +1% |
  | autotune, native (Q04 vs Q02/Q03) | 20.9 | 22.0, 21.6 | +3–5%. But the two K1 runs came first, with the body at 50–51 °C, and Q04 was at 52 °C. This stretch warmed fastest, so the whole difference cannot be credited to K1 |

- **The pass count did drop:** from about 308 to about 248 per frame. The main view went from 78 passes to 12. The Z-prepass picked up a colour attachment. About 67 alternating passes were merged into 5, one of which is the 71-draw lighting pass.
- **Main-view GPU time**, normalised against the unaffected passes:
  - In the sysmem ABBA sets: ±0.
  - Under autotune: −8%. But this set is Q03 vs Q04, with a 1 °C warm-up in between, and only single frames. It is less reliable than the sysmem set.
  - Both are within the single-frame timing noise (±6%).
- **Neither hypothesis holds:**
  - Hypothesis 1 (autotune chooses GMEM): u_trace shows that under both KEEP_COLOR=0 and 1, autotune chooses sysmem for **100% of passes** (Q16, Q17, Qsmoke).
  - Hypothesis 2 (depth-only framebuffers have a faster path under sysmem, for example LRZ): under forced sysmem and nolrz, K1 is also not slower. And turning off LRZ for the whole frame changes only about 0.5% (Q08 20.2 vs Q09 20.1).
  - Note: whether `nolrz` really took effect is confirmed only as far as the string `TU_DEBUG=sysmem,nolrz` reaching the driver (the `diag env` line in the log). `gmem` and `perf` sent along the same path do show their effects, but these runs did not use u_trace to check the `lrz` field.
- **Likely source of the −9% on the phone** (speculation, not verified on the phone):
  - The phone uses PurpleVK (Mesa 26.3-devel). Upstream changed autotune in 2026 (`TU_AUTOTUNE_ALGO`).
  - On the tablet, the Z-prepass with a colour attachment after K1 is 2.8 times slower once it uses GMEM (3.6 → 10.1 ms, +6.5 ms).
  - If PurpleVK puts this pass, or another large merged pass, on GMEM, that would make it 9–15% slower.
  - Another possibility: according to the 2026-10-07 record, the phone experiment came before PRs #313/#315. These two PRs made depth-only passes about 20% faster, which changes the relative cost of the two structures.
  - It could also be a difference in the Adreno 840 (a8xx) hardware itself, which cannot be measured on the tablet.
- **Recommendation: repeat on the phone** (needs the maintainer's consent): run KEEP_COLOR=0/1 each with `LO_TU_DEBUG=sysmem`, then run u_trace again (this round's `LO_ENV_MESA_GPU_TRACES`) and check `tiledRender` for each pass under K1. If the −9% disappears when sysmem is forced, the cause is autotune.

### CPU: recording cost by pass type (render thread, native, K0, median of 14 frames)

- Total recording per frame is about 35 ms (including the overhead of the trace itself; the `draw_ms` median in the same period was 29–31 ms). At internal 1080 it is almost the same (34 ms), which shows this part does not depend on resolution.
- By category:

  | Category | Pass count | CPU ms/frame | Notes |
  |---|---|---|---|
  | Main-view large passes (Z-prepass + two colour passes, about 2070 operations) | 3 | 23.9 | 8–15 µs per draw, the bulk of recording |
  | Main-view alternating passes | 72 | 3.6 | 30–70 µs per pass |
  | Shadow / off-screen depth | 57 | 2.4 | |
  | **Class A EDRAM alias clear** | **139** | **2.2** | About 16 µs per pass. Of this, A2 (depth alias clear) 1.44 ms, A1 0.41 ms, A3 0.18 ms |
  | Main-view colour-only passes | 4 | 0.95 | |
  | present | 1 | 0.5 | |
  | Small RT, transfer, resolve blit | 27 | 1.1 | |

- **Per-pass boundary cost:** KEEP_COLOR removes 60 passes, and the CPU measured by trace drops from 35.4 to 34.9 ms, which is about 8–10 µs per pass. This is the figure this report uses.
  - Each class A pass is about 16 µs higher, because it also includes the renderer logic of alias traversal, `MapDepthClear` and rectangle merging.
  - `draw_ms` is also about 0.5–1 ms lower, in the same direction. But the K1 runs came first and were cooler, so this difference is the same size as the thermal drift. It is supporting evidence only.
- Note: a pass starts at its first draw, so the "inside" time of each pass also includes the preparation work for the first draw of the next pass. When totalled by category, this shifts by one pass. The effect on the estimates for class A and alternating passes is within ±30%, and it has no effect on the total.

### LRZ per-frame baseline (`TU_DEBUG=perf`, Q15, native, K0, 433 frames in the window)

| Reason | Count per frame |
|---|---|
| `CmdClearAttachments` (turns off LRZ for the whole pass) | 72.8 |
| `stencil write based on depth test` (the rest of this pass does not write LRZ) | 43.8 |
| `FS writes depth or has side-effects` | 34.8 |
| `Depth write + ALWAYS/NOT_EQUAL` | 22.5 |
| `Stencil may kill fragments` | 2.0 |

- Consistent with the result of recounting the window the same day for P3 (the normal APK): 72.9 / 44.0 / 34.9 / 22.6 / 2.0. Q15 used the diagnostic APK, but the numbers on both sides are the same, which shows these counts are not affected by the diagnostic patch.
- But **LRZ is worth very little to the whole frame:** under sysmem, turning off LRZ for the whole frame (`nolrz`, Q08 20.2 → Q09 20.1 FPS, adjacent runs) differs by only about 0.5%, within drift. The main view has a Z-prepass, and the colour passes are already fine with early-Z. So the LRZ gain from phase 1 removing `CmdClearAttachments` is negligible.

### Other switches

- `LO_VK_NO_END_DEP=1` (Q13): 19.9 FPS, and the sandwich Q14 was also 19.9. The main-view/other ratio is 1.98 vs 1.96. **No measurable difference.**
- `TU_AUTOTUNE_ALGO`: **does not exist** in Turnip 26.0. It is not in the `tu_debug_options` table of 26.0, and the strings `prefer_sysmem`/`prefer_gmem` cannot be found in the driver binary.
- Switches in 26.0 relevant to this question (source: `src/freedreno/vulkan/tu_util.cc`, 26.0 branch, taken from the GitHub mirror `AHMETBAYIR/mesa-mirror` and checked against the strings in the driver `vulkan.ad07xx.so`): `sysmem`, `gmem`, `nolrz`, `nolrzfc`, `nobin`, `forcebin`, `nobinmerging`, `perf`, `log_skip_gmem_ops`, `nocb`/`forcecb` (concurrent binning). This round used `nolrz`.
- For future A/B tests, `TU_DEBUG_FILE` can be used. It supports switching `sysmem/gmem/nolrz/perf` and other flags at runtime, without restarting the process (code in `tu_runtime_debug_flags`).

### Recommendations

- **Phase 1 (deferred clears + loadOp=CLEAR): go ahead, but lower the expectation and change the reasoning.**
  - Do not rely on GMEM or LRZ. Autotune already picks sysmem for everything, and LRZ is barely worth anything.
  - What can actually be saved:
    - GPU: at native hot state, class A is 2.3–3.0 ms, which is 6–7% of pass time. Only the part that no longer needs to run can be saved.
    - CPU: class A is about 2.2 ms on the render thread, 6%.
  - Under native (GPU-limited), expect at most about 5% faster.
  - At settings like 720p / 1080 that are close to the render-thread limit, the CPU part is more useful: at 1080 hot, `draw_ms` is 30 ms, longer than the GPU's 27–28 ms.
  - Apple gains will be measured separately.
- **Phase 1b**: the number of passes in the shadow section will drop, but the shadow section's total GPU time is only about 1 ms, so the GPU gain is small. On the CPU, the shadow section is 2.4 ms, which is the main reason to do it.
- **Phase 2a (merge main-view alternating passes): not recommended on Adreno.**
  - Even on the tablet, and GMEM does not help.
  - On the phone it may even get slower because of the driver's autotune.
  - Keep it on by default on Apple and off by default on Android. Decide after the phone repeat (see Question 2).
- **Phase 2b (draw transfer into the target pass)**: G class is 3.3 ms, but the transfer itself still has to be drawn. Under sysmem, what is saved is only the pass boundary, estimated at under 0.5 ms (estimate). Low priority. A "constant transfer" (the §2.4 design) could remove the transfer itself, and this part is still worth counting first in phase 1.
- **Phase 3 (tune GMEM, storeOp, and in-pass resolve for Turnip): recommend dropping.**
  - Even if the main view is merged, and even if load/store is removed, GMEM is still slower than sysmem.
  - The only lighting pass that wins saves just 0.3 ms.
- **API-level rewrite: not worth doing for pass merging.**
  - The data shows that pass structure is not the bottleneck on Adreno:
    - Under sysmem, pass boundaries, tail dependencies and LRZ together are within noise.
    - In this scene, GMEM loses overall.
  - Gains on the scale of a multiple can only come from two ends:
    - GPU cost per pixel: the 8-byte-per-pixel FP16 host format and shading. But changing the format cannot guarantee byte-for-byte identity. Lowering resolution is not accepted by the maintainer.
    - CPU recording cost per draw: 8–15 µs per draw, times 2300 draws, is about 25 ms.
  - If a rewrite is considered later, the target should be the CPU cost per draw and the GPU cost per pixel, not how render passes are organised.

---

## 1. Data

### 1.1 Native resolution timing (20 s window, FPS and so on are window medians; trace columns take the first traced frame of each run)

| run | Config | Thermal start → end (status/body) | FPS | Frame interval median (p90) ms | draw_ms | GPU queue ms | gpubusy | pass/frame | Main-view pass / GPU ms | Class A pass / GPU ms |
|---|---|---|---|---|---|---|---|---|---|---|
| Q01-base0 | autotune, K0 | 0/35 → 0/46 °C | 24.7 | 41 (46) | 25.2 | 39.5 | 100% | 309 | 79 / 25.5 | 139 / 2.66 |
| Q02-keep1 | autotune, K1 | 3/50 → 3/51 | 22.0 | 45 (53) | 29.0 | 43.7 | 99% | 247 | 12 / 24.3 | 138 / 2.66 |
| Q03-keep1 | autotune, K1 | 3/51 → 3/51 | 21.6 | 46 (53) | 29.2 | 44.8 | 99% | 248 | 12 / 23.9 | 141 / 2.59 |
| Q04-base0 | autotune, K0 | 3/52 → 3/52 | 20.9 | 48 (55) | 29.5 | 44.4 | 96% | 308 | 78 / 27.7 | 138 / 2.84 |
| Q05-sys0 | sysmem, K0 | 3/52 → 3/52 | 20.8 | 48 (55) | 30.0 | 44.2 | 96% | 308 | 79 / 21.4 | 141 / 2.27 |
| Q06-sys1 | sysmem, K1 | 3/52 → 3/52 | 20.6 | 49 (55) | 29.7 | 44.6 | 96% | 248 | 12 / 25.9 | 138 / 2.66 |
| Q07-sys1 | sysmem, K1 | 3/53 → 3/53 | 20.5 | 49 (56) | 29.8 | 44.9 | 96% | 248 | 12 / 29.2 | 139 / 2.91 |
| Q08-sys0 | sysmem, K0 | 3/53 → 3/53 | 20.2 | 50 (56) | 30.4 | 44.8 | 94% | 308 | 78 / 29.4 | 138 / 2.99 |
| Q09-nolrz0 | sysmem+nolrz, K0 | 3/53 → 3/53 | 20.1 | 50 (58) | 30.4 | 46.2 | 96% | 308 | 78 / 30.5 | 138 / 3.07 |
| Q10-nolrz1 | sysmem+nolrz, K1 | 3/54 → 3/54 | 20.3 | 49 (57) | 30.2 | 45.9 | 96% | 247 | 12 / 30.6 | 138 / 3.17 |
| Q11-gmem0 | gmem, K0 | 3/54 → 3/53 | 9.1 | 110 (115) | 26.8 | 108.0 | 100% | 307 | 78 / 76.7 | 141 / 16.19 |
| Q12-gmem1 | gmem, K1 | 3/54 → 3/54 | 12.0 | 77 (82) | 23.4 | 75.5 | 100% | 251 | 12 / 41.9 | 143 / 16.27 |
| Q13-noend0 | autotune, K0, NO_END_DEP=1 | 3/54 → 3/54 | 19.9 | 50 (59) | 31.2 | 46.7 | 96% | 308 | 78 / 30.1 | 138 / 3.09 |
| Q14-base0 | autotune, K0 (sandwich) | 3/54 → 3/54 | 19.9 | 50 (58) | 30.9 | 47.0 | 97% | 307 | 78 / 30.0 | 138 / 3.00 |

Drift:

- Q01 is a cold start at 24.7 FPS and is not used in comparisons.
- From Q04 to Q14, the same configuration fell from 20.9 to 19.9 (−5%), about −0.5% per run. All comparisons are adjacent or in ABBA order.

### 1.2 GPU time by category (trace n=0, pass count / ms)

| Category | autotune K0 (Q04) | autotune K1 (Q03) | sysmem K0 (Q08) | sysmem K1 (Q07) | GMEM K0 (Q11) | GMEM K1 (Q12) |
|---|---|---|---|---|---|---|
| A1 depth fill colour alias clear | 54 / 2.18 | 54 / 2.01 | 54 / 2.30 | 54 / 2.27 | 54 / 8.94 | 54 / 8.74 |
| A2 depth alias clear | 68 / 0.48 | 69 / 0.41 | 68 / 0.49 | 69 / 0.45 | 69 / 3.86 | 71 / 3.73 |
| A3 colour clear rectangle replay | 17 / 0.19 | 17 / 0.18 | 17 / 0.20 | 17 / 0.20 | 17 / 3.39 | 17 / 3.80 |
| B main view colour+depth ≥100 operations | 2 / 11.20 | 3 / 13.08 | 2 / 12.08 | 3 / 16.30 | 2 / 16.20 | 3 / 25.08 |
| B main view colour+depth <100 | 37 / 6.71 | 5 / 6.48 | 37 / 7.16 | 5 / 7.80 | 36 / 32.72 | 5 / 11.17 |
| B main view depth-only ≥100 (Z-prepass) | 1 / 3.96 | merged into the row above | 1 / 4.17 | merged into the row above | 1 / 5.81 | merged into the row above |
| B main view depth-only <100 | 35 / 1.36 | - | 35 / 1.45 | - | 34 / 16.56 | - |
| B main view colour-only | 4 / 4.52 | 4 / 4.35 | 4 / 4.56 | 4 / 5.13 | 4 / 5.39 | 4 / 5.62 |
| C shadow / off-screen depth | 57 / 1.08 | 38 / 0.75 | 57 / 1.13 | 38 / 0.89 | 58 / 3.46 | 38 / 2.41 |
| E small RT | 11 / 2.90 | 36 / 2.91 | 11 / 3.10 | 36 / 3.49 | 11 / 3.39 | 37 / 5.91 |
| F resolve blit | 8 / 2.39 | 8 / 2.31 | 8 / 2.42 | 8 / 2.70 | 8 / 2.62 | 8 / 3.20 |
| G EDRAM transfer | 8 / 3.33 | 8 / 3.22 | 8 / 3.48 | 8 / 3.45 | 8 / 4.41 | 8 / 4.21 |
| I present | 1 / 1.54 | 1 / 1.36 | 1 / 1.63 | 1 / 1.73 | 1 / 1.66 | 1 / 1.68 |
| **Class A total** | 139 / 2.84 | 140 / 2.59 | 139 / 2.99 | 140 / 2.91 | 140 / 16.19 | 142 / 16.27 |
| **Main-view total** | 79 / 27.74 | 12 / 23.92 | 79 / 29.43 | 12 / 29.23 | 77 / 76.69 | 12 / 41.87 |
| **Pass total (plus gaps between passes)** | 301 / 41.8 (+4.3) | 244 / 37.1 (+4.1) | 301 / 44.2 (+4.4) | 242 / 44.4 (+4.7) | 304 / 108.4 (+4.0) | 247 / 75.6 (+4.0) |

- Under K1, "small RT" goes from 11 passes to 36. Off-screen depth-only draws also kept a colour attachment, so they moved from category C to category E.
- AO, TAA and present sites are now marked separately. present is confirmed to fall on the site in presentation.cpp (200861). With default settings AO and TAA are both off, so there is no H class in the table. "GTAO is no longer mislabelled" has not yet been verified in practice with AO turned on.
- **Noise in single-frame traces:** the same configuration (sysmem K0) in Q05 and Q08 gave trace totals of 33.4 and 44.2 ms, a 30% difference. The GPU frequency state in a single frame has a large effect. So FPS conclusions use only the 20 s window. When comparing traces, use the ratio of "main view / other passes" (`qnorm.py`): autotune K0 is 1.96 / 1.97, K1 is 1.79 / 1.82; sysmem K0 is 1.78 / 1.99, K1 is 1.91 / 1.93.

### 1.3 Large main-view passes: sysmem and forced GMEM compared one by one (native, hot)

| pass | Operations | sysmem ms | GMEM ms | Multiple |
|---|---|---|---|---|
| K0 Z-prepass (depth only, Q08→Q11) | 1090 | 4.17 | 5.81 | ×1.39 |
| K0 main colour pass | 620 | 7.72 | 10.06 | ×1.30 |
| K0 second colour pass | 359 | 4.36 | 6.15 | ×1.41 |
| K0 single-draw depth-only alternating pass (average per pass) | 1 | 0.043 | 0.486 | ×11 |
| K0 single-draw colour+depth alternating pass (average per pass) | 1 | 0.194 | 0.875 | ×4.5 |
| K1 Z-prepass (with colour attachment, Q07→Q12) | 1090 | 3.59 | 10.10 | ×2.81 |
| K1 main colour pass | 620 | 8.29 | 9.50 | ×1.15 |
| **K1 lighting pass after merging** | 71 | 5.21 | **4.90** | **×0.94** |
| K1 second colour pass | 359 | 4.42 | 5.48 | ×1.24 |

Corresponding multiples at internal 1080: K0 is ×2.18 / ×1.84 / ×1.72; K1 Z-prepass ×2.78, main colour ×1.81, lighting ×1.26, second colour ×1.79.

### 1.4 Internal 1080 (main view 1920x1227)

| run | Config | FPS | draw_ms | GPU queue ms | gpubusy | Main-view pass / GPU ms |
|---|---|---|---|---|---|---|
| Q19-1080b0 | autotune, K0 | 26.4 | 30.3 | 27.9 | 82% | 78 / 15.9 |
| Q20-1080k1 | autotune, K1 | 26.4 | 30.1 | 27.3 | 81% | 12 / 16.3 |
| Q21-1080g0 | gmem, K0 | 13.8 | 27.6 | 66.9 | 99% | 78 / 49.6 |
| Q22-1080g1 | gmem, K1 | 18.2 | 27.9 | 52.1 | 100% | 12 / 30.7 |
| Q23-1080k1 | autotune, K1 | 26.0 | 30.5 | 27.2 | 79% | 12 / 17.8 |
| Q24-1080b0 | autotune, K0 (sandwich) | 26.0 | 30.6 | 28.3 | 82% | 78 / 17.4 |

At hot 1080, `draw_ms` (30 ms) is longer than the GPU time. gpubusy is about 80%, so the render thread is also one of the bottlenecks at this point.

### 1.5 Turnip u_trace (`MESA_GPU_TRACES=print_csv`, only the `render_pass` trace point enabled; the GMEM run also enabled load/store/draw sub-events; the last 20 complete frames were taken)

| run | Passes per frame | Share choosing GMEM | Notes |
|---|---|---|---|
| Qsmoke / Q17 (autotune, K0) | 304–310 | 0% | `tilingDisableReason` is "Autotune selected sysmem" in every case. Main-view passes with ≥100 draws had LRZ on 100% of the time. |
| Q16 (autotune, K1) | 248 | 0% | Merged passes were also all sysmem. |
| Q18 (forced GMEM, K1) | 250 | 100% | The main-view FP16+D32S8 pass has 22 bins; FP16 alone has 14; small targets have 2. Each bin loads FP16 colour in about 30–40 µs. Depth load and store are almost invisible in the timestamps (asynchronous). |

### 1.6 CPU recording time: K0 vs K1 (ms per frame, median across frames)

| Category | K0 (7 runs, 14 frames) | K1 (5 runs, 10 frames) |
|---|---|---|
| Total | 35.4 | 34.9 |
| Main-view total | 29.0 | 28.6 |
| Class A total | 2.18 (139 passes) | 1.88 (139) |
| A2 depth alias clear | 1.44 (68) | 1.26 |
| A1 | 0.41 (54) | 0.41 |
| A3 | 0.18 (17) | 0.15 |
| Shadow / off-screen depth | 2.39 (57) | 2.23 (38) |
| transfer | 0.40 (8) | 0.38 |
| resolve blit | 0.24 (8) | 0.22 |

---

## 2. Derivation: how much could be saved by merging the main view into a single-digit number of passes and running it on GMEM

Take the KEEP_COLOR=1 main view (12 passes, Q07 sysmem 29.2 ms, Q12 GMEM 41.9 ms; the two are similar hot states, body at 53–54 °C) as an approximation of the "merged" case. Estimate in three steps:

1. **Pick per pass:** for each pass take whichever of sysmem and GMEM is faster. Only the lighting pass chooses GMEM, saving 5.21 − 4.90 = **0.31 ms** (1% of the main view).
2. **Assume all loads and stores are removed** (an optimistic upper bound, not achievable in practice, because resolve needs the write-back):
   - The single-draw GMEM colour+depth pass totals 0.875 ms. It can be treated as roughly one full load plus store, about 0.8 ms.
   - This pass also includes binning and the replay of one draw in each of the 22 bins, so 0.8 ms is only an upper bound for load plus store. Subtracting that much makes the 33 ms below a **lower bound** on the merged GMEM cost, so the conclusion is conservative.
   - Of the 12 passes, remove 11 instances: 41.9 − 11 × 0.8 ≈ **33 ms**, still about 13% slower than sysmem's 29.2 ms.
   - At 1080, scaled by pixel count to about 0.45 ms each: 30.7 − 11 × 0.45 ≈ 25.7 ms, while sysmem is 16.3–17.8 ms, about 50% slower.
3. **Why it cannot be saved:**
   - What remains is mostly the per-bin replay cost. In the Q18 u_trace, the K1 Z-prepass under GMEM has 22 bins. The whole pass takes 8.15 ms, of which FP16 load is about 0.8 ms and the remaining about 7.3 ms is executing draws bin by bin. Sysmem takes only 3.6 ms in total.
   - This part does not depend on how the passes are split. It depends only on the number of draws and bins. The bin count is in turn set by the attachment format: FP16 + D32S8 is 12–13 bytes per pixel.

**Uncertainties:**

- Single-frame trace GPU time has ±10–15% noise.
- The load plus store figure of 0.8 ms is estimated by back-calculating from single-draw passes. It was not measured directly (write-back is asynchronous and not visible in u_trace timestamps).
- With GMEM forced, the driver uses GMEM for all passes, including those it would itself choose not to use. This affects only the non-main-view part. The main-view comparison is not affected.
- Only the Uhra scene was measured. In scenes with fewer geometry and more blending, GMEM's relative performance would probably be better.

Even so, GMEM loses 15–40% on the three large passes, and 70–180% at 1080. That magnitude is far larger than the uncertainties above.

---

## 3. Method and limits

- **Diagnostic patch:** `apply_diag_q.py`, built on `apply_diag_v100.py` with the following additions. Reverted with `restore_diag_q.py`.
  - Per-pass CPU time: `steady_clock`, sampled before `vkCmdBeginRenderPass` and after `vkCmdEndRenderPass`. Two segments are recorded: inside the pass, and from the end of the previous pass to the start of this one. The frame-boundary segment (from present to the first pass of the next frame) is listed separately and not counted in any category.
  - Site markers for AO, present and TAA, with offsets 100000, 200000 and 300000. The renderer.cpp site numbers are the same as D1/D2.
  - The trace is triggered with the command file `LO_DIAG_CMD_FILE`, and recorded after the 20 s window ends, so it does not affect the timing window.
  - `LO_ENV_*` is forwarded, to pass `MESA_GPU_TRACES`, `TU_GPU_TRACEPOINT` and similar.
  - At startup one line prints the switch state, to check that KEEP_COLOR and NO_END_DEP took effect. Each run's log has the `diag env:` line.
- **A defect in the APK:** within one run, the second and later frames' traces reused the same query pool and read stale GPU timestamps. So GPU data uses only trace n=0. CPU data is valid for every frame. The script has been changed to use a new pool for each trace and it compiles, but it has not been re-run on the device.
- **logcat occasionally drops lines:** when 300 lines are written at once, logd sometimes loses some. So the analysis scripts prefer the runtime log that is pulled back.
- **Single-frame traces sometimes show idle time at the frame boundary:** between present and the first pass of the next frame, the GPU is sometimes idle for 1.5–12.5 ms, varying between runs and unrelated to KEEP_COLOR. These were excluded from category statistics. No further investigation.
- **Hot state:** the maintainer asked not to warm the device separately. The first run (Q01, cold) is clearly high and is for reference only. The others were all at status 3, body 50–55 °C.

## 4. Files (all in `C:\Users\freefrank\worktrees\LostOdysseyRecomp\apk-v100\out\perf\`)

- Per run: `Q*.logcat`, `.runtime.log`, `.steps.txt`, `.summary.json`, `.cats.txt`, `.thermal`, `.gpubusy`, `.top`, `shots/Q*.jpg`. Raw u_trace CSVs: `Qsmoke/Q16/Q17/Q18.utrace.csv`.
- Scripts:
  - `apply_diag_q.py`, `restore_diag_q.py`: apply and revert the diagnostic patch.
  - `qrun.sh`: a single run.
  - `qseq1.sh`, `qseq2.sh`: the two run sequences.
  - `qcats.py`: summary by category.
  - `qsum.py`: one-line summary per run.
  - `qnorm.py`: main-view normalisation.
  - `utrace.py`: u_trace summary.
  - `qlog.py`: writes `PERF_LOG.md`.
- The "Phase-0 pass-merge measurements" section at the end of `PERF_LOG.md`.
- End state:
  - The tablet has the `lo-0.9.22-dev-v100.apk` reinstalled and was force-stopped. `settings.ini` is the default profile.
  - In the apk-v100 tree only the `LO_TU_DEBUG` forwarding line in `video.cpp` remains (checked with `git diff` and `cmp`).
  - The bash, logcat and tail processes started this session have all ended. Another four tail/grep groups started at 15:01–15:42 are watching `batch.out` and `W0-warmup.steps.txt`. They are not from this session, do not touch the device, and were left alone.
