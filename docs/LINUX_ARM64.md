# Linux AArch64 / ARM64 build

This is an **experimental source-build path**, not an official ARM64 release target. The project currently publishes Linux x86-64 artifacts; Microsoft also publishes Linux DXC binaries for x86-64, not AArch64.

The goal of this path is to make a native Linux AArch64 ELF reproducible while retaining both shader mechanisms used by the desktop runtime:

1. the portable Vulkan shader pack (`portable_vk.lospv`) for known shaders, and
2. an adjacent ARM64 `libdxcompiler.so` for cache misses and shaders first encountered in scenes that are not represented in the pack.

The portable pack remains the preferred fast path. Live DXC is the correctness fallback.

## Tested shape

The cross-build path was exercised with an x86-64 Ubuntu/WSL-style build host and a Linux AArch64 target. A ROCKNIX-style target uses Wayland/KMSDRM; other AArch64 Linux distributions may enable X11 instead.

Android is not part of this build path.

## Host prerequisites

At minimum the host needs:

- Clang/Clang++
- LLD
- CMake 3.28+
- Ninja
- Python 3.11+
- Git
- `gcc-aarch64-linux-gnu`, `g++-aarch64-linux-gnu`, and binutils for AArch64
- target development libraries/sysroot required by libcurl, Vulkan and the SDL backends selected for the target
- `clang-format` (tested with `clang-format-18`) when building DXC

On Debian/Ubuntu, use a dedicated cross-build environment if possible. Mixing x86-64 and arm64 `*-dev` packages in a long-lived workstation can create package conflicts.

## 1. Prepare the repository

Initialize submodules, provide your own game data, build the recompilation tools, and generate the PowerPC sources as described in [BUILDING.md](BUILDING.md).

The runtime compiles generated PPC sources directly, so there is no architecture-specific prebuilt PPC library to replace.

## 2. Build an AArch64 DXC shared library

Microsoft's Linux release archive is x86-64-only, so the ARM64 fallback compiler must be built from source.

Run:

```bash
./tools/build_dxc_linux_arm64.sh
```

The script defaults to the DXC revision validated with this path:

```text
9efbb6c3242cbb40c1844a2589171ff1c27cf956
```

It performs a split cross-build:

- host/NATIVE TableGen tools are built as x86-64 executables,
- `libdxcompiler.so` and the `dxc` CLI are built for Linux AArch64,
- SPIR-V code generation is enabled,
- the resulting shared library is staged at
  `tools/XenosRecomp/thirdparty/dxc-bin/lib/arm64/libdxcompiler.so`.

Override `DXC_REF`, `DXC_SOURCE_REPO`, `DXC_BUILD_DIR`, or `DXC_STAGE_DIR` if needed.

If `qemu-aarch64` is installed, the script also runs a small HLSL -> SPIR-V smoke test and verifies the SPIR-V magic `0x07230203`.

## 3. Build the AArch64 runtime

Generic AArch64 Linux:

```bash
./tools/build_linux_arm64.sh
```

For a ROCKNIX-style Wayland/KMSDRM target:

```bash
LO_ARM64_WSI=rocknix ./tools/build_linux_arm64.sh
```

The resulting ELF is:

```text
out/build/linux-arm64/LostOdysseyRecomp/LostOdysseyRecomp
```

When the ARM64 DXC staging file exists, CMake copies `libdxcompiler.so` next to the ELF.

Set `LO_ARM64_ALLOW_NO_DXC=1` only when intentionally testing a portable-pack-only runtime.

## 4. Portable shader pack plus live DXC

The portable Vulkan pack is deliberately independent of host install paths, host OS/CPU, and the local DXC binary hash. A valid pack produced on x86-64 can therefore be reused by an ARM64 Linux runtime when its contract matches.

Place the verified pack at:

```text
<runtime>/shaders/portable_vk.lospv
```

or set `LO_SHADER_PACK_PATH`.

For an x86-64 cross-build host, prefer verifying the pack with a native host
`LoShaderPackTool` and then copying it into the ARM64 runtime directory.
Passing `LO_PORTABLE_SHADER_PACK` directly to a cross configure may cause the
target AArch64 verification tool to be invoked on the x86-64 build host.

A complete ARM64 runtime directory should contain at least:

```text
LostOdysseyRecomp
libdxcompiler.so
shaders/portable_vk.lospv
```

At startup and during gameplay:

- pack hits use the precompiled SPIR-V directly,
- a pack miss can fall back to live DXC using the adjacent ARM64 `libdxcompiler.so`,
- `LO_DXC_PATH=/absolute/path/to/libdxcompiler.so` can force a specific compiler library.

This matters for scenes or effects that were not covered when the portable pack was produced. A zero-DXC startup is still desirable; live DXC is the fallback that prevents a missing shader from becoming a missing visual.

## 5. Verify architecture and loader requirements

Before copying to a target device:

```bash
file out/build/linux-arm64/LostOdysseyRecomp/LostOdysseyRecomp
file tools/XenosRecomp/thirdparty/dxc-bin/lib/arm64/libdxcompiler.so
aarch64-linux-gnu-readelf -d tools/XenosRecomp/thirdparty/dxc-bin/lib/arm64/libdxcompiler.so | grep NEEDED
aarch64-linux-gnu-readelf --version-info tools/XenosRecomp/thirdparty/dxc-bin/lib/arm64/libdxcompiler.so
```

Both ELF files should report AArch64. Check the highest required GLIBC/GLIBCXX versions against the target firmware before deployment.

## 6. WSI notes for handheld Linux

The generic toolchain does not force a window-system backend. `LO_ARM64_WSI=rocknix` configures vendored SDL with:

- X11 disabled,
- Wayland enabled,
- KMSDRM enabled.

Use the backend set that matches the target firmware. A working Vulkan device alone is not sufficient; SDL must also be able to create a Vulkan surface/swapchain through the active compositor/DRM path.

## 7. Runtime diagnostics

Useful checks:

- confirm logs show the Vulkan device and a created swapchain,
- verify there is no `dxcompiler library not available` warning when a live compile is needed,
- keep the portable pack enabled and inspect shader logs for misses/compiles,
- test at least one scene not visited while producing the shader pack.

This path is intended to make ARM64 development reproducible. It does not by itself establish an official ARM64 release, full-game compatibility, or support for every Linux handheld distribution.
