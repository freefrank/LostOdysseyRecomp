#!/usr/bin/env bash
# Builds LostOdysseyRecomp.nro for the Nintendo Switch.
#
# Runs inside the ghcr.io/autorunhq/switch-dev Docker image (devkitA64, libnx,
# mesa-switch NVK, CMake, Ninja, host Clang). On Windows use
# tools/switch/build-switch.ps1, which starts the container for you.
#
#   docker run --rm -v "$PWD:/work" -w /work ghcr.io/autorunhq/switch-dev:2026.10.05 \
#       bash tools/switch/build-switch.sh
#
# Inputs kept out of Git (see docs/SWITCH.md):
#   LostOdysseyRecompLib/private/disc1/default.xex   your Disc 1 executable
# Generated here when missing (host tools, built with the image's Clang):
#   LostOdysseyRecompLib/private/image_disc1.bin(.sym)   xexdump
#   LostOdysseyRecompLib/ppc/                            XenonRecomp
#
# Environment:
#   JOBS=N             parallel compile jobs (default: CPUs; lower it if the
#                      container runs out of memory, ~1.5 GB per job)
#   BUILD_TYPE=...     Release (default) or RelWithDebInfo
#   NVK_LIBRARY=path   libvulkan.a from mesa-switch (default: searched under $DEVKITPRO)
#   CLEAN=1            delete the Switch build folder first
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$root"
: "${DEVKITPRO:=/opt/devkitpro}"
export DEVKITPRO
JOBS="${JOBS:-$(nproc)}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
build="$root/out/build/switch"
hostbuild="$root/out/build/switch-host-tools"
private="$root/LostOdysseyRecompLib/private"

log() { printf '\n==== %s ====\n' "$*"; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }

# ---------------------------------------------------------------- toolchain
[ -x "$DEVKITPRO/devkitA64/bin/aarch64-none-elf-g++" ] || die "devkitA64 not found under $DEVKITPRO (run inside ghcr.io/autorunhq/switch-dev)"
[ -f "$DEVKITPRO/libnx/lib/libnx.a" ] || die "libnx not found under $DEVKITPRO/libnx"
if [ -z "${NVK_LIBRARY:-}" ]; then
    NVK_LIBRARY="$(find "$DEVKITPRO" -name libvulkan.a -path '*switch*' 2>/dev/null | head -n1 || true)"
fi
[ -n "$NVK_LIBRARY" ] && [ -f "$NVK_LIBRARY" ] || die "mesa-switch libvulkan.a not found; set NVK_LIBRARY"
echo "NVK driver: $NVK_LIBRARY"
command -v cmake >/dev/null || die "cmake missing"
command -v ninja >/dev/null || die "ninja missing"
HOST_CC="${HOST_CC:-$(command -v clang || command -v gcc)}"
HOST_CXX="${HOST_CXX:-$(command -v clang++ || command -v g++)}"

# ---------------------------------------------------------------- sources
log "Submodules"
git config --global --add safe.directory '*' 2>/dev/null || true
git submodule update --init --recursive --depth 1

# Apply a chain of patches in order. When the chain's last patch is already in
# place the tree is ready. Otherwise the dependency is reset to its pinned
# commit (git checkout + clean) and the whole chain is applied, so a tree that
# carries only the PC patches, or an older Switch patch, is brought up to date.
apply_chain() { # <dir> <patch>...
    local dir="$1"; shift
    local last="${@: -1}"
    if git -C "$dir" apply --reverse --check "$last" 2>/dev/null; then
        echo "$dir: already patched"
        return
    fi
    git -C "$dir" checkout -q -- .
    git -C "$dir" clean -fdq
    local patch
    for patch in "$@"; do
        git -C "$dir" apply "$patch" || die "$(basename "$patch") does not apply to $dir"
        echo "$dir: applied $(basename "$patch")"
    done
}
P="$root/tools/patches"
log "Patches"
apply_chain tools/XenonRecomp "$P/XenonRecomp-lostodyssey.patch" "$P/XenonRecomp-switch.patch"
apply_chain thirdparty/plume "$P/plume-lostodyssey.patch" "$P/plume-sdl3.patch" "$P/plume-switch.patch"
apply_chain thirdparty/plume/contrib/volk "$P/volk-switch.patch"
apply_chain thirdparty/SDL-switch "$P/sdl-switch-no-egl.patch" "$P/sdl-switch-audio-priority.patch"

# ---------------------------------------------------------------- game inputs
[ -f "$private/disc1/default.xex" ] || die "copy your Disc 1 default.xex to LostOdysseyRecompLib/private/disc1/ (docs/SWITCH.md)"

need_tools=0
[ -f "$private/image_disc1.bin.sym" ] || need_tools=1
[ -f "$root/LostOdysseyRecompLib/ppc/ppc_func_mapping.cpp" ] || need_tools=1
if [ "$need_tools" = 1 ]; then
    log "Host tools (xexdump, XenonRecomp)"
    cmake -S tools/xexdump -B "$hostbuild" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$HOST_CC" -DCMAKE_CXX_COMPILER="$HOST_CXX"
    cmake --build "$hostbuild" --target XenonRecomp xexdump -j "$JOBS"
fi
if [ ! -f "$private/image_disc1.bin.sym" ]; then
    log "Disc 1 image dump"
    "$hostbuild/xexdump" "$private/disc1/default.xex" "$private/image_disc1.bin"
fi
if [ ! -f "$root/LostOdysseyRecompLib/ppc/ppc_func_mapping.cpp" ]; then
    log "Recompiling PowerPC code"
    python3 -B tools/ppc_codegen.py generate --executable "$hostbuild/XenonRecomp/XenonRecomp/XenonRecomp"
fi
# The generated ppc_context.h must carry the Switch (GCC) shims.
cp tools/XenonRecomp/XenonUtils/ppc_context.h LostOdysseyRecompLib/ppc/ppc_context.h

# ---------------------------------------------------------------- build
if [ "${CLEAN:-0}" = 1 ]; then rm -rf "$build"; fi
log "Configure ($BUILD_TYPE)"
cmake -S . -B "$build" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$root/cmake/toolchains/switch-devkitA64.cmake" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_CXX_SCAN_FOR_MODULES=OFF \
    -DDEVKITPRO="$DEVKITPRO" \
    -DLO_SWITCH_NVK_LIBRARY="$NVK_LIBRARY"

log "Build ($JOBS jobs)"
# -k 0: keep going after a failure so one run reports every failing file.
cmake --build "$build" --target LostOdysseyRecomp -j "$JOBS" -- -k 0

out="$root/out/switch"
mkdir -p "$out"
cp "$build/LostOdysseyRecomp/LostOdysseyRecomp.nro" "$out/"
cp "$build/LostOdysseyRecomp/LostOdysseyRecomp.elf" "$out/"
log "Done"
echo "NRO: out/switch/LostOdysseyRecomp.nro  (copy to sdmc:/switch/LostOdysseyRecomp/)"
echo "ELF: out/switch/LostOdysseyRecomp.elf  (keep it: crash.log addresses resolve against it)"
