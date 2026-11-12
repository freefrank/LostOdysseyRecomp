# First-Use Pipeline Stalls: Plan (2026-10-05)

[简体中文](pipeline-first-use-stalls.zh-CN.md)

> Status: P0 and P1 implemented (2026-10-06, see "P0/P1 implementation and measurements" at the end). P2, P3 and P4 implemented (2026-10-06, see "P2 implementation and measurements", "P3 implementation and measurements" and "P4 implementation and measurements" at the end). Android tablet measurements are at the end under "Android measurements" (2026-10-06; the maintainer accepted the effect and decided not to tune further). Affects all platforms: Windows (D3D12 / Vulkan), Linux, macOS, Android.

## Symptom

The first time you enter a map you have not visited, the first time you fight a new enemy, or the first time you watch a certain cutscene, the game stalls for one frame to more than a second. Collecting pipelines with the recipe mechanism described in [Shader preparation](shader-preparation.md) cannot capture all of them, no matter how much is collected.

## Evidence

**Pipeline key.** `gpu::pipeline_cache::Key` consists of these fields:

- Shader pair: VS, PS.
- Render state: blend, depth control (including stencil operations), cull and polygon offset modes, color write mask, primitive type.
- Formats: RT format, depth format.
- Stencil reference value and masks (front and back).
- Depth bias and slope bias.

The comment in the code says "no normalization is done at all."

**Recipe statistics (`pipelines.bin` on Windows; the statistics method is at the end):**

| Source | Recipes | After normalizing irrelevant bits | After removing all dynamically settable state | VS/PS pairs | VS | PS |
|---|---|---|---|---|---|---|
| i212-run (316-map tour) | 1950 | 1903 | 1246 | 1241 | 183 | 1088 |
| Local play install (Vulkan) | 920 | 888 | 548 | 545 | 114 | 497 |

Conclusions:
- State permutations account for only about one third. Removing every field that can be made dynamic reduces recipes by only 36%.
- The missing pipelines mostly come from new VS/PS combinations. The number of combinations grows with maps, enemies and effects, so recording cannot capture them all.
- The number of distinct shaders is much smaller (183 VS, 1088 PS). The shader pack has 28,687 records in total, which is a finite and known set.

**Android (TB321FU, Turnip Mesa 26, Vulkan, with the matching shader pack installed):**

| Case | Interval to the first frame of the scene | Pipelines created in that frame | `pipeline_ms` |
|---|---|---|---|
| First entry to Highlands of Wohl | 1.34 s | 37 | 1056 |
| Second launch, same scene | 0.40 s | 5 | — |

- On the second launch, the startup stage pre-built 213 recipes in 2.5 s (7 workers), about 80 ms each on average. This shows the driver had no disk cache hits, so it recompiled on every launch.
- The remaining 0.4 s is mainly `bind_ms` at 200 ms and `taa_ms` at 161 ms. These are not pipeline problems and are handled separately.

**No backend currently has a persistent driver cache:**

| Backend | Current state |
|---|---|
| plume Vulkan | `vkCreateGraphicsPipelines` is passed `VK_NULL_HANDLE`, so there is no `VkPipelineCache`. Android also does not set `MESA_SHADER_CACHE_DIR`. |
| plume D3D12 | There is no `ID3D12PipelineLibrary`. NVIDIA and AMD drivers have built-in implicit disk caches, so re-entry on desktop is fine, but the first time still stalls. |
| plume Metal | Shaders are compiled from MSL with `newLibraryWithSource`. PSOs use `newRenderPipelineState`. There is no `MTLBinaryArchive`. |

**Other known limits:**
- The `SDL_QUIT` path uses `_Exit`, which bypasses Shutdown. Recipes learned after the last 60-frame checkpoint are lost (see [Shader preparation](shader-preparation.md)). This means the "collected pipelines" are in fact not saved.
- The recipe limit is 16384 entries.

**Driver capability:** The Turnip driver binary on the tablet contains `VK_EXT_graphics_pipeline_library`, `VK_EXT_extended_dynamic_state` 1/2/3, `VK_KHR_pipeline_binary` and `MESA_SHADER_CACHE_DIR`. The implementation must confirm them with device extension queries.

## Goals and acceptance

- Tablet and PC: on the first entry to a map that has not been visited, the per-frame pipeline cost `pipeline_ms` must be below 30 ms.
- First fight with a new enemy: `pipeline_ms` must be below 16 ms for every frame.
- Startup preparation time must not be longer than now. The disk footprint of the persistent cache has a limit and is cleaned up.
- Measurement method: `LO_RENDER_TIMING=1`, with statistics by frame interval and `pipelines=` / `pipeline_ms=`.

## Plan

### P0 Runtime miss log (half a day, do first)

Log one line each time a pipeline is created during drawing. The line includes:

- Frame number, current map ID or battle number.
- VS/PS hashes and the time taken.
- Whether this key is in the recipes, and whether each of the two shaders has been seen before.

Also write a summary script that lists misses per scene. Every later stage is accepted using it.

### P1 Persistent driver cache (all platforms, 1–2 days)

- **Vulkan:** plume creates one `VkPipelineCache` per device. All `vkCreate*Pipelines` calls pass it in.
  - The cache is stored in the shader cache directory, for example `pipeline_cache_vk.bin`.
  - Before loading, validate the Vulkan cache header: `vendorID`, `deviceID`, `pipelineCacheUUID`.
  - Writing to disk happens asynchronously with `SavePipelineRecipes`, and again on normal exit.
  - Can later be replaced with `VK_KHR_pipeline_binary`.
