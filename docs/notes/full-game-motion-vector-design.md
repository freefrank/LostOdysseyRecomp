# Full-Game Motion Vector Architecture & Design (Phase 1 & 2)

**Date:** 2026-09-18  
**Scope:** Host Motion-Vector (MV) Representation, Pipeline Architecture, and Contract Definition  
**Prerequisite:** `docs/notes/full-game-motion-vector-audit.md`  
**Reference Handoff:** `Lost Odyssey Recomp — Full-Game Motion Vector Development Handoff.md`

---

## 1. Design Overview

Based on the Phase 0 findings:
1. **The Native Velocity Path** is strictly gated by engine thresholds (`sub_8274CEA8`) for dramatic motion-blur effects on fast-moving objects. It omits static scene geometry, subtle sway animations (e.g. the bell stand), and steady-state gameplay meshes.
2. **The Host Automatic MV Pipeline** must therefore be the primary motion provider:
   - For every eligible scene draw, compute both the **current screen position** and the **previous screen position**.
   - Output the geometric displacement:
     $$\mathbf{v} = \mathbf{x}_{\text{prev}} - \mathbf{x}_{\text{curr}}$$
   - Store velocity in a dedicated host texture (`RG16_FLOAT` or `RGBA16_FLOAT`) in **render pixel units**.

```text
                     Lost Odyssey Draw Submission
                                 │
                   ┌─────────────┴─────────────┐
                   │                           │
            Standard Color/Depth        Motion Vector Generation
            Passes (Unchanged)          (Host Geometric Replay)
                   │                           │
                   │               ┌───────────┴───────────┐
                   │               │ Match Frame N-1 State │
                   │               └───────────┬───────────┘
                   │                           │
                   │             ┌─────────────┴─────────────┐
                   │             │                           │
                   │     Temporal Match Proven     No Proven Match / Discontinuous
                   │             │                           │
                   │     Replay Prev Transform        Output Reactive Flag (w = 1.0)
                   │             │                           │
                   │             └─────────────┬─────────────┘
                   │                           │
                   │                 Render into Host MV RT
                   │                 (Format: RG16F / RGBA16F)
                   │                           │
                   └─────────────┬─────────────┘
                                 │
                        TAA Resolve / Upscaler
                      (Consumes Color, Depth, MV)
```

---

## 2. Motion Vector Specification & Conventions

### 2.1 Coordinate Space and Direction
- **Direction:** **Current to Previous (Backward)**.
  A pixel currently located at raster coordinate $(x_{\text{curr}}, y_{\text{curr}})$ originated at:
  $$x_{\text{prev}} = x_{\text{curr}} + v_x$$
  $$y_{\text{prev}} = y_{\text{curr}} + v_y$$
  *Rationale:* This matches the sampling requirement of temporal accumulation in TAA, DLSS, and FSR without per-texel negation.

- **Units:** **Continuous Render Pixels**.
  $v_x$ and $v_y$ are expressed in unnormalized render texels:
  - Moving 1 pixel to the left from current to previous: $v_x = -1.0$.
  - Moving 1 pixel down: $v_y = +1.0$.
  *Rationale:* Keeps the internal motion field independent of resolution changes, dynamic resolution scaling, and vendor-specific normalizations (e.g. Streamline $[0, 1]$ UV vs NGX pixels).

### 2.2 Camera Jitter Separation
- **Jitter Convention:** **Unjittered Geometric Motion**.
  The motion vector represents pure physical/geometric movement across frames.
  - The current vertex transform uses the unjittered camera matrix $VP_{\text{curr}}$.
  - The previous vertex transform uses the unjittered previous camera matrix $VP_{\text{prev}}$.
- Any sub-pixel camera jitter applied during rasterization (via `xeHalfPixelOffset` / `temporal_jitter.h`) is tracked separately as metadata and passed directly to upscalers/TAA.
- *Rationale:* Eliminates jitter cancellation errors and ensures compatibility with FSR 3.1 / DLSS requirements.

### 2.3 Format & Channel Allocation
- **Primary Texture Format:** `plume::RenderFormat::R16G16_FLOAT` (or `R16G16B16A16_FLOAT` where validity/reactivity is packed into BA).
  - Channel `R`: $v_x$ (Horizontal motion, render pixels).
  - Channel `G`: $v_y$ (Vertical motion, render pixels).
  - Channel `B`: Depth difference or motion magnitude (optional diagnostics).
  - Channel `A`: Validity / Reactive mask ($0.0 = \text{Valid}$, $1.0 = \text{Reactive/Invalid}$).

### 2.4 Invalid / Discontinuous Pixel Handling
- If a draw has no valid previous frame state (e.g. newly spawned object, particle, teleportation, LOD switch, camera cut):
  - **Do NOT write $(0, 0)$ motion.** $(0, 0)$ asserts a valid stationary surface.
  - Mark the pixel as **Reactive / Invalid** (e.g. write $(0, 0)$ with Reactive flag = 1.0, or clear with a sentinel value).
  - TAA resolve will identify reactive pixels and fall back to clamping/current frame color instead of historical reprojection.

---

## 3. Draw History & State Retainment

### 3.1 Draw History Key
To avoid false pairings across frames, history is matched using a strict conservative key:

```cpp
struct DrawHistoryKey
{
    uint64_t vsHash;
    uint64_t indexBufferAddress;
    uint64_t positionBufferAddress;
    uint32_t firstIndex;
    uint32_t indexCount;
    int32_t  baseVertex;
    uint32_t topology;
};
```

### 3.2 Retained State per Draw
For each qualifying draw in frame $N$:
1. **VS Constant Bank:** Save the relevant 256 `float4` constants (4096 bytes), or the subset containing transform and bone palette.
2. **Camera Anchor:** Associate the draw with the active `SceneAnchor` (camera matrices, viewport).
3. **Dynamic Buffer Generation:** If vertex buffers are dynamically updated by CPU on guest, retain the previous buffer generation tag.

### 3.3 Ring History Buffer & Synchronization
- History buffers are managed per frame in an $N$-buffered ring ($N \ge 2$) owned by `HistoryOwner`.
- GPU resources (retained depth, history color, MV target) are transitioned and protected by GPU fences before recycling to prevent CPU/GPU race conditions.

---

## 4. Reset & Invalidation Rules

History must be completely reset (`Reset()`) upon:
1. First render frame after startup.
2. Map loading / level transition.
3. Camera cut / scene discontinuity (detected by `ContinuousHistoryCamera` exceeding quarter-screen displacement).
4. Window resize or resolution change.
5. Graphics device loss or recreation.
6. Temporal epoch increment.

---

## 5. Verification Matrix

| Test Case | Scenario | Expected Outcome |
|---|---|---|
| **Test 1: Static Scene & Camera** | Camera stationary, geometry stationary | $v_x \approx 0.0, v_y \approx 0.0$ across entire raster. |
| **Test 2: Camera Pan** | Camera moving, geometry stationary | MV field exactly matches analytic camera reprojection from depth. |
| **Test 3: Rigid Moving Object (Bell Stand)** | Camera stationary, bell swaying | Background $v \approx 0$, bell stand $v \neq 0$ matching sway trajectory. TAA shimmer eliminated. |
| **Test 4: Camera Cut** | Sudden view change | Epoch bumped, history invalidated, no ghosting/streaking. |
