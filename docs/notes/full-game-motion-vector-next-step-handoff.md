# Full-Game Motion Vector — Next Development Handoff

**Target branch:** `mv`  
**Current base commit:** `7002f9ca711d1998cc43349900f23f2ca19dbdea`  
**Goal of this phase:** turn the existing MV plumbing into a real, validated full-game motion-vector producer.  
**Do not integrate DLSS / FSR / XeSS yet.** Vendor upscalers come only after the canonical MV field is correct.

---

## 0. Read this first

The current `mv` branch already has:

- a canonical MV convention: **current pixel -> previous pixel**;
- render-pixel units;
- unjittered geometric motion;
- an `R16G16_FLOAT` motion-vector texture owned by `HistoryOwner`;
- TAA input plumbing for `motionVector`;
- a prototype `DrawTemporalTracker`;
- a large manually verified `PositionVPSlot()` table;
- frame/epoch/history reset infrastructure;
- camera reprojection and temporal diagnostics.

But it does **not** yet have the critical part:

> There is no complete render pass that actually populates the full-frame `motionVector_` texture with validated camera/object/skinned motion.

The current TAA shader also keeps MV consumption disabled by default until that producer exists.

This phase is therefore about producing correct MV, not adding vendor SDKs.

---

# 1. Mandatory evidence source: do not guess VP / VS semantics

Before changing shader classification, constant handling, or position reconstruction, inspect:

**Repository:** `freefrank/LostOdysseyRecomp-build-inputs`  
**Archive root:** `feedback/`

This archive already contains a large amount of real-world VP/VS evidence collected from users.

Important paths:

- `feedback/README.md`
- `feedback/triage/ledger.json`
- `feedback/triage/REPORT.md`
- `feedback/data/observations/`
- `feedback/data/programs/vs/`
- `feedback/data/programs/ps/`
- `feedback/data/shader_sources/`
- `feedback/data/shader_source_observations/`
- `feedback/data/temporal_sequences/`
- `feedback/data/temporal_payloads/`

Current archive report states:

- **3401 archived observations**
- **431 shader sources**
- **18 TAA position candidate VS cases** in the triage ledger

The archive stores original shader microcode and observed VS/PS associations. Use it as the primary evidence source when deciding:

- which VS constants contribute to `oPos`;
- which shader families are static / rigid / skinned / fullscreen / UI;
- whether relative constants are used;
- which VS/PS pairs occur in practice;
- how often a shader is observed;
- which variants are important enough to support first.

### Important rule

Do **not** infer a universal VP slot only from one shader or one capture.

Use:

1. archived microcode;
2. current `PositionVPSlot()` mapping;
3. translated HLSL from `LoShaderTool` / Xenos translator;
4. archived observations and VS/PS pair statistics.

If evidence conflicts, prefer invalidating MV coverage over fabricating motion.

---

# 2. Fix the temporal draw state model before rendering MV

The current prototype key is too weak:

```cpp
struct DrawHistoryKey {
    uint64_t vsHash;
    uint32_t baseVertex;
    uint32_t startIndex;
    uint32_t indexCount;
    uint32_t vertexFetchAddress;
};
```

This can collide when multiple objects use the same mesh/shader layout.

Replace it with a conservative key containing at least:

```cpp
struct DrawHistoryKey {
    uint64_t vsHash;

    uint32_t indexBufferAddress;
    uint32_t positionBufferAddress;

    uint32_t firstIndex;
    uint32_t indexCount;
    int32_t  baseVertex;

    uint32_t primitiveType;
};
```

If available cheaply, also include:

- position stream slot / stride;
- dynamic buffer generation or guest backing identity;
- instance-relevant state when the same geometry is reused several times in one frame.

### Matching policy

False negatives are acceptable.

False positives are not.

If the previous-frame identity is not trustworthy:

> mark the draw/pixels invalid/reactive for temporal reuse.

Never assume `(0,0)` means "unknown motion". Zero means "valid stationary surface".

---

# 3. Preserve the complete previous VS input first

The current prototype stores:

```cpp
c64..c191
```

for shaders using relative constants.

That is not safe as a general solution.

Archived/translated shaders already demonstrate relative addressing patterns such as:

```text
c[8 + a0]
c[9 + a0]
c[10 + a0]
```

For the first correct implementation, store the **entire VS float constant bank** for matched scene draws:

```text
256 float4 = 4096 bytes per tracked draw
```

Also retain any other VS inputs that can change vertex position between frames:

- relevant vertex stream contents or a stable previous generation;
- relative addressing state;
- loop/bool constants if position logic depends on them.

Do not optimize this memory footprint until MV correctness is proven.

After validation, a shader dependency analysis may reduce the retained subset.

---

# 4. Separate current and previous frame state cleanly

Do not let `RecordDraw()` mutate the same record that the MV producer is trying to consume.

Preferred model:

```text
PreviousFrameDrawTable
CurrentFrameDrawTable

begin frame N
    previous = completed frame N-1
    current.clear()

during guest draw submission
    capture CurrentDrawState

MV producer
    match current against previous
    replay previous position where valid

end frame
    swap tables
```

Suggested data model:

```cpp
struct DrawTemporalState {
    DrawHistoryKey key;

    std::array<float4, 256> vsConstants;

    // only if required by the translated VS:
    std::array<uint32_t, 8> boolConstants;
    std::array<uint32_t, 32> loopConstants;

    // geometry identity / replay information
    ...
};
```

This is easier to reason about than promoting `current -> previous` inside the lookup entry.

---

# 5. Implement the actual full-game MV producer

This is the main deliverable.

The output remains:

```text
Format: R16G16_FLOAT
Direction: current -> previous
Units: render pixels
X: right positive
Y: down positive
Jitter: excluded from geometric MV
```

For a pixel currently at `(100,100)` that came from `(98,100)`:

```text
MV = (-2, 0)
previous_sample = current_pixel + MV
```

## Preferred first implementation: geometry replay

For every eligible current scene draw:

1. use current geometry;
2. evaluate current clip-space position using current VS inputs;
3. evaluate previous clip-space position using matched previous VS inputs;
4. convert both to the same unjittered render-pixel coordinate system;
5. output:
   ```text
   previousPixel - currentPixel
   ```
6. rasterize into the MV target using the **current frame geometry coverage**.

The replay must preserve relevant coverage rules:

- primitive topology;
- culling;
- depth test;
- alpha/discard behavior where necessary;
- viewport/scissor;
- scene-view classification.

Do not write MV for:

- UI;
- shadow maps;
- unrelated offscreen passes;
- clears;
- arbitrary fullscreen post-process passes.

Use the existing scene classification and `PositionVPSlot()` evidence as the starting point.

---

# 6. Static/rigid/skinned handling

## Static geometry

If geometry is static but camera moves, MV must still be non-zero.

Camera-only depth reprojection is useful as a diagnostic reference, but the final field should follow the same canonical convention as object MV.

## Rigid moving geometry

Use the previous full VS constants. Do not assume the VP matrix is purely a camera matrix; in this title some fixed constant ranges can already include object/pass transforms.

## Skinned geometry

The previous-frame pose must affect MV.

Replay the translated VS position path with previous constants.

Acceptance requires limb-level motion. A walking character with only camera/body-center MV is not sufficient.

---

# 7. Invalid / reactive coverage

Unknown temporal correspondence must not silently produce zero velocity.

Add an explicit validity policy.

Options:

### Option A

Maintain a separate `R8_UNORM` validity/reactive texture.

### Option B

Keep MV as RG16F and feed invalid coverage through the existing reactive-mask infrastructure.

For unmatched, newly spawned, topology-changing, or discontinuous draws:

```text
MV may be cleared to 0
BUT temporal validity/reactivity must reject history there.
```

Camera cuts, map changes, resolution changes and temporal epoch changes must invalidate the field/history.

---

# 8. Clear the MV render target every coherent scene frame

The MV texture must not retain stale pixels.

At the start of a valid scene MV pass:

- clear MV to `(0,0)`;
- initialize validity/reactive coverage to invalid;
- mark only proven valid rasterized coverage as reusable.

This avoids old-frame velocities leaking into uncovered pixels.

---

# 9. Enable TAA MV consumption only after producer validation

Currently the TAA shader has MV plumbing but keeps it disabled.

Do not simply flip the enable bit after the render pass compiles.

First add diagnostics capable of dumping:

- current color;
- depth;
- MV;
- reactive/validity;
- TAA result.

Only enable the MV branch when acceptance tests below pass.

---

# 10. Required tests

Add unit/offline tests where possible, then validate in-game.