- **Android:** before loading Turnip, set `MESA_SHADER_CACHE_DIR` to the app cache directory. The change is small and uses Mesa's disk cache directly.
- **D3D12:** use `ID3D12PipelineLibrary`, with key names `StorePipeline` / `LoadGraphicsPipeline`. Drivers already have implicit caches, so this has lower priority.
- **Metal:** use `MTLBinaryArchive` to store PSOs.
- **Fix along the way:** before the `_Exit` exit path, flush the recipes and the pipeline cache to disk.

**Effect:** From the second time on, there is almost no stall. Pre-building at startup drops from seconds to hundreds of milliseconds. Combinations seen for the first time will still stall. This stage does not solve that.

### P2 Vulkan graphics pipeline library (main Vulkan fix: Windows, Linux, Android)

Idea: change the compile cost from "once per combination" to "once per shader." The set of shaders is finite and known (they are the records in the pack).

Split the pipeline into four parts:

| Part | Contents | Granularity |
|---|---|---|
| vertex input | Vertex input (Xenos vfetch is done inside the shader, so this part is very small) | Almost one only |
| pre-raster | VS + raster state | One per VS. Cull, front face and depth bias become dynamic state (EDS1 + Vulkan 1.0 core) |
| fragment shader | PS + depth/stencil state | One per PS. Depth test/write/compare and stencil operations use EDS1. Stencil reference value and mask use core dynamic state |
| fragment output | RT/depth formats + blend + write mask | Contains no shaders. Very cheap to create. Created on demand and cached |

- **Miss during drawing:** fast-link the four libraries (no LTO). Expected to take sub-millisecond time. At the same time, compile the full pipeline with `LINK_TIME_OPTIMIZATION` in the background and swap it in when ready.
- **Pre-compiling libraries:**
  - At startup, first build libraries for the shaders that appear in the recipes.
  - Afterwards, build libraries for the remaining shaders in the pack gradually in the background. With the P1 cache, each driver version is compiled only once.
  - When a new shader is seen for the first time (`GetShader` sees it), build its library immediately.
- **plume changes:** add interfaces for "create library pipeline" and "link pipeline". The change is medium to large.
- **Fallback:** if the device does not support GPL or EDS, use the current full pipeline path.
- **Risk:** on some drivers, fast-linked pipelines run slower. Background optimized compilation can make up for it. How much memory 28k libraries take on Android must be measured. If needed, keep only the recipes and the high-frequency shaders from the pack resident.

### P3 Recipe corpus and per-scene prefetch (all platforms; D3D12 and Metal rely mainly on this)

- **Normalizing irrelevant bits:** clear the stencil fields when stencil is off. Clear the compare function when depth is off. Clear the bias when polygon offset is off. Clear blend when the write mask is 0. The gain is small (1950 → 1903), but it has no cost, and it reduces duplicate compilation. `kPipelineRecipeVersion` must be upgraded at the same time.
- **Distributing the corpus:** collect recipes with automatic tours and publish them with the shader pack. The format in the recipe key is `RenderFormat`, which is independent of the backend.
  - The existing map tour (316 maps) and cutscene tour (178 segments).
  - **New battle tour:** trigger each enemy group through the debug entry. New enemies are exactly where the most is currently missed.
  - Recipes that players learn locally continue to be merged in.
- **Per-scene tagging and prefetch:** each recipe records the map ID, battle group or cutscene where it appears. When the game starts loading a map or a battle, worker threads pre-build that scene's recipes during the loading screen. At startup, only the globally common part is pre-built.

### P4 Remaining misses: parallelization and limits

- Currently, multiple misses in the same frame are created serially on the render thread (37 × about 28 ms). When a miss is hit, the corpus can be searched for sibling recipes that share the same VS or PS, and these can be pre-built in parallel.
- Optional switch: skip this draw when its pipeline is not yet compiled (similar to DXVK async). This causes short-term missing objects, so it is off by default.

## Platform matrix

| Platform / backend | P1 | P2 | P3 | Expected |
|---|---|---|---|---|
| Android / Vulkan (Turnip) | VkPipelineCache + Mesa cache | GPL | Corpus prefetch | After P1, repeated visits almost never stall. After P2, first visits mostly do not stall either |
| Windows / Vulkan, Linux / Vulkan | VkPipelineCache | GPL | Corpus prefetch | Same as above |
| Windows / D3D12 | ID3D12PipelineLibrary (driver cache already exists) | Not applicable | Main method | First-time stalls depend on corpus coverage |
| macOS / Metal | MTLBinaryArchive | MSL is already compiled per shader. PSOs are still per combination | Main method | Same as D3D12 |

## Recommended order

P0 → P1 → P2 → P3 → P4.

- P1 is a small change. It immediately improves repeated visits on Android and all platforms.
- P2 solves the first-time stall on Vulkan platforms.
- P3 is the main method for D3D12 and Metal. It also raises the startup pre-build hit rate on all backends.

## Statistics method

Record format: a 48-byte file header. After that, each record is 72 bytes. The first 64 bytes are the key (in `Encode` order: vs, ps, then 12 32-bit fields).

- Using only Vulkan core dynamic state and EDS1, keep (vs, ps, blend when write mask is present, write mask, RT format, depth format, whether it is a rect list): i212-run keeps 1371 records.
- Also use EDS3 to make blend and write mask dynamic: 1246 records remain. This is the "after removing all dynamically settable state" value in the table.

## P0/P1 implementation and measurements (2026-10-06)

