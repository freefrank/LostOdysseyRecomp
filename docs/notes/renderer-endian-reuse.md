# Renderer endian-conversion follow-up

Source baseline: `ce45afe2774efe0cea87e64ccc1fafc0199e8f62` on `feature/native-pm4-bypass`.

## Findings and implemented change

`CommandProcessor::ReadRegister` already returns nonzero native register values without swapping. Zero entries fall back to the guest-endian MMIO mirror. `DrawImpl` still constructs two 4-KiB ALU snapshots per draw, but that is not an 8-KiB unconditional endian conversion. `UploadUnchanged` avoids some uploads, not snapshot construction.

`GetVertexBuffer` already reuses converted arena data on a valid cache hit. Its misses use `CopyDwordsSwapped`, which has a SIMD path. Indexed geometry also already skips `ConvertIndices` and primitive expansion on an exact-content cache hit (currently at least 256 source indices).

The remaining index-hit path copied the cached little-endian vector into `indexScratch` before copying it to the GPU upload allocation. This change borrows the immutable cached vector directly for the current draw. A hit therefore removes one CPU vector copy of `4 * final_index_count` bytes; the GPU upload itself is still required. Misses, primitive expansion, exact source validation, cache keys and eviction limits are unchanged.

The cache entry was already retained during the draw for `motionIndexHash`. On a hit, the draw does not insert into or erase from the index cache. On a miss, insertion completes before the scratch result is selected. No borrowed vector survives the current draw; recorded GPU commands continue to use their upload allocation and existing fence lifetime. This is renderer data reuse, not additional PM4 bypass coverage.

## Rejected constant-snapshot experiment

A prototype kept little-endian ALU snapshots and compared 16-word native/MMIO chunks before updating changed chunks. Exact MMIO observation was retained because guest writes can bypass a dirty flag. The CPU equivalence fixture passed, including zero fallbacks, direct MMIO writes, reset and temporary-copy isolation.

However, the three-round Windows clang-cl `/O2 -march=sandybridge` microbenchmark was slower in every tested update-density median. Each iteration reads two 1,024-word banks; checksums matched. These are synthetic CPU measurements, not renderer frame times or GPU timings.

| Changed words per iteration | Existing bulk read, median ns | Prototype, median ns |
|---:|---:|---:|
| 0 | 138.555 | 207.134 |
| 4 | 139.769 | 228.505 |
| 64 | 143.715 | 160.313 |
| 2048 | 1032.149 | 1613.027 |

The prototype and its runtime wiring were removed. The production constant read and jitter paths are unchanged. Raw measurements and the rejected prototype are retained locally under `out/endian-constants/`; the sanitized measurement record is [renderer-endian-reuse-results.json](renderer-endian-reuse-results.json).

## Validation and remaining work

The changed `renderer.cpp` compiled successfully with the local Windows clang-cl development flags. The isolated development executable linked successfully, replacing the renderer object while reusing the previous branch's unchanged runtime, generated guest and dependency objects. This was not a clean release build; existing compiler warnings remain.

No live-game A/B, Linux execution, GPU validation or measured index-hit/frame-time improvement is claimed for this follow-up. The earlier title comparison's four-vertex fan does not exercise the large-index cache, so its receipts cannot validate this optimization. The native command prototype's earlier CPU regression remains unresolved and its default remains off.

Next work should separate avoided conversions from content checking, intermediate copies and GPU uploads. Removing the remaining PM4 generation/dispatch overhead still requires work on producer state and command transport; this patch does not establish that goal as complete.
