# Developer tools catalog

This directory contains version-controlled developer utilities, build helpers, offline analysis tools, profiling scripts, and verification harnesses for Lost Odyssey Recomp.

## Directory and storage rules

- **`tools/` contains formal reusable code**: All persistent utilities, analysis scripts, and test drivers belong in this directory. Every tool must specify its inputs and outputs explicitly.
- **`out/` is strictly for ignored outputs**: The `out/` directory is listed in `.gitignore` and `.stignore`. It is reserved for build binaries, ephemeral test outputs, capture extractions, intermediate reports, and temporary caches. Do not index `out/`, commit files from `out/`, or write tools that depend on executable scripts located in `out/`. Tools may explicitly consume build artifacts, captures, or logs stored in `out/` as inputs when passed via command-line arguments.
- **Side-effect awareness**: Tools differ in their operational side effects. Analysis scripts that parse input files and write reports produce no application state mutations. Build tools compile artifacts into explicit output directories. Active game drivers spawn game processes, send inputs, and alter save data. Network scripts fetch remote assets or push git commits. Check each tool's side-effect classification before execution.

---

## Tool catalog

### 1. Shader tools and analysis

| Tool / Path | Purpose | Type & side effects | Reference |
|---|---|---|---|
| `LoShaderPackTool` (`tools/shader_pack/`) | Inspect, verify, and merge portable Vulkan shader packs (`portable_vk.lospv`). Commands: `inspect`, `verify`, `verify-runtime`, `merge`. | `inspect`/`verify`: Read-only verification.<br>`merge`: Compiles microcodes and writes new pack/report to an exclusive output directory; rejects existing destinations. | [`docs/PORTABLE_SHADER_PACK.md`](../docs/PORTABLE_SHADER_PACK.md) |
| `LoShaderTool` (`tools/xenos_shader_tool/`) | Offline Xenon microcode translator and compiler to DXIL and SPIR-V. | Build tool: Compiles microcode binaries into output targets. | Source: `tools/xenos_shader_tool/` |
| `tools/shader_analysis/collect_sources.py` | Collects and deduplicates raw `vs_*.bin` and `ps_*.bin` microcode files by FNV-1a hash across labeled directories. | Write: Generates deduplicated source directory and `provenance.json` at explicit `--output`. Inputs are read-only. | [`tools/shader_analysis/README.md`](shader_analysis/README.md) |
| `tools/shader_analysis/audit_vs.py` | Statically analyzes vertex shader HLSL for `oPos` position and matrix slot dependencies. | Input read-only; writes JSON report to explicit `--output`. | [`tools/shader_analysis/README.md`](shader_analysis/README.md) |
| `tools/shader_analysis/audit_ps.py` | Statically analyzes pixel shader HLSL for texture bindings, sampler usage, and taint propagation. | Input read-only; writes JSON report to explicit `--output`. | [`tools/shader_analysis/README.md`](shader_analysis/README.md) |
| `tools/shader_analysis/inspect_spirv_position.py` | Inspects compiled SPIR-V binary position decorations using an explicit SPIR-V core grammar. | Input read-only; writes JSON report to explicit `--output`. | [`tools/shader_analysis/README.md`](shader_analysis/README.md) |
| `tools/shader_analysis/cpx.py` | Decodes a single CPX package binary to an output file using pure Python. | Write: Writes decoded package to explicit `--output`. Input is read-only. | [`tools/shader_analysis/README.md`](shader_analysis/README.md) |
| `tools/generate_shader_index.py` | Generates shader resource mapping indices from FPD archive files. | Generation utility: Parses game resources, emits index definitions. | Source: `tools/` |
| `tools/generate_cpx_shader_index.py` | Generates shader resource indices from CPX archives. | Generation utility: Parses game resources, emits index definitions. | Source: `tools/` |
| `tools/generate_shader_variants.py` | Generates shader permutation variants. | Generation utility: Precomputes shader definitions. | Source: `tools/` |

### 2. Capture and visual analysis