**P0 miss log.** Pipelines created during drawing (`GetPipeline` misses) are counted for the current frame. A frame with misses prints one line at the end of the frame: `renderer: pipeline misses frame=… count=… ms=… map=… battle=… first_vs=… first_ps=… first_ms=… recipe=… vs_seen=… ps_seen=…`. `LO_PIPELINE_MISS_LOG=1` also prints one line `renderer: pipeline miss …` for each creation.

- `map`: the map definition ID from `debug/map_info`. It is `-` during a transition. `battle`: which battle of this run (the battle core has ticked within the last second). It is 0 when not in battle.
- `recipe=1`: the key is already in the recipe set (loaded or learned in this run), but has not been built yet. For example, startup pre-building was skipped.
- `vs_seen` / `ps_seen`: earlier a recipe or built pipeline used this shader. If both are 1, this is a "new combination of known shaders," which is the type P2 is meant to solve.
- `tools/pipeline_misses.py <runtime log>` lists frame counts, miss counts, total time and worst frame per scene. If there are per-miss logs, it also lists the number of new VS/PS. The `pipelines=` / `pipeline_ms=` from `LO_RENDER_TIMING=1` are unchanged, and the two sets of numbers agree.

**P1 driver cache.** The files are placed in the shader cache directory: `pipeline_cache_vk.bin`, `pipeline_cache_dx12.bin`, `pipeline_cache_metal.bin`. They are read before startup pre-building. They are written once after pre-building finishes. Afterwards, while recipes are being written (checked every 60 frames, written only when there are new recipes), they are written back asynchronously. They are written once more on normal exit. The file is written only when its data has grown. The limit is 256 MB (under vkd3d-proton, each pipeline is about 18 KB). When the limit is exceeded, the old file is kept. Replacement is atomic, through a temporary file.

- Vulkan: plume creates one `VkPipelineCache` per device. It is used for both graphics and compute pipelines. Before loading, the header fields `headerSize`, `headerVersion`, `vendorID`, `deviceID` and `pipelineCacheUUID` are checked. If they do not match, it starts from an empty cache and overwrites it.
- D3D12: `ID3D12PipelineLibrary`. Only game pipelines created by `CreatePipeline` are named (the key hash is the hash of the VS/PS/GS bytecode). Variants are not stored. If the driver rejects old data, the library starts empty.
- Metal: `MTLBinaryArchive` is implemented but off by default. `LO_METAL_BINARY_ARCHIVE=1` turns it on. The reason is in the table below: a hit is not faster than Metal's own compile cache, and loading a 62 MB file takes 1.8 seconds. Most of the PSO cost is not backend compilation, so the archive cannot save it.
- Android: the Java side sets `MESA_SHADER_CACHE_DIR` (the `mesa_shader_cache` directory under the app cache directory) and `MESA_SHADER_CACHE_MAX_SIZE=256M`.
- `_Exit` exit: after renderer initialization, an exit callback is registered with `os::shaderlog`. Every `_Exit` path that first calls `CloseForExit()` (window `SDL_QUIT`, kernel termination, shader-only preparation), and the path in `video.cpp` on shutdown where GPU drain fails, writes the recipes and the driver cache first, without waiting for the GPU.
- `LO_NO_DRIVER_PIPELINE_CACHE=1` turns off the driver cache, for comparison.

**Measurements.** Startup pre-building used the same 1950 recipes (i212-run map tour, 1933 with all shaders present), with `--prepare-shaders-only`, on each machine, with the same cache directory run back to back. "Cold" means there is no cache file. "Control" means `LO_NO_DRIVER_PIPELINE_CACHE=1`. "Warm" means reading the file written by the previous run.

| Host / backend | Driver's own disk cache | Cold | Control | Warm | Cache file |
|---|---|---|---|---|---|
| psvita, Proton GE10-34 / vkd3d-proton D3D12, RADV | Off (`VKD3D_SHADER_CACHE_PATH=0`, `MESA_SHADER_CACHE_DISABLE=1`) | 2648 ms | 2631 ms | 888 / 894 ms (read 18 ms) | 35.4 MB |
| psvita, native Linux Vulkan, RADV (Radeon 8060S) | Off (`MESA_SHADER_CACHE_DISABLE=1`) | 2124 ms | 2112 ms | 1022 / 955 ms (read 4 ms) | 8.8 MB |
| M1 Max, Metal (macOS 26.6.2) | System cache already warm | 13499 ms | 5245 ms | 5437 / 5265 ms (read 1901 / 1764 ms) | 62.4 MB |
| M1 Max, Metal | System cache cleared | — | 44847 ms | 44848 ms (all 1933 hit) | 62.4 MB |

Play: psvita D3D12, entering the map from the Uhra save at boot (`skip_shader_prebuild=1`, so all pipelines are created during drawing). Only the cache directory was kept between the two runs:

| Run | Pipelines created during drawing | Total `pipeline_ms` | Worst frame |
|---|---|---|---|
| Cold | 243 | 3221 ms | 1893 ms (171 items) |
| Warm | 244 | 50 ms | 26 ms (171 items) |

Not measured: Android tablet (TB321FU, Turnip). The 0.8.38-dev-pipecache debug package was installed, but the tablet locked its screen and the game stayed in the startup stage, so no pre-build numbers were obtained. The Mac and native Linux were measured only for startup pre-building, not for play.

## P2 implementation and measurements (2026-10-06)

**Implementation.** plume Vulkan reports `fastLinkPipelines` when the device supports `VK_EXT_graphics_pipeline_library` (with `graphicsPipelineLibraryFastLinking` true) and `VK_EXT_extended_dynamic_state`. Pipelines that miss during drawing (`GetPipeline`) are fast-linked from four library parts, with no link-time optimization:

