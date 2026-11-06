# Merging Render Passes: v1.0.0 Design Research

[简体中文](render-pass-merging-design-2026-10-10.zh-CN.md)

- Date: 2026-10-10. Read-only research. No code was changed, nothing was built, and the game was not run.
- Code baseline: main `aaaf38fa` (worktree `li-1-0-0-progress-a93a50`). `gpu/renderer.cpp` and `tools/patches/` did not change from v0.9.22 (`d3888a0d`) to `aaaf38fa`, so the tablet v0.9.22 trace and the line numbers referenced here match.
- plume references: line numbers from `tools/patches/plume-lostodyssey.patch`, plus the patched `C:\Users\freefrank\worktrees\LostOdysseyRecomp\apk-v100\thirdparty\plume\`. The copy of this file in apk-v100 carries diagnostic instrumentation and its line numbers change, so below the references use function names.
- Data sources (all are measured, except where marked "estimate"):
  - **Tablet D1/D2**: TB321FU, Adreno 750, Turnip 26.0.0 R8, v0.9.22 with a diagnostic patch, native 2560x1600, Uhra, hot state status 3. Files are in `apk-v100\out\perf\`. D1 is the default settings. D2 is high settings (FXAA + GTAO + shadow 4x).
  - **Phone T1**: OPPO PLP110, Adreno 840, 0.8.53-dev, 2616x1140, FXAA + GTAO + shadow 4x, before PR #302. Files are in `apk-289\out\perf\T1-trace.runtime.log`. PurpleVK timestamps read low. Multiply by about 1.65. The relative proportions of the parts are not affected.

---

## 0. Conclusions

### Why the passes are so fragmented

A frame has roughly 305–320 Vulkan render passes. Tablet D1 has 307, of which 253 do only one operation. Classified by what starts each pass:

| Category | D1 pass count (single-operation) | D1 GPU ms | Reason for the break |
|---|---|---|---|
| A. EDRAM alias clear propagation | **145 (138)** | 2.69 | The game clears EDRAM with one pitch. The renderer copies each clear to the other host textures (aliases) at the same tile base. Each alias calls `setFramebuffer` once and becomes its own pass. |
| B. Main view 2560x1636 | **77 (65)** | 25.8 | About 67 are depth-only (stencil marking) ↔ color+depth (lighting) alternating per light source, each pass with one draw. The rest are the Z-prepass, two large color passes, and passes broken by resolves and transfers. |
| C. Shadow / off-screen depth | 57 (24) | 0.99 | Shadow cast batches are chopped up by A-category clears. 48 passes carry 171 draws in total. |
| E. Small game RT (post-processing chain) | 11 (10) | 2.73 | Real target switches: half-resolution, bloom and similar. |
| F. Resolve blits (D32→R32F, FP16→RGBA8) | 8 (8) | 2.22 | A resolve must read the full source, so it must happen after the source pass ends. |
| G. EDRAM format conversion (transfer) | 8 (7) | 3.17 | The same tile changes color class. A conversion pass is drawn alone, then the original framebuffer is bound again. |
| I. Present | 1 | 1.42 | |
| Resolve copies outside passes (not counted as passes) | 9 times | gap 3.44 | Same-format resolves use `vkCmdCopyImage`. Of D1's 4.0 ms gap, this accounts for 3.44 ms. |

How passes end: about 260 framebuffer switches through `setFramebuffer`, about 46 barriers, 0 copies. Copies never show up because the barrier before the copy has already ended the pass.

Phone T1 has the same structure: of 324 passes, 149 are A-category (143 single-operation), 73 are alternating main-view passes, 40 are shadow passes, 8 are blits and 8 are transfers.

### Mechanism: why Turnip uses sysmem for the whole frame, and LRZ is also turned off

- Each plume render pass is `LOAD/STORE`. Clears are always done inside the pass with `vkCmdClearAttachments`.
- In the Turnip autotune bandwidth model:
  - The GMEM cost counts each LOAD attachment and each STORE attachment once per cpp;
  - The sysmem cost counts only the cleared attachments.

  So a pass with only one or two draws almost always chooses sysmem. If GMEM is forced, every pass loads the whole attachment and writes it back, which is 3 times slower. (Sources are in §5, based on Mesa's AOSP mirror, a 2025-03 snapshot.)
- LRZ (Turnip's low-resolution depth culling):
  - Clearing depth with `vkCmdClearAttachments` turns off LRZ for the current pass.
  - `loadOp=CLEAR` does not turn it off.
  - A650 and later track direction. When depth enters the next pass with LOAD, that pass can reuse the previous pass's LRZ state.

  On the tablet, the main depth is first cleared in D1 pass 5 with `vkCmdClearAttachments` (A-category, single operation). Pass 6 is the Z-prepass with 1090 draws. If it changed to loadOp=CLEAR, the Z-prepass would have valid LRZ from the start. How much this saves must be measured on the tablet.
- The LRZ-off reasons reported by TU_DEBUG=perf (the counts below are totals over a P3 run of about 22 seconds, not per frame):
  - `CmdClearAttachments` (55k) and part of `Depth write + ALWAYS` (22k, the game's clear-rectangle draws) can be removed by this plan.
  - `stencil write based on depth test` (37k) and `FS writes depth` (25k) come from the game's own shaders and state. Merging passes cannot fix them.

### What merging can bring

- **The fixed cost of a pass itself is small under sysmem.** The gap between alternating main-view passes is about 1 µs. The smallest single-operation pass takes about 4–10 µs. 300 passes add up to roughly 1.5–3 ms. (Estimate, from the distribution of run times of the smallest single-operation passes in D1.) What can really be saved is mainly work that no longer runs (A-category, 2.7 ms), plus LRZ.
- **The largest potential gain is running the main view with GMEM**: 25.8 ms, 60% of pass time, much of it FP16 read/write bandwidth. This needs first merging the 77 passes into single digits, then pairing with CLEAR/DONT_CARE load/store. But the binning overhead of 2440 draws may eat the gain. On phone, the KEEP_COLOR experiment cut passes from 318 to 253 but was 9% slower. So this item is **not verified**. It must be decided by the stage 0 experiment.
- **At the 720p new default, the tablet is close to the render thread limit**: GPU 17.1 ms, `draw_ms` 20.2 ms. GPU savings matter mainly at 1080p and above (GPU 27.0 ms vs 26.1 ms) and at native resolution, where they turn into FPS. At 720p they only appear as a less busy GPU and less heat.
- **Gains on Apple should be larger**: each encoder switch writes tiles back to memory. KEEP_COLOR cut only 60 encoders on M1 Max and was 15% faster. Mali needs tiling for every pass and would also benefit, but Mali is currently blocked from the game by its int64 and descriptor-set count requirements.
- **Desktop**: D3D12 has no concept of passes. Stage 1 only defers, merges or drops clear commands, so there are only fewer commands. Desktop Uhra at 720p is CPU-limited. The plan cannot add CPU overhead per draw. Stage 1 adds only one extra check at bind time.

### Recommended first stage: defer EDRAM clears, plume Vulkan supports loadOp=CLEAR

- **How**:
  - Alias clear propagation (A1 color wipe, A2 depth alias clear), resolve-attached clears (`RB_COPY_CONTROL` 0x100/0x200), and `ClearDepthTarget` no longer record commands immediately. Instead they record a "pending clear" on the texture.
  - The next time the texture is bound as an attachment, the clear runs at the start of that pass. A full-coverage clear uses loadOp=CLEAR. A partial clear uses `vkCmdClearAttachments` inside the pass.
  - The next time it is read (resolve, transfer, copy, readback), the clear runs first.
  - When a new full clear overwrites it, the old pending record is simply dropped.
- **Expected** (estimate):
  - Passes drop from about 305 to about 170–185, a reduction of 120–135.
  - Most of the 2.7 ms of A-category work no longer runs.
  - Main depth and shadow depth start with loadOp=CLEAR. LRZ may benefit, but this is not quantified.
  - On Metal, the matching encoders disappear.
  - On D3D12, redundant clears disappear.
- **Why the risk is low**: clear values do not change, and the order of operations on each texture does not change, so results are byte-for-byte identical. The only real risk is missing an "execution point" (see §3).
- **Effort**: 5–7 days.

### Stages

| Stage | Contents | Expected pass reduction (estimate) | Effort |
|---|---|---|---|
| 0 | Measurement and diagnostics: resident pass classification counts; `LO_VK_NO_END_DEP` A/B; KEEP_COLOR retested under `TU_DEBUG=sysmem` and `gmem`; record whether each pass chose GMEM or sysmem | 0 | 1.5–2 days |
| 1 | Deferred clear execution; plume Vulkan supports loadOp=CLEAR | 120–135 | 5–7 days |
| 1b | The game's own depth-clear rectangle draws also become deferred clears (conservative conditions); replay of color clear rectangles is also deferred | 30–45 | 3–4 days |
| 2 | Transfers drawn into the target's own pass; merging main-view depth-only and color alternation (decided by stage 0 results); renderer-owned full-screen passes use loadOp=DONT_CARE | 70–75 | 3–5 days, plus measurement |
| 3 (suggested after 1.0, or decide based on stage 0–2 data) | Frame-level deferred recording, decide storeOp; copy-on-write for resolve copies; in-pass resolve (`dynamic_rendering_local_read` or subpass); tune GMEM and autotune for Turnip | Depends on design | 10+ days |

After stages 0–2, the estimate is 60–90 passes remaining per frame.

### Biggest risks

- **An alias read happens before its pending clear runs**: if a read or write path misses an execution point, it reads EDRAM content from before the clear. Countermeasures: centralize all access into three entry points; in debug mode, paint missed cases magenta; run same-binary eager/lazy A/B and fixture readback comparison.
- **Adreno autotune flip**: after passes get bigger and load ops change, autotune may switch to GMEM, which can make things slower. KEEP_COLOR's −9% is a precedent. Each step must be measured per pass on the tablet in a hot state.

### Separately: GTAO is expensive on Adreno (unrelated to pass merging)

- D2 GTAO has two passes: visibility at 1280x800 RG32F 2.79 ms, and composite at 2560x1600 RGBA8 7.99 ms. Together 10.8 ms, 18% of pass time.
- In the tablet trace these two passes are labeled "TAA resolve/motion". The reason: `ambient_occlusion.cpp` has no site instrumentation, so it inherited the previous site. D2 does not have TAA enabled at all.
- For comparison, RTX 5080 at 2160p costs only 1.30 ms more for GTAO. On phone, the composite pass after PR #302 is 3.30 ms (PurpleVK units).
- Suggested as a separate item: reduce the number of samples in the composite pass on Adreno, or change it to half-resolution composite. Android keeps it off by default (it is off now).

---

## 1. Current map

### 1.1 plume Vulkan: when a render pass starts and when it ends

| Behavior | Location | Notes |
|---|---|---|
| Start pass | `VulkanCommandList::checkActiveRenderPass` | Starts only on draw, clear, or `beginOcclusionQuery`. The renderArea is always the whole framebuffer. There is only 1 subpass, `VK_SUBPASS_CONTENTS_INLINE`. |
| Framebuffer switch ends pass | `setFramebuffer` (from patch line 3275) | Binding the same framebuffer again returns immediately, without ending the pass. |
| Barrier ends pass | `barriers` (patch lines 3123–3150) | Ends the pass only when a barrier is actually recorded. Entries with the same layout, read-only, and already covered stages are skipped. Access masks are always `MEMORY_READ\|WRITE`. |
| Copy, resolve, dispatch, query reset, and external command end the pass | `copyTextureRegion`, `copyTexture`, `copyBuffer*`, `resolveTextureRegion`, `dispatch`, `resetQueryPool`, `beginExternalCommands` (patch line 3083) | |
| Clear does not end pass | `clearColor`, `clearDepthStencil` | Both are `vkCmdClearAttachments` inside the pass. If no pass has started, one is started first, so a single standalone clear becomes a single-operation pass. |
| load/store | `VulkanFramebuffer` constructor; `VulkanGraphicsPipeline::createRenderPass` | Color and depth are always `LOAD/STORE`, with fixed initial and final layouts. Read-only depth uses `NONE` where supported. There is no DONT_CARE, transient, or lazily allocated. |
| Subpass dependencies | `setAttachmentPassDependencies` (patch lines 2033–2081) | Start dependency: EXTERNAL→0, src includes TRANSFER and FRAGMENT_SHADER. End dependency: 0→EXTERNAL, dst includes TRANSFER and FRAGMENT_SHADER. The comment says the end dependency is not framebuffer-local, so the tiler waits and flushes caches at the end of every pass. `LO_VK_NO_END_DEP=1` removes the end dependency, but it has never been measured. |
| Pipeline compatibility | `getCompatibleRenderPass` | Pipelines are built on a compatible LOAD/STORE render pass. Compatibility ignores loadOp and storeOp, so adding CLEAR/DONT_CARE variants does not require rebuilding pipelines. |

### 1.2 Metal and D3D12

- **Metal**:
  - `setFramebuffer` ends the encoder. `plume-macos.patch:566` adds a check so the same framebuffer does not end the pass again.
  - `clearColor` and `clearDepthStencil` record a full-target clear as pendingClears. The next encoder uses `LoadActionClear`. Partial clears draw a quad.
  - storeAction is always Store.
  - If a full-target clear has no draws after it, `handlePendingClears` opens and closes a separate encoder for that clear alone. It acts like a clear-only pass, and must also write the whole target back.
- **D3D12**:
  - There are no render passes. `setFramebuffer` is `OMSetRenderTargets`, clears are `Clear*View`, and barriers are `ResourceBarrier`.
  - For D3D12, merging passes only means fewer clear and barrier commands. Subpass-style approaches have no equivalent on D3D12, so they can only be enabled per backend.

### 1.3 EDRAM emulation model (renderer.cpp)

- **Render target cache**:
  - The key is `(tile base, storage class, pitch, is depth)`. See `RenderTargetKey` at 486–492 and `GetRenderTarget` at 7083–7176.
  - Height is not in the key. Height is guessed from the scissor. When it grows, the target is reallocated and the overlapping part is copied over (7146–7170).
  - Each `(base, pitch, class)` has its own host texture. Accessing the same EDRAM region with a different pitch or class gives a different texture. That is an "alias."
- **Host formats**:
  - 8888, 2_10_10_10, 7e3 and 16_16_16_16 are all stored as `RGBA16F` (`ClassHostFormat` at 6909).
  - 16_16 uses `RG16F`. 32F uses `R32F`/`RG32F`.
  - Depth is always `D32_FLOAT_S8_UINT` (7130).

  That is 8 bytes per pixel, a fixed cost for sysmem bandwidth and for the number of GMEM bins. Changing formats would change blend precision, which cannot be byte-for-byte identical, so it is outside this plan.
- **Class switching (transfer)**:
  - `tileOwner` records which class currently owns the same `(base, pitch)` (`AcquireColorTarget` 7189–7214).
  - When the class changes, `TransferRegion` (2698–2745) draws a full-screen triangle that reinterprets the guest bits. The target framebuffer is `(dst, nullptr)`, a separate pass.
- **Clear**:
  - The Xbox 360 Clear is a screen-space rectangle draw. It often uses a different pitch or bit depth from the scene.
  - The renderer first draws this draw normally on its own target.
  - It then propagates the effect to all aliases at the same tile base (12190–12350):
    - **Depth fill erases all color views** (12238–12269, trace site 12257, class A1): iterates all color textures at the same base. For each, `setFramebuffer(tex, nullptr)` plus one full `clearColor`. At the end, at 12269, it rebinds the original framebuffer.
    - **Depth alias clear** (12280–12312, site 12292, class A2): depth textures at the same base but different pitch. `MapDepthClear` maps them to tile rectangles. On Vulkan the rectangles are merged, then `clearDepthStencil(rects)` is called.
    - **Color clear rectangle replay** (12313–12345, site 12332, class A3): the game's rectangle draw is redrawn on aliases using a stretched viewport. It is a draw, not an API clear.
- **Resolve**:
  - `ResolveImpl` (13100–13299) copies a rectangle of an EDRAM texture to a `ResolvedSurface` that represents the target memory.
  - With the same format, `ResolveOnGpu` uses `copyTextureRegion` outside a pass (12950–12968).
  - With different formats (FP16 to RGBA8), `BlitRegion` (3855–3889) is used. It forms its own single-draw pass.
  - On Vulkan, depth also uses `BlitRegion`, writing D32 to R32F (12848).
  - Clears attached to a resolve: color at 13280–13296, `setFramebuffer(color, nullptr)` plus a rectangle clear. Depth uses `ClearDepthTarget` at 13301–13312. Each is its own single-operation pass.
  - A newly allocated resolve target is also fully cleared once (12935).
  - When the sampled logical size is smaller than the resolve storage, there is one more copy. See `fetchViews` (7746–7787).
- **Normal draw**:
  - `DrawImpl` (from 8477) first gets the color and depth targets and does layout transitions (8611–8650).
  - A depth-only draw (color write mask 0) binds `(nullptr, depth)` by default. On Apple it keeps the color attachment (8723–8746, `draw_attachment_policy.h`).
  - The framebuffer is bound at 10469–10470. It is called on every draw, and binding the same framebuffer does not break the pass.
- **Other places that insert passes outside the main path**:
  - AO: `ambient_occlusion.cpp:157–170`. Visibility and composite are two passes, each with `barriers(ALL)` before and after.
  - HDR sidecar: 10825–10845 and 10959–10977. The sidecar forms its own pass, then rebinds the game's framebuffer.
  - FSR alpha replay: 11133–11158. Depth transitions back and forth between DEPTH_READ and DEPTH_WRITE.
  - Motion-vector replay: only Metal queues it in batch (8990). Vulkan inserts it per draw, and it appears only when TAA/SR is enabled.
  - Present and FXAA: `presentation.cpp:820–918`. When there are black bars, it clears inside the pass (861).
- **Submission points** (also end the pass):
  - End of frame: `command_processor.cpp:1041`.
  - Waiting on occlusion queries (renderer.cpp 4351–4358).
  - The upload ring or descriptor pool runs out (8515–8529).
  - The query reset at the start of a new command list runs outside the pass (`Begin` at 4514).

### 1.4 Frame structure (D1, pass numbers)

| Number | Contents |
|---|---|
| 0 | Present, 1.42 ms |
| 1–17 | The game clears EDRAM through a 640-pitch surface. Then A1 erases the color views of base 0 (2560x1636, 1760x1728, 960x1067, 640x640, 480x512, …), then A2. |
| 5 | Main depth alias clear, single operation |
| 6 | Z-prepass, 1090 draws, 3.15 ms, ends with a transfer barrier |
| 18 | Transfer to main color, 0.35 ms |
| 19 | Main color + depth, 620 draws, 7.05 ms, ends with a resolve barrier |
| 20 | Main depth resolve blit, 0.35 ms. Before it, there is about 1 ms of resolve copy gap outside passes |
| 34–126, 169–207 | Shadow: 880x896, 48 draw passes totaling 171 draws. Separated by many A2 clears (480x512, 880x896). 3 shadow resolves (864x864 R32F) |
| 55–62, 127–168, 208–224 | Main view, alternating per light source: about 67 single-draw passes, about 7 ms total |
| 225–231 | Resolve, two transfers, main color + depth with 359 draws (3.84 ms), color pass |
| 232–306 | Post-processing chain (960x1067, 480x512, 1760x1728), A1/A3 clears, 4 RGBA8 resolves (0.25–0.5 ms), UI and composite |

Where the 77 main-view passes go:

| Change | Passes involved | Result |
|---|---|---|
| Stage 1 | Pass 5 | Merged into pass 6, which becomes loadOp=CLEAR |
| Stage 2a | About 67 alternating passes | Merged into 2–4 |
| Stage 2b | Transfer passes 18 and 228 | Merged into 19 and 229 |
| Cannot merge | Resolves (19→20, 230/231) and the Z-prepass/color boundary | Kept |

After the changes, the main view will have about 8–10 passes.

### 1.5 Break reason classification (three traces)

| Category | Phone T1 | Tablet D1 | Tablet D2 | Can it be removed |
|---|---|---|---|---|
| A1 depth fill erases color views | 54 | 54 (2.02 ms) | 54 (2.26 ms) | Stage 1 |
| A2 depth alias clear | 78 | 74 (0.49 ms) | 74 (0.70 ms) | Stage 1 |
| A3 color clear rectangle replay | 17 | 17 (0.18 ms) | 17 (0.20 ms) | Stage 1b |
| B main view (full screen) | 73 alternating + 10 other | 77 (25.8 ms) | 77 (29.6 ms) | Alternating part: stage 2a, provided stage 0 proves a gain |
| C shadow / off-screen depth | 40 | 57 (0.99 ms) | 55 (1.79 ms) | Merges naturally after stages 1 and 1b |
| E small RT | 31 | 11 (2.73 ms) | 11 | Real switching, left alone |
| F resolve blit | 8 | 8 (2.22 ms) | 8 (4.94 ms) | In-pass resolve, stage 3 |
| G transfer | 8 | 8 (3.17 ms) | 8 (4.26 ms) | Stage 2b |
| H AO / post-processing | 4 | 0 | 3 (12.1 ms) | Already MRT, cannot merge; see the GTAO note |
| I present | 1 | 1 | 1 | Stage 2 uses DONT_CARE |
| Pass gaps (mostly copies outside passes) | Total gap 1.95 ms | Total gap 4.02 ms (of which 3.44 after resolve copies) | Total gap 5.09 ms | Copy-on-write, stage 3 |
| **Total** | 324 | **307 (253 single-operation)** | 308 | |

Notes:
- The "resolve copy 1.4 ms" the coordinator reported is actually pass 0, which is present (site 13774 is the `Transition` in the present path).
- "TAA resolve/motion" is actually GTAO.
- This table's classification was checked against the script by site, size and format. Sites equal the current code line number plus an instrumentation offset of 5 or 6.

---

## 2. Elimination plan and impact for each category

### 2.1 Deferred clear execution (stage 1, the core)

**Renderer side**

- **Data structure**: `HostTexture` gets a `PendingClear` entry. It records either a color value, or a depth value plus a stencil value and two aspect masks, and a list of host pixel rectangles.
  - A clear is only recorded, not turned into GPU commands.
  - A new full clear overwrites the old record.
  - Partial clears are appended in order. That would create "overlapping rectangles with different values," so the implementation can restrict each texture to a single record: if one exists, run it first, then record the new one.
- **Where to defer**:
  - 12251–12252 (A1).
  - 12286–12304 (A2).
  - 13280–13296, color clear attached to a resolve.
  - 13301–13312, `ClearDepthTarget`.
  - Not changed: 12935–12936, the initial clear of a new resolve target. It runs only at allocation time, so deferring it would not reduce passes per frame. Also `rs.tex` goes through several paths that call `commandList->barriers` directly (for example 12654, 9978, 13016). Deferring there only adds risk.
- **Execution points**: every path that touches the texture must run the pending clear first. Suggested to collect them into three entry points:
  - `BindForDraw(tex)`: DrawImpl before 10469, the targets of TransferRegion and BlitRegion, the HDR sidecar, and the destination of the clear replay. How it runs is described in the plume section below.
  - `SettleForRead(tex)`: every place that `Transition`s to SHADER_READ, COPY_SOURCE, or DEPTH_READ. That is the `Transition` at 4908 itself, `GetTexture` binding, the `TransferRegion` source at 2733, the `BlitRegion` source at 3864, `ResolveOnGpu`'s copy and blit, the depth resolve, the `fetchViews` copy at 7771, the depth read of FSR alpha replay at 11133, and the captures for AO, TAA, SR and FG.
  - `SettleForWrite(tex)`: writes outside a pass, that is, the COPY_DEST of resolve targets, the copy at 7163 when `GetRenderTarget` grows the height, and targets partly covered by `TransferRegion`.
  - In addition, readback and debug paths (`DumpTexture`, `DumpRenderTargets`, `ReadbackTexture`, `LO_CLEAR_RT`, debug captures, `present_capture`) call `SettleAll()` before they start.
  - When a texture is replaced there are two cases:
    - If the new texture inherits the old content, run pending clears on the old texture first, then copy. This includes the overlap copy when `GetRenderTarget` grows the height (7150–7170, reading `oldTarget`), and the scene-copy promotion during upscaling (`PrepareSceneCopyDestination` / `ActivateSceneCopyDestination` replace `renderTargets[key]` in the middle of a frame). The existing code already calls `SettleRestoreDebt()` at these places (7123, 7207, 7212). Adding the same calls at the same places is enough. No new mechanism is needed.
    - If it does not inherit the content (`retiredTextures`, resolution change plans), discard the record directly.
- **Why the cross-pitch semantics are equivalent**: in this model, draws do not propagate between aliases of different pitch. Only clears do. A pending clear on texture B is only affected by B's own later accesses. As long as every access to B first runs the clear, the result is exactly the same as clearing immediately.
- **Bookkeeping that must stay at the original time point**:
  - `hdrValid=false`, `sdrProducerFrame`, `aaProvenance.Invalidate`, `sdrLoss`, `HandleFsrAlphaRgbWriter`, `fsrAlphaBridge->RecordFullColorClear`, `DropRestoreDebt` (12247), `FlushMotionReplayQueue`.
  - Reason: these describe logical content. They must be updated at the logical clear time. Only the GPU command is deferred.

**plume side** (Vulkan, following Eden and Dolphin. Metal already has this)

- `VulkanFramebuffer` caches several `VkRenderPass` variants keyed by "which attachments use CLEAR": one color plus depth/stencil, up to 8 kinds. The original `VkFramebuffer` is compatible with these variants.
- After `setFramebuffer` and before the first draw, if a full `clearColor/clearDepthStencil` arrives, it is recorded as a clear at the start. `checkActiveRenderPass` uses the matching variant and passes `pClearValues`.
- Partial clears, or a pass that has already started, still use `vkCmdClearAttachments`.
- Depth and stencil can be set separately. For example, A2 clears only depth, and stencil still uses LOAD.
- If a pass has only a start clear and ends with no draw, it can record an empty CLEAR pass, or be changed to an out-of-pass `vkCmdClearColorImage` / `vkCmdClearDepthStencilImage`. The latter takes the fast-clear LRZ path on Turnip.

**Partial rectangles of the main depth**

- The main depth texture is 2560x1636. The game's clear rectangle, once mapped, may not cover the 36 rows beyond the guest height. If it does not cover them, only `vkCmdClearAttachments` can be used, and the LRZ benefit is lost.
- Optional approach: if the uncovered area lies completely beyond `ScaleY(guestHeight)`, expand the clear to the full texture. No draw or resolve can read those pixels.
- Risk: the copy when the texture grows (7150–7170) would bring those rows into the new texture. So this may only be done if "handling growth with the same clear value" holds. First add a count to see how many cases there are, then decide.

**Impact on each platform** (estimate)

| Platform | Impact |
|---|---|
| Adreno/Turnip (sysmem) | 120–135 fewer passes. Of the native A-category 2.7 ms, everything that no longer needs to run is saved. Work that still must run becomes loadOp or in-pass clears. Pure work saving is estimated at 1.5–2.5 ms (about 4–6%). The LRZ benefit is extra and not quantified yet. loadOp=CLEAR raises the sysmem cost in autotune's eyes, so some passes may switch to GMEM. Must be measured. |
| Apple | Most clear-only encoders disappear. Each of them is a full write-back. Expected gain is larger than Adreno. |
| Mali | Same as Apple, but currently blocked from the game. |
| Desktop D3D12/Vulkan | Clear commands only decrease, but each draw gets one extra check. It should not regress, but must be verified with a same-binary switch on the RX 9070 XT and RTX, checking `draw_ms` and GPU ms. |

### 2.2 Deferring the game's own clear-rectangle draws (stage 1b)

- In the shadow and post-processing segments, each clear first draws a single-draw pass on a 640/880/480-pitch surface, then propagates.
- When these conditions hold, the draw can be skipped. Instead, record a pending clear on this target and all aliases:
  - Depth only, `!colorWrites`.
  - zfunc is ALWAYS and writes depth (12199).
  - The rectangle covers the whole target.
  - Stencil writes are off, or are REPLACE with a full mask, which can be converted to a stencil clear value.
  - There is no active guest occlusion query (`occlusion.DrawSeen()`).
  - No polygon offset and no PARA offset.
  - All vertex z values are the same.
- Gains:
  - The shadow segment drops from about 57 passes to 3–6, with one pass per shadow resolve.
  - Part of the LRZ-off cases such as main depth `Depth write + ALWAYS` is removed.
- A3 (color clear rectangle replay, 17 cases) is a draw whose color comes from the game's PS. It cannot be converted to a clear value. It can follow the motion-vector replay's `QueuedDraw` approach: queue the pipeline, constant offset and descriptor set, then draw them into the alias's pass the next time that alias is bound.
- Prerequisite for byte-for-byte identity: the rasterized z must be bit-identical to the renderer's computed `rectZ`. A2 already makes this assumption by default, but it must still be verified on each backend with a fixture. If the condition does not hold, keep drawing the draw.

### 2.3 Alternating main-view depth-only and color passes (stage 2a, decided by experiment)

- Current state: about 67 single-draw passes, alternating between stencil marking (depth-only, `(nullptr, depth)`) and lighting (`(color, depth)`). See D1 passes 55–62, 127–168, 208–224.
- Existing switch: `LO_DEPTH_ONLY_KEEP_COLOR=1` keeps the color attachment for depth-only draws (write mask 0). The alternation can then merge into 2–4 passes. On Apple this is already on by default, and it gives +15%.
- In the phone ABBA experiment it was 9% slower. Scene pass time, normalized by the unaffected passes, rose 29%. Two hypotheses:
  1. After merging, passes get bigger. Autotune estimates the sysmem cost is higher than the GMEM cost of LOAD plus STORE, so it switches to GMEM. But this frame has heavy geometry, so binning is slower.
  2. Under sysmem, depth-only framebuffers take a faster path, for example CCU allocation or LRZ.

  How to tell them apart (stage 0): on the tablet in a hot state, run ABBA with `KEEP_COLOR=1` under `TU_DEBUG=sysmem` and `TU_DEBUG=gmem` separately. Record the GMEM/sysmem choice for each pass. If the Turnip build has `TU_AUTOTUNE_ALGO`, compare it too. Mesa 2026 added `prefer_sysmem`/`prefer_gmem`/`profiled`. It is not yet confirmed whether Turnip 26.0.0 R8 has them.
- **Constraint**: KEEP_COLOR must not be simply turned on again on Adreno. Only after stage 1 has fixed the load ops, and the experiment proves a gain, should it be enabled together with the GMEM setting.
- Byte-for-byte identity: same shader and same state, with color mask 0. Apple already uses it.

### 2.4 Transfers drawn into the target's own pass (stage 2b)

- `TransferRegion` currently binds `(dst, nullptr)`. Change to:
  - When the immediately following game draw uses `(dst, depth)`, the transfer uses a pipeline variant "with depth format, depth test and write off," and draws directly onto `(dst, depth)`.
  - The source texture's barrier is already before `setFramebuffer`.
- This saves 8 passes per frame, 2 of them in the main view. The shader and pixels do not change.
- For the tiler, the conversion happens inside the tile, which saves one full write-back and re-read.
- Another possibly larger gain (to be confirmed by counting):
  - In D1, transfers total 3.17 ms, more than all of A-category.
  - In D1, the full-screen A1 erase at 2560x1636 (passes 9/12/16, 25/28/32) is followed directly by transfers to 2560x1636 (passes 18, 22, 54).
  - If the transfer's source was only fully cleared earlier in this frame and has no draws since, then after stage 1 the source carries just one pending full clear, and the transfer reads a constant. In that case the value `UnpackGuest(PackGuest(v, source class), target class)` can be computed on the CPU. The transfer itself then becomes a pending clear on the target, which saves its execution and 0.35–0.85 ms of transfer draw.
  - The trace does not show the class correspondence, so stage 1 first adds a count: "whether the transfer's source was only fully cleared within this frame." The upper bound of the gain is estimated from measured transfer times.

### 2.5 Resolve

- **Same-format copies outside passes**: 9 in D1, gap 3.44 ms, about 8%.
  - Copy-on-write idea: at resolve time, let `ResolvedSurface` refer directly to the EDRAM texture. Copy for real only when the EDRAM texture is next written.
  - Base 0 is written repeatedly within a frame, so the saving is uncertain. First count "the fraction of sources not written again before the resolve result is consumed," then decide (stage 3).
- **FP16→RGBA8 and depth→R32F blits**: they need to read the full source, so they can only run after the source pass ends. A true in-tile resolve would need the resolve target bound as a second color attachment at the start of the pass. Then `VK_KHR_dynamic_rendering_local_read` or a second subpass reads the source inside the tile.
  - Problem 1: it must know which resolve will come later. It can only predict from the previous frame's structure. If the prediction is wrong, it falls back to the current approach.
  - Problem 2: render passes with different subpass counts are incompatible, which doubles the number of pipelines.
  - Conclusion: stage 3.
- **Clears attached to resolves**: the standard flow on the 360 is "render → resolve and clear." Stage 1 turns this clear into loadOp=CLEAR on the next pass. In GMEM mode, that is equivalent to saving one full load.

### 2.6 Load/store ops (stage 2; storeOp goes to stage 3)

- **loadOp=DONT_CARE**: renderer-drawn passes that are fully covered and do not blend (the two AO passes, present when there are no black bars, the FXAA intermediate result, the `BlitRegion` that covers the whole target, and the HDR sidecar at 10827, which is cleared before a full copy).
  - Under sysmem there is no difference. Under GMEM it saves the load, and it also lowers the GMEM cost in autotune's eyes.
  - plume's existing `discardTexture` is an empty implementation on Vulkan. It can change to "the next pass uses DONT_CARE for this attachment."
  - Prerequisite: every pixel in the render area gets written, so the scissor must cover the whole target.
- **storeOp=DONT_CARE and transient attachments**: EDRAM content must survive across passes, even across frames. It can be discarded only when the next access is known to be a full clear. But storeOp must be decided when the pass starts, which requires frame-level deferred recording (stage 3).
- **AO intermediate textures**: only the composite pass reads them, so they can be transient. But the composite pass samples neighboring pixels, so they cannot become subpass input attachments.

### 2.7 Synchronization at barriers and pass boundaries (stage 0)

- `LO_VK_NO_END_DEP=1` is implemented but has never been measured. In D1 the gaps between alternating main-view passes are only about 1 µs, so the expected gain is small. One A/B in stage 0 plus a VVL synchronization run is enough.
- Barrier access masks are `MEMORY_*` and the stage sets are wide. Narrowing the masks only affects cache flushing, not pass count. Low priority.
- Merging the barriers of adjacent resolves could save a few pass breaks. The old note that "the same target reopens after a barrier" accounts for only 2.7%, which does not justify separate work.

### 2.8 Can post-processing and AO be merged

- The game's post-processing changes target at every step, and often changes resolution (960x1067, 480x512, 1760x1728), and it samples neighboring pixels. Subpasses that process per tile only work for "same-pixel read and write," which does not apply here.
- AO composite was already merged into one MRT pass in PR #302.
- This part will not be merged.

### 2.9 Vulkan mechanism trade-offs

| Mechanism | Trade-off |
|---|---|
| Keep `VkRenderPass` and add load-op variants (recommended, stages 1–2) | Vulkan 1.2 is enough. Pipelines do not need to change. Turnip and the Qualcomm official driver both support it. |
| Dynamic rendering (1.3, or `VK_KHR_dynamic_rendering`) | Does not reduce passes by itself. It removes the need for render pass objects, and load/store is specified at begin. Only with `dynamic_rendering_local_read` does in-tile reading become possible. Consider only in stage 3, when in-pass resolve or in-tile post-processing is needed. |
| Multiple subpasses | Pipeline compatibility requires the same subpass structure, which doubles the number of pipelines. Not recommended. |
| Deferred recording with secondary command buffers | Lets the load/store decision wait until the pass is recorded, but only the load can be decided, because store depends on future accesses. Adds CPU overhead, cannot be used on desktop, belongs to stage 3. |

### 2.10 Metal

- Stage 1 directly reduces encoders: clear-only encoders disappear, or merge into the next encoder's `LoadActionClear`.
- KEEP_COLOR is already on by default on Apple.
- Later additions: `StoreActionDontCare` for depth that is known to be never read again, and memoryless for intermediate textures (stage 3, with the same prerequisite of knowing future accesses).
- Must verify encoder count and FPS on the M1 Max over SSH.

---

## 3. Risks

1. **Missed execution point, reading EDRAM content from before the clear** (the biggest risk).
   - Countermeasure: gather all accesses into the three entry points in §2.1.
   - Debug mode `LO_EDRAM_LAZY_CLEAR=poison`: when a pending clear runs, use magenta in place of the real value, so missed paths become visible on screen. The idea is the same as `LO_CLEAR_RT=magenta`.
   - Keep a same-binary switch `=0` that returns to immediate clears.
   - Transfer (`tileOwner`) reads the texture of the old class and writes the texture of the new class. Both must run their pending clears first.
2. **Bookkeeping time points**: see the end of §2.1. Those flags are updated at the logical clear time and must not be deferred with the GPU command. `FlushMotionReplayQueue` is still called at the logical clear point, so queued motion-vector replays are recorded before the depth clear.
3. **Temporal effects**: TAA, SR, motion-vector replay and FSR alpha read only resolve results and depth resolves, plus scene depth at draw time. The clear values and their order do not change, so the inputs do not change. What must be confirmed is that these paths read depth through the `SettleForRead` or `Transition` entry point.
4. **HDR sidecar**: the sidecar is not an EDRAM alias. Its `hdrValid` is invalidated at clear time. That logic must not be changed. A DONT_CARE on the sidecar holds only when the following copy covers the whole target.
5. **Screenshots and debug tools**: `DumpRenderTargets`, `DumpTexture`, debug captures, `taa_debug` readback, and `present_capture` must all call `SettleAll()` before reading back. Otherwise debugging would show old content that has not been cleared yet, which would mislead investigation.
6. **Which changes can be proven byte-for-byte identical, and which cannot**:

   | Change | Byte-for-byte identical? |
   |---|---|
   | Stage 1 | Yes. `vkCmdClearAttachments` and loadOp=CLEAR convert the clear value to the attachment format by the same rules. |
   | Stage 1b | Requires the rasterized z to be bit-identical to `rectZ`. Must be proven with a fixture on each backend. If not, fall back to drawing the draw. |
   | Stages 2a/2b | Shader and state do not change. |
   | Between GMEM and sysmem | The driver does not guarantee identical blend results. Capture an image under `TU_DEBUG=gmem` and under `TU_DEBUG=sysmem` and compare. |
   | Whole game | Frames are not identical between runs of the game, so whole-game consistency is guaranteed by "equivalence by construction" plus fixtures, not by proof from sampled screenshots. |
7. **Driver issues**:
   - ANGLE recorded that the Qualcomm official driver had errors with "loadOp=Clear followed by inline commands" (2020, reportedly fixed in newer drivers. Only a search summary was read).
   - Dolphin has `BUG_BROKEN_CLEAR_LOADOP_RENDERPASS` for ImgTec.

   Countermeasure: keep a per-driver switch to turn off loadOp=CLEAR. The default Turnip is not affected. But when users switch to the official driver, it must be verified on Adreno 750/840.
8. **Autotune flips and limits of LRZ fast clear**:
   - loadOp=CLEAR changes autotune's cost estimate, and may switch some passes to GMEM.
   - LRZ fast clear is valid only for 0.0 and 1.0.

   Each stage must be measured per pass on the tablet in a hot state.
9. **Occlusion queries**: a query must be in the same pass as the draws it counts. A clear at the start happens before the query begins, so it is not affected. Stage 1b does no conversion while a query is active.
10. **Desktop CPU**: each draw adds only one check, and clear records are produced only at clear events. Uhra at 720p is CPU-limited, so `draw_ms` must confirm there is no regression.

---

## 4. Stage plan

### Stage 0: measurement and diagnostics (1.5–2 days, do first)

- Turn the per-pass timing from `apply_diag_v100.py` into a resident debug switch. It takes effect only when the switch is on, and it is not part of the release path:
  - Record each pass's start category and end reason.
  - Add a count of single-operation passes.
  - Also add site markers to `ambient_occlusion.cpp` and `presentation.cpp`, so that errors like "TAA resolve" do not happen again.
  - On Metal, record the encoder count.
- Tablet in hot state (reuse `hot.sh` NOGATE heat soak and before/after bracketing). Run in order:
  1. ABBA of `LO_VK_NO_END_DEP=0/1`.
  2. `LO_DEPTH_ONLY_KEEP_COLOR=0/1`, with `TU_DEBUG=sysmem` and with `gmem`.
  3. `TU_DEBUG=perf` counts of each LRZ-off reason per frame, as the baseline for later stages.
  4. If Turnip supports it, add `TU_AUTOTUNE_ALGO=prefer_sysmem/prefer_gmem`.
- Output: knowing why KEEP_COLOR is 9% slower, and whether the main view could win under GMEM. This decides whether stage 2a is done, and whether stage 3 is worth the investment.

### Stage 1: deferred clear execution with loadOp=CLEAR (5–7 days, recommended first stage)

- **Scope of changes**:
  - renderer.cpp: A1, A2, clears attached to resolves, `ClearDepthTarget`, plus the three execution entry points.
  - Counting: whether the transfer's source was only fully cleared within this frame (preparing for the constant transfer in §2.4). Whether the main depth's clear rectangle covers the full texture.
  - plume-lostodyssey.patch: Vulkan load-op variants, and the start clear.
  - Metal needs no changes.
- **Expected** (estimate):
  - D1 drops from 307 passes to about 175–185. The 145 A-category passes mostly disappear, and the shadow segment begins to merge.
  - At native resolution, GPU drops 1.5–2.5 ms. The LRZ benefit is extra.
  - At 720p, GPU drops about 1 ms. FPS is limited by the render thread, so it stays essentially the same.
  - Apple's gain must be measured separately.
- **Verification**:
  - CPU unit tests: pending-clear state machine. Feed it sequences of operations, and check execution points and the commands finally recorded.
  - GPU fixture, modeled on `LoDepthClearGpuTest`: the same script of "clear, draw, resolve, transfer, readback" runs once with immediate clears and once with deferred clears. On D3D12 and Vulkan (local machine), and Metal (Mac), read back all resolve targets and compare byte for byte.
  - In-game same-binary switch: A/B on static frames (title, pause menu, first frame of Scenario Jump). Sample the map tour and cutscene tour. Investigate any differences one by one.
  - Tablet hot-state per-pass timing at native, 1080, and 720. The number of `CmdClearAttachments` in `TU_DEBUG=perf` should drop noticeably.
  - Desktop RX 9070 XT (D3D12 and Vulkan), RTX, plus M1 Max: GPU ms and `draw_ms` must not get worse.
  - VVL synchronization validation (psvita native VVL environment).

### Stage 1b: deferring clear-rectangle draws (3–4 days)

- Content in §2.2. Also add a fixture: compare rasterized z with `rectZ`, and enable the conversion only when they are bit-identical.
- Expected: 30–45 fewer passes. The shadow segment keeps only one pass per shadow resolve (estimate).

### Stage 2: transfers into passes, main-view alternation merging, DONT_CARE (3–5 days, plus measurement)

- 2b transfers into passes: 8 fewer.
- 2a alternation merge: 60–65 fewer. Enabled only if stage 0 proves a gain. It may be enabled only on Apple plus Turnip, and not on the official driver.
- Renderer-owned full-screen passes use DONT_CARE.
- Expected total: 70–75 fewer passes (estimate). Under sysmem on Adreno, the direct gain of this part is close to 0. Its real value is giving GMEM and LRZ a chance to work.
- Verification as in stage 1. KEEP_COLOR requires a tablet ABBA.

### Stage 3 (suggested after 1.0, or decided by data)

- Frame-level deferred recording to decide storeOp and transient: enabled only on tile-based GPUs. Desktop does not take this path.
- Copy-on-write for resolve copies, with statistics first.
- In-pass resolve using `dynamic_rendering_local_read`, predicted from the previous frame's structure.
- Tuning GMEM settings for Turnip.
- Effort 10+ days, high risk.

---

## 5. Approaches to borrow (only those with sources)

- **Eden (successor to yuzu)**:
  - `vk_scheduler.cpp` has `DeferColorClear`, `DeferDepthStencilClear`, and `RealizeDeferredClear`.
  - `vk_texture_cache.cpp` chooses the CLEAR variant by `Framebuffer::RenderPassVariant`, and `vk_render_pass_cache.cpp` maps these to `LOAD_OP_CLEAR`.
  - Deferral conditions (`vk_rasterizer.cpp`): cover the whole render area, no pass is active, layer 0, and all channels are written. These are the same conditions as this plan.
  - DONT_CARE is used only for MSAA discards, and there is no subpass merging.
  - <https://git.eden-emu.dev/eden-emu/eden/src/branch/master/src/video_core/renderer_vulkan>
- **Dolphin**:
  - `VKGfx::ClearRegion` has three paths: a full clear outside a pass uses `BeginClearRenderPass` (loadOp=CLEAR). Inside a pass it uses `vkCmdClearAttachments`. Otherwise it draws a quad.
  - `DriverDetails.cpp` marks ImgTec with `BUG_BROKEN_CLEAR_LOADOP_RENDERPASS`.
  - EFB copies also end the pass, the same as resolves here.
  - <https://github.com/dolphin-emu/dolphin/tree/master/Source/Core/VideoBackends/Vulkan>
- **Xenia**:
  - `VulkanRenderTargetCache::PerformTransfersAndResolveClears` gives transfers their own render pass. Draws go into `last_update_render_pass()`. The pass ends when the pass or framebuffer changes.
  - Clears attached to resolves use `vkCmdClearAttachments` inside the transfer pass. All attachments are LOAD/STORE.
  - Its structure is the same as this project's current one, so it should not be used as a model for improvements. Its FSI/ROV path keeps EDRAM in a buffer and has no pass breaks, but its option description says it is "much slower."
  - <https://github.com/xenia-project/xenia/tree/master/src/xenia/gpu>
- **Mesa Turnip** (AOSP mirror, 2025-03 snapshot, about Mesa 25.0/25.1. Changes from 2026 came from AlmaLinux revert patches. The current upstream main could not be read):
  - `tu_autotune.cc` and `tu_pass.cc`: bandwidth cost model. `fallback_use_bypass` uses sysmem when draws ≤5.
  - `use_sysmem_rendering` in `tu_cmd_buffer.cc`: conditions that force sysmem. These include an in-pass non-by-region barrier, which sets `disable_gmem`.
  - `tu_lrz.cc`: `vkCmdClearAttachments` on depth turns LRZ off. loadOp=CLEAR keeps LRZ valid. A650+ with LOAD carries over the earlier state.
  - The 2026 revert patches show upstream added `TU_AUTOTUNE_ALGO` (`prefer_sysmem`/`prefer_gmem`/`profiled`) and `TU_AUTOTUNE_FLAGS`.
  - "stencil write based on depth test" cannot be found in the 2025 snapshot. It is a newer rule, and this is not verified.
  - <https://android.googlesource.com/platform/external/mesa3d/+/refs/heads/main/src/freedreno/vulkan/>
- **Vendor guidance**:
  - Arm's load/store blog: CLEAR plus DONT_CARE reads 36% less and writes 62% less. <https://developer.arm.com/community/arm-community-blogs/b/mobile-graphics-and-gaming-blog/posts/picking-the-most-efficient-load-store-operations>
  - Khronos render pass sample: use loadOp=CLEAR instead of `vkCmdClear*`. Attachments used in only one pass should be transient. <https://docs.vulkan.org/samples/latest/samples/performance/render_passes/README.html>
  - Imagination: prefer DONT_CARE or CLEAR. If the store is not needed, use DONT_CARE. <https://docs.imgtec.com/performance-guides/graphics-recommendations/html/topics/using-render-passes-and-sub-passes.html>
  - Qualcomm "Avoid GMEM loads": <https://docs.qualcomm.com/bundle/publicresource/80-71528-1/topics/avoid_gmem_loads.md>. Qualcomm's Tiling page and Apple's TBDR page could not be opened directly, so nothing from them is cited here.
- **Not found with a reliable source**: Skyline (source removed after takedown), Ryujinx and Ryubing (repositories no longer accessible), and Triang3l's statements about tilers.

---

## 6. Appendix: data and derivations

- **Tablet D1/D2**: `apk-v100\out\perf\D1-trace.logcat`, `D2-trace.logcat`, trace frame number 1292.
  - Classification by site: a site equals the current code line number, plus 5 for lines before 4908 and plus 6 after.
  - Mapping: 10476 is the `setFramebuffer` in DrawImpl. 12257/12292/12332 are A1/A2/A3. 2732 is `TransferRegion`. 3873 is `BlitRegion`. 9699 is AO (no site, inherited from the previous). 10017 is the game draw after AO. 13774 is present.
  - The main view is judged by width 2560 and height 1600–1699. D2's shadow is 3456x3456, so it cannot count as the main view.
- **Tablet frame time** (hot state):
  - Default settings 23 FPS, GPU about 41.5 ms. High settings 17 FPS, GPU about 56 ms.
  - Resolution sweep: native GPU 42.6 ms, 1080 is 27.0 ms, 720 is 17.1 ms (`draw_ms` 20.2).
  - Rough fit: GPU ≈ 7–8 ms fixed part + about 8.5 ms per megapixel.
- **Phone T1**: `apk-289\out\perf\T1-trace.runtime.log`. Site mapping was checked against `renderer.cpp.before-diag`, with offsets of 27 and 6.
- **Old conclusion ([OPTIMIZATION_PLAN.md](../archive/OPTIMIZATION_PLAN.md))**: measured on M1 Pro on 2026-09-29. Reasons for pass ends: color target switches 39%, barriers 22%, depth target switches 21%. At that time it was judged that there was little room for merging. That measurement did not separate alias clear propagation. This time, after classifying by site, it accounts for nearly half the passes.
- **To be confirmed**:
  - Whether Turnip 26.0.0 R8 has `TU_AUTOTUNE_ALGO`.
  - The reason KEEP_COLOR is 9% slower.
  - Whether the main depth's clear rectangle covers the full texture (needs counting).
  - What fraction of resolve copy sources are not written afterward.
