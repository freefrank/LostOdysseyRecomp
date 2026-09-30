# Geometric motion replay: implementation and validation boundary

Date: 2026-09-18 (America/Edmonton). Replaces the camera-only regression on `mv`, code baseline `455b420d3027ac719f09a77d88c0a46878cf965e` (runtime equivalent to `9d7b778`).

## What changed

The old `motion_vector_gpu.h` fullscreen camera-only producer and its automatic TAA dispatch are removed. The default remains the established camera-reprojection TAA: no MV tracking, target allocation, shader compilation or replay pass unless explicitly enabled. The previous leaked per-frame descriptor/framebuffer queue is gone. This removes identified regression sources; it is **not a measurement that the user's scene has recovered 4K60**.

The new runtime path is real geometry, not a CPU camera oracle:

1. `renderer.cpp` captures all 256 float4 VS constants **before** `ApplyDrawJitter`. It retains the first 208 bytes of shared state (bool/loop banks, viewport conversion, fixed half pixel and VTE/flags).
2. Bounded, reusable `DrawTemporalTracker` tables collect current/previous draw states. Every vertex-fetch stream and its immutable arena generation contributes to geometry identity, alongside shader pair, index contents, topology and scene allocation. First-active-fetch is no longer assumed to mean position.
3. `shader/motion_replay_hlsl.h` wraps the **actual translated Xenos VS** twice, with current and previous inputs. Relative constant addressing, weighted skinning arithmetic, control flow and vertex decoding execute in the original program. No universal world matrix or bone window is guessed.
4. The actual current PS retains its alpha-test/discard coverage. Current geometry is depth-tested against the guest depth pass and writes backward unjittered pixel motion (RG16F), current/previous host depth (RG32F) and a draw tag (R32_UINT).
5. After scene collection closes, all duplicate/ambiguous draw tags are invalidated, including a first draw whose duplicate was discovered later. A GPU validity pass combines finalized tags with the visible scene depth and writes R8 reactive/reject coverage. Zero velocity remains a valid stationary vector; invalidity is separate.
6. `HistoryOwner` validates frame, epoch, scene depth allocation, dimensions and all required textures. The dedicated TAA MV path samples using the geometric **previous depth**, rather than pretending a moving surface stayed at its camera-only predicted depth. Jitter is converted once into raw or stable history coordinates.

Snapshots are contiguous and preallocated, capped at 8192 unique draws per frame. Overflow/gaps/epoch changes/unfinished collection fail closed. After warm-up, the CPU fixture records 10,000 draws with zero heap allocations. This bounds allocations; it does not make the enabled feature free of CPU cost.

GPU mask buffers/descriptors are pooled and reused only after their own recorded submission serial completes. The renderer stamps a separate MV serial in each existing GPU slot. TAA/diagnostic consumers extend resource lifetime; external reads of HistoryOwner depth also enter the history owner's serial. No new per-frame GPU wait/readback is added to gameplay. The synchronous waits in GPU fixtures are test-only.

## Runtime controls

Use the TAA antialiasing mode (mode 3). Set variables **before launching** the newly built executable. These options are captured at renderer initialization, not polled live.

```powershell
# Normal gameplay / performance baseline: established TAA, new MV completely off.
$env:LO_MV_ENABLE = "0"

# Experimental geometric MV + TAA consumption.
$env:LO_MV_ENABLE = "1"
$env:LO_MV_REPLAY = "1"
$env:LO_MV_CONSUME = "1"
$env:LO_MV_LOG = "1"

# Optional visualization: magenta = rejected/uncovered, other colors encode velocity.
$env:LO_MV_DEBUG = "1"
```

`LO_ENABLE_MV_DRAW_TRACKING` is superseded and no longer enables hidden work. Remove old capture/per-draw diagnostic variables for performance comparisons. `LO_MV_DEBUG=0` restores normal scene color.

A/B separation, same build/settings/scene and warmed shader caches:

| Run | ENABLE | REPLAY | CONSUME | Measures |
|---|---:|---:|---:|---|
| A | 0 | - | - | Original TAA baseline |
| B | 1 | 0 | 0 | Snapshot/matching CPU overhead |
| C | 1 | 1 | 0 | Snapshot + real geometry replay/validity cost |
| D | 1 | 1 | 1 | Full experimental TAA consumption |

There is no meaningful object-MV “GPU only, no previous state” configuration. Compare B/C for the incremental replay cost. Log output every 120 renderer frames includes tracked/matched/ambiguous/overflow draws, actual replay draws/failures, ready/consume state and pending GPU batches. First-time variant compilation is not a steady-state benchmark.

## Reproducible tests

Only redistributable **synthetic microcode** is checked in. No private game shader bytes, XEX, save or image is published.

Prepare the same pinned dependencies as the normal build and apply `tools/patches/plume-lostodyssey.patch` (only if not already applied), then:

```sh
cmake -S tools/tests/motion_replay -B out/mv-build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build out/mv-build --parallel 2
export LO_DXC_PATH="$PWD/tools/XenosRecomp/thirdparty/dxc-bin/lib/x64/libdxcompiler.so"
# Set VK_ICD_FILENAMES to a software Vulkan ICD for reproducible headless tests.
ctest --test-dir out/mv-build -V --output-on-failure
```

