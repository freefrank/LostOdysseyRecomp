# Native Vulkan DLSS Color Qualification Plan & Implementation Specification

Date: 2026-09-21
Status: **Implemented qualification path; focused boundary evidence passed; broad visual acceptance pending**
Baseline Commit: `c2f0602`
Reference Captures: `out/p2-game-cwd-c2f0602/captures/render-17900119851502951-f11889.zip` (complete frames 11889–11891)

## 1. Overview & Capture Evidence Summary

On 2026-09-21, Lost Odyssey Recomp was built with `LO_ENABLE_DLSS=ON` and executed in a visible foreground window (`PID 42720`, isolated CWD `out/p2-game-cwd-c2f0602`, assets from `LostOdysseyRecompLib/private/disc1`). The user entered the 3D world and triggered render state capture via `F1` (not `LO_CAPTURE_REQUEST`, which only triggers legacy single-frame dumps).

A full 3-frame capture was obtained (`frames 11889–11891`). Read-only oracle inspection of `p2-oracle.jsonl`, `capture-info.txt`, shader disassembly, and render state dumps confirmed the exact post-processing pipeline for this capture sequence:

1. **HDR Scene Buffer**: Allocation 9, address `0x09fa0000`, guest format 32.
2. **Post-Processing Tonemap Draw (Draw 632 in this capture)**:
   - Vertex Shader: `9b81c55ca39bb529`.
   - Pixel Shader: `b4b4d54a7a2d6b96`.
   - Output Target: EDRAM Allocation 3, guest base 720 decimal (`0x2d0`), guest color format 0 (which maps to host FP16 / format 10), 1280×736 buffer with 1280×720 active scene rectangle.
   - Blend / Color Mask: `colorMask = 7` (RGB written, Alpha channel preserved; predicate requires `(colorMask & 7) == 7`).
   - Pixel Shader Constant: `c10.x = 0x3ee8ba2e` (`0.4545454383` ≈ `1.0 / 2.2`), identical across all three captured frames.
    - Mathematical Transfer: Computes `pow(saturate(M + 0.1725 * bloom), 1.0 / 2.2)`. For this captured pipeline, this establishes that the output is **display-encoded SDR** (gamma 2.2 curve). It is **not** an exact standard sRGB transfer, **not** sRGB-decoded, and **not** linear HDR. This observation applies strictly to this analyzed capture chain and is not generalized to the whole game.
3. **Resolve Operation**:
   - Resolve Target: Allocation 20, address `0x0b0d9000`, guest format 6 (`R8G8B8A8_UNORM`, host 20).
   - Resolve Setup: Red/Blue swap enabled.
   - Texture Sampling: Sampled in Draw 659 via `shared_texture_info = 0x00160a00`, `sign = 0` (fetch BGR).
   - Net Transfer: The resolve RB swap combined with the sample BGR fetch results in net identity RGB transfer (no additional gamma or color space shift).
4. **Scene Copy / UI / Frontbuffer Flow**:
   - Draw 659: VS `8bbd4da701845d16`, PS `cda578aef1724fdc`, `colorMask = 15`, alpha = 1.0, copy blend `ONE / ZERO / ADD`.
   - Between Draw 632 and Draw 659: 24 intermediate draws with `(colorMask & 7) == 0` (depth/stencil or alpha-only passes; zero RGB modification).
   - Draw 660+: Transparent UI overlays rendered directly into the target.
   - Frontbuffer: Allocation 4, address `0x00714000`.
   - Sequential Write Ordinals:
     - Allocation 9: 207457 (f11889), 207475 (f11890), 207493 (f11891).
     - Allocation 20: 207464 (f11889), 207482 (f11890), 207500 (f11891).
     - Frontbuffer (Alloc 4): 207465 (f11889), 207483 (f11890), 207501 (f11891).
   - Candidate State: `candidate_ready = true`, `scene_copies = 1`, `rejections = 0`.
   - Frame Planning Consumer: Evaluated to `consumer = 0 (None)`, `input = 1280x720`, `output = 1280x720`, `geometryEpoch = 1`. Downscaling did not activate despite `LO_DLSS_INPUT_PROBE=1` being set in the environment.

