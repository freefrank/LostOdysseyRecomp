# Building and running

[Overview](../README.md) · [Current status](STATUS.md)

## Prerequisites

The tested environment is Windows x64 with Direct3D 12 or the optional Vulkan backend, Visual Studio 2022 Build Tools and Windows SDK, LLVM clang-cl, CMake 3.28+, Ninja and Python 3.11+ (the code-generation guard uses the standard-library `tomllib` module). The runtime requires Clang; the `windows-msvc` preset is not a supported runtime alternative. Generated code uses AVX instructions. Windows 10 1803+ is required by the current memory-mapping path; this is not a guarantee for every GPU/driver combination.

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

After a pull that updates the tracked XenonRecomp patch, inspect and synchronize the *actual* modified `tools/XenonRecomp/` tree with the updated patch before rebuilding. Do not blindly reapply the patch on an already modified tree or discard unrelated local changes. Once the tree matches the intended patch, run the three commands above in order: rebuild the generator tools, regenerate PPC sources, then rebuild the runtime. A runtime-only incremental build or `ppc_codegen.py check` cannot establish that the actual dependency tree matches the tracked patch; stale `PPCTimeBase` code in a local XenonRecomp header can leave generated guest code on the old clock path despite a correct tracked patch. If `build_tools.bat` cannot handle a partially updated patched tree, reconcile that tree first rather than stamping an old generator binary.

`build_tools.bat` builds the generator and records a receipt containing the generator binary and source hashes. `ppc_codegen.py generate` verifies that receipt, hashes the TOML and generator inputs before and after execution, writes an output manifest for the generated C++/header files and rejects obsolete 64-bit jump-table switches. Use `python -B tools/ppc_codegen.py check` to verify an existing generated tree without regenerating it; if the inputs or outputs changed, regenerate from the repository root. Configured runtime builds also run `LoPpcCodegenCheck` as an order dependency before compiling guest objects. See [recompilation notes](notes/recomp.md) for function boundaries and switch-table maintenance. These commands describe the checked-in scripts; a new-machine end-to-end bootstrap has not been retested as part of this documentation update.

### PowerPC recompilation from source

All platforms compile recompiled PowerPC guest code directly from generated sources in `LostOdysseyRecompLib/ppc/`. The build does not depend on prebuilt static libraries or remote PPC synchronization, ensuring clean provenance, reproducible multi-platform builds, and forward compatibility with additional architectures (such as ARM64).

After generating the guest sources with `tools/ppc_codegen.py generate`, simply configure CMake and build the target. CMake enforces that `LostOdysseyRecompLib/ppc/` contains valid recompiled code and runs `LoPpcCodegenCheck` to verify input/output consistency.

Audio configuration fetches the pinned Xenia FFmpeg source via CMake FetchContent, so first configuration needs network access. See [ffmpeg.cmake](../thirdparty/ffmpeg.cmake) and its [license](../thirdparty/ffmpeg-LICENSE.txt). This is a frame-level XMAFRAMES decoder, not a system FFmpeg executable requirement.

Release builds do not require a separately installed Vulkan SDK. Windows Vulkan headers, volk and VMA come from the patched plume submodule; the GPU driver supplies `vulkan-1.dll` and its ICD. The runtime requests Vulkan 1.2, buffer-device-address, geometry shaders and Win32 WSI. Use the exact paired DXC v1.8.2407 DLLs copied by CMake and tracked in [DXC provenance](../thirdparty/dxc-licenses/PROVENANCE.json); do not substitute one DLL independently. Release packaging ships the single `LostOdysseyRecomp.exe` binary; the updater and importer run from that binary.

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

SDL2 build dependencies are already vendored in the repository tree.

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

CMake automatically copies the Linux DXC shared library from `tools/XenosRecomp/thirdparty/dxc-bin/lib/x64/libdxcompiler.so` into the output folder next to the `LostOdysseyRecomp` ELF during build. If you need a custom DXC location, set the `LO_DXC_PATH` environment variable before running.

### Packaging AppImage

Linux releases can package an AppImage using `tools/package_appimage.py` with `linuxdeploy`:

```bash
python3 tools/package_appimage.py --build out/build/linux-clang --output out/releases --linuxdeploy /path/to/linuxdeploy
```

The tool stages the executable, icons, desktop entry, metainfo, vendored DXC library and licenses into an AppDir layout and produces `LostOdysseyRecomp-linux-x64-<tag>.AppImage`.

### Packaging Flatpak

The repository provides an automated offline Flatpak packaging tool `tools/package_flatpak.py` using manifest template `packaging/linux/io.github.freefrank.LostOdysseyRecomp.json`. The package targets the `org.freedesktop.Platform 26.08` runtime and SDK with Clang/LLVM 22 (`org.freedesktop.Sdk.Extension.llvm22`).