Local validation before delivery:

- 33 CPU lifecycle/reference checks, including exact constant bits, geometry-generation hashing, duplicate revocation, epoch/gap/unfinished-frame rejection, bounded overflow, reverse-Z endpoints and zero steady-state heap allocations.
- 78 shader/GPU checks on Mesa llvmpipe (LLVM 20.1.2), including compilation to **both DXIL and SPIR-V**; actual SPIR-V raster execution/readback; rigid motion expected −6.4 px, observed −6.398438 px (FP16); vertex-dependent weighted relative-palette motion; original alpha discard; late duplicate invalidation; TAA sampling; unequal jitter; motion in Z; occlusion rejection.
- A 1200-frame GPU resource reuse test and a resize test with an unreclaimed completed mask batch. Pending batches return to zero; pooled batches remain bounded.
- The existing `temporal_aa_test.cpp` runs unchanged on software Vulkan, including original camera/jitter/history fixtures and synthetic Halton geometry. Offline telemetry is disabled by a test adapter.
- The actual `renderer.cpp` translation unit compiles against the actual patched Plume interfaces. Its **compile-only guest ABI boundary** replaces unused generated PPC declarations in the PCH. This is explicitly **not** a full-game link or a test of recompiled PPC code.

The former `mv-validation.yml` hosted workflow ran Linux software-Vulkan,
validation-layer and CPU-sanitizer checks plus Windows MSVC/CPU/DXIL-SPIR-V
checks. Those results remain historical. Future changes should use focused
local checks for their affected behavior; Release CI builds and packages from
verified inputs. A build does not replace those tests, and `--compile-only`
does not execute a GPU.

## What remains unverified / unsupported

- No user-machine FPS, hardware D3D12 rendering, Steam Deck measurement, real Bell sequence or real character pose capture has been validated here. Software GPU execution proves the exercised implementation, not all game content.
- This is an **experimental scene-qualified geometry path**, not a claim of complete full-game MV coverage. Existing `PositionVPSlot`/scene guards are preserved; no new hash is blindly whitelisted.
- Matching uses geometry/resource identity, not a recovered stable guest Object ID. Repeated mesh instances are rejected. Replacement of one unique instance by another with reused resources across frames remains a game-specific identity limitation. Abrupt view/scene changes need the existing epoch/reset guards.
- Changed vertex-stream generations cannot replay an unavailable previous deformation and reject matching. Previous VTF texture content is not retained. Vertex-texture-fetch shaders, point-size/rectangle/GS paths, PS depth writes, stencil and biased depth writes are not enabled by this first path; unsupported scene visibility writes trigger conservative frame fallback.
- Non-depth-writing transparency/particles do **not** gain their own motion or a separately proven material reactive mask. Opaque geometric coverage is not proof of correct translucent history. Inspect those effects with the feature opt-in; do not promote default-on until their coverage is handled.
- Conservative history-depth footprints can still reject thin silhouette samples. Correct MV alone does not prove every form of Bell shimmer is gone. Keep raw MV/validity and TAA on/off captures, not just a screenshot of normal scene color.
- The private archive `freefrank/LostOdysseyRecomp-build-inputs/feedback` remains the evidence source for real variants and scene bindings. Its `triage/context.json` is a September 10 review context with unbound/null mappings; historical counts/reviews are not current implementation validation. This change does not claim a fresh audit of all archived shader binaries.
- DLSS/FSR/XeSS are not integrated. First validate real scene motion, transparency, instance identity and the enabled CPU/GPU budget.

## Next acceptance gate

Build the full game normally from this revision and confirm the executable's source revision. First measure A against the last good build. Then test D in a fixed-camera Bell scene and a walking character: capture velocity/reject coverage and verify local motion independently of camera motion. Repeat map changes, camera cuts, duplicate instances, transparency and a long run. Only measured, supported scenes should authorize a wider default or a vendor upscaler adapter.

## Recent development updates (Unpublished / 未发布)

- **Polygon offset gating**: permits self-consistent constant depth bias while continuing to reject finite slope bias and non-finite values.
- **Depth-only replay PS**: added `#define XE_SAMPLE(t, s, uv) t.Sample(s, uv)` to resolve DXIL/SPIR-V compilation errors for null/depth-only microcode.
- **Async shader compilation**: throttled to at most 2 concurrent background worker jobs to eliminate scene transition loading hangs and black screens.
- **Ordered duplicate matching**: matches repeated `DrawHistoryKey` instances by stable occurrence order across adjacent frames, improving automated Bell debug tracking from ~533/955 to stable 955/955 matched draws (`ready=true consume=true`) and eliminating reactive mask gaps.
- **Status & limits**: Windows runtime BUILD OK; `motion_vector_test` 40 checks passed; `motion_replay_gpu_test --compile-only` 11 checks passed; automated Vulkan execution is stable. User confirmed MV is actively consumed; Bell visible jitter remains under active TAA investigation and is NOT marked resolved or accepted. D3D12 replay PSO creation `E_INVALIDARG 0x80070057` remains tracked follow-up work.
