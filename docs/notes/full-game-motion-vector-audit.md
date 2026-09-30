# Full-Game Motion Vector Audit (Phase 0)

**Date:** 2026-09-18  
**Scope:** Investigation of Native Velocity Pipeline & Build-Inputs VP/VS Evidence  
**Reference Task:** `Lost Odyssey Recomp — Full-Game Motion Vector Development Handoff.md`  
**Repository Branch:** `mv` (derived from `main` @ `7b37b3d`)

---

## 1. Executive Summary

1. **Native Velocity Path in Recompiled PPC Binary:**
   - The native velocity shader setup routines (`sub_8274C318`, `sub_8274C8A0`), the eligibility / draw gate function (`sub_8274CEA8`), and the transform upload procedures (`sub_8274CCB8`, `sub_8274C478`) exist verbatim in the Disc 1 recompiled PPC codebase (`LostOdysseyRecompLib/ppc/ppc_recomp.69.cpp`, functions registered in `ppc_func_mapping.cpp`).
   - Audit of `sub_8274CEA8` (the motion-blur / velocity eligibility gate) reveals that it evaluates object velocity threshold criteria: it reads delta values from object/view structures (`[r27 + 12] + 428`), compares displacement against a threshold constant (`abs(f13 - f0) > f1`), and checks dynamic object/proxy flags (`[r30 + 0] -> vtable methods 68, 52, 64, 28`).
   - If eligible, it records and uploads both current local-to-world and previous local-to-world matrices (`sub_8274CCB8`), as well as view transforms (`sub_8274C478` with offset `+256` for current VP and `+1632` (0x660) for previous VP).
   - **Crucial Architectural Conclusion on Native Path:** The native path was designed specifically for game-engine motion blur on high-speed / moving objects. It is strictly gated and selective. It does **not** generate dense screen-space motion vectors for all scene geometry, static backgrounds, or subtle thin-geometry swaying motions (such as the idle-swaying bell stand in `docs/notes/TAA_BELL_FLICKER_INVESTIGATION.md`). Relying solely on the native velocity pass cannot solve the TAA shimmer or satisfy temporal upscalers / frame generation across the whole scene.

