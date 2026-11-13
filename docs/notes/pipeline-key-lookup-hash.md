# Pipeline-key CPU lookup hash

Based on main `5fd18cc`. Reviewed #303, #314, #407, #418, #420, #436,
#445, #447 and open descriptor PR #718. This is one CPU optimization;
`renderer.cpp`, descriptor code, shadow selection and GPU programs are untouched.

`gpu::pipeline_cache::KeyHash::operator()` previously serialized a 64-byte
portable key and hashed it with 64 dependent FNV multiplications. The renderer
uses this functor for pipeline/recipe maps and sets. Its recent-key front cache
already avoids many map accesses, so this cost applies only when hashing is
actually needed, not to every draw.

Combine the eight explicit 64-bit field groups and avalanche the result for
in-memory bucket selection. All 14 equality fields participate, including
signed depth-bias bits and exact slope-bias bits. No raw struct bytes, allocation,
new cache, synchronization change, extra object storage or external dependency.
Key equality, normalization, portable encoding, FNV checksums and persistent
driver pipeline identifiers keep their existing implementations. Unordered-map
iteration order can change; serialized records supplied in the same order do not.

## Validation

- Production pipeline-cache fixture passes GCC 13.3 C++20 Release, C++23 O2,
  and ASan/UBSan. This includes round trips, deduplication, normalization,
  legacy v1 migration, malformed files, atomic-write failures and the 16,384
  record limit. Existing incomplete-aggregate warnings are suppressed with
  `-Wno-missing-field-initializers`; no unrelated initializers were changed.
- New checks flip each of 512 encoded bits independently against two base
  keys, then exercise 4,096 distinct keys per equality field, bucket-bit spread
  and lookups after rehashing. A golden persistent FNV hash is checked separately.
- All five valid persistent fixture files are byte-identical between baseline
  and modified builds, including v1, v3 texture-info and scene records.
- Existing driver-pipeline-cache header, growth/cap and file round-trip fixture
  passes. `git diff --check` passes.
- The sanitizer run uses `ASAN_OPTIONS=detect_leaks=0`; address and
  undefined-behavior checks are enabled, leak detection is not exercised.

Reproduce from the repository root (supply a scratch directory to the fixture):

```sh
g++ -std=c++20 -O3 -DNDEBUG -I LostOdysseyRecomp tools/tests/pipeline_cache_test.cpp -o /tmp/pipeline-cache-test
/tmp/pipeline-cache-test /tmp/lo-pipeline-cache-tests
g++ -std=c++20 -O3 -DNDEBUG -I LostOdysseyRecomp tools/tests/pipeline_key_hash_bench.cpp -o /tmp/pipeline-hash-bench
taskset -c 2 /tmp/pipeline-hash-bench
```

## Synthetic measurements

AMD EPYC 9V74 x86-64 VM, GCC 13.3 `-O3 -DNDEBUG`, pinned to CPU 2. Both arms
use identical inputs and warm maps. Five ABBA rounds, 2,300 operations repeated
400 times per timed arm; report the median of the five paired round means.
The scalar hash wrappers are not inlined, to prevent cross-key vectorization.
The lookup model includes the renderer's unchanged 256-slot raw-key front index,
normalization and last hit. Map-lookup percentages are synthetic, not captured
Uhra frequencies. VM frequency/background load are not controlled.

| Workload | Map lookup rate | Before ns/op | After ns/op |
| --- | ---: | ---: | ---: |
| Scalar hash | n/a | 96.899 | 7.915 |
| Front lookup, 1 key | 0% | 9.367 | 9.260 |
| Front lookup, 8 keys | 0% | 12.955 | 13.204 |
| Front lookup, 128 keys | 23.61% | 51.046 | 22.599 |
| Front lookup, 512 keys | 59.57% | 117.368 | 43.672 |
| Front lookup, 2,300 keys | 100% | 190.908 | 76.112 |

The isolated hash reduction is 91.8%. The map-heavy model improves clearly;
the front-hit-only differences are within host noise and are not a benefit.
These are **not Android results, whole-draw CPU costs, frame time or FPS gains**.
Actual impact depends on how often production draws reach these hash tables.

## Remaining acceptance gates

No Android NDK, Clang/CMake runtime build environment, game assets or physical
Adreno/Mali device is available here. The complete renderer, Android ARM64 APK
and other graphics backends have not been built or run. Host fixture compilation
is not a full desktop build. Keep the contribution draft pending those checks.

On matched Uhra and battle scenes, warm both builds and alternate ABBA with the
same settings, device clocks/thermal state and background workload. Use existing
`LO_RENDER_TIMING=1`, `LO_RECORD_TIMING=1` and simpleperf to measure render-thread
cycles/instructions and draw time; record FPS, p95/p99 frame times, GPU time and
memory, plus identical captures. Also measure with timing disabled. Confirm that
KeyHash has meaningful remaining samples/map traffic before attributing a frame
benefit to it. No sustained FPS, GPU, memory or thermal improvement is claimed.