- Vertex input: only the primitive topology and primitive restart.
- Pre-rasterization: VS (with GS for rect lists), distinguished by whether depth clamp and depth bias are enabled.
- Fragment shader: PS.
- Fragment output: blend, write mask, formats.

Parts are cached on the device by shader module and render pass compatibility class (RT format, depth format, sample count). Cull, front face, depth bias, depth test/write/compare, and stencil test/operation/mask/reference value are dynamic state, and they are set again each time a linked pipeline is bound. Startup pre-building still builds full pipelines.

After pre-building finishes, a background thread builds parts for the shaders of the known recipes (`prepareGraphicsPipelineLibraries`). Afterwards, new combinations of known shaders need only linking. `LO_NO_PIPELINE_LIBRARY=1` turns it off, for comparison. D3D12 and Metal are not affected.

**Measurements.** psvita, the Windows build's Vulkan backend under Proton GE10-34 (winevulkan → RADV, Radeon 8060S). Entering the map from the Uhra save at boot (Uhra residential area, map 20), with `MESA_SHADER_CACHE_DISABLE=1` and an empty driver cache.

| Case | Off (`LO_NO_PIPELINE_LIBRARY=1`) | On |
|---|---|---|
| No recipes, `skip_shader_prebuild=1`: total pipeline creation during drawing / worst frame | 1807 ms / 1198 ms (171 items) | 1004 ms / 710 ms |
| Pre-build 1707 recipes (316-map tour minus the 243 from Uhra), then enter Uhra: total / worst frame | 1259 ms / 880 ms | 439 ms / 285 ms |

In the second case, the background pre-build of parts took 6.0 s (1700 recipes), and it finished before entering the map. Looking at each item (`LO_PIPELINE_MISS_LOG=1`): 172 items where both VS and PS had been seen averaged 0.24 ms. 62 items where the PS was new averaged 5.2 ms, totaling 324 ms. That is the main remaining part. New shaders must be covered in advance by the P3 recipe corpus.

**Verification.** Native Linux psvita build (RADV, Mesa 26.2.2), Vulkan validation layer 1.4.341, entering Uhra at boot. Each mode was run with the same settings, on and off:

- No messages appeared only on the link path. `VUID-vkCmdDraw-renderPass-02684` (render pass dependency count 2 is incompatible with 0) was already present on main. The framebuffer's render pass has two external dependencies, while the pipeline's has none. With `LO_VK_NO_PASS_DEPS=1` removing the dependencies, both modes report 0. This shows that the render pass used by the parts has no other incompatibility.
- The background thread built parts for 1700 recipes (0 failures), while drawing was linking at the same time. There were no thread-safety messages and no dynamic-state messages.
- Compared with full pipeline builds, screenshots matched for static scenes.

**Steady-state cost.** Worst case: `skip_shader_prebuild=1`, empty cache, all game pipelines linked, compared with all full pipeline builds (native Linux, ABBA twice each, sampled 25 s after 10 s in the map):

| Resolution | GPU time | Recording | Frame time |
|---|---|---|---|
| 1600x900 | 4.63 vs 4.31 ms (+0.32) | +0.21 ms | 9.91 vs 9.68 ms |
| Internal 3840x2160 | 16.04 vs 15.70 ms (+0.34) | +0.55 ms | 22.62 vs 22.20 ms |

The extra GPU time does not change with resolution. It is a fixed cost per draw (VS→PS interface without link-time optimization), not slower pixel shading. In actual play, only pipelines seen for the first time in a run are linked. On the next launch, recipes pre-build them into full pipelines, so there is no background optimized compile. When front and back stencil state are the same, it is set only once (`VK_STENCIL_FACE_FRONT_AND_BACK`), which reduces calls during binding.

## P3 implementation and measurements (2026-10-06, branch feat/pipeline-prefetch)

**Normalization.** `gpu::pipeline_cache::Normalize` clears the bits that `DescribePipeline` does not read, before lookup, recording and creation. These are: the Z compare function when Z is off or there is no depth target. All stencil fields and the reference value/mask when stencil is off. The back-face stencil fields when back-face stencil is not enabled. The early-Z bit. The polygon offset switch (the offset is already included in the bias). The bias when depth test is off. `-0.0` slope. The blend when the write mask is 0 (`DescribePipeline` now treats such a draw as not blending). Primitives other than points, lines, line strips, triangle strips and rect lists are treated as triangle lists. The draw's own heuristics still use the original key.

- Recipe version 2. Version 1 files are normalized when read, and rewritten on the next save. The 1950 recipes from the map tour became 1902.
- Verification: using the patched version, the 1950 original keys were compared one by one: `DescribePipeline(original key)` and `DescribePipeline(normalized key)`. Only 346 keys with `-0.0` slope differed in bits. Their numeric values were equal.

**File version 2.** Each record is 104 bytes: a 64-byte key, 8 scene tags, and a checksum. A tag is `type << 28 | ID`. For a map it is the map definition ID. For a battle it is the battle ID in the loader's battle slot. `0xF0000000` means drawn outside any scene (title, boot menu). More than 8 scenes are recorded as "multiple scenes." `tools/pipeline_recipes.py` reads and writes both versions (`info`, `merge`, `subtract`, `strip`). `merge OUT IN@battle:ID` adds tags to old untagged files.

