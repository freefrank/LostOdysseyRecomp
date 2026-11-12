# Android ARM64 DXC Build (2026-10-02)

[简体中文](android-dxc-build-2026-10-02.zh-CN.md)

The runtime's HLSL→SPIR-V path needs `libdxcompiler.so`. The repository file
`tools/XenosRecomp/thirdparty/dxc-bin/lib/x64/libdxcompiler.so` is a desktop
x86-64 build and cannot be loaded into an Android ARM64 process. This script
builds an Android 26 / ARM64 shared library from the [Microsoft DXC v1.8.2407
source](https://github.com/microsoft/DirectXShaderCompiler/releases/tag/v1.8.2407).
The tag's commit is `416fab6b5c4ba956a320d9131102304da995edfc`. The existing
Windows `dxc.exe --version` reports `1.8.2407.7 (416fab6b5)`, so the same source
commit was chosen.

## Reproduction

On Linux/WSL, install the Android SDK's NDK `28.2.13676358`, CMake `3.22.1`,
and Ninja. Set `ANDROID_HOME`, then run from the repository root:

```sh
export ANDROID_HOME=/path/to/Android/Sdk
bash tools/android/build-dxc.sh
```

The source, submodules and incremental build are kept in
`${XDG_CACHE_HOME:-$HOME/.cache}/lostodysseyrecomp/android-dxc`. Set
`LO_DXC_CACHE_DIR=/path/to/cache` to change this. The default output is
`out/android-dxc/libdxcompiler.so`, which matches the default input of
`tools/android/build-runtime.sh`. Set `LO_DXC_OUTPUT=/path/to/libdxcompiler.so`
to change it. Set `LO_DXC_JOBS=4` to limit parallel compilation. An existing
output that passes the version, Android 26, ARM64 and exported symbol checks is
reused directly, without downloading or building again.

The script makes a shallow clone of `v1.8.2407`, deepens only 10 commits, and
checks that the upstream base `737a12a663f1697d3755a522d8fbf30481ecd2f6` in
`utils/version/latest-release.json` is exactly 7 commits before the tag.
It then builds with `HLSL_OFFICIAL_BUILD=ON` to produce `1.8.2407.7 (416fab6b)`.
With only a depth-1 history, the upstream default version generator reports
`1.8.0.1` by mistake. The script initializes the `DirectX-Headers`,
`SPIRV-Headers` and `SPIRV-Tools` submodules at the commits pinned by the tag.
It uses the NDK CMake toolchain, `ANDROID_ABI=arm64-v8a`,
`ANDROID_PLATFORM=android-26` and `ENABLE_SPIRV_CODEGEN=ON`, and builds only the
`dxcompiler` target. The [upstream CMake build notes](https://github.com/microsoft/DirectXShaderCompiler/blob/v1.8.2407/docs/BuildingAndTestingDXC.rst)
list the `PredefinedParams.cmake` presets. This script also turns off tests and
unrelated tools.

Upstream automatically creates a `NATIVE` sub-build, which runs tablegen on the
x86-64 host. Its default `LLVM_ENABLE_EH=OFF` makes the upstream LLVM Support
`try`/`throw` code fail to compile. Before building the Android target, the
script reconfigures `NATIVE` with `LLVM_ENABLE_EH=ON` and
`LLVM_ENABLE_RTTI=ON`. The upstream source sets CMake `CMP0051=OLD`, and the
local CMake 4.4 rejects that setting. For this reason the script pins the SDK
CMake 3.22.1, so no upstream source change is needed.

## Evidence and limits of this build

- The source commit and the abbreviated commit of the existing Windows binary
  match. `dxc-bin/inc/dxcapi.h` is identical, byte for byte, to
  `include/dxc/dxcapi.h` at this tag upstream.
- The `dxcompiler` target compiled and linked completely. With debug information
  removed, the output is about 34 MiB. Its SHA-256 in this build is
  `924fb3d8695676ed59ee1aaa2c73e1853f77972a8320824d93fe52bf77c2b6a8`.
- `file` identifies the output as an Android 26 ARM64 ELF built with NDK
  `28.2.13676358`. Its dynamic symbol table exports `DxcCreateInstance` and
  `DxcCreateInstance2`. Its dynamic dependencies are only `libdl.so`, `libm.so`
  and `libc.so`. The embedded version string is `dxcoob 1.8.2407.7 (416fab6b)`.

These are build and ELF static evidence. Still needed on a device: `dlopen`, a
DXC COM instance, an actual HLSL→SPIR-V compile, and a full game run. Successful
cross-compilation does not establish these.

## License

This output is a source build. Before distribution, check the APK or release
material and keep the `LICENSE.TXT` from the source used (LLVM/University of
Illinois/NCSA terms), `external/DirectX-Headers/LICENSE` (MIT),
`external/SPIRV-Headers/LICENSE` and `external/SPIRV-Tools/LICENSE`
(Apache-2.0), together with any third-party notices they require. The existing
`thirdparty/dxc-licenses/` in the repository records the source and license of
the official desktop binary archive. It cannot be assumed to cover all submodule
notices of this Android source build.
