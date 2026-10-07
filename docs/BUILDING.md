# Building and running

[Overview](../README.md) · [Current status](STATUS.md)

## Prerequisites

The tested environment is Windows x64 with Direct3D 12 or the optional Vulkan backend, Visual Studio 2022 Build Tools and Windows SDK, LLVM clang-cl, CMake 3.28+, Ninja and Python 3.11+ (the generation wrapper uses the standard-library `tomllib` module). The runtime requires Clang; the `windows-msvc` preset is not a supported runtime alternative. Generated code uses AVX instructions. Windows 10 1803+ is required by the current memory-mapping path; this is not a guarantee for every GPU/driver combination.

The scripts [build_runtime.bat](../tools/build_runtime.bat), [build_tools.bat](../tools/build_tools.bat) and [CMakePresets.json](../CMakePresets.json) discover Visual Studio with `vswhere` and resolve tools from `PATH`. Use `LO_VCVARS64` or `LLVM_ROOT` for custom installations. Keep personal overrides in the ignored `CMakeUserPresets.json`.

## Game data and dependencies

1. Supply your own extracted Asian multilingual edition, matching the [XEX details](notes/xex.md). Place Disc 1 at `LostOdysseyRecompLib/private/disc1/`, including `default.xex`, `LO.fpi` and its resource archives. Four-disc integration is not complete.
2. Initialize submodules and apply the [project patches](../tools/patches/README.md). Do not reapply patches over a modified dependency tree.
3. Build the generator tools, then generate local PowerPC sources. Generated code and game data are ignored by Git.

From the repository root, after preparing dependencies and data:

```powershell
.\tools\build_tools.bat
python -B tools/ppc_codegen.py generate
.\tools\build_runtime.bat
```

After a pull that updates the tracked XenonRecomp patch, inspect and synchronize the *actual* modified `tools/XenonRecomp/` tree with the updated patch before rebuilding. Do not blindly reapply patches over a modified dependency tree or discard unrelated local changes. Once the tree matches the intended patch, rebuild the generator, explicitly regenerate PPC sources, then rebuild the runtime. If `build_tools.bat` cannot handle a partially updated patched tree, reconcile that tree first.

`build_tools.bat` builds the generator. `ppc_codegen.py generate` generates the guest sources and performs basic output and failure handling; a failed generation preserves the previous output tree. Use `python -B tools/ppc_codegen.py generate` after generator or configuration changes. Runtime builds consume the generated sources directly without a per-build repository scan, manifest, stamp or hash gate. See [recompilation notes](notes/recomp.md) for function boundaries and switch-table maintenance.

### PowerPC recompilation from source

All platforms compile recompiled PowerPC guest code directly from generated sources in `LostOdysseyRecompLib/ppc/`. The build does not depend on prebuilt static libraries or remote PPC synchronization, ensuring clean provenance, reproducible multi-platform builds, and forward compatibility with additional architectures (such as ARM64).

After generating the guest sources with `tools/ppc_codegen.py generate`, simply configure CMake and build the target. CMake compiles the generated sources directly; regenerate explicitly when the generator or its configuration changes.

Audio configuration fetches the pinned Xenia FFmpeg source via CMake FetchContent, so first configuration needs network access. See [ffmpeg.cmake](../thirdparty/ffmpeg.cmake) and its [license](../thirdparty/ffmpeg-LICENSE.txt). This is a frame-level XMAFRAMES decoder, not a system FFmpeg executable requirement.

Release builds do not require a separately installed Vulkan SDK. Windows Vulkan headers, volk and VMA come from the patched plume submodule; the GPU driver supplies `vulkan-1.dll` and its ICD. The runtime requests Vulkan 1.2, buffer-device-address, geometry shaders and Win32 WSI. Keep the paired DXC v1.8.2407 DLLs together with their license files; do not substitute one DLL independently. Release packaging ships the single `LostOdysseyRecomp.exe` binary; the updater and importer run from that binary.

### Windows Direct3D 12 DLSS and FSR development paths

The local Windows Direct3D 12 paths for DLSS SR/DLAA and FSR 3.1 are enabled
by the normal Clang runtime build and have bounded build, fixture and Uhra
validation; broader game coverage remains experimental. DLSS uses the local NGX SDK root:

```powershell
cmake -S . -B out/build/d3d12-upscalers -G Ninja `
  -DLO_ENABLE_DLSS=ON -DLO_REQUIRE_DLSS=ON `
  -DLO_DLSS_SDK_ROOT='C:\path\to\nvidia-dlss-sdk'
```