**Scene identification.** The engine tick reads the loader's request slot (`0x832631F0 + 3168`, slot 0 for map jumps and slot 1 for battles. The phase is at +0, the name FString at +4, the ID at +16). It also records maps that newly appear in the world. Current scene: during battle, the battle ID. Otherwise, the current map. During a transition, the scene being loaded.

**Prefetch.** When a map or battle starts loading, the render thread hands all not-yet-built known recipes for that scene to the worker threads at the end of the frame. The number of worker threads is half the logical core count. Built pipelines are collected into the pipeline table on the render thread. If drawing hits a queued task, it builds the pipeline itself. If it hits a task that is being compiled, it waits for it (with a pipeline library, it fast-links directly). `LO_NO_PIPELINE_PREFETCH=1` turns prefetch off.

**Corpus.** The optional `shaders/pipelines_corpus.bin` (next to the opened shader pack, or in the pack's install location. `LO_PIPELINE_CORPUS` can specify it) is merged into the known recipes. It is not written into the player's file, and the translator version is not checked. The corpus is published as a separate file in the `shader-packs` prerelease, listed in `index.json` as a `pipeline-corpus` entry. After the shader pack check, the runtime downloads it with a background thread into the install directory's `shaders/pipelines_corpus.bin`. It downloads when missing. When it exists, auto-update is on, and it checks at most once per day. If the hash differs, it is replaced, and it takes effect on the next launch (see [PORTABLE_SHADER_PACK.md](../PORTABLE_SHADER_PACK.md#pipeline-recipe-corpus)). The first corpus has 3934 records. It merges the map, cutscene and battle tours. It removed 4 rect list recipes that startup pre-building could never build (the VS variant of a rect list is generated only on the first draw of that kind). The 646 cutscene records have no scene tags, and they are all pre-built at startup. In the future, a build with scene tags can re-record the cutscenes so they use scene prefetch instead.

**Startup pre-building.** By default, pre-build all recipes the player has learned, plus recipes in the corpus that have no tag, were drawn outside a scene, or appear in at least 4 scenes (`LO_PIPELINE_PREBUILD_SCENES`). The rest are left to scene prefetch. `LO_PIPELINE_PREBUILD=all` pre-builds everything. `=common` applies the same rule to learned recipes too. Vulkan's pipeline library pre-compilation covers all known recipes.

**Measurements.** Scenes: entering u3b (map 20) from the Uhra save at boot. After 15 seconds, a debug jump to u36 (map 16), which was visited for the first time. Each stays for 20 seconds. The corpus is the recipes learned in the same scene by the previous run (411–416 recipes), so this is the upper limit when "the corpus is complete." Each run starts from an empty shader cache directory. On psvita, the vkd3d and Mesa disk caches are off. The "longest frame" column in the table is `draw_ms` from `LO_RENDER_TIMING`, which includes the time to create shader modules during drawing (on Metal, this part costs more than the pipeline itself).

psvita, Proton GE10-34 / vkd3d-proton D3D12, RADV:

| Run | Startup pre-build | Longest frame at boot map | Longest frame after jump | Pipelines created during drawing |
|---|---|---|---|---|
| P2 baseline (cea0413a, no recipes) | — | 2046 ms | (baseline has no jump command) | 243, 3189 ms |
| Corpus, no pre-build, no prefetch | — | 2072 ms | 1662 ms | 415, 5387 ms |
| All pre-built (`LO_PIPELINE_PREBUILD=all`) | 412 recipes, 1036 ms | 53 ms | 56 ms | 2, 10 ms |
| Default (learned + out-of-scene + common) | 13 recipes, 56 ms | 66 ms | 176 ms | 15, 188 ms (another 16 waited for prefetch tasks, total 38 ms) |
| Prefetch only (no pre-build) | — | 66 ms (title 121 ms) | 122 ms | 25, 293 ms |

- At boot: from the load request to the map appearing took about 3 seconds. In default mode, 218 recipes (including 178 shader modules) were built within 0.9 seconds.
- Jump: from the request to the map appearing took only 0.3–0.5 seconds. 172 recipes were built within 0.3 seconds. The last few frames still had some misses or waits. This is the upper limit of lead time. P4 can handle it further.

psvita, Vulkan under Proton (RADV, pipeline library on): without prefetch, 220 draw-time creations of all fast-links totaled 17 ms. In default mode, 9 creations totaled 14 ms. In both, the longest frame in the map was within 56 ms. The difference is that prefetch provides fully optimized pipelines. Pipeline library pre-compilation covers the corpus, and 407 items were done in the background in 1.5 seconds.

M1 Max, Metal (second and later runs of the same binary, system shader cache already warm):

| Run | Startup pre-build | Longest frame at boot map | Longest frame after jump |
|---|---|---|---|
| No corpus (first run of new binary, cold) | — | 5148 ms | 1849 ms + 1229 ms |
| Corpus, no pre-build, no prefetch | — | 734 ms | 300 ms |
| All pre-built | 412 recipes, 382 ms | 61 ms | 46 ms |
| Default | 13 recipes, 50 ms | 59 ms | 49 ms |
| Prefetch only | — | 60 ms | 48 ms |

- On Metal, the expensive part is shader modules (SPIR-V converted to MSL and then compiled). Previously they were created one by one on the render thread. Preparing one scene prefetch took 7.8 seconds, and pre-building everything took 10.3 seconds. Now modules are also built on worker threads: prefetching 390 recipes and 267 modules took 0.4 seconds, and pre-building everything took 0.38 seconds.

**Trade-off in startup pre-building.** By default, pre-build only the recipes the player learned and the out-of-scene/common recipes from the corpus. The rest relies on scene prefetch. In the tested scenes, the result of first entry is the same as full pre-building. Startup time does not grow with corpus size (D3D12 cold start with full pre-building is about 2.5 ms per recipe, and on Android about 80 ms per recipe). Recipes the player has learned are still all pre-built, as before P3.

**Not measured:** Android. Battles (the request slot reading for battle IDs comes from `patches/encounter_defer.cpp`, and the `scene load: battle` log has not yet been checked in a real battle). Prefetch under a cold Metal system cache.

## P4 implementation and measurements (2026-10-06, branch feat/pipeline-miss-parallel)

**One worker pool.** Scene prefetch (P3), sibling recipes and skipped draws share P3's worker threads (half the logical core count). Three queues pick tasks by priority: pipelines needed by a skipped draw > the scene being loaded > sibling recipes (the newest miss first. When there are more than 128, the oldest is dropped). Scene tasks may occupy all threads. Draw-time tasks (sibling recipes, skipped draws) are limited to `clamp(logical cores / 4, 2, 6)` at the same time. `LO_PIPELINE_MISS_WORKERS` can change this. 0 turns both off.

**Sibling recipes.** On a draw miss, the render thread first hands the known recipes that share this PS, then those that share this VS (those with the same RT/depth format and primitive first), to the workers. Up to 16 at a time. Within each group, recipes drawn in the current scene come first. Only then does it build this pipeline itself or wait for it, so the worker and the render thread compile at the same time. The index is built once, at the first miss, using the known recipes not yet built (normalized keys). Recipes learned later are already built by then, so no update is needed. `LO_NO_PIPELINE_SIBLINGS=1` turns this off.

- When the other shader of a sibling recipe is not yet loaded, the worker reads it using its own pack reader, builds the module, and then builds the pipeline. When the render thread collects the task, it puts the module into the shader table. When the render thread itself needs a shader that has a task: if the task is queued, it runs the task itself. If it is being built, it waits. So a shader has only one module. When the same hash appears with a second module, it is not released, because a pipeline may already use it, and the pipeline library caches by module handle.
- An approach that queued shader tasks first, then queued pipeline tasks after collecting the shaders into the shader table (33aab39c), was tested. Most sibling recipes did not have time to start, and the render thread built them itself. The longest frame interval was about 200 ms longer than now.
- **Metal is off by default** (`LO_PIPELINE_SIBLINGS=1` turns it on). On Metal, pipelines are cheap. The expensive part is the shader module (MSL compilation). Sibling modules built on workers blocked the render thread's own modules. On M1 Max, one of two runs had its first boot frame change from 0.75 seconds to 4.9 seconds. The other run gained nothing.

**Waiting rules.** A draw that hits a queued task builds it itself (`src=claim`). When it hits a task being built: if the task builds the same kind of pipeline (with a pipeline library, all are fast-links. Without one, all are full compiles), it waits. When there is a pipeline library and the task is a full compile from scene prefetch, it fast-links itself (`src=inline`), and the task's result is discarded. The same kind of pipeline is never built twice.

**Skipped draws (`LO_PIPELINE_ASYNC=1`, off by default).** On the main draw path, when the pipeline is not yet built (including one that scene prefetch is building), this draw is skipped. The pipeline is handed to a worker (`src=defer`). Objects will be briefly missing. The scene-copy promotion path never skips. Skipped draws are not counted in `drops.pipeline`. They are recorded in `pipeline workers ... skipped=`. A skipped draw may also be a depth, stencil or later-resolved draw, so it is not just one missing object.

**Pack reading.** Each worker thread opens its own pack reader. `Reader::Get` decompresses the whole block inside a lock. If one reader were shared, it would block the render thread's own shader loading. At startup, `LoadPackShaders` also uses one reader per thread (up to 32 on psvita, each reading a 2.5 MB index). Startup became faster. For Vulkan, building modules for 1270 known shaders went from 1026 ms to 124 ms. B's startup pre-build went from 2000 ms to 1654 ms on D3D12, and from 2093 ms to 1099 ms on Vulkan.

**Logging.** Each miss now has `src=create|claim|inline|defer`. Each frame has one extra line `renderer: pipeline workers frame=… queued= built= hits= skipped= shaders=` (`hits` needs `LO_PIPELINE_MISS_LOG=1`). Loading of pack shaders is now counted in `shader_ms`, so take care when comparing with earlier versions. `tools/pipeline_misses.py` has a new column for sibling recipes.

**Measurements.** psvita, boot from the Uhra save. Each run starts from an empty shader cache, with the vkd3d and Mesa disk caches off. The baseline is the head of the P3 branch, 0cbc7d6c. "Total / worst frame" is the time spent creating pipelines and waiting for tasks during drawing. "Longest frame interval" and "total of frames over 50 ms" are taken from `between_ms` after the map appears. Three scenes:

- A: the 1950 recipes from the map tour (no scene tags) as the player file, with no pre-building. The map's recipes are all known but not built. This is the case sibling recipes must handle (player skipped startup pre-building, prefetch did not keep up).
- B: 1707 recipes with the Uhra recipes removed, with startup pre-building. The map's pipelines are all new, so there are no sibling recipes to build. This checks that nothing gets slower.
- C: P3's corpus (411 recipes learned in the same scene, tagged), default pre-building, jump to u36 after 15 seconds.

Proton / vkd3d-proton D3D12:

| Scene | Run | Total | Worst frame | Longest frame interval | Total of frames over 50 ms |
|---|---|---|---|---|---|
| A | P3 baseline (two runs) | 3049 / 3148 ms | 1831 / 1962 ms | 1995 / 2140 ms | 3129 / 3239 ms |
| A | P4 | 1672 / 1630 ms | 1285 / 1329 ms | 1443 / 1490 ms | 1823 / 1769 ms |
| A | P4 + skipped draws | 13 ms | 9 ms | 129 ms | 184 ms (5691 draws skipped) |
| B | P3 baseline / P4 | 2921 / 2928 ms | 1768 / 1779 ms | 1878 / 1881 ms | 3186 / 3151 ms |
| C | P3 baseline | 164 ms (15 items) | 100 ms | 162 ms | 503 ms |
| C | P4 | 61 ms (6 items) | 43 ms | 61 ms | 336 ms |
| C | P4 + skipped draws | 13 ms | 13 ms | 61 ms | 280 ms |

Vulkan under Proton (RADV, pipeline library on): since P3, startup builds shader modules and libraries for all known recipes. In A, the misses are all fast-links, and P4 has nothing more to save. The differences in all three scenes are within the variation between the two baselines (A total 20 / 17 ms, longest frame interval 57–58 / 54–57 ms; B 410 / 417 ms; C 13 / 9 ms). With skipped draws, A's total was 4 ms.

M1 Max, Metal (system shader cache warm): no measurable difference. A's longest frame interval: baseline 750 / 742 ms (mostly the render thread building shader modules), P4 default (sibling recipes off) 738 / 729 ms, with sibling recipes on 4881 / 780 ms. B 341 / 343 ms. C's total of frames over 50 ms was 298 / 300 / 250 ms (the third run has sibling recipes off), and the 242 ms with it on is within the same range. The 4.9-second frame had 282 sibling shader modules, and the render thread's `shader_ms` was 4.8 seconds. It may be waiting for modules on workers, or contention between MSL compilations. The logs cannot tell which. Skipping draws helps little on Metal (A 713 ms), because the expensive part is shader modules, not pipelines.

**Waste from sibling recipes.** On D3D12 scene A, on boot into the map, about 1400 sibling recipes were queued and about 800 were built. Fewer than 100 of them are used on this map. The extra ones are real recipes from other maps and will be used later. But each such burst adds about 14 MB to the P1 driver cache, and it also uses more VRAM.

**Not measured:** Android (on 8 cores, draw-time tasks had only 2 threads, and contention with the game thread was not measured). Native Linux Vulkan. Cold Metal system cache.

## Corpus tour (2026-10-06, branch feat/battle-tour)

**Battle entry.** `LO_DEBUG_BATTLE_FILE` points to a command file. Each command runs once when its serial number increases: `serial battle <group> [battle map]`, `serial victory` (the debug menu's win), and `serial list` (writes the group table to the log). If this variable is not set, the whole feature does nothing. The group goes through the game's own random encounter path. The walking update `sub_829E3048` (character vtable +1632) calls the draw `sub_829E3268` (+1636), which is patched to return 1 directly and writes the group into its output slot. The walking update then calls `sub_82B53AD8`, `RequestBattle 0x828278A0(GEngine, group)`, and `sub_8285E380` as usual. It triggers even when standing still.

**Group table.** A TArray at `0x83264978 + 128`, 399 entries, 160 bytes each: +0 battle map (if empty, use the area's battle map at `0x832631F0 + 1604`, and if that is empty too, `u10_b3_scrw`), +12 music, +60 AI script, +72/+84 opening/ending sequences, +148/+152 enemy slot arrays (each slot 32 bytes: slot number, enemy config (that is, model), enemy parameters). In total there are 170 enemy models and 56 fixed battle maps. Another 33 area battle maps are not directly referenced by any group.

**Tools.** `tools/recipe_tour.py` (usage in the file header):
- `list` exports the group table. `battles` runs groups in the order "first cover all enemy models and fixed maps, then cover combinations of model parameters." Groups whose models have all been fought are skipped. `--part K/N` can split the work across machines. `stages` fights group 3 (two weak enemies) on each given battle map. `maps` jumps map by map and turns the camera. It needs a diagnostic build with map jumps, disc-change requests and package probe command files (not in main).
- Each segment has a maximum of 15 minutes. On a shared test machine, if another task is waiting for the lock, it yields immediately.
- In battle, A is pressed every 2 seconds. The win is requested 45 seconds after the first command. On failure, forced win or getting stuck, the game is restarted. On natural victory or escape, it returns to the map and continues to the next battle.

**Results.** psvita (D3D12) and M1 Max (Metal) in parallel: 94 groups completed, covering 163/170 enemy models. The 7 uncovered models appeared only in 4 groups that got stuck or whose resources are not on the disc. 33 area battle maps. 250 maps (another 2 reported missing resources, and 5 are not on any disc). Merged into the scene-tagged corpus: 3005 map recipes, plus 646 cutscene recipes (no scene tags), 271 battle recipes, and 16 battle map recipes. Total 3938 records, 285 VS, 2075 PS. The save contains only Kaim at level 10. Most battles failed after the enemies' first round of actions, so each enemy type showed only one round of skills.

## Android measurements (2026-10-06)

Lenovo TB321FU (Adreno 750), Turnip Mesa 26.0.0-devel (libadrenotools), debug package 0.8.45-dev-pipeline (main `2b5044af`), `graphics pipeline libraries on`, shader pack hit. Route: auto-continue the save into Highlands of Wohl (u13), jump after 20 seconds to the Uhra residential area (u3b) for the first time, then return to u13 and fight group 3. "Cold" means the 224 learned recipes before the test were restored, and the driver pipeline cache and Mesa cache were deleted.

| Run | Startup pre-build | Map load | Pipelines created on first map draw (count / total / worst frame) | Battle |
|---|---|---|---|---|
| Old conditions (GPL, prefetch and sibling recipes off; no corpus), cold | 203 recipes, 1.8 s | Uhra 18.7 s | 155 / 7349 ms / 2028 ms | 16, worst 144 ms |
| Default, no corpus, cold | 203 recipes, 2.5 s | Uhra 18.7 s | 155 / 3761 ms / 1225 ms | 16, worst 207 ms |
| Default + corpus, cold (two runs) | 1948 recipes, 18.3 / 17.8 s | Uhra 36.0 / 36.3 s | 0 | 0 |
| Default + corpus, second launch | 2056 recipes, 0.48 s (driver cache 70 MB, read 131 ms) | Uhra 16.3 s | 0 | 0 |
| GPL off + corpus, cold | 1948 recipes, 18.5 s | Uhra 21.3 s | 0 | 0 |
| Old conditions, cold, u32 | 203 recipes, 2.5 s | u32 10.6 s | 68 / 3035 ms / 813 ms | — |
| Default + corpus, cold, u32 | 1948 recipes, 19.1 s | u32 23.4 s | 22 / 1487 ms / 446 ms | — |

- With the corpus, neither the first map entry nor the first battle in Uhra creates pipelines during drawing. The remaining long frames of several hundred milliseconds are recording and binding during map entry. They also occur under the old conditions.
- GPL on Turnip reduced the median pipeline time for new shaders from 68 ms to 28 ms. For known VS/PS combinations, the time is close to 0 under both conditions (the driver's own cache absorbs it).
- Cost: cold-start pre-building went from about 2 s to about 18 s (the second launch takes 0.48 s). After a cold start, background library part building takes 115–140 s, during which map loading is about twice as slow. The u32 cold start still has 446 ms frames: those recipes are in the corpus but tagged with other scenes, and the parts are not yet built. The maintainer accepted the effect and decided not to tune it.
- The `MESA_SHADER_CACHE_DIR` directory was never generated. With this Turnip version, persistent caching relies only on `VkPipelineCache`.
- Corpus download (#264) succeeded on the device (409184 bytes, SHA-256 matches the published one). In a real battle, `scene load: battle id=3` appeared, and the battle scene tag was confirmed at runtime.
- No crash, device loss or pipeline creation failure. Screenshots were normal. The 155 GPL-linked pipelines matched the picture with GPL off.

## Background startup pre-building (2026-10-09, branch feat/background-pipeline-prebuild)

**Approach.** Startup pre-building no longer blocks boot. After `PrepareKnownPipelines` reads the recipes and corpus, it hands the startup set (same rules as above: learned, plus out-of-scene / common / untagged corpus recipes) to the P3/P4 worker pool at the lowest priority (skipped draws > scene being loaded > sibling recipes > startup pre-build). Recipes drawn outside any scene (title, menus) come first. Startup tasks use at most `worker count - 1` threads, leaving one for scene and draw-time tasks. On Metal, they get only the draw-time share (in P4, workers building MSL modules slowed the render thread's own modules). `LO_PIPELINE_STARTUP_WORKERS` can change this.

- Shader modules: when a startup task needs a pack shader that has no module yet, the task builds it itself on a worker (the same as for sibling recipes). The render thread puts it into the shader table when it collects the task. The Vulkan pipeline library thread now also gets modules through the same shader tasks, and no longer builds all modules synchronously at startup.
- When drawing hits a startup task that is still queued or being built, it follows the P3/P4 rules (queued ones are built by the draw, a same-kind pipeline being built is waited for, and fast-link is used when there is a pipeline library). The same pipeline is never built twice. When a scene starts loading, its startup tasks still queued (and their shader tasks) move to the scene queue.
- The render thread now reads results by looking only at newly completed tasks (workers put completed tasks into a list). It no longer walks the whole task table every frame, or on every miss. The startup set has over two thousand entries. The old walk would have become a fixed cost on every miss.
- Progress: the render thread publishes the completed count and total each frame. A semi-transparent thin bar is drawn at the bottom of the game picture (its width equals the output width, its height is 1/360 of the output height, at least 2 pixels). After completion, it stays for 0.3 seconds, then fades out over 0.7 seconds. Host menus such as the settings menu and debug menu use the CPU picture, and this bar is not drawn on them. It is not shown when there are no recipes to build. There is no UI-separated frame generation path (the log shows `ui=unavailable`). The progress bar, like the title menu prompt, goes through frame interpolation.
- Logs: at the start, one line `renderer: pipeline preparation: background, …` (counts, number of modules needed, number of workers). At the end, one line `renderer: pipeline preparation: background done, …` (built by workers, built by drawing, failures, time taken, and seconds since startup). `tools/pipeline_misses.py` lists both lines. The driver cache is written once asynchronously at the end.
- `LO_PIPELINE_PREPARE_SYNC=1` restores the original synchronous pre-building (preparation screen, building library modules synchronously). `--prepare-shaders-only` also uses the synchronous path. `skip_shader_prebuild` and `LO_NO_PIPELINE_PREPARE` still skip pre-building as before. The Vulkan pipeline library thread was already in the background and still runs as before.
- Still on the startup path: shader preparation when there is no shader pack (startup bundle, compilation), and reading the driver cache file. Both must finish before the first pipeline.

**Not measured:** the game is not run on this machine. There is no run data on any platform yet.
