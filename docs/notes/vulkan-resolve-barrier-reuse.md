# Reused color resolves without transfer-layout round trips

Based on upstream `f9da0ae9` (v1.1.0), 2026-10-26.

`ResolveOnGpu` already recognizes an identical adjacent same-format color copy.
It previously transitioned the source to COPY_SOURCE and the destination from
SHADER_READ to COPY_DEST before recognizing reuse, then returned the destination
to SHADER_READ. Removing the copy left two redundant destination transitions.

Decide reuse before the transfer transitions. Keep logical resolve metadata,
destination FG writable invalidation and the final readable-layout guarantee.
Never reuse across a pending source/destination clear: execute the ordinary
transition/copy path, which consumes clears and invalidates copy evidence.
Existing allocation/rectangle/format/command-batch invalidation and
`LO_RESOLVE_COPY_REUSE=0` are unchanged. Converted/depth/readback resolves are
unchanged. This does not introduce a new copy-elision policy.

## Validation

- GCC 13.3 C++20 Release and ASan/UBSan: 33,409 production-block fixture checks
  pass. The upstream block fails the zero-transfer-barrier regression. 4,000
  repeated resolves emit zero transfer commands/barriers instead of 8,000
  destination transitions, with identical modeled pixels. This is a synthetic
  command count, not a measured game workload.
- Existing copy-policy fixture: 30/30 checks pass.
- Full production `renderer.cpp` compiles against actual Plume/Vulkan interfaces
  using the existing motion-replay compile boundary. Generated guest/PCH names
  are stand-ins; this is not a complete game link.
- Updated Vulkan FP16 fixture runs on Mesa 25.2.8 lavapipe/LLVM 20.1.2:
  six full-image readbacks, zero differing pixels, including partial updates,
  allocation changes, repeated copies and source-clear invalidation. Lavapipe
  executes Vulkan on the CPU; this is not Adreno/Mali hardware evidence.
- Review suite: 47 tests passed. The remaining import-home namespace test timed
  out after reporting unsupported unprivileged mount/user namespaces. SDL tests
  were disabled for this local suite. No import code was changed.

Reproduce the focused CPU checks:

```sh
python3 tools/tests/resolve_barrier_reuse_test.py --out build/resolve-reuse
ASAN_OPTIONS=detect_leaks=0 python3 tools/tests/resolve_barrier_reuse_test.py \
    --out build/resolve-reuse-asan --sanitize
```

`--renderer /path/to/baseline/renderer.cpp` executes the baseline recording block.
LeakSanitizer is disabled only because this environment cannot inspect `/proc`.
The existing `LoResolveCopyGpuTest --vulkan` fixture now also builds on Unix when
linked to Plume, its Vulkan dependencies, dl/pthreads and X11 as appropriate.

## Android acceptance

No physical Android device, game assets or complete runtime build inputs are
available. No GPU milliseconds, FPS, temperature or power improvement is claimed.
Keep this a draft until full Android and desktop runtime checks are available.

For a matched comparison, build upstream and this branch with identical compiler,
driver and settings. Preserve resolution, effects, AA/upscaling and FPS cap. Warm
shader caches, then alternate A/B/B/A in the same Uhra view, battle and the same
CGI-to-gameplay transition. Restart each run and match starting thermal state.
Collect `LO_RENDER_TIMING=1`, `LO_RECORD_TIMING=1`, `LO_FRAME_TIMING=1` in both
builds. The existing `renderer resolve copies` counter identifies actual reused
copies; an affected adjacent reuse normally saves two destination barriers beyond
the already-skipped copy. Do not count these as newly eliminated copies.

Compare GPU timestamps, CPU recording/waits, median/p95/p99 frametimes and
screenshots. Inspect a Vulkan capture for transfer barriers/pass breaks. Run at
least ten minutes for temperature/frequency/power comparisons. Report the actual
reuse rate: if the scene has no adjacent repeated resolves, this change cannot
improve that scene. Validate native and upscaled modes, HDR and partial resolves
on Adreno/Turnip, Mali and desktop backends before acceptance. Audio/XMA and movie
decoders are untouched; their independent problems cannot be attributed to this
change.
