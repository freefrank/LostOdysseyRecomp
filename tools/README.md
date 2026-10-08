# Developer tools catalog

This directory contains version-controlled developer utilities, build helpers, offline analysis tools, profiling scripts, and verification harnesses for Lost Odyssey Recomp.

## Directory and storage rules

- **`tools/` contains formal reusable code**: All persistent utilities, analysis scripts, and test drivers belong in this directory. Every tool must specify its inputs and outputs explicitly.
- **`out/` is strictly for ignored outputs**: The `out/` directory is listed in `.gitignore` and `.stignore`. It is reserved for build binaries, ephemeral test outputs, capture extractions, intermediate reports, and temporary caches. Do not index `out/`, commit files from `out/`, or write tools that depend on executable scripts located in `out/`. Tools may explicitly consume build artifacts, captures, or logs stored in `out/` as inputs when passed via command-line arguments.
- **Side-effect awareness**: Tools differ in their operational side effects. Analysis scripts that parse input files and write reports produce no application state mutations. Build tools compile artifacts into explicit output directories. Active game drivers spawn game processes, send inputs, and alter save data. Network scripts fetch remote assets or push git commits. Check each tool's side-effect classification before execution.

## Quick navigation by task

| Task | Start here | Main effects |
|---|---|---|
| Build a read-only resource catalog for Mod development | [`asset_inventory/README.md`](asset_inventory/README.md), [`docs/wiki/Asset-Inventory.md`](../docs/wiki/Asset-Inventory.md) | Reads four-disc FPI/FPD data and writes metadata-only SQLite/CSV reports; does not launch the game and does not require WSL or Docker. |
| Inspect an F1 capture, compare frames, or review temporal-jitter candidates | [`capture_analysis/README.md`](capture_analysis/README.md) | Read capture files; write explicit reports, previews, or reviewed fixtures. No game launch. |
| Translate or audit shaders and portable shader packs | [`shader_analysis/README.md`](shader_analysis/README.md), [`PORTABLE_SHADER_PACK.md`](../docs/PORTABLE_SHADER_PACK.md) | Read inputs and write explicit reports/build outputs; shader-pack merge compiles and writes a new pack. |
| Archive private opt-in feedback and review cases | [`feedback_archive/README.md`](feedback_archive/README.md) | Offline ledger writes selected private archive paths; archive refresh performs read-only D1 queries. No public data or Git publication. |
| Build runtime, tools, or release packages | [`BUILDING.md`](../docs/BUILDING.md), [`release/README.md`](release/README.md) | Compiles or packages into explicit outputs; release fetchers use network access. |
| Run a live benchmark or inspect a running process | [`perf/README.md`](perf/README.md), [`asm-profiler/README.md`](asm-profiler/README.md) | May launch/control the game, modify isolated saves, or attach to a process. Read prerequisites first. |
| Install the reusable OpenCode render workflow | [`opencode/README.md`](opencode/README.md) | Writes only the four managed Markdown files under the explicit `.opencode/` output. |

The catalog below lists maintained groups and representative root utilities. `thirdparty/`, generated outputs, caches, and `out/` artifacts are not cataloged as project tools. Deprecated installer/updater or runner-only copies remain historical and are not maintained entry points.

---

## Tool catalog

### 1. Shader tools and analysis

