# Test suites

A short catalog of the checks that stay in the repository. Each protects player-facing behavior or a contract that is cheap to keep. Results, check counts and investigation history are not recorded here; see [project status](../../docs/STATUS.md) for acceptance and [the roadmap](../../docs/ROADMAP.md) for open work.

`tools/test.bat` (forwarding to `run.py`) is a convenience runner for some suites, not a registry of all of them. Run only the checks that match the changed area.

## Runtime fixtures (`LostOdysseyRecomp/CMakeLists.txt`)

Most single-file fixtures in `tools/tests/*_test.cpp` are CMake targets of the main build. They are `EXCLUDE_FROM_ALL`: build the one you need, then run its executable.

```powershell
cmake --build out/build/windows-clang --target LoHidTest LoFramePlanTest
out\build\windows-clang\LostOdysseyRecomp\LoHidTest.exe
```

Targets that touch `settings.ini` or other files (for example `LoSaveAnywhereConfigTest`) must run in an empty temporary directory with a copy of the executable. GPU fixtures (`Vulkan`, `D3D12`, `Presentation`, `*Gpu*`) need a real device and have device side effects. They are not CI gates.

## Independent suites

Each directory configures on its own, without game data or the full dependency tree.

| Directory | Covers | Configure |
|---|---|---|
| `review_regressions/` | Sanitizer fixtures from the 2026-09-30 project review, updater and packaging scripts | see below |
| `resolve_reconfigure/` | Production settings reconfiguration retains GPU-only guest resolve contents and drops old frame-plan lineage | `cmake -S tools/tests/resolve_reconfigure -B out/resolve-reconfigure` |
| `cinematic_bars/` | Native cutscene, fade-tail and game-pause signals against guest-call stubs; also included in review regressions | `cmake -S tools/tests/cinematic_bars -B out/cinematic-tests` |
| `settings_menu/` | Settings menu interaction and raster contracts | `cmake -S tools/tests/settings_menu -B out/menu` |
| `streamline_fg/` | GPU-free frame generation, selection and SR input contracts (plus `settings_menu/`) | `cmake -S tools/tests/streamline_fg -B out/fg-contracts` |
| `frame_generation/` | Reusable frame generation core | `cmake -S tools/tests/frame_generation -B out/fg-core` |
| `native_dlss/` | Native DLSS/DLAA and FSR policy contracts | `cmake -S tools/tests/native_dlss -B out/native-dlss -DLO_NATIVE_DLSS_CPU_ONLY=ON` |
| `motion_replay/` | Motion vector replay and TAA (GPU) and renderer compile boundary | `cmake -S tools/tests/motion_replay -B out/motion` |
| `optiscaler/`, `vrr/`, `fsr/` | OptiScaler loader, VRR config, FSR policy fixtures | configure the directory |
| `recompiler_switch/`, `recompiler_semantics/` | PPC recompiler regressions | see below |
| `controller_atlas/` | Controller glyph atlas decode and the renderer's Xbox/PlayStation atlas swap; needs the local UI export ([README](controller_atlas/README.md)) | `cmake -S tools/tests/controller_atlas -B out/atlas` |

Build and run with `cmake --build <dir>` then `ctest --test-dir <dir> --output-on-failure`.

## Project review regressions (2026-09-30)

Builds without game data, generated PPC sources or a GPU. It covers the 13 issues in [the fix record](../../docs/archive/notes/PROJECT_REVIEW_FIXES_20260930.md). Needs GCC/Clang, Python 3.11+ and CMake 3.20+ on Linux. The SDL fixtures use an installed SDL3 package when CMake finds one and otherwise build the pinned `thirdparty/SDL` submodule.

```bash
git submodule update --init --depth 1 tools/XenonRecomp thirdparty/SDL
git -C tools/XenonRecomp submodule update --init --depth 1 thirdparty/xxHash thirdparty/tomlplusplus
cmake -S tools/tests/review_regressions -B out/review-regressions -DCMAKE_BUILD_TYPE=Debug
cmake --build out/review-regressions --parallel 2
ctest --test-dir out/review-regressions --output-on-failure --parallel 2
```

ASan/UBSan are on by default (`-DLO_REVIEW_SANITIZE=OFF` disables them). `-DLO_REVIEW_SDL_TESTS=OFF` omits the SDL fixtures. The suite also runs `updater_appimage_cleanup_test.py`, `package_appimage_test.py` and `package_flatpak_test.py`; each also runs directly with `python3`. Packaging tests use stand-ins for linuxdeploy and flatpak, so the release workflow remains the real check.

The suite includes `LoImportGameTest`, `LoImportIsoLocatorTest` and `LoImportReimportTest`, which use synthetic disc data. On Linux, `LoImportLinuxHomeTest` checks imports beneath the Fedora Atomic `/home` → `/var/home` system link and rejects other symlinks. It needs `bubblewrap` (`bwrap`) and unprivileged user/mount namespaces; it exits 77 (CTest skip) if either is unavailable. The fixture creates an isolated root and does not alter the host's `/home`. To run it directly after configuring the suite:

```bash
cmake --build out/review-regressions --target LoImportGameTest
bash tools/tests/import_linux_home_test.sh out/review-regressions/LoImportGameTest
```