#### Prerequisites

Install the required Freedesktop 26.08 platform, SDK, and LLVM 22 extension from Flathub:

```bash
flatpak --system install flathub \
  org.freedesktop.Platform//26.08 \
  org.freedesktop.Sdk//26.08 \
  org.freedesktop.Sdk.Extension.llvm22//26.08
```

Ensure `flatpak` and `flatpak-builder` are available on the host system.

#### Staged offline packaging

Packaging runs with network access unshared (`--unshare=network`). `tools/package_flatpak.py` prepares an isolated build directory, staging Git-tracked files, submodules, generated PPC translation sources, private disc assets, pinned dependencies, and prebuilt shaders:

```bash
python3 -B tools/package_flatpak.py \
  --source . \
  --output out/flatpak-build \
  --ppc LostOdysseyRecompLib/ppc \
  --codegen-manifest LostOdysseyRecompLib/ppc/codegen-manifest.json \
  --default-xex LostOdysseyRecompLib/private/disc1/default.xex \
  --image-disc1 LostOdysseyRecompLib/private/image_disc1.bin \
  --image-sym LostOdysseyRecompLib/private/image_disc1.bin.sym \
  --ngx-sdk out/deps/nvidia-dlss \
  --fsr-sdk out/deps/fidelityfx-sdk \
  --fsr-shaders out/fsr-shaders-vk \
  --shader-pack out/build/linux/shaders/portable_vk.lospv \
  --ffmpeg-source out/deps/ffmpeg-flatpak \
  --zstd-source out/deps/zstd-flatpak
```

The script builds inside the sandbox, finishes the app permissions, validates the install tree (ensuring required libraries and licenses are present while private assets and sources are excluded), exports an OSTree repository, and creates a standalone `.flatpak` bundle alongside SHA-256 and `source.json` manifests.

#### Standalone bundle installation and update limits

Install the generated bundle locally:

```bash
flatpak --user install --bundle out/flatpak-build/LostOdysseyRecomp-v<version>-<commit>-dev.flatpak
```

Standalone bundles installed directly from `.flatpak` files do not attach an OSTree remote repository and cannot receive updates via `flatpak update`. Upgrading a local installation requires installing a newly generated `.flatpak` bundle. Regular updates will be available once published via an OSTree remote or Flathub (submission pending).

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
| `LO_BACKGROUND=1` | Hidden rendering window; background audio is muted by default. |
| `LO_DEBUG_MENU_OPEN=1` | Open the Windows debug panel at startup for UI validation; default is closed. |
| `LO_HEADLESS=1` | No video device/window; not equivalent to hidden rendering. |
| `LO_AUDIO_MUTE=1` | Mute device output. |
| `LO_AUDIO_CAPTURE=<path>` | Up to 60 seconds of raw 48kHz stereo float PCM before mute. |
| `LO_CONTROLLER_RUMBLE=1` | Enable controller rumble; default is off. |
| `LO_GRAPHICS_API=d3d12\|vulkan` | Override the persisted `graphics_backend` choice for one launch; unset/`auto` uses the saved choice. |

Clear test-only environment variables before manual play. Do not treat a window staying open, a heartbeat, or nonzero PCM as proof a scene is correct.

The shader compiler uses the paired `dxcompiler.dll`/`dxil.dll` copied beside the runtime. Custom development builds must preserve the v1.8.2407 pair and its license/provenance checks; the Windows SDK fallback is not the tested packaging contract.

Generated baseline mappings, branch targets, import listings and Ghidra exports are local analysis artifacts. They are ignored; regenerate them from your own data when extending the recompiler configuration. Checked-in TOML and manual boundary/switch overrides remain the build inputs.

## Verification

Use `tools\test.bat --list` to select checks, then run only the relevant suites, for example `tools\test.bat shaders pipeline`. Runtime suites use an existing CMake build root (`--build-dir out/build/release` by default), with their target built explicitly; the runner does not implicitly build the game. See the [test guide](../tools/tests/README.md) for per-suite build commands, prerequisites and CI separation.

[Rendering tests](notes/rendering-validation.md) cover memory aliases, shader ALU, stencil and texture layout. [Audio notes](notes/audio-output.md) cover `LoXmaLoopTest`; [storage notes](notes/save-storage.md) describe `LoStorageTest`. Some investigation targets and input hooks remain local changes; consult [status](STATUS.md) before expecting them in a clean checkout.

The [selected native targets](../tools/tests/README.md#selected-native-targets) are excluded from the default build. Build only the target needed for the change, for example `cmake --build out/build/release --target LoVulkanBackendTest`, then run it explicitly. Vulkan fixtures use the installed driver's loader and an isolated working directory; they do not establish broad GPU compatibility or gameplay correctness. The updater helper and probe targets are host-side checks and do not require guest generation.
