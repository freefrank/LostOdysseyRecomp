#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${LO_ARM64_BUILD_DIR:-$ROOT/out/build/linux-arm64}"
TARGET="${1:-LostOdysseyRecomp}"
DXC_SO="$ROOT/tools/XenosRecomp/thirdparty/dxc-bin/lib/arm64/libdxcompiler.so"

if [[ ! -f "$DXC_SO" && "${LO_ARM64_ALLOW_NO_DXC:-0}" != "1" ]]; then
    echo "ARM64 libdxcompiler.so is missing: $DXC_SO" >&2
    echo "Run tools/build_dxc_linux_arm64.sh first, or set LO_ARM64_ALLOW_NO_DXC=1" >&2
    echo "to build a portable-shader-pack-only runtime." >&2
    exit 2
fi

cmake_args=(
    -S "$ROOT"
    -B "$BUILD_DIR"
    -G Ninja
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/toolchains/linux-aarch64.cmake"
    -DLO_TARGET_ARCH=armv8-a
)

# ROCKNIX-style images commonly expose Wayland/KMSDRM but not X11 development
# libraries. Keep the generic build untouched unless explicitly requested.
if [[ "${LO_ARM64_WSI:-}" == "rocknix" ]]; then
    cmake_args+=(
        -DSDL_X11=OFF
        -DSDL_WAYLAND=ON
        -DSDL_KMSDRM=ON
    )
fi

cmake "${cmake_args[@]}"
cmake --build "$BUILD_DIR" --target "$TARGET" -j "${LO_ARM64_JOBS:-$(nproc)}"

runtime="$BUILD_DIR/LostOdysseyRecomp/$TARGET"
echo "Linux AArch64 build complete: $runtime"
if [[ -f "$BUILD_DIR/LostOdysseyRecomp/libdxcompiler.so" ]]; then
    echo "Bundled live DXC: $BUILD_DIR/LostOdysseyRecomp/libdxcompiler.so"
else
    echo "No adjacent live DXC was staged; portable shader pack hits can still avoid DXC."
fi