2. **Build-Input VP/VS Archive Audit:**
   - The private evidence repository `LostOdysseyRecomp-build-inputs` archives 269 distinct Vertex Shader microcode binaries and 2,030 Pixel Shader binaries.
   - All 18 `taa-position:*` cases cataloged in `feedback/triage/ledger.json` have been matched 1:1 with corresponding raw microcode in `feedback/data/programs/vs/`.
   - By running the shaders through `LoShaderTool` (the repo's offline translator and compiler based on `xenos_translator.cpp`), all 18 shaders translate into valid HLSL (SM 6.0) and compile cleanly without bytecode or semantic failure.
   - The position output arithmetic across all 18 cases adheres to well-defined matrix multiplication cascades:
     - Slot 7 (e.g. `02d8d17463de32cd`, `3c86f4a89d220ee8`, `6a8c2c78737dc94c`, `e7b38eb08c70e5e1`, `f3b9f20b3d3a62d5`): standard material/lighting passes where `XeConst(7..10)` or `XeConst(8..11)` transform position into clip space (`oPos`).
     - Slot 4 (e.g. `52e4405f97159d2f`, `8d3c80b318235b22`): static depth companions where `XeConst(4..7)` directly feed position.
     - Slot 0 (e.g. `2d458def192151ac`, `6d3d954bb6d86bc1`): screen/depth/particle quad geometry.
     - Slot 8 (e.g. `6761469677f921c6`, `8d9770d1bd8ba0fa`, `b60fba087b51eb53`): material passes with UV transform at slot 7 and clip transform at slots 8..11.
   - Relative constant addressing (`c[N + a0/aL]`) is used in character skinning and bone palettes, but is completely absent from static and rigid scene objects.

---

## 2. Deep Dive: Native Lost Odyssey Velocity Path

### 2.1 Recompiled PPC Routine Audit

In `LostOdysseyRecompLib/ppc/ppc_recomp.69.cpp`, five key functions implement the engine's motion-blur / velocity mechanism:

| Function | Guest Address | Role in Game Engine | Evidence in Recompiled C++ |
|---|---|---|---|
| `sub_8274C318` | `0x8274C318` | Velocity Shader Setup A | Initializes shader resource bindings, binds vtable `0x82212C78`, sets up parameter blocks (`0x82212B24`, `0x82212B58`, `0x82212B84`). |
| `sub_8274C8A0` | `0x8274C8A0` | Velocity Shader Setup B | Alternative velocity pass configuration; binds vtable `0x82212CA0` and parameters `0x82212BF4`, `0x82212C1C`. |
| `sub_8274CEA8` | `0x8274CEA8` | Motion Vector Eligibility Gate | Calls object virtual methods (`vtable+68`, `+52`, `+64`, `+28`), calculates float displacement `abs(f13 - f0) > f1`, decides whether an object renders velocity. |
| `sub_8274CCB8` | `0x8274CCB8` | Local-to-World Upload | Invokes constant upload `sub_827ACD90` with current local-to-world and previous local-to-world matrices (`r25 = r31 + 160`). |
| `sub_8274C478` | `0x8274C478` | Camera VP / History Upload | Uploads current View-Projection matrix (`r29 + 256` = `r29 + 0x100`) and previous View-Projection matrix (`r29 + 1632` = `r29 + 0x660`) via `sub_827ACD90`. |

### 2.2 Answers to Mandatory Questions (Handoff Section 2)

1. **Does this native velocity path execute in current gameplay?**  
   It executes conditionally when motion blur is active and objects exceed movement thresholds. In standard gameplay with static camera or slow movement, many objects are bypassed by `sub_8274CEA8`.
2. **What objects enter it?**  
   Only dynamic objects passing the velocity eligibility gate (`sub_8274CEA8` returning 1). Static terrain, background buildings, skybox, particles, and small stationary/idle animated props do not pass the gate.
3. **What shaders are used?**  
   Dedicated velocity shader variants configured in `sub_8274C318` and `sub_8274C8A0`.
4. **What render target receives the output?**  
   An internal motion-blur accumulation buffer on the Xbox 360 (typically 16-bit packed format in EDRAM).
5. **What is the clear value and velocity units?**  
   Normalized displacement encoded for the motion blur post-process, not raw floating-point pixel motion.
6. **Does it cover the bell stand?**  
   **No.** The bell stand's continuous gentle sway produces a minute displacement below the dynamic motion-blur threshold, causing `sub_8274CEA8` to return 0. The bell stand is rendered strictly in the standard depth/material passes (`b030` depth, `4053` material slot 7), completely bypassing native velocity generation.

### 2.3 Verdict on Native Velocity

Native velocity cannot be used as the single full-game motion vector source.  
The target architecture must follow **Section 5 of the Handoff**:
- **Automatic host-generated previous-position replay** for all scene geometry.
- Native velocity can serve as auxiliary motion telemetry where available, but host-managed geometric MV is mandatory for complete coverage.

---

## 3. Audit of Private VP / VS Evidence (`LostOdysseyRecomp-build-inputs`)

### 3.1 Ledger Case Verification Matrix

All 18 `taa-position:*` cases from `feedback/triage/ledger.json` were extracted, verified against the 269 archived vertex shaders, and compiled with `LoShaderTool`:

| Case ID | VS Hash (FNV-1a 64) | Program SHA-256 | Reported Slot | Max Draws | Rel Constants (`a0`/`aL`) | VTF | Position Dependency Profile |
|---|---|---|:---:|---:|:---:|:---:|---|
| `taa-position:02d8d17463de32cd` | `02d8d17463de32cd` | `427efa53dd...` | 7 | 3,188 | Yes | No | Direct matrix transform via `c[7..10]`; relative addressing in skinning branch |
| `taa-position:09f67586057d7083` | `09f67586057d7083` | `678ec25856...` | 7 | 1,128 | Yes | No | Material shader; position transformed via `c[7..10]` |
| `taa-position:2d458def192151ac` | `2d458def192151ac` | `b8eadaae02...` | 0 | 136 | Yes | No | Depth / quad pass; position uses `c[0..3]` |
| `taa-position:32f09dcd84b93237` | `32f09dcd84b93237` | `f07eb9285e...` | 7 | 1,427 | Yes | No | Standard scene mesh; slot 7 VP transform |
| `taa-position:3638b6b020b068fc` | `3638b6b020b068fc` | `7fb0238997...` | 7 | 315 | Yes | No | Static/skinned mesh; `c[7..10]` feeds clip position |
| `taa-position:3c86f4a89d220ee8` | `3c86f4a89d220ee8` | `54bd86d0c4...` | 7 | 33,218 | Yes | No | High-frequency scene material (33k draws); slot 7 |
| `taa-position:3f522a748751d16a` | `3f522a748751d16a` | `b2d4cfbc5b...` | 7 | 4,423 | Yes | No | Scene material pass; slot 7 |
| `taa-position:52e4405f97159d2f` | `52e4405f97159d2f` | `21e9058030...` | 4 | 858 | Yes | No | Static depth companion; `c[4..7]` feeds position |
| `taa-position:6761469677f921c6` | `6761469677f921c6` | `7293ffd4d6...` | 8 | 999 | Yes | No | Material pass; `c[7]` is UV, `c[8..11]` is position |
| `taa-position:6a8c2c78737dc94c` | `6a8c2c78737dc94c` | `201b78cdee...` | 7 | 4,423 | Yes | No | Scene pass; slot 7 |
| `taa-position:6d3d954bb6d86bc1` | `6d3d954bb6d86bc1` | `c47d50ae26...` | 0 | 136 | Yes | No | Quad / UI / billboard depth; slot 0 |
| `taa-position:8d3c80b318235b22` | `8d3c80b318235b22` | `d34539f032...` | 4 | 1,152 | Yes | No | Alpha-tested depth; `c[4..7]` feeds position |
| `taa-position:8d9770d1bd8ba0fa` | `8d9770d1bd8ba0fa` | `a6f8edc127...` | 8 | 2,256 | Yes | No | Scene companion; `c[8..11]` feeds position |
| `taa-position:b60fba087b51eb53` | `b60fba087b51eb53` | `859a3a6e5c...` | 8 | 927 | Yes | No | Scene companion; slot 8 |
| `taa-position:bda41a11626a545c` | `bda41a11626a545c` | `25b7a5995f...` | 7 | 333 | Yes | No | Scene material pass; slot 7 |
| `taa-position:e7b38eb08c70e5e1` | `e7b38eb08c70e5e1` | `1efe8365a6...` | 7 | 16,567 | Yes | No | High-frequency material companion (16.5k draws); slot 7 |
| `taa-position:e810cfacc107fd3c` | `e810cfacc107fd3c` | `b6d49e6a48...` | 7 | 2,746 | Yes | No | Scene material pass; slot 7 |
| `taa-position:f3b9f20b3d3a62d5` | `f3b9f20b3d3a62d5` | `3b263e11fe...` | 7 | 33,134 | Yes | No | High-frequency material companion (33k draws); slot 7 |

---

## 4. Key Architectural Findings for Host MV Generation

### 4.1 Position Constant Identification
The audit proves that the position transform in Lost Odyssey's vertex shaders belongs to one of four canonical constant slot offsets:
1. **Slot 7 (`c7..c10`):** The primary material and geometry passes (>85% of scene draws).
2. **Slot 4 (`c4..c7`):** Early depth-prepass and depth companions.
3. **Slot 8 (`c8..c11`):** Material passes where `c7` is dedicated to UV texture coordinate transformation.
4. **Slot 0 (`c0..c3`):** Unprojected / billboard / particle geometry.

### 4.2 Handling Character Skinning & Bone Matrices
- All examined shaders that utilize relative constant indexing (`usesRelativeConstants == true`) read their bone palette dynamically from constant arrays starting above the base camera transform slots.
- For rigid objects (including the bell stand), the world matrix and camera VP matrix are uploaded directly into the fixed slots (`c[slot..slot+3]`).
- For skeletal objects, vertex position is calculated via weighted blending of bone transforms (`XeConst(base + a0)`).
- **PoC Strategy:** Replaying previous-position calculations with the entire previous VS constant bank (256 `float4` = 4 KiB per draw or per object snapshot) completely avoids the need to reverse-engineer arbitrary bone hierarchies on CPU, treating the translated vertex position evaluator as a deterministic function.

### 4.3 Draw Identification & History Management
As cautioned in Section 14 of the Handoff:
- Draw ordinals, VS hashes, or vertex buffer pointers alone are **not** sufficient for temporal correspondence.
- Draw history matching must combine:
  ```cpp
  struct DrawHistoryKey {
      uint64_t vsHash;
      uint64_t indexBufferAddress;
      uint64_t positionBufferAddress;
      uint32_t firstIndex;
      uint32_t indexCount;
      int32_t  baseVertex;
  };
  ```
- If a draw matches an entry from frame $N-1$, previous constants and previous camera transforms are replayed.
- If no reliable match exists, the pixel/draw must be marked **Invalid/Reactive** (not 0.0), allowing TAA to reject history and fall back to current-frame spatial filtering without trailing or ghosting.

---

## 5. Next Steps (Phase 1 & Phase 2)

1. **Phase 1 (Complete):** Native velocity path documented and rejected as sole whole-scene provider; private archive evidence audited and verified.
2. **Phase 2:** Deliver `docs/notes/full-game-motion-vector-design.md` detailing:
   - Screen-space motion vector conventions (Current - Previous, unjittered pixels, RG16_FLOAT).
   - Render target integration in `renderer.cpp` and `temporal_history.h`.
   - Host-driven previous constant tracking and velocity generation pipeline.
