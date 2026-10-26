# Omit depth-only draws without observable writes

Based on upstream `f9da0ae9` (v1.1.0), 2026-10-26.

`DrawImpl` previously submitted mode-5 depth-only draws even when depth writes
and stencil were disabled. Mode 5 explicitly ignores the active guest pixel
shader, including depth exports/discard. The current translated vertex shaders
only read resources; their exports feed rasterization, not guest memory or a
storage buffer. With no color exports, no depth writes and no stencil operation,
these draws cannot change an attachment.

Omit only their final native indexed/non-indexed draw command. Keep normal
frame/batch initialization, shader/resource preparation, EDRAM alias handoffs,
pending-clear execution, framebuffer binding and all after-draw bookkeeping.
Acquiring a color view can change EDRAM ownership even for a read-only draw;
an early return would incorrectly remove that observable conversion sequence.
Keep every draw in an active host occlusion query, since read-only geometry still
produces observable sample counts. Retain every stencil-enabled draw, even KEEP
operations or a zero stencil write mask. Color-mode draws are untouched.
EDRAM alias clear propagation requires a depth/color write, so these candidates
cannot perform it. No geometry visibility/camera guess is used.

`LO_SKIP_READ_ONLY_DEPTH_DRAWS=0` disables this optimization. With
`LO_RENDER_TIMING=1`, `renderer read-only depth draws` reports the exact count
omitted in the current frame. This counter, not an assumed percentage of
invisible objects, determines whether a real scene benefits.

## Checks performed

- GCC 13.3 C++20 Release and ASan/UBSan: 4,947,993 checks pass. The test compiles
  production mode handling and native command emission, using the real query
  tracker. It sweeps indexed/non-indexed commands, modes/depth-stencil state,
  active/inactive queries and the opt-out. Pre/post preparation markers protect
  against early return; extraction checks the production placement after target
  acquisition/binding. Strict, History and Fast retain measured draws;
  Strict/History retain exact sample counts. Unmodified upstream fails the no-op
  regression, and an early-return implementation fails the bookkeeping check.
- The full renderer translation unit compiles with GCC 13 and Clang 18 against
  pinned Plume/Vulkan interfaces using the existing motion-replay compile
  boundary. Only generated guest/PCH names are stand-ins; no game link/execution
  is claimed. Clang reports two existing switch-coverage warnings outside the
  changed code.
- Five related CTest checks pass: this production gate, occlusion bookkeeping,
  pending clears, temporal jitter and production GPU-command safety.
- Android NDK r28c/Clang 19, arm64-v8a/API 26: the production-gate fixture
  compiles and links as an AArch64 executable. This cross-build is not a device
  test. The combined renderer boundary is checked separately with all three
  independent optimizations.

```sh
python3 tools/tests/read_only_depth_draw_test.py --out build/read-only-depth
ASAN_OPTIONS=detect_leaks=0 python3 tools/tests/read_only_depth_draw_test.py \
    --out build/read-only-depth-asan --sanitize
```

The fixture supports `--renderer /path/to/baseline/renderer.cpp` and
`--compile-only --cxx /path/to/cross/compiler`. LeakSanitizer alone is disabled
because this environment cannot inspect `/proc`.

## Acceptance still needed

No actual game workload or physical Android/desktop GPU was run. The number of
eligible draws in Uhra, battles or after CGI, and their GPU/CPU time, are unknown.
Do not claim improved FPS, thermals or battery life from this static proof.

Compare one identical build with the opt-out set to 0 and 1. Warm caches, keep
resolution/effects/AA/driver/FPS cap fixed, and alternate A/B/B/A at matched
thermal state. Collect render/frame timing and the new skipped-draw counter.
Compare output captures, GPU timestamps, CPU draw recording and waits, frame-time
median/p95/p99, memory, frequencies and sustained temperatures. Include occlusion
modes, shadow volumes, scene transitions and CGI resume on Adreno/Turnip, Mali
and desktop backends. A zero candidate count means no benefit in that scene.

The optimization relies on the current vertex translation contract. A future
guest memory-export/transform-feedback implementation must revisit this gate.
No pixel shader, shadow budget, quality setting, audio decoder or movie lifecycle
was modified. Draft pending full runtime builds and hardware acceptance.
