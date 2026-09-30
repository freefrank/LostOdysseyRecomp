# Offline shader analysis helpers

These scripts analyze only inputs explicitly supplied on the command line. They do not scan game directories, generate a production shader index, run the game, translate shaders, or compile a shader pack. Python 3.10+ standard library only; no NumPy, Pillow, native CPX DLL, or other third-party dependency. The modules perform no application writes on import (Python's standard `__pycache__` behavior still applies). All output paths must be provided, and existing outputs are refused.

From the repository root, for example:

```sh
python tools/shader_analysis/collect_sources.py --source fpd=/path/to/extracted-fpd/source --source cpx=/path/to/extracted-cpx/source --output out/local-source-collection
python tools/shader_analysis/audit_vs.py --hlsl /path/to/vs_0123456789abcdef.hlsl --output out/local-vs-audit.json
python tools/shader_analysis/audit_ps.py --hlsl /path/to/ps_0123456789abcdef.hlsl --output out/local-ps-audit.json
python tools/shader_analysis/audit_ps.py --hlsl /path/to/ps_0123456789abcdef.hlsl --clip-input 4 --output out/local-ps-clip-input4.json
python tools/shader_analysis/inspect_spirv_position.py --spirv /path/to/shader.spv --grammar /path/to/spirv.core.grammar.json --output out/local-position-report.json
python tools/shader_analysis/cpx.py --input /path/to/package.cpx --output out/local-package.decoded
```

`--source LABEL=DIR` may repeat; each directory is searched **nonrecursively** for `*.bin`. These are already-extracted microcode files named `vs_<FNV64>.bin` or `ps_<FNV64>.bin`, not SDK containers, raw FPD archives, or HLSL. Names and bytes are checked against the same FNV-1a identity used by the production shader cache. The new output directory contains deduplicated `source/*.bin` and `provenance.json`, listing only the paths and labels supplied. Duplicate names with different content fail. The collector does not extract XEX static arrays or compare runtime holdouts; supply those already-extracted sources as another labeled directory when appropriate. It keeps the extracted bytes in its output; place that directory under ignored/private storage, never bundle it with a release.

Both `--hlsl` options accept a file or a directory searched **nonrecursively** for `vs_*.hlsl` / `ps_*.hlsl`, and may repeat. JSON reports record the explicit input paths and per-shader findings. VS guest `oPos` dependency slicing excludes the known VTE epilogue; matrix slot findings are static candidates and require observed camera/pass/viewport agreement. The analyzer recognizes legacy `c[N]` references and current `XeConst(N)` literal references; dynamically indexed constants remain unresolved. `audit_ps.py` can optionally inspect explicit varying indices with repeatable `--clip-input N` (N=0..15) and reports component reads in `clip_input_review`. Any branch or unknown data flow, including fixed host epilogues, remains unsupported. A `candidate_no_clip_xy_reads` result is a manual-review candidate only and never authorizes a mapping. PS dependency tracking executes branch text in source order; control flow, dynamic constants and sampled texture identities remain unresolved. The PS report does not infer depth identity from resource addresses or parse F1 captures. Neither audit changes production whitelist or game behavior.

The SPIR-V script accepts a `.spv` file (or a nonrecursive directory of them) and an explicit SPIR-V core grammar JSON. It can read a cache header before the first SPIR-V magic word, but does not write raw SPIR-V or disassembly. Only directly decorated `BuiltIn Position` stores are traced via grammar operand IDs; member-decorated interface blocks, control-flow alternatives and numerical equivalence remain unproven. The report flags untraced member decorations. Provide a grammar matching the input SPIR-V version.

`cpx.py` decodes a **single** supplied CPX package (decoded size capped at 256 MiB) to a new file. It is an independent offline prototype, not a substitute for the production `cpx_decode.h` decoder. It does not discover game packages or extract their shaders.

## Origin and overlap

| Helper | Adapted from | Scope retained / omitted |
| --- | --- | --- |
| `collect_sources.py` | `out/shader-offline/combine_sources.py` | Explicit labeled microcode directories and FNV dedup/provenance; removed top-level writes, fixed resource count, fixed XEX/capture paths and holdout comparison. |
| `audit_vs.py` | `out/taa-whole-game-scan/audit_vs.py` | Guest `oPos` static slice; removed implicit whole-game inventory, whitelist parsing, fixed capture references and fixed output location. |
| `audit_ps.py` | `out/taa-whole-game-scan/pixel-audit.py` | PS static taint triage; omitted scene-specific F1 binding associations, including a hard-coded shadow address. |
| `inspect_spirv_position.py` | `out/taa-bloom-fix/inspect_spirv_position.py` | Explicit SPIR-V + grammar + report; removed hard-coded Windows cache path, fixed shader filter, and copied raw/disassembly artifacts. |
| `cpx.py` | `out/shader-offline/cpx.py` | Single-package Python decoder with CLI and bounded output; no native DLL or bulk scan. |

`out/shader-offline/cpx_fast.py` / `cpx_fast.cpp` are Windows DLL prototypes with platform-specific build/loading; no native wrapper is migrated. `out/shader-offline/scan_all_cpx.py`, `compile_all.py`, and the scene/capture-specific reporting scripts remain experiments. Production CPX resource indexing already lives in `tools/generate_cpx_shader_index.py`, FPD indexing in `tools/generate_shader_index.py`, and translation / prebuild in `LoShaderTool`; these helpers do not replicate those pipelines. The original `out/` scripts and data remain untouched and are not runtime dependencies of these tools.

The reviewed feedback mapping batch is indexed by the public manifest
[`reviews/feedback_mapping_20260925.json`](reviews/feedback_mapping_20260925.json),
with corresponding synthetic cases in
[`../tests/feedback_mapping_cases.h`](../tests/feedback_mapping_cases.h). The
manifest records source and paired-PS identities, slots, families and review
decisions; it is source evidence and does not establish GPU pixels or player
acceptance.
