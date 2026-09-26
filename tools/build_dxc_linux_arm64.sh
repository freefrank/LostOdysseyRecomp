#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DXC_REF="${DXC_REF:-9efbb6c3242cbb40c1844a2589171ff1c27cf956}"
CACHE_REPO="${DXC_SOURCE_REPO:-$ROOT/out/thirdparty/DirectXShaderCompiler}"
WORK_SRC="${DXC_WORK_SRC:-$ROOT/out/thirdparty/DirectXShaderCompiler-arm64-src}"
BUILD_DIR="${DXC_BUILD_DIR:-$ROOT/out/build/dxc-linux-arm64}"
STAGE_DIR="${DXC_STAGE_DIR:-$ROOT/tools/XenosRecomp/thirdparty/dxc-bin/lib/arm64}"
TOOLCHAIN="$ROOT/cmake/toolchains/dxc-linux-aarch64.cmake"

for tool in git cmake ninja python3 aarch64-linux-gnu-gcc aarch64-linux-gnu-g++ gcc g++; do
    command -v "$tool" >/dev/null || { echo "Missing required tool: $tool" >&2; exit 2; }
done

CLANG_FORMAT="$(command -v clang-format-18 || command -v clang-format || true)"
if [[ -z "$CLANG_FORMAT" ]]; then
    echo "clang-format (tested with clang-format-18) is required by this DXC revision" >&2
    exit 2
fi

if [[ ! -d "$CACHE_REPO/.git" ]]; then
    mkdir -p "$(dirname "$CACHE_REPO")"
    git clone --filter=blob:none https://github.com/microsoft/DirectXShaderCompiler.git "$CACHE_REPO"
fi

git -C "$CACHE_REPO" fetch --depth 1 origin "$DXC_REF"
git -C "$CACHE_REPO" worktree remove --force "$WORK_SRC" >/dev/null 2>&1 || true
rm -rf "$WORK_SRC"
git -C "$CACHE_REPO" worktree add --detach "$WORK_SRC" "$DXC_REF"

# Apply the small compatibility fixes required by the validated DXC revision
# in the detached worktree only. The cached source checkout remains untouched.
python3 - "$WORK_SRC" "$CLANG_FORMAT" <<'PY'
from pathlib import Path
import sys

root = Path(sys.argv[1])
clang_format = sys.argv[2]

# HLSL extends ASTContext::getFunctionType with ParamMods. Two ObjC rewriter
# call sites in this revision still use the older three-argument form.
for rel in (
    "tools/clang/lib/Frontend/Rewrite/RewriteObjC.cpp",
    "tools/clang/lib/Frontend/Rewrite/RewriteModernObjC.cpp",
):
    p = root / rel
    text = p.read_text()
    old = "return Context->getFunctionType(result, args, fpi);"
    new = "return Context->getFunctionType(result, args, fpi, None);"
    if old in text:
        p.write_text(text.replace(old, new))
    elif new not in text:
        raise SystemExit(f"unexpected DXC source at {rel}")

# DXC's nested NATIVE configure must build tools for the x86-64 host, not for
# the AArch64 target. Pass host compilers and the same EH/RTTI settings used by
# the validated Linux build if the nested configure is regenerated.
p = root / "cmake/modules/CrossCompile.cmake"
text = p.read_text()
needle = "function(llvm_create_cross_target_internal target_name toochain buildtype)\n"
insert = """function(llvm_create_cross_target_internal target_name toochain buildtype)

  set(LO_NATIVE_HOST_FLAGS)
  if("${target_name}" STREQUAL "NATIVE" AND UNIX AND CMAKE_CROSSCOMPILING)
    set(LO_NATIVE_HOST_FLAGS
        -DCMAKE_C_COMPILER=/usr/bin/gcc
        -DCMAKE_CXX_COMPILER=/usr/bin/g++
        -DCMAKE_AR=/usr/bin/ar
        -DCMAKE_RANLIB=/usr/bin/ranlib
        -DLLVM_ENABLE_EH=ON
        -DLLVM_ENABLE_RTTI=ON
        -DLLVM_INCLUDE_TESTS=OFF
        -DLLVM_INCLUDE_DOCS=OFF
        -DLLVM_INCLUDE_EXAMPLES=OFF
        -DHLSL_INCLUDE_TESTS=OFF
        -DHLSL_OPTIONAL_PROJS_IN_DEFAULT=OFF
        -DHLSL_COPY_GENERATED_SOURCES=1
        -DLLVM_TARGETS_TO_BUILD=None
        -DCLANG_FORMAT_EXE=""" + clang_format + """)
  endif()
"""
if "set(LO_NATIVE_HOST_FLAGS)" not in text:
    if needle not in text:
        raise SystemExit("unexpected DXC CrossCompile.cmake")
    text = text.replace(needle, insert, 1)
    text = text.replace(
        '        ${CROSS_TOOLCHAIN_FLAGS_${target_name}} ${CMAKE_SOURCE_DIR}',
        '        ${LO_NATIVE_HOST_FLAGS}\n        ${CROSS_TOOLCHAIN_FLAGS_${target_name}} ${CMAKE_SOURCE_DIR}'
    )