FSR D3D12 additionally requires offline DXIL inputs. Generate the pinned FSR
shader headers and adapter conversion headers with the repository tools, then
provide the resulting directories through `LO_FSR_DX12_SHADER_DIR` and
`LO_FSR_DX12_ADAPTER_DIR`:

```powershell
python tools/fsr/generate_dx12_shaders.py `
  --sdk 'C:\path\to\fidelityfx-sdk' `
  --dxc-dir 'C:\path\to\dxc' `
  --output 'C:\path\to\fsr-dx12-shaders'
python tools/fsr/prepare_adapter_shaders_dx12.py `
  --dxc 'C:\path\to\dxc\dxc.exe' `
  --output 'C:\path\to\fsr-dx12-adapter'
```

```powershell
cmake -S . -B out/build/d3d12-upscalers -G Ninja `
  -DLO_ENABLE_FSR=ON -DLO_REQUIRE_FSR=ON `
  -DLO_FSR_SDK_ROOT='C:\path\to\fidelityfx-sdk' `
  -DLO_FSR_SHADER_DIR='C:\path\to\fsr-vulkan-shaders' `
  -DLO_FSR_DX12_SHADER_DIR='C:\path\to\fsr-dx12-shaders' `
  -DLO_FSR_DX12_ADAPTER_DIR='C:\path\to\fsr-dx12-adapter'
```

The checked local D3D12 evidence now includes a full Windows Clang build,
RTX 5080 DLSS Quality/DLAA and FSR Quality/Native AA fixture readback, hybrid
motion checks (D3D12 3527 / Vulkan 3415), and 11 DXIL/SPIR-V motion shader
compilation checks. It does not establish broad game-scene rendering,
performance, or player acceptance. Offline
shader generation belongs to the build pipeline; it is not a player runtime
step. Linux continues to use the Vulkan FSR path and does not require these
D3D12 directories.

### Windows Direct3D 12 Intel XeSS development path