| Tool / Path | Purpose | Type & side effects | Reference |
|---|---|---|---|
| `tools/capture_analysis/inspect.py` | Inspects F1 render captures (ZIP archive or directory) and summarizes capture-info and frame render-state events. | Input read-only; writes JSON report to explicit `--output` without payload extraction. | [`tools/capture_analysis/README.md`](capture_analysis/README.md) |
| `tools/capture_analysis/image_diff.py` | Computes mean/maximum RGB channel difference and thresholded outlier metrics across two images with optional ROI. | Input read-only; writes metrics JSON to explicit `--output` and optional difference PNG to `--diff-image`. | [`tools/capture_analysis/README.md`](capture_analysis/README.md) |
| `tools/ppm2png.py` | Standalone converter from binary Netpbm P6 PPM to PNG without external dependencies. | Conversion: Reads PPM, writes PNG to destination. | Source: `tools/ppm2png.py` |

### 3. Performance analysis and benchmark drivers

| Tool / Path | Purpose | Type & side effects | Reference |
|---|---|---|---|
| `tools/perf/analyze-city-comparison.py` | Compares two city benchmark `drive-summary.json` runs and prints comparison metrics to stdout. | Read-only: Reads existing summary JSON and logs; outputs JSON to stdout. | [`tools/perf/README.md`](perf/README.md) |
| `tools/perf/drive-city.ps1` | Automated city performance capture runner. Automates launch, menu navigation, movement, screenshot capture, and log collection. | **ACTIVE GAME DRIVER**: Launches executable, sends inputs, creates screenshots, modifies and checks game saves. Requires isolated directories. | [`tools/perf/README.md`](perf/README.md) |
| `tools/perf/classify-city-timing.ps1` | Classifies runtime log frame intervals into benchmark phases. | Read-only: Parses log files, outputs classified timing data. | [`tools/perf/README.md`](perf/README.md) |

### 4. FSR and scaling tooling

| Tool / Path | Purpose | Type & side effects | Reference |
|---|---|---|---|
| `tools/fsr/generate_shaders.py` | Generates FSR Vulkan shader permutation sources from FidelityFX SDK. | Build utility: Emits shader permutations into output directory. | [`tools/fsr/README.md`](fsr/README.md) |
| `tools/fsr/prepare_adapter_shaders.py` | Compiles internal FSR adapter GLSL shaders to C++ SPIR-V headers using `glslangValidator`. | Build utility: Compiles shaders and writes C++ headers. | [`tools/fsr/README.md`](fsr/README.md) |
| `tools/fsr/sdk_link_check.cpp` | Minimal translation unit validating standalone FidelityFX SDK linkage. | Test fixture: Compilation check only. | Source: `tools/fsr/` |
| `tools/fsr/native_compile_check.sh` | Linux shell verification for FSR compilation. | Test driver: Shell compilation check. | Source: `tools/fsr/` |

### 5. Release, packaging and verification

| Tool / Path | Purpose | Type & side effects | Reference |
|---|---|---|---|
| `tools/release/verify_package.py` | Verifies Windows release ZIP checksum, manifest, version, commit, and runtime provenance. | Input read-only; writes JSON verification report to explicit `--output`. | [`tools/release/README.md`](release/README.md) |
| `tools/release/extract_release_notes.py` | Extracts version-specific ATX-heading body from `CHANGELOG.md` for GitHub Release publication. | Input read-only; writes extracted Markdown to explicit `--output`. | Source: `tools/release/` |
| `tools/release/fetch_shader_pack.py` | Downloads portable shader pack release asset from GitHub Release tags. | Network I/O: Fetches asset from GitHub; writes local file. | Source: `tools/release/` |
| `tools/release/fetch_dlss_sdk.py` | Downloads NVIDIA DLSS SDK assets for build packaging. | Network I/O: Fetches external dependency; writes local directory. | Source: `tools/release/` |
| `tools/release/fetch_build_input.py` | Downloads external release build dependencies. | Network I/O: Fetches external assets; writes local file. | Source: `tools/release/` |
| `tools/release/sync_shader_pack.py` | Synchronizes portable shader pack release payloads. | **Remote git push**: Chunks `portable_vk.lospv` and commits + pushes directly to the private build-inputs repository (`freefrank/LostOdysseyRecomp-build-inputs:main`); pass `--dry-run` to chunk locally without pushing. | Source: `tools/release/` |
| `tools/package_release.py` | Builds Windows release ZIP packaging binaries, licenses, and shader pack. | Packaging: Creates release ZIP archive in output directory. | Source: `tools/package_release.py` |
| `tools/package_appimage.py` | Packages Linux x86_64 AppImage using `linuxdeploy`. | Packaging: Assembles AppImage bundle. | Source: `tools/package_appimage.py` |

