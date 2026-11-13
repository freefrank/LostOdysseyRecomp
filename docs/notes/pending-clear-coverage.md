# Remove overwritten and contained partial EDRAM clears

Based on upstream `f9da0ae9` (v1.1.0), 2026-10-26. Extends the existing deferred
clear queue from #414/#445; it does not reimplement whole-target clear elision.

The queue already drops a clear overwritten by a later whole-target clear, but
retained an old rectangle wholly overwritten by a later rectangle. Equal-value
merges also appended contained/identical rectangles, repeatedly clearing the
same pixels within one clear command.

For one incoming rectangle, remove old fully covered operations aspect by
aspect. Preserve any old stencil when only depth is replaced, and vice versa.
When merging equal-bit values, omit an incoming rectangle already contained in
an old rectangle and replace old rectangles contained in the new rectangle.
Do not infer coverage from a disjoint union's bounding box or remove partial
overlaps. Whole-target clears keep their existing behavior and bitwise value
comparison (including signed zero and NaN payloads).

The new scans stop at 32 rectangles per old operation. Larger lists retain the
previous behavior, bounding added CPU work. The queue's execution points,
framebuffer extents, pass/clear API, resource lifetime, shader/precision, query
and EDRAM ownership logic are unchanged.

## Results actually verified

- GCC 13.3 C++20 Release and ASan/UBSan: 1,751,629 checks pass. The new image
  oracle compares every color component, depth and stencil bit at every read
  boundary over 10,000 randomized sequences. Upstream fails the targeted
  overwritten-partial-clear assertion.
- The same 10,000 sequences against the already-optimized upstream queue replay
  682,576 rectangles / 23,037,022 aspect-pixels. This change replays 680,254 /
  22,996,037: 2,322 fewer rectangles and 40,985 fewer aspect-pixel writes, with
  identical images. These are synthetic oracle counts, not hardware bandwidth.
  The larger reduction versus eager clearing includes pre-existing optimizations
  and must not be attributed to this patch.
- 4,000 identical 4x4 partial clears replay one rectangle instead of 4,000:
  64,000 -> 16 pixel writes in the model. Upstream already merges these into one
  clear API call; this case removes rectangle overdraw, not 3,999 API calls.
  Separately, a later containing rectangle with another value removes the older
  operation and its clear call entirely.
- Existing pending-clear test passes its 3,000 randomized renderer sequences and
  execution-point tests. Related occlusion and temporal-jitter CTests pass.
- Actual `renderer.cpp` compiles with GCC 13 and Android NDK r28c/Clang 19,
  arm64-v8a/API 26, against pinned graphics interfaces using the existing
  generated-guest/PCH compile boundary. The Android fixture also links as an
  AArch64 executable. Neither is a full game build or an on-device run.

```sh
g++ -std=c++20 -O2 -I LostOdysseyRecomp \
    tools/tests/pending_clear_coverage_test.cpp -o build/clear-coverage
build/clear-coverage
```

Compile with `-fsanitize=address,undefined -fno-omit-frame-pointer` for the
sanitizer run (`ASAN_OPTIONS=detect_leaks=0` only where `/proc` tracing prevents
LeakSanitizer). To measure the incremental baseline, compile this same source
against upstream's `gpu/pending_clear.h` in a separate include root and run both
executables with `--random-only`. The default targeted rules intentionally fail
on upstream. The shared review-regression CMake suite includes the new test.

## Hardware acceptance

No physical Adreno, Mali or desktop GPU game session was run. No GPU/CPU
milliseconds, FPS, battery or thermal gain is known. Draft until full runtime
builds and matched A/B captures confirm frequency and value of these patterns.

Compare identical scenes/settings/driver and warm shader caches in A/B/B/A order
with matched thermal state. Capture actual clear operation/rectangle counts and
GPU queue timestamps. Use `LO_RENDER_TIMING=1`, `LO_RECORD_TIMING=1`,
`LO_FRAME_TIMING=1` and Android `LO_FRAME_STATS=10` in both builds; the latter
reports frame interval p50/p95/p99 and render/main thread CPU time. GPU batch
timestamps exclude presentation/compositor, and CPU wall timers overlap.
Keep high-volume capture/probes out of sustained FPS/temperature measurements.

Check partial color clears, combined and separate depth/stencil, differently
sized attachments, alias transfers, shadows, scene changes and CGI resume, with
AA/upscaling/HDR where supported. Compare screenshots and depth/stencil captures.
Use Vulkan validation and at least ten-minute frequency/temperature runs on real
devices. No audio or movie-decoder change is included.