XeSS Super Resolution (`LO_ENABLE_XESS`) and XeSS frame generation with Xe Low
Latency (`LO_ENABLE_XESS_FG`) are optional Direct3D 12 providers. Point
`LO_XESS_SDK_ROOT` at an extracted [Intel XeSS SDK](https://github.com/intel/xess)
3.x release (checked with 3.0.2) containing `inc/`, `bin/` and `LICENSE.txt`. The
FG adapter uses the multi-frame XeSS-FG API, which 2.x does not have. No shaders are generated
and no import library is linked: the build stages `libxess.dll` (SR) or
`libxess_fg.dll` and `libxell.dll` (FG) beside the executable together with
`licenses/LICENSE-XeSS.txt`, and the runtime loads them on demand.

```powershell
cmake -S . -B out/build/d3d12-upscalers -G Ninja `
  -DLO_ENABLE_XESS=ON -DLO_ENABLE_XESS_FG=ON `
  -DLO_XESS_SDK_ROOT='C:\path\to\xess-sdk'
```

`tools/build_release.bat` forwards the same three variables from the
environment. XeSS SR appears as *XeSS* under *Anti-aliasing / Upscaling* and
uses the FSR quality IDs mapped to the same-named XeSS presets (Quality 1.7×,
Balanced 2.0×, Performance 2.3×, Native AA). XeSS frame generation appears as
*XeSS* under *Frame generation* with a fixed 2× multiplier, like FSR. Both require the D3D12 backend; on
Vulkan, or without the DLLs, the menu reports XeSS as unavailable and normal
rendering is kept. `LO_XESS_RUNTIME_PATH` (a `libxess.dll` path) and
`LO_XESS_FG_RUNTIME_PATH` (a directory) override the DLL lookup;
`LO_XESS_JITTER_SCALE=x,y` and `LO_XESS_MV_SCALE=x,y` feed the SDK's jitter and
motion-vector scale for on-hardware convention checks; `LO_FG_PROVIDER=xess`
selects XeSS FG like the other providers.

## Building on Linux

Building the native Linux ELF works on Linux distributions (such as Ubuntu or Manjaro) or under WSL2.

### Linux host prerequisites

- Clang / Clang++ (LLVM toolchain)
- LLD linker
- Ninja
- CMake 3.28+
- Python 3.11+ (needs standard-library `tomllib`)
- Vulkan loader and Mesa (or another Vulkan ICD compatible with your hardware)
- libcurl development package (`libcurl4-openssl-dev` on Debian/Ubuntu) for updater HTTP support on UNIX
- Typical C++ build development packages
- Running `vulkaninfo` is useful for verifying your driver setup, though not strictly required by CMake

SDL 3.4.18 build dependencies are already vendored in the repository tree.

### Linux PowerPC source generation

Linux compiles generated PowerPC source code directly from `LostOdysseyRecompLib/ppc/` using Clang. Release CI for Linux generates PPC sources from repository tools and inputs on Ubuntu 24.04 before building.

If `LostOdysseyRecompLib/ppc/` is empty or missing, generate the sources from the repository root:

```bash
# Build XenonRecomp generator tools if not already present
cmake -B out/tools -S tools/XenonRecomp -G Ninja
cmake --build out/tools

# Generate PowerPC sources
python3 -B tools/ppc_codegen.py generate
```

### Configure and build

Configure and compile using the `linux-clang` preset:

```bash
cmake --preset linux-clang
cmake --build --preset linux-clang
```

The resulting executable is written to:

```bash
out/build/linux-clang/LostOdysseyRecomp/LostOdysseyRecomp
```

### DXC shared library on Linux

CMake selects the bundled Linux DXC shared library by target architecture: `lib/x64/libdxcompiler.so` for x86-64 and `lib/arm64/libdxcompiler.so` for AArch64. If you need a custom DXC location, set the `LO_DXC_PATH` environment variable before running.

For the experimental Linux AArch64 cross-build path, including building an ARM64 `libdxcompiler.so` and combining live DXC fallback with the portable Vulkan shader pack, see [Linux AArch64 build notes](LINUX_ARM64.md).

### Packaging AppImage

Linux releases can package an AppImage using `tools/package_appimage.py` with `linuxdeploy`:

```bash
python3 tools/package_appimage.py --build out/build/linux-clang --output out/releases --appdir out/releases/linux.AppDir --linuxdeploy /path/to/linuxdeploy
```

The tool stages the executable, icons, desktop entry, metainfo, vendored DXC library and licenses into an AppDir layout and produces `LostOdysseyRecomp-linux-x64-<tag>.AppImage`.

The release workflow keeps this AppDir for the Linux packaging job. Its
Flatpak export reuses the already packaged `usr` tree from the persistent
AppDir instead of compiling the source a second time. The stable bundle export
and isolated user installation/sandbox shell checks passed. Release CI [36500844014](https://github.com/freefrank/LostOdysseyRecomp/actions/runs/36500844014)
ran this workflow successfully for v0.7.15; the published package facts and
validation limits are recorded in [development status](STATUS.md).

### Packaging Flatpak

The repository provides `tools/package_flatpak.py` to export a Flatpak from an existing AppImage AppDir using the manifest metadata in `packaging/linux/io.github.freefrank.LostOdysseyRecomp.json`. The AppDir is the only binary input; this exporter does not compile source code or use `flatpak-builder`.

#### Prerequisites

Install the required Freedesktop 26.08 platform runtime and SDK from Flathub:

```bash
flatpak --system install flathub \
  org.freedesktop.Platform//26.08 \
  org.freedesktop.Sdk//26.08
```

Ensure `flatpak` is available on the host system.

#### AppDir export

Export a stable release bundle when the AppDir runtime version matches the
checkout source version:

```bash
python3 -B tools/package_flatpak.py \
  --appdir out/releases/linux.AppDir \
  --output out/releases/flatpak-v0.7.3 \
  --version v0.7.3
```

For a development bundle, omit `--version`; the exporter uses a `dev` branch
name and emits a commit-suffixed development filename. It validates the
payload tree and copies the AppDir `usr` tree into a Flatpak runtime. It runs
the runtime dependency probe; no runtime-wide digest gate is required.
The output may contain internal source records for debugging; these are not
required public Release attachments or runtime gates.

#### Standalone bundle installation and update limits

Because standalone `.flatpak` bundles do not embed a remote runtime repository URL, ensure the required `org.freedesktop.Platform 26.08` runtime is installed before installing the bundle:

```bash
flatpak --system install flathub org.freedesktop.Platform//26.08
```

Install the published stable release bundle:

```bash
flatpak --user install --bundle LostOdysseyRecomp-linux-x64-v0.7.2.flatpak
```

Or install a locally packaged development bundle:

```bash
flatpak --user install --bundle out/flatpak-build/LostOdysseyRecomp-v<version>-<commit>-dev.flatpak
```

Launch the installed sandbox application:

```bash
flatpak run io.github.freefrank.LostOdysseyRecomp
```

Standalone bundles installed directly from `.flatpak` files do not configure an OSTree remote repository and cannot receive updates via `flatpak update`. Upgrading a local installation requires installing a newly downloaded or generated `.flatpak` bundle. Automatic updates will be available once published via an OSTree remote or Flathub (submission pending).

## Building on macOS

Experimental, Apple Silicon only (arm64). The runtime renders through plume's Metal backend; shaders are compiled to SPIR-V by DXC as on Vulkan and translated to MSL at runtime with SPIRV-Cross (`thirdparty/SPIRV-Cross`). CMake rejects the DLSS, FSR and Streamline options (`LO_ENABLE_DLSS`, `LO_ENABLE_FSR`, `LO_ENABLE_STREAMLINE_FG`) and the Windows-only DLSS and FSR frame-generation adapters at configure time on macOS. MetalFX frame generation (fixed 2×) is built by default (`LO_ENABLE_METALFX_FG`): its MetalFX calls compile only with the macOS 26 SDK, it needs macOS 26 and a supported GPU at run time, and it is experimental and has not been run on Mac hardware ([technical note](notes/vulkan-fg-fsr4-metalfx.md#metalfx-fg-on-the-existing-macos-port)). v0.7.35 is the first release with a macOS package, followed by v0.8.0 and v0.8.5: an ad-hoc signed disk image that is not notarized and is built on a Mac ([macOS releases](MACOS_RELEASE.md)). CI cannot link the complete runtime without private game data.

### Prerequisites

- macOS 15 or later on Apple Silicon
- Xcode (Apple Clang and the Metal toolchain; set `DEVELOPER_DIR` to select a specific Xcode)
- CMake 3.28+ and Ninja (`brew install cmake ninja`)
- Python 3.11+

### Dependencies and patches

```bash
git submodule update --init --recursive
git -C tools/XenonRecomp apply ../patches/XenonRecomp-lostodyssey.patch
git -C thirdparty/plume apply ../../tools/patches/plume-lostodyssey.patch
git -C thirdparty/plume apply ../../tools/patches/plume-sdl3.patch
git -C thirdparty/plume apply ../../tools/patches/plume-macos.patch
```

`plume-macos.patch` applies after the upstream and SDL3 plume patches; see [tools/patches/README.md](../tools/patches/README.md#macos-plume-metal-patch).

### Generate and build

Place Disc 1 at `LostOdysseyRecompLib/private/disc1/`, then:

```bash
cmake -S tools/xexdump -B out/build/tools -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build out/build/tools --target XenonRecomp XenonAnalyse xexdump
out/build/tools/xexdump LostOdysseyRecompLib/private/disc1/default.xex LostOdysseyRecompLib/private/image_disc1.bin
python3 -B tools/ppc_codegen.py generate
cmake -S . -B out/build/macos-gpu -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build out/build/macos-gpu --target LostOdysseyRecomp
```

The build copies the universal `libdxcompiler.dylib` from `tools/XenosRecomp/thirdparty/dxc-bin` beside the executable. By default, macOS builds compile pinned zstd sources with the same deployment target. To use an installed static zstd, set `LO_PACK_FETCH_ZSTD=OFF` and check that it supports the deployment target. The target is macOS 15.0, the version the bundled `libdxcompiler.dylib` is built for; configuring a build directory that cached an older target raises it to 15.0 ([details](MACOS_RELEASE.md#minimum-macos-version)). Hardware validation is recorded separately in [development status](STATUS.md).

### Run

```bash
cd out/build/macos-gpu/LostOdysseyRecomp
./LostOdysseyRecomp --game ../../../../LostOdysseyRecompLib/private/disc1
```

A writable build folder uses the portable layout (settings, saves, cache and logs beside the executable). An `.app` bundle never does; it uses `~/Library/Application Support/LostOdysseyRecomp` and writes logs to `~/Library/Logs/LostOdysseyRecomp/logs/`.

### Package

```bash
python3 -B tools/package_macos.py         # out/releases/LostOdysseyRecomp-macos-arm64-v<version>-<commit>-dev.zip
python3 -B tools/package_macos.py --dmg   # the same name ending in .dmg
```

`--dmg` writes a compressed disk image that holds `LostOdysseyRecomp.app` and a link to `/Applications`, instead of the ZIP. Without `--identity` the `.app` is ad-hoc signed and runs on the building Mac. macOS blocks the first launch of a downloaded app that is not notarized until the player approves it ([steps](INSTALLING.md#macos)); a Developer ID signature with notarization would remove that step ([signed release workflow](MACOS_RELEASE.md#signed-release-workflow)).

v0.7.35, v0.8.0 and v0.8.5 ship an ad-hoc signed, not notarized disk image. It is built on a Mac and uploaded to the release by hand, because CI cannot link the runtime without game data; the release workflow's publish step accepts it as one optional asset next to the packages built by CI (Windows ZIP, AppImage, Flatpak and, from v0.8.5, the Android APK). [macOS releases](MACOS_RELEASE.md#disk-image-v085) has the release command, the asset name the in-game updater looks for and the validation boundary.

## Launch with a consistent working directory

```powershell
$gameData = (Resolve-Path .\LostOdysseyRecompLib\private\disc1).Path
Push-Location .\out\build\windows-clang\LostOdysseyRecomp
.\LostOdysseyRecomp.exe --game $gameData --quiet-kernel
Pop-Location
```

With explicit `--game`, `save/`, `profile/`, `cache/` and default `logs/` remain relative to the
process working directory, allowing isolated regression runs. Launching without `--game`
first selects the executable directory, then resolves `game-path.txt` or the adjacent `game`
folder. A fresh installation opens initial settings before guest startup. `LO_PROFILE_DIR`
overrides the profile location. Back up saves before testing; use independent save/profile copies.

When launching with `--game`, start from the ELF's directory. Explicit game candidates skip the
executable-directory `chdir`, so the working directory controls relative saves, profiles, caches
and logs.

| Setting | Effect |
|---|---|
| `LO_LOG_FILE=<path>` | Append logs to a selected file; `0` disables the duplicate file sink. Default: a separate timestamped file under `logs/`. |
| `LO_LOG_DIR=<folder>` | Folder for the default timestamped logs instead of `logs/`; the Android app sets it to its external `files/logs/`. `LO_LOG_FILE` takes precedence. |
| `LO_BACKGROUND=1` | Hidden rendering window; background audio is muted by default. |
| `LO_HEADLESS=1` | No video device/window; not equivalent to hidden rendering. |
| `LO_AUDIO_MUTE=1` | Mute device output. |
| `LO_AUDIO_CAPTURE=<path>` | Up to 60 seconds of raw 48kHz stereo float PCM before mute. |
| `LO_CONTROLLER_RUMBLE=0` | Disable host controller rumble; it is enabled by default. Physical device support remains driver/controller dependent. |
| `LO_GRAPHICS_API=d3d12\|vulkan` | Override the persisted `graphics_backend` choice for one launch on Windows (`dx12` is accepted for `d3d12`); unset/`auto` uses the saved choice. Linux always uses Vulkan and macOS Metal. |

Clear test-only environment variables before manual play. Do not treat a window staying open, a heartbeat, or nonzero PCM as proof a scene is correct.

The shader compiler uses the paired `dxcompiler.dll`/`dxil.dll` copied beside the runtime. Custom development builds must preserve the v1.8.2407 pair and its license files; the Windows SDK fallback is not the tested packaging contract.

Generated baseline mappings, branch targets, import listings and Ghidra exports are local analysis artifacts. They are ignored; regenerate them from your own data when extending the recompiler configuration. Checked-in TOML and manual boundary/switch overrides remain the build inputs.

## Verification

Use `tools\test.bat --list` to select checks, then run only the relevant suites, for example `tools\test.bat shaders pipeline`. Runtime suites use an existing CMake build root (`--build-dir out/build/release` by default), with their target built explicitly; the runner does not implicitly build the game. See the [test guide](../tools/tests/README.md) for per-suite build commands, prerequisites and CI separation. The FG contract, FG game integration compile, reusable FG and review-regression pull request checks run on Gitea Actions; see [Pull request checks and releases on Gitea](notes/ci-gitea.md).

[Rendering tests](notes/rendering-validation.md) cover memory aliases, shader ALU, stencil and texture layout. [Audio notes](notes/audio-output.md) cover `LoXmaLoopTest`; [storage notes](notes/save-storage.md) describe `LoStorageTest`. Some investigation targets and input hooks remain local changes; consult [status](STATUS.md) before expecting them in a clean checkout.

The [selected native targets](../tools/tests/README.md#selected-native-targets) are excluded from the default build. Build only the target needed for the change, for example `cmake --build out/build/release --target LoVulkanBackendTest`, then run it explicitly. Vulkan fixtures use the installed driver's loader and an isolated working directory; they do not establish broad GPU compatibility or gameplay correctness. The updater helper and probe targets are host-side checks and do not require guest generation.