Note: Draw numbers 632 and 659 are capture-specific timeline indices from this run, not hardcoded engine rules.

## 1.1 Current production execution boundary

The later live-game record `.cache/evidence/game-sr-runtime.json` confirms
Quality NGX SR on an RTX 5080 at `1707x960 -> 2560x1440`, with
`DisplayEncoded` color and reversed-Z depth. Runtime qualification accepted the
actual physical uploaded quad, exact resolve ordinal and net RGB view; other
candidate paths remain `Unknown`. The CPU qualification boundary passed
(`.cache/evidence/p2-native-run/color_qualification_cpu_boundary.log`).

This record confirms production execution only. It does not claim visual
quality, motion response, occlusion, UI, reset behavior or player acceptance.

## 2. Color Qualification Architecture (Lightweight Tracking)

Color qualification is tracked using localized state without global scene graphs or redundant image copies:

### 2.1 HostTexture Metadata Extensions
`HostTexture` only records the producer frame and qualified rectangle:
```cpp
uint64_t sdrProducerFrame = ~0ull;
uint32_t qualifiedSdrWidth = 0;
uint32_t qualifiedSdrHeight = 0;
```
(`HostTexture` does not maintain `sdrWriteOrdinal`.)

### 2.2 ResolvedSurface Metadata Extensions
`ResolvedSurface` holds the write qualification ordinal created at resolve time:
```cpp
uint64_t sdrWriteOrdinal = 0;
```

### 2.3 Lifetime & Invalidation Rules
- **Mark Producer**: On successful recording of a verified producer draw on `HostTexture`, set `sdrProducerFrame = frame`, `qualifiedSdrWidth = activeRect.width`, `qualifiedSdrHeight = activeRect.height`.
- **Invalidation Triggers on Producer**: Any operation writing RGB outside the qualified producer resets `sdrProducerFrame = ~0ull`:
  - Unknown RGB draw writes (`(colorMask & 7) != 0`).
  - Render target clears (`Clear`, `ClearColor`).
  - Region transfers / blits (`TransferRegion`, `BlitRegion`).
  - Promotion resamples (`DrawPromotionResample`).
  - Movie / letterbox bar clears.
  - Resolve clears.
- **Permitted Operations (Do NOT Invalidate Producer)**:
  - Draws with `(colorMask & 7) == 0` (e.g. depth/stencil passes or alpha-only writes with `colorMask == 0` or `colorMask == 8`).
  - Normal mid-frame command list `Flush` (persists across flushes).
- **New Allocations**: Newly allocated textures start with `sdrProducerFrame = ~0ull`.
- **Resolve Immutability**: Resolving creates `rs.sdrWriteOrdinal = currentWriteOrdinal`. Partial resolve clears `rs.sdrWriteOrdinal = 0`. Once resolved into `rs`, subsequent UI draws modifying the source `HostTexture` do not retroactively invalidate the resolved snapshot.

## 3. Producer Verification Predicate

To qualify a draw as the SDR tonemap producer, all of the following runtime conditions must be satisfied:

1. **Shader Hashes**:
   - Vertex Shader hash == `0x9b81c55ca39bb529`.
   - Pixel Shader hash == `0xb4b4d54a7a2d6b96`.
2. **Tonemap Exponent Constant**:
   - Pixel shader constant `c10.x` bit representation == `0x3ee8ba2e` (`0.4545454383`).
3. **Blend & Mask State**:
   - `(colorMask & 7) == 7` (RGB mask fully enabled; alpha channel may be masked or written, e.g. 7 or 15).
   - Blend state == Copy (`ONE / ZERO / ADD`).
4. **Rasterizer & Pipeline State**:
   - Depth test disabled or pass-all without rejection.
   - Stencil test disabled.
   - Triangle culling disabled.
   - Alpha test disabled.
   - No debug overrides active.
5. **Target Format & Precision**:
   - Target format is guest color format 0 (maps to host FP16 / format 10; distinct from host enum `k_FMT_16_16_16_16_FLOAT`).
   - Target exponent bias == 0.