p.write_text(text)
PY

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR/NATIVE"

# DXC's cross build uses host TableGen tools. Configure those explicitly for
# x86-64 before the outer AArch64 configure so they are executable on the host.
native_args=(
    -S "$WORK_SRC"
    -B "$BUILD_DIR/NATIVE"
    -G Ninja
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_C_COMPILER=/usr/bin/gcc
    -DCMAKE_CXX_COMPILER=/usr/bin/g++
    -DCMAKE_AR=/usr/bin/ar
    -DCMAKE_RANLIB=/usr/bin/ranlib
    -DCLANG_FORMAT_EXE="$CLANG_FORMAT"
    -DLLVM_TARGETS_TO_BUILD=None
    -DLLVM_ENABLE_EH=ON
    -DLLVM_ENABLE_RTTI=ON
    -DLLVM_INCLUDE_TESTS=OFF
    -DLLVM_INCLUDE_DOCS=OFF
    -DLLVM_INCLUDE_EXAMPLES=OFF
    -DHLSL_INCLUDE_TESTS=OFF
    -DHLSL_OPTIONAL_PROJS_IN_DEFAULT=OFF
    -DHLSL_COPY_GENERATED_SOURCES=1
)
cmake "${native_args[@]}"

target_args=(
    -S "$WORK_SRC"
    -B "$BUILD_DIR"
    -G Ninja
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN"
    -DCLANG_FORMAT_EXE="$CLANG_FORMAT"
    -DENABLE_SPIRV_CODEGEN=ON
    -DLLVM_TARGETS_TO_BUILD=
    -DLLVM_DEFAULT_TARGET_TRIPLE=dxil-ms-dx
    -DLLVM_ENABLE_EH=ON
    -DLLVM_ENABLE_RTTI=ON
    -DLLVM_INCLUDE_TESTS=OFF
    -DLLVM_INCLUDE_DOCS=OFF
    -DLLVM_INCLUDE_EXAMPLES=OFF
    -DHLSL_INCLUDE_TESTS=OFF
    -DHLSL_OPTIONAL_PROJS_IN_DEFAULT=OFF
    -DSPIRV_BUILD_TESTS=OFF
    -DSPIRV_SKIP_EXECUTABLES=ON
    -DHLSL_COPY_GENERATED_SOURCES=1
)
cmake "${target_args[@]}"

cmake --build "$BUILD_DIR" --target dxcompiler dxc -j "${DXC_JOBS:-$(nproc)}"

mkdir -p "$STAGE_DIR"
cp -f "$BUILD_DIR/lib/libdxcompiler.so" "$STAGE_DIR/libdxcompiler.so"
chmod 755 "$STAGE_DIR/libdxcompiler.so"

echo "Staged ARM64 DXC: $STAGE_DIR/libdxcompiler.so"
file "$STAGE_DIR/libdxcompiler.so"
sha256sum "$STAGE_DIR/libdxcompiler.so"

# Optional executable smoke test when qemu-user is available on an x86-64 host.
if command -v qemu-aarch64 >/dev/null; then
    dxc_bin="$(find "$BUILD_DIR/bin" -maxdepth 1 -type f -name 'dxc*' -perm -111 | head -n 1 || true)"
    if [[ -n "$dxc_bin" ]]; then
        test_hlsl="$BUILD_DIR/live-dxc-smoke.hlsl"
        test_spv="$BUILD_DIR/live-dxc-smoke.spv"
        printf '%s\n' 'float4 main(float4 p:SV_Position):SV_Target{return float4(1,0,0,1);}' >"$test_hlsl"
        LD_LIBRARY_PATH="$BUILD_DIR/lib" qemu-aarch64 -L /usr/aarch64-linux-gnu             "$dxc_bin" -spirv -T ps_6_0 -E main -Fo "$test_spv" "$test_hlsl"
        python3 - "$test_spv" <<'PY'
from pathlib import Path
import struct, sys
data = Path(sys.argv[1]).read_bytes()
if len(data) < 4 or struct.unpack("<I", data[:4])[0] != 0x07230203:
    raise SystemExit("DXC smoke test did not produce SPIR-V")
print(f"ARM64 DXC HLSL->SPIR-V smoke test passed ({len(data)} bytes)")
PY
    fi
fi