| Tool / Path | Purpose | Type & side effects | Reference |
|---|---|---|---|
| `LoShaderPackTool` (`tools/shader_pack/`) | Inspect, verify, and merge portable Vulkan shader packs (`portable_vk.lospv`). Commands: `inspect`, `verify`, `verify-runtime`, `merge`. | `inspect`/`verify`: Read-only verification.<br>`merge`: Compiles microcodes and writes new pack/report to an exclusive output directory; rejects existing destinations. | [`docs/PORTABLE_SHADER_PACK.md`](../docs/PORTABLE_SHADER_PACK.md) |
| `LoShaderTool` (`tools/xenos_shader_tool/`) | Offline Xenon microcode translator and compiler to DXIL and SPIR-V. | Build tool: Compiles microcode binaries into output targets. | Source: `tools/xenos_shader_tool/` |
| `tools/shader_analysis/collect_sources.py` | Collects and deduplicates raw `vs_*.bin` and `ps_*.bin` microcode files by FNV-1a hash across labeled directories. | Write: Generates deduplicated source directory and `provenance.json` at explicit `--output`. Inputs are read-only. | [`tools/shader_analysis/README.md`](shader_analysis/README.md) |
| `tools/shader_analysis/audit_vs.py` | Statically analyzes vertex shader HLSL for `oPos` position and matrix slot dependencies. | Input read-only; writes JSON report to explicit `--output`. | [`tools/shader_analysis/README.md`](shader_analysis/README.md) |
| `tools/shader_analysis/audit_ps.py` | Statically analyzes pixel shader HLSL for texture bindings, sampler usage, taint propagation, and optional `--clip-input N` components. | Input read-only; writes JSON report to explicit `--output`; unresolved branches remain unsupported. | [`tools/shader_analysis/README.md`](shader_analysis/README.md) |
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
| `tools/capture_analysis/preview.py` | Exports selected capture screenshots/resolves as PNG and optional full-resolution ROI metrics. | Input read-only; writes a new preview directory. Requires Pillow. | [`tools/capture_analysis/README.md`](capture_analysis/README.md) |
| `tools/capture_analysis/coverage.py` | Summarizes unmapped VS evidence across complete draw intervals in one or more explicit captures. | Input read-only; writes a new JSON report and never edits the production map. | [`tools/capture_analysis/README.md`](capture_analysis/README.md) |
| `tools/capture_analysis/trace.py` / `compare_traces.py` | Reads cumulative register state and compares selected F1 capture frames. | Input read-only; writes JSON reports to explicit new paths. | [`tools/capture_analysis/README.md`](capture_analysis/README.md) |
| `tools/capture_analysis/jitter_candidates.py` | Triage unlisted position shaders against an explicit mapping and draw boundary. | Input read-only; writes candidate JSON and never edits the production shader map. | [`tools/capture_analysis/README.md`](capture_analysis/README.md) |
| `tools/capture_analysis/export_jitter_fixture.py` | Serializes explicitly reviewed capture draw/register pairs into a compact C++ fixture. | Input read-only; writes a fixture to an explicit path and does not choose mappings or edit production code. | [`tools/capture_analysis/README.md`](capture_analysis/README.md) |
| `tools/capture_analysis/suspect_log.py` | Reads runtime `temporal suspect` lines (unmapped scene-camera draws under jitter) and the preceding map line from ordinary runtime logs; optionally emits the same compact fixture without an F1 capture. | Logs read-only; writes a new JSON summary and optional fixture. | [`tools/capture_analysis/README.md`](capture_analysis/README.md#runtime-temporal-suspects-without-a-capture) |
| `tools/capture_analysis/fg_ui_recompose.py` | Performs a dual-background numeric RGB recomposition diagnostic from an explicit `scene`/`black`/`white`/`final` raw-image manifest. | Input read-only; writes a new report directory. Requires Python 3.10+, NumPy, and Pillow; never launches the game or calls an FG provider. Reports `capture_equation_fit` only for a valid explicit replay and always reports `ui_separation=unavailable`, `provider_ready=false`. | [`tools/capture_analysis/README.md`](capture_analysis/README.md) |
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
| `tools/release/extract_release_notes.py` | Extracts version-specific ATX-heading body from `CHANGELOG.md` for GitHub Release publication. | Input read-only; writes extracted Markdown to explicit `--output`. | Source: `tools/release/` |
| `tools/release/publish_shader_packs.py` | Checks shader packs against the runtime contracts, stages them with the merged `index.json`, uploads them to the `shader-packs` prerelease (`--publish`) or checks that the published index covers a runtime (`--check`). | Runs `LoShaderPackTool`; reads the published index over HTTPS; with `--publish`, **uploads release assets** with `gh`. | [`docs/PORTABLE_SHADER_PACK.md`](../docs/PORTABLE_SHADER_PACK.md#publishing) |
| `tools/release/fetch_dlss_sdk.py` | Downloads NVIDIA DLSS SDK assets for build packaging. | Network I/O: Fetches external dependency; writes local directory. | Source: `tools/release/` |
| `tools/release/prepare_streamline_sdk.py` | Extracts the pinned official Streamline SDK headers, FG runtime libraries and redistribution licenses. | Reads the downloaded SDK archive; writes only to the explicit SDK output directory. | Source: `tools/release/` |
| `tools/release/fetch_build_input.py` | Downloads external release build dependencies. | Network I/O: Fetches external assets; writes local file. | Source: `tools/release/` |
| `tools/package_release.py` | Builds Windows release ZIP packaging binaries and licenses (no shader pack). | Packaging: Creates release ZIP archive in output directory. | Source: `tools/package_release.py` |
| `tools/package_appimage.py` | Packages Linux x86_64 AppImage using `linuxdeploy`. | Packaging: Assembles AppImage bundle. | Source: `tools/package_appimage.py` |
| `tools/package_flatpak.py` | Builds offline Linux x86_64 Flatpak bundle using `flatpak-builder` and Freedesktop 26.08 SDK/runtime. | Packaging: Stages tracked source, generated PPC code, private disc inputs, pinned dependencies and licenses (no shader pack); exports OSTree repo and builds standalone `.flatpak` bundle. | [`docs/BUILDING.md`](../docs/BUILDING.md#packaging-flatpak) |
| `tools/package_macos.py` | Packages the macOS runtime as an ad-hoc signed `.app` in a ZIP, or with `--dmg` in a disk image that also holds an Applications link; `--identity` signs with a Developer ID and `--notarize` submits to Apple (no shader pack). | Packaging: Needs a finished Mac build (`source-version.txt`, runtime and `libdxcompiler.dylib`); runs macOS system tools (`codesign`, `hdiutil`, `ditto`), and with `--notarize` contacts Apple's notary service; writes the archive under `out/releases/` by default. The v0.7.35, v0.8.0 and v0.8.5 disk images were made with `--version <tag> --dmg`. | [`docs/MACOS_RELEASE.md`](../docs/MACOS_RELEASE.md#disk-image-v085) |
| `tools/release_macos.sh` | Builds, Developer ID signs, notarizes and packages a macOS release. | Packaging and network I/O: Needs a clean tracked tree, a keychain signing identity and a notary profile; builds the runtime and calls `package_macos.py` without `--dmg`, so it writes a ZIP. Not used for v0.7.35. | [`docs/MACOS_RELEASE.md`](../docs/MACOS_RELEASE.md#signed-release-workflow) |

### 6. Build entrypoints

| Script | Platform | Purpose |
|---|---|---|
| `tools/build_release.bat` | Windows | Builds release runtime target using clang-cl and Ninja with optional DLSS and FSR flags. |
| `tools/build_runtime.bat` | Windows | Fast incremental build for the primary runtime target `LostOdysseyRecomp`. |
| `tools/build_tools.bat` | Windows | Builds offline developer tools (`LoShaderPackTool`, `LoShaderTool`, etc.). |
| `tools/build_target.bat` | Windows | Configures and builds a specific CMake target with specified arguments. |
| `tools/build_worktree.bat` | Windows | Configures and builds a target in a git worktree, taking the SDKs, FSR shaders and dependency checkouts from the main checkout (`LO_MAIN_CHECKOUT`, `LO_DEPS_DIR`, `LO_BUILD_JOBS`; the main checkout defaults to the first `git worktree list` entry). The worktree must be prepared first; see the header of the script. |
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

### 8. Modding tools

| Tool / Path | Purpose | Type & side effects |
|---|---|---|
| `tools/modding/lo_mod.py` | Builds v1 image mod ZIPs with `LOTEX1` payloads and validated relative asset keys. | Local write: Creates a mod archive at an explicit output path; does not modify imported game files. |
| `tools/modding/publish_wiki.py` | Stages the maintained Modding API and workflow pages into an existing cloned Wiki repository. | Local write: Updates only managed Wiki pages and navigation; requires an explicit cloned destination. |
| [`tools/asset_inventory/inventory.py`](asset_inventory/README.md) | Scans a complete four-disc game root, hashes complete resource payloads, decodes package metadata, classifies exports, and supports `scan`, `reparse`, `report`, and `query`. | Read-only game input; writes metadata-only SQLite, reports, and CSV/CSV.GZ files to an explicit external output directory. Does not extract original payloads, launch the game, or require WSL/Docker. |
| [`tools/asset_inventory/export_fmv.py`](asset_inventory/README.md) | Explicitly exports indexed ASF/WMV and CPX FMV payloads to a new `movies/` directory and JSON/CSV manifests, preserving raw streams and de-duplicating decoded output by SHA-256. | Opt-in local write: reads the catalog and game data, copies/decodes video to an explicit output directory, and validates streams with the required `ffprobe`; does not transcode, repair damaged input, or add a runtime Movie provider. |

### 9. Project management and issue triage

| Tool / Path | Purpose | Type & side effects |
|---|---|---|
| `tools/issue_triage/triage.py` | Automated triage script for GitHub Issues using LLM code context matching; the analysis comes from Claude Code (`claude -p`, no tools, `--safe-mode`) with `CLAUDE_CODE_OAUTH_TOKEN`. | **REMOTE I/O**: Queries GitHub API; runs the Claude Code CLI and posts comments if authorized. |
| `tools/issue_triage/code_context.py` | Extracts codebase symbol context for issue reports. | Read-only: Scans repository code. |
| `tools/project_management/` | Helper scripts for syncing GitHub Project fields, items, and roadmap mirrors. | Workflow integration: Updates project tracking state. |
| `tools/discord_sync/discord_sync.py` | Copies new posts and replies from Discord forums to GitHub (`discord` label): `#help` becomes issues (and starts Issue triage), `#discussion`, `#ideas` and `#show-and-tell` become Discussions in General, Ideas and Show and tell; mapping in `DISCORD_FORUMS`; run every 15 minutes by `.github/workflows/discord-discussions-sync.yml`. Content is one-way; edits, deletions and attachments are not copied (attachments link back to Discord). Issue open/closed state and the forum's Solved tag follow each other both ways. | **REMOTE I/O**: Reads Discord with `DISCORD_BOT_TOKEN` and edits only the Solved tag of `#help` posts (needs Manage Threads there); creates, comments on, updates, closes and reopens issues and Discussions; dispatches `issue-triage.yml`. `DRY_RUN=true` only prints. Stops if the bot lacks the Message Content intent; exits quietly when the token is not set. |
| `tools/discord_sync/notify.py` | Posts each push to any branch to Discord `#development` (one embed listing the new commits; new branches and force pushes are labelled, deletions skipped), opened, reopened or closed-unmerged pull requests to the same channel (via `pull_request_target`, which never checks out PR code), and each published release to `#announcements` as one line, crossposted to following servers, with the changelog in a thread under it (re-running for the same release updates that post); run by `.github/workflows/discord-notify.yml`, which can also (re)post an existing release tag by hand. | **REMOTE I/O**: Sends Discord messages as the sync bot with `DISCORD_BOT_TOKEN` (no pings allowed); reads a release from the GitHub API for manual runs. `DRY_RUN=true` only prints. |
| `tools/supporter_relay/` | Cloudflare Worker at `https://supporters.dotslash.pro` (`/kofi`, `/bmc`) that turns Ko-fi and Buy Me a Coffee webhooks into thank-you posts in Discord `#supporters`: name and message, never the amount; private supporters appear as "Someone"; Ko-fi renewals and refunds are not posted. Deploy with `npx wrangler deploy` from the folder. | **REMOTE I/O**: Verifies the Ko-fi token or the BMC HMAC signature, then posts through the `#supporters` channel webhook. Secrets `DISCORD_WEBHOOK_URL`, `KOFI_VERIFICATION_TOKEN` and `BMC_WEBHOOK_SECRET` live in Cloudflare, not in the repo. |

### 10. Private feedback archive

| Tool / Path | Purpose | Type & side effects | Reference |
|---|---|---|---|
| `tools/feedback_archive/` | Archives opt-in D1 feedback into an explicitly selected private directory and maintains offline review ledgers and the `lo-feedback-triage` skill. | Archive refresh: read-only remote queries plus local staged writes. Ledger and review: local writes only. No game operations, public publication, or Git push. | [`tools/feedback_archive/README.md`](feedback_archive/README.md) |
| `tools/feedback_archive/scripts/jitter_coverage.py` | Streams private VS/PS observations against an explicit mapping to rank jitter-coverage candidates. | Read-only archive input; writes a new report outside the archive. Does not edit the production map or represent player counts. | [`tools/feedback_archive/README.md`](feedback_archive/README.md) |
| `tools/feedback_archive/scripts/export_programs.py` | Exports explicitly selected, identity-verified VS/PS payloads for source review. | Read-only archive input; writes verified payloads and provenance to a new directory outside the archive. | [`tools/feedback_archive/README.md`](feedback_archive/README.md) |

### 11. OpenCode render investigation workflow

| Tool / Path | Purpose | Type & side effects | Reference |
|---|---|---|---|
| `tools/opencode/install.py` | Installs the maintained `lo-render-flicker` skill and three render investigation agents into `.opencode/`. | Local write: creates or updates only the four managed Markdown files under the explicit output directory. | [`tools/opencode/README.md`](opencode/README.md) |

Install from the repository root with `python -B tools/opencode/install.py --output .opencode`; add `--overwrite` only when updating those managed files. The installed `.opencode/` state is ignored.

### 12. Recipe tour patches, corpus builder and settings driver

| Tool / Path | Purpose | Type & side effects | Reference |
|---|---|---|---|
| `tools/tours/diag-tour.patch`, `mac-tour.patch` | Never-merged source patches for a diagnostic build: map jump, disc request and package probe command files that `recipe_tour.py maps` and `battles --disc` use. | Patch files only; apply to a worktree, never the main checkout. Neither applies cleanly to current main (see the reference). | [`tools/tours/README.md`](tours/README.md) |
| `tools/tours/build_corpus.py` | Merges per-scene tour recipe files into the scene-tagged `pipelines_corpus.bin` with `tools/pipeline_recipes.py`. | Input read-only; writes `group-*.bin` and the corpus to the explicit `--out` folder. | [`tools/tours/README.md`](tours/README.md) |
| `tools/settings_driver/` (`boot.py`, `drive.py`, `burst.py`) | Boot a build into the field of a staged save, press buttons, take presented screenshots and frame bursts, to check the host settings menu. Windows only, needs Pillow. | **ACTIVE GAME DRIVER**: copies a run folder, launches the exe, sends inputs, writes screenshots, kills the process. Paths come from options or `LO_SETTINGS_*` variables. | [`tools/settings_driver/README.md`](settings_driver/README.md) |

---

## Testing infrastructure

All automated and manual test suites live in [`tools/tests/`](tests/). Refer to [`tools/tests/README.md`](tests/README.md) for target groupings, execution prerequisites, and side-effect classifications.