## Selected native targets

Examples of fixtures that stay out of the default build. Build only what the change needs.

| Target | Area |
|---|---|
| `LoStorageTest`, `LoGuestDispatchTest`, `LoGuestPrintfTest` | Guest file I/O, dispatch tables, string formatting |
| `LoHidTest`, `LoQuitActionHookTest`, `LoQuitTextHookTest` | Input mapping, quit routing and text overrides |
| `LoSaveAnywhereConfigTest`, `LoDebugMenuInteractionTest` | Debug settings persistence and menu behavior |
| `LoFramePlanTest`, `LoFramePacerTest`, `LoRenderTimingTest` | Frame planning and pacing |
| `LoPortableShaderPackTest`, `LoPortableShaderPackIntegrationTest`, `LoPortableShaderContractTest` | Shader pack format and runtime contract |
| `LoImportGameTest`, `LoInstallerControllerTest` | Importer and installer state machines |
| `LoVertexCacheTest`, `LoTextureKeyTest`, `LoTextureDescriptorCacheTest`, `LoRegisterSnapshotTest` | Render caches and register/constant snapshots |
| `LoCrashCaptureTest`, `LoCaptureArchiveTest`, `LoLogCaptureTest`, `LoShaderLogTest` | Crash, log and shader-log capture |
| `LoDlssStatusLogTest` | DLSS status and settings log lines |
| `LoTemporalJitterTest`, `LoTemporalSceneTest`, `LoMotionVectorTest`, `LoPositionEvidenceTest` | Temporal AA inputs and the position classifier behind the temporal suspect check |
| `LoVulkanBackendTest`, `LoBackendLifecycleTest`, `LoPresentationTest`, `LoPresentCaptureTest`, `LoTemporalAATest` | GPU backend, presentation, capture and TAA shader (real device) |

`LoBackendLifecycleTest` needs `LO_SHADER_CACHE_DIR` pointing at a warmed cache for the close and restart cases. `shader_log_test.py --exe <LoShaderLogTest> --dxc <dxcompiler> --output <new dir>` runs `LoShaderLogTest` and checks its output. `shader_source_collection_test.cpp` has no CMake target; compile it directly with C++20.

## Python scripts

Run directly with `python`. Unless noted, they use synthetic data.

- Offline tools: `shader_analysis_tools_test.py`, `capture_analysis_tools_test.py`, `portable_shader_merge_test.py`, `jitter_capture_tools_test.py`, `mod_tools_test.py`, `mod_catalog_test.py`, `ppc_codegen_test.py`, `disc_set_test.py`, `test_release_notes.py`. All `test_*.py` files run together with `python -m unittest discover -s tools/tests -p 'test_*.py'`.
- Runtime regressions that compile extracted production code: `file_io_regression.py`, `io_lifetime_test.py`, `kernel_lifetime_regression.py`, `page_allocator_test.py`, `gpu_command_safety_regression.py`, `guest_main_exit_regression.py`, `crash_capture_test.py`, `startup_path_test.py`, `menu_boundary_regression.py`, `shader_prebuild_regression.py`, `video_exit_pump_test.py` (Windows; needs a static SDL3 build).
- `guest_language_test.py` runs the generated PPC language routines; it needs local generated PPC sources and `clang++`.
- `android_bda_spirv_contract.py` compiles Android's vertex-fetch prelude and validates its SPIR-V; it needs DXC and SPIRV-Tools.
- `fsr/check-fsr-isolated-timing.py` summarizes `FSR GPU timing:` lines from a runtime log; `temporal_geometry_analysis.py` analyzes the frames that `LoTemporalAATest` keeps from its geometry case.
- Android touch: `android_touch_layout_test.py`, `android_touch_visibility_test.py`.

## Recompiler word-switch regression

```powershell
python -B tools/tests/recompiler_switch_test.py --output out/tests/recompiler-switch/run-01
```

Builds the production decoder and recompiler, emits functions from synthetic PPC words and executes them. It needs no game assets and takes a new output directory each run. It is an explicit entry point, not a `tools/test.bat` suite.

## Recompiler semantics regression

```powershell
python -B tools/tests/recompiler_semantics_test.py --output out/tests/recompiler-semantics/run-01
```

Same approach for instruction semantics (carry, masks, shifts, Rc forms, memory addressing, branches). Needs Python, CMake, Ninja, `clang-cl`, the Windows SDK and the checked-out recompiler dependencies. Takes a new output directory each run.

## Tools that use fixtures

`controller_atlas_glyph_test.cpp` is the glyph decoder fixture used by `tools/ui_assets/preview_controller_atlas.py`. `tools/asm-profiler/test_report.py` checks the profiler report ([README](../asm-profiler/README.md)). `tools/perf/drive-city.ps1` drives the real game for benchmarks ([README](../perf/README.md)).

## CI

CI runs on GitHub-hosted runners: `fg-cpu-contracts.yml` (FG contracts and the reusable FG core), `fg-game-integration.yml`, `review-regressions.yml` and `android-apk.yml` (see [CI guide](../../docs/notes/ci.md) for push, pull request and dispatch triggers). The macOS arm64 workflow on GitHub builds and runs the targets that need no game data. A passing check is not gameplay or visual acceptance.
