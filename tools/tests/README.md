# Test suites

Run commands from the repository root. Select checks appropriate to the changed behavior; this entry point does not imply that every suite is required for every change.

```powershell
tools\test.bat --list
tools\test.bat importer
tools\test.bat shaders pipeline
```

`tools/test.bat` forwards to `tools/tests/run.py`. Python is required; native fixture compilation also needs `clang-cl` and the Windows SDK discovered by `tools/setup_windows.bat`. Runtime suites use existing build outputs and never trigger an implicit full build. `--build-dir` defaults to `out/build/release`; it takes the CMake build root, not the directory containing the executable. The runner appends `LostOdysseyRecomp/<target>.exe`. A configured `out/build/windows-clang` can be supplied instead.

For runtime checks, choose the relevant pair below after configuring the build and generating its required game sources; these are alternatives, not a required full sequence:

```powershell
# Storage changes
cmake --build out/build/release --target LoStorageTest
tools\test.bat storage --build-dir out/build/release
```

```powershell
# Input changes
cmake --build out/build/release --target LoHidTest
tools\test.bat hid --build-dir out/build/release
```

```powershell
# Startup-path changes
cmake --build out/build/release --target LostOdysseyRecomp
tools\test.bat startup --build-dir out/build/release
```

| Suite | Scope |
|---|---|
| `importer` | Importer input and extraction checks |
| `shader-index` | Resource shader index/scanner fixtures |
| `shaders` | Resource scanning, CPX decoding and bounded dynamic-VS fixtures |
| `pipeline` | Pipeline recipe validation, corruption/truncation and atomic-write fixtures |
| `storage` | Built `LoStorageTest`: disc selection and save round trips |
| `hid` | Built `LoHidTest`: controller and keyboard input |
| `startup` | Built `LostOdysseyRecomp`: synthetic headless Unicode startup-path checks |

Fixture executables go to `out/tests/bin`. Synthetic temporary input is cleaned up automatically. Preserve logs or reports needed to review a result; remove only disposable files created by the current task, never user saves, profiles or unrelated artifacts.

The older `tools/test_shader_index.bat` and `tools/test_shader_preparation.bat` remain forwarding entry points; preparation selects `shaders pipeline`.

## CI and build boundaries

Importer, shader and pipeline workflows are independent, path-filtered checks for main pushes, pull requests and manual dispatch. Tags do not repeat these jobs. The runtime workflow is manual only and selects one of `storage`, `hid` or `startup`, with its own explicit generation/build steps. CMake test targets are excluded from the default build; request the needed targets explicitly.

Release packaging accepts a manual `release_tag` and checks out that existing tag. Changing the workflow on main does not change the tagged game sources or require retagging them.

Release packaging is separate from test CI. A build or fixture pass is not gameplay or visual acceptance. Passing checks should not be repeated or expanded without a new change, failure or unresolved concern. Avoid tests for reversible low-impact edits and tests that only mirror implementation details.
