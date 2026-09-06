# Built-in shader resource index

`gpu/shader/resource_index.h` contains metadata for the tested four-disc resource set:
25 distinct file layouts and 3,538 shader locations, yielding 2,000 unique shaders.
It contains file names/sizes, sampled fingerprints, byte offsets/lengths, shader stages and
hashes, not game shader bytecode. Identical disc resources share a layout profile.

On a cold cache, the scanner matches each FPD by name, size and four bounded probes, reads its
indexed shaders, and verifies each shader hash before publishing any indexed result from
that file. Files with no shaders have profiles too, avoiding large movie/audio scans.
An unknown profile, bounds violation, or shader hash mismatch falls back to the original
scanner for that file. Previously validated files keep the fast path. Shader compilation and
runtime on-demand shader discovery remain unchanged.

The probes identify a layout; they are not a full-file integrity check of unrelated assets.
The index covers the shader containers found by the current scanner in the supported data,
not a claim that every possible runtime shader or graphics pipeline is precompiled.

The existing extracted-source cache is still verified on reuse. Inventory manifest version 2
invalidates older manifests once, then rebuilds through the index. `LO_SHADER_FULL_SCAN=1`
disables the index for diagnostics; use a separate empty cache when comparing paths.

## Regeneration and verification

```powershell
python tools/generate_shader_index.py LostOdysseyRecompLib/private LostOdysseyRecomp/gpu/shader/resource_index.h
tools/test_shader_index.bat
out/shader-index-test/test.exe <disc1> <empty-index-cache> indexed
out/shader-index-test/test.exe <disc1> <empty-full-cache> full
```

Regeneration requires private FPDs only on the maintainer's machine. The metadata header is
tracked, so hosted runtime builds do not need FPDs. The generator refuses an empty inventory.
Tests cover indexed reads, bounded I/O, shader hash failure, layout mismatch, per-file fallback,
deduplication, cache reuse and damaged cache recovery. Public CI runs the synthetic tests.

Local comparison (2026-09-05), same imported four-disc resources with separate empty caches:

| Path | Time | Bytes read | Unique shaders |
|---|---:|---:|---:|
| Full scan | 36,238 ms | 23,428,562,944 | 2,000 |
| Built-in index | 1,074 ms | 6,863,356 | 2,000 |

All 2,000 source filenames and SHA256 values were identical. All 52 FPD files took the indexed
path. This is about 34x faster for discovery on this machine, excluding subsequent compilation;
filesystem caching and storage speed affect results. Evidence: `out/shader-index-test/comparison.json`.
