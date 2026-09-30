# Motion Vector Vertex Shader Position Dependency Analysis (M1 Report)

Date: 2026-09-18
Dataset Source: `LostOdysseyRecomp-build-inputs/feedback` (Triage Ledger, Shader Reviews, Microcode)

## 1. Executive Summary

This report establishes the ground-truth vertex shader (VS) position reconstruction dependencies for Lost Odyssey Recomp, derived directly from upstream triage datasets, microcode disassembly, and shader frequency ledgers.

To generate accurate per-pixel motion vectors:
1. Every draw call outputs clip position $p_{\text{curr}} = \text{Transform}_{\text{curr}}(v)$.
2. If tracked from the previous frame with matching geometry identity, the vertex can be transformed into previous clip space $p_{\text{prev}} = \text{Transform}_{\text{prev}}(v)$.
3. The motion vector is the backward screen-space displacement (in pixels):
   $$\mathbf{v}_{\text{pixel}} = \text{Raster}(p_{\text{prev}}) - \text{Raster}(p_{\text{curr}})$$
4. If previous draw state is not matched, fallback camera reprojection from depth is used, and high-velocity/disocclusion boundaries are flagged in the reactive mask.

---

## 2. Vertex Shader Families & Position VP Slots

From `feedback/triage/ledger.json` and microcode audits in `feedback/reviews/`:

### 2.1 VP Slot 7 (High Frequency World + ViewProjection)
- **Top Shaders**:
  - `3c86f4a89d220ee8` (33,218 draws recorded): Static/rigid world transforms.
  - `f3b9f20b3d3a62d5` (33,134 draws recorded): Core geometry / environment passes.
  - `e7b38eb08c70e5e1` (16,567 draws recorded): Props and architecture.
  - `6a8c2c78737dc94c` (4,423 draws recorded): Battle and field scene assets.
  - `3f522a748751d16a` (4,423 draws recorded): Character / prop lighting passes.
- **Position Calculation**:
  - Microcode extracts position via vertex stream (typically stream 0 / r0).
  - World matrix resides in constants `c0..c3` (or local translation).
  - ViewProjection matrix resides in `c7..c10`.
  - Clip position: $p_{\text{world}} = v \times \text{Matrix}(c0..c3)$, $p_{\text{clip}} = p_{\text{world}} \times \text{Matrix}(c7..c10)$.
  - **Replay Rule**: To replay previous position, evaluate $p_{\text{clip, prev}} = (v \times \text{Matrix}_{\text{prev}}(c0..c3)) \times \text{Matrix}_{\text{prev}}(c7..c10)$.

### 2.2 VP Slot 8 (Terrain & Extended Environment)
- **Key Shaders**:
  - `8d9770d1bd8ba0fa` (2,256 draws): Terrain / multi-uv terrain surfaces.
  - `6761469677f921c6` (999 draws): Water / blended surfaces.
  - `b60fba087b51eb53` (927 draws): Terrain decals.
- **Position Calculation**:
  - ViewProjection matrix resides in `c8..c11`.
  - World matrix resides in `c0..c3`.
  - **Replay Rule**: Evaluate with previous $c0..c3$ and previous $c8..c11$.

### 2.3 VP Slot 4 (Rigid Scene Props & Standard Objects)
- **Key Shaders**:
  - `8d3c80b318235b22` (1,152 draws)
  - `52e4405f97159d2f` (858 draws)
  - `ea0c0e0e98031d27` (846 draws)
- **Position Calculation**:
  - ViewProjection resides in `c4..c7`.
  - **Replay Rule**: Evaluate with previous $c0..c3$ and previous $c4..c7$.

### 2.4 VP Slot 0 (Screen-Space / Post / Direct Projection)
- **Key Shaders**:
  - `2d458def192151ac` (136 draws), `6d3d954bb6d86bc1` (136 draws).
  - Direct 2D/orthographic projection in `c0..c3`.

### 2.5 High-Index Slots (c230, c233 - Skinned Characters & Dynamic Actors)
- **Position Calculation**:
  - Bone matrices stored dynamically starting from `c20..c220` indexed via register indexing (`usesRelativeConstants`).
  - ViewProjection resides at high register space `c230..c233` or `c233..c236`.
  - **Replay Rule**: Because bone palettes span dozens of float4 vectors across `c20..c220`, storing the full 256 float4 vector constant array (`DrawTemporalState::vsConstants`) is essential and sufficient to reconstruct skinned vertex positions in the previous frame.

---

## 3. Position Replay & Motion Vector Formulas

### 3.1 Pinhole / Raster Conversion
Given NDC coordinates $p_{\text{ndc}} = (x/w, y/w)$:
$$\text{Raster}(p) = \begin{pmatrix} \frac{1}{2}(p_{\text{ndc}}.x + 1) \cdot W \\ \frac{1}{2}(1 - p_{\text{ndc}}.y) \cdot H \end{pmatrix}$$

### 3.2 Backward Displacement
The TAA resolve pass in `temporal_aa.cpp` samples history at:
$$q = \text{position}_{\text{pixel}} + \mathbf{v}_{\text{pixel}}$$
Therefore:
$$\mathbf{v}_{\text{pixel}} = q - \text{position}_{\text{pixel}} = \text{Raster}(p_{\text{prev}}) - \text{Raster}(p_{\text{curr}})$$

---

## 4. Replay Decision Matrix

| Geometry Type | Identity Match | Previous State Available | Strategy |
| :--- | :--- | :--- | :--- |
| **Rigid / Static** | Yes | Yes | GPU Replay: Transform with previous constants ($c0..c3$ and VP slot). Exact pixel motion vector. |
| **Skinned Actor** | Yes | Yes | GPU Replay: Transform with previous bone palette + previous VP ($c230/c233$). Non-rigid actor motion. |
| **New Draw / Teleport** | No | No | Camera Reprojection Fallback from depth. Mark reactive mask on depth edge. |
| **Skybox / Clear** | - | - | Zero motion vector, reactive mask = 1.0 (no history smearing). |
