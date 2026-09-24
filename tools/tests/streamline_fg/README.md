# Streamline FG and native NGX Vulkan coexistence probe

This is an **experimental, standalone Windows x64 Vulkan probe** for native NGX DLSS SR alongside Streamline DLSS FG. Its `CMakeLists.txt` is the entry point; the game's root CMake does not include it. The probe implements a small synthetic scene and FG off/on/resize and retirement sequences. It is not production FG integration, and **P0 Gate 1 has not passed**.

It needs the official Streamline v2.14.1 SDK described in `sdk-manifest.json`, a separate local NGX 310.9.1 SDK, and Visual Studio 2022 with CMake 3.28 or newer. Configure does not download either SDK. The build copies only the required Streamline runtime DLLs from the local SDK and the native NGX SR runtime from `LoDlss.cmake`; `sl.dlss.dll` must not be placed beside the executable. SDK archives, headers and binaries remain outside this source checkpoint.

From the repository root in PowerShell, after placing the SDKs in the local `.cache/deps` paths:

```powershell
$manifest = Get-Content tools/tests/streamline_fg/sdk-manifest.json -Raw | ConvertFrom-Json
$zip = '.cache/deps/streamline-v2.14.1/streamline-sdk-v2.14.1.zip'
if ((Get-Item $zip).Length -ne $manifest.asset_size -or
    (Get-FileHash $zip -Algorithm SHA256).Hash.ToLowerInvariant() -ne $manifest.asset_sha256) {
    throw 'Streamline SDK archive does not match sdk-manifest.json'
}
# Extract the verified archive so its include/ and bin/x64/ directories live under .cache/deps/streamline-v2.14.1/sdk/.
cmake -S tools/tests/streamline_fg -B out/streamline-fg-p0 -G 'Visual Studio 17 2022' -A x64 -DLO_STREAMLINE_SDK_ROOT="$PWD/.cache/deps/streamline-v2.14.1/sdk" -DLO_ENABLE_DLSS=ON -DLO_REQUIRE_DLSS=ON -DLO_DLSS_SDK_ROOT="$PWD/.cache/deps/nvidia-dlss-37495948"
cmake --build out/streamline-fg-p0 --config Release --target LoStreamlineFgProbe --parallel 2
```

The SDK manifest pins the release ZIP by size and SHA-256; the CMake helper also fails when its required headers or DLLs are missing. NGX SDK discovery and its native SR runtime are checked by `LoDlss.cmake`. This standalone build does not build the game or turn on an FG production path.

For a separately authorized manual hardware diagnostic, run `out/streamline-fg-p0/Release/LoStreamlineFgProbe.exe` from a dedicated results directory, or pass `--no-activate` for a nonactivating borderless window. The probe prints requested and actual client/surface extents. Exit code 0 means its bounded sequence completed on the tested hardware; 77 indicates an unavailable capability or environment (including an unfocused background window without generated presents); 1 indicates failure, including cleanup or Vulkan validation errors. A zero exit alone is not physical-display or P0 Gate 1 acceptance evidence. SDK state, hook counts and captured host images are not generated-frame captures.

Earlier local evidence in `out/streamline-fg-p0/` is limited to those specific builds and runs. The `gate1-codex-background-03` manifest records an older binary (`366657...`) with no generated presents. Foreground/background comparisons used another binary (`c13bdc...`): the foreground run reported `actual_presents=2` in FG-on intervals, but both runs exited 1 because validation reported layout/synchronization errors (`p0-foreground-sync-audit.md`). A later foreground API-dump run used binary SHA-256 `f47431b7746cdf54d847d7008547c0d7af0bce8a579f7596cccd5d95334c9efa` (`gate1-codex-apidump-01/manifest.json`), again reported generated intervals and exited 1. The captured first SDK pacer layout mismatch and R16F write hazard are documented in `p0-api-layout-trace.md` and `p0-host-remediation-followup.md`; their diagnosis does not validate the remaining hazards or demonstrate a host-side fix. None of these runs establishes a validation-clean generated frame or physical display. P0 Gate 1 remains **NOT PASSED**.