## Test A — static camera, static scene

Expected:

```text
MV ~= (0,0)
```

for stable scene geometry.

Tolerance should be sub-pixel and documented.

## Test B — camera pan, static scene

Expected:

- coherent scene-wide velocity;
- correct sign;
- matches analytic depth/camera reprojection on sampled rigid landmarks.

## Test C — rigid animated object / bell stand

This is an important regression target.

Expected:

- background near zero with stationary camera;
- bell/stand moving pixels have non-zero MV;
- direction follows actual frame-to-frame sway;
- TAA no longer treats the moving silhouette as stationary history.

## Test D — walking skinned character

Expected:

- limbs have independent motion;
- previous pose is visible in the vector field;
- no single rigid body approximation.

## Test E — camera cut / map transition

Expected:

- history invalidated;
- no giant residual vector field;
- no temporal streak.

## Test F — duplicate geometry instances

Render multiple instances using the same VS/mesh family.

Expected:

- no cross-instance previous-frame matching;
- unmatched ambiguous cases become invalid rather than receiving another instance's MV.

---

# 11. Diagnostics required from the implementation

Add a development-only MV diagnostic mode that reports at minimum:

```text
frame
scene draw count
tracked current draws
matched previous draws
unmatched draws
ambiguous/rejected matches
rigid matches
relative/skinned matches
MV pixels written
invalid/reactive pixels
```

Optional but very useful:

- per-VS match counts;
- top unmatched VS hashes;
- top collision-prone DrawHistoryKeys.

Cross-reference these hashes against:

`LostOdysseyRecomp-build-inputs/feedback`

instead of blindly adding shader hashes to `PositionVPSlot()`.

---

# 12. Use the feedback archive to prioritize coverage

The agent should build a simple offline report from the archive before trying to support every shader.

Recommended output:

```text
VS hash
observed draw count / max_draws
number of PS pairs
program source available?
PositionVPSlot known?
relative constants?
classified static / rigid / skinned / fullscreen / unknown
MV implementation status
```

Prioritize high-frequency scene VS families first.

The existing triage report already contains examples such as high-observation/high-draw TAA position candidates. Do not spend equal effort on rare unknown shaders and dominant scene shaders.

---

# 13. Do not integrate DLSS / FSR / XeSS in this phase

The existing architecture is intentionally vendor-neutral.

Do not add:

- Streamline;
- NGX;
- FidelityFX SDK dispatch;
- XeSS SDK dispatch;

until the following canonical inputs are validated independently:

```text
scene color
scene depth
unjittered current camera
unjittered previous camera
jitter
frame/reset token
dense backward MV
validity/reactive coverage
```

Vendor SDK integration must not become a debugging layer for broken MV.

---

# 14. Definition of done for this phase

This phase is complete only when all of the following are true:

- [ ] `motionVector_` is actually populated each valid scene frame.
- [ ] stale MV pixels cannot survive from earlier frames.
- [ ] draw identity is strengthened beyond the current prototype.
- [ ] full previous VS constants are preserved for the initial implementation.
- [ ] rigid motion works.
- [ ] camera motion works.
- [ ] skinned motion works on at least one real character.
- [ ] unmatched/ambiguous draws reject history instead of reporting valid zero velocity.
- [ ] bell stand regression case produces visible non-zero object MV.
- [ ] MV diagnostics/readback exist.
- [ ] current TAA can consume the validated MV field without obvious ghosting/trailing.
- [ ] evidence from `LostOdysseyRecomp-build-inputs/feedback` is referenced in the implementation notes.

Only after this milestone should the next phase be:

> internal render-resolution contract + first temporal upscaler adapter.

Recommended adapter order after that:

1. FSR
2. XeSS
3. DLSS / Streamline

---

# 15. Final instruction to the development agent

Do not redesign the whole temporal subsystem.

The current `mv` branch already contains useful infrastructure.

Focus on the missing producer path:

```text
reliable draw match
    ->
previous VS state
    ->
previous-position replay
    ->
rasterized dense R16G16F MV
    ->
validity/reactivity
    ->
existing TAA consumer
```

Most importantly:

**Inspect the archived feedback dataset before inventing new shader assumptions.**

The project already has a large body of real VS/PS observations and original microcode under:

`freefrank/LostOdysseyRecomp-build-inputs/feedback`

Use that evidence.