6. **Geometric Coverage Predicate (Runtime Predicate Required)**:
   - *Evidence status*: Current capture ZIPs contain state dumps but lack direct vertex upload buffer dumps proving full screen-space coverage. Vertex coverage cannot be inferred purely from viewport dimensions.
   - *Runtime verification requirement*: The predicate must inspect the active vertex upload arena (6 indices, triangle list, 2 triangles in vertex slot 95, stride 32, offset 0, format `float4 position`).
   - Must verify:
     - Finite coordinates across all vertices (no NaN / Inf).
     - `W == 1.0` for all quad vertices.
     - Vertex format is float4 (vtxFmt 4).
     - Coordinates satisfy Z-clipping boundaries.
     - Shared screen projection aligned to the half-pixel physical pixel grid.
     - Two triangles share a common diagonal and touch all four corners of the actual active scissor / activeRect (which scales with resolution, not hardcoded to 1280×720).

## 4. Resolve & Consumer Verification Predicate

When resolving and sampling into the temporal scene copy:

1. **Resolve Operation**:
   - Source texture must have `sdrProducerFrame == frame`.
   - The qualified rectangle `[qualifiedSdrWidth, qualifiedSdrHeight]` must fully cover the resolved region without partial clipping.
   - Destination `ResolvedSurface` must be guest format 6 (`R8G8B8A8_UNORM`, host format 20).
   - Full-surface resolve without partial overwrite.
   - Set `rs.sdrWriteOrdinal = currentWriteOrdinal`.
2. **Sampling & RB Swizzle Identity**:
   - Evaluate net color transfer of texture fetch swizzle + resolve RB swap using a dedicated helper that inspects raw fetch instruction bits and swap flags directly. (Do not read unpopulated runtime `shared.textureInfo` structs).
   - Confirm net RGB transfer is identity `(R->R, G->G, B->B)` (e.g. matching `shared_texture_info = 0x00160a00`, `sign = 0` with RB swap).
3. **Consumer Verification Prior to Capture Inputs**:
   - Evaluate before `CaptureColorInputs`: lookup via `FindResolved(fetchAddress, fetchFormat)`.
   - Verify `rs.tex.get() == boundTexture`.
   - Verify `rs.frame == currentFrame`.
   - Verify `rs.sdrWriteOrdinal == writeOrdinal && rs.sdrWriteOrdinal != 0`.
   - Verify matching `temporalSceneCopy` ordinal, depth bounds, and resolution extent guardrails against `activePlan.input`.
   - When verified: Assign `CaptureColorInputs` with `ColorEncoding::Sdr`.
   - When unverified: Assign `ColorEncoding::Unknown`.
   - **Bypass vs Error Latching**: If color qualification fails, assign `Unknown` and perform a clean per-frame bypass without permanently latching the user's DLSS configuration request as disabled. True NGX initialization/recording failures continue to disable the request, while device loss triggers a fatal latch.

## 5. Verification & Test Plan

1. **Unit & Policy Fixtures (CPU)**:
   - Positive policy cases verifying `HostTexture` producer marking and `ResolvedSurface` ordinal qualification.
   - Negative policy cases verifying invalidation on `(colorMask & 7) != 0` unknown writes, clears, blits, partial resolves, and mismatched `c10.x`.
   - Verify that alpha-only passes `(colorMask & 7) == 0` (e.g. mask 0 or mask 8) do not invalidate producer qualification.
2. **Renderer Wiring & Predicate Tests**:
   - Add a focused mock test in `LoNativeDlssRendererTest` verifying helper wiring and qualification parameter propagation without running full guest `DrawImpl`.
   - Runtime geometric quad predicate logic tested separately against synthetic vertex buffers.
3. **Targeted Gameplay Acceptance**:
   - Run game in visible foreground mode.
   - Verify runtime predicate passes on real scene quad uploads.
   - Confirm `p2-oracle.jsonl` reports `ColorEncoding::Sdr` and `temporal_history_verified = true`.
4. **Execution Gate Boundary**:
    - The 5 native RTX test results may be reused while behavior and configuration remain unchanged; rerun only for a relevant change or new failure evidence.
    - In-game Super Resolution motion and visual quality acceptance remains pending after the qualification, extent-growth and cold-start evidence already documented.