### 6. Build entrypoints

| Script | Platform | Purpose |
|---|---|---|
| `tools/build_release.bat` | Windows | Builds release runtime target using clang-cl and Ninja with optional DLSS and FSR flags. |
| `tools/build_runtime.bat` | Windows | Fast incremental build for the primary runtime target `LostOdysseyRecomp`. |
| `tools/build_tools.bat` | Windows | Builds offline developer tools (`LoShaderPackTool`, `LoShaderTool`, etc.). |
| `tools/build_target.bat` | Windows | Configures and builds a specific CMake target with specified arguments. |
| `tools/build_linux.sh` | Linux | Native Linux Clang build script for runtime and tools. |
| `tools/build_wsl.bat` | Windows/WSL | Dispatches Linux builds inside Windows Subsystem for Linux. |

### 7. Profiling, reverse engineering and diagnostics

| Tool / Path | Purpose | Type & side effects | Reference |
|---|---|---|---|
| `lo_asm_profiler` (`tools/asm-profiler/collector.cpp`) | Live sampling profiler attaching to a running Windows process to sample thread RIPs and module symbols. | **Active process inspection**: Attaches to target process by `--pid`, suspends threads briefly to read RIPs and symbols, writes JSON capture to `--output`. | [`tools/asm-profiler/README.md`](asm-profiler/README.md) |
| `tools/asm-profiler/report.py` | Offline disassembly and hotspot report generator for captured samples using Capstone. | Input read-only; disassembles captured RIPs and writes HTML report to `--output` and companion JSON. | [`tools/asm-profiler/README.md`](asm-profiler/README.md) |
| `tools/xex_info.py` | Dumps Xbox 360 XEX executable headers, section bounds, and security descriptors. | Read-only: Inspects XEX file, prints headers. | Source: `tools/xex_info.py` |
| `tools/find_ppc_helpers.py` | Scans disassembly for PowerPC runtime helper function call sites. | Read-only: Scans assembly text. | Source: `tools/find_ppc_helpers.py` |
| `tools/find_vtable_targets.py` | Scans executable images for virtual table pointer tables. | Read-only: Scans binary image. | Source: `tools/find_vtable_targets.py` |
| `tools/gen_import_stubs.py` | Generates C++ import thunk stubs from symbol tables. | Generation: Outputs source code. | Source: `tools/gen_import_stubs.py` |
| `tools/gen_function_bounds.py` | Extracts function boundary markers from map files. | Generation: Outputs function bounds table. | Source: `tools/gen_function_bounds.py` |
| `tools/xexdump/` | Dumps decrypted and decompressed XEX memory image to a flat binary and extracts symbols. Usage: `xexdump <in.xex> <out.bin>`. | Write: Dumps flat memory image to `<out.bin>` and symbol table to `<out.bin>.sym`. | Source: `tools/xexdump/` |

### 8. Project management and issue triage

| Tool / Path | Purpose | Type & side effects |
|---|---|---|
| `tools/issue_triage/triage.py` | Automated triage script for GitHub Issues using LLM code context matching. | **REMOTE I/O**: Queries GitHub API; may invoke LLM model APIs and post comments if authorized. |
| `tools/issue_triage/code_context.py` | Extracts codebase symbol context for issue reports. | Read-only: Scans repository code. |
| `tools/project_management/` | Helper scripts for syncing GitHub Project fields, items, and roadmap mirrors. | Workflow integration: Updates project tracking state. |

---

## Testing infrastructure

All automated and manual test suites live in [`tools/tests/`](tests/). Refer to [`tools/tests/README.md`](tests/README.md) for target groupings, execution prerequisites, and side-effect classifications.
