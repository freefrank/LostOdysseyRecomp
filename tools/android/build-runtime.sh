#!/usr/bin/env bash
set -euo pipefail

repo=$(cd "$(dirname "$0")/../.." && pwd)
sdk=${ANDROID_HOME:-${ANDROID_SDK_ROOT:?Set ANDROID_HOME or ANDROID_SDK_ROOT}}
ndk="$sdk/ndk/28.2.13676358"
build=${LO_ANDROID_BUILD_DIR:-"$repo/out/build/android-runtime"}
gradle=${GRADLE:-"$repo/packaging/android/gradlew"}
dxc=${LO_ANDROID_DXC:-"$repo/out/android-dxc/libdxcompiler.so"}
if [[ ! -f "$dxc" ]]; then
    echo "Build Android DXC with tools/android/build-dxc.sh and set LO_ANDROID_DXC to its stripped libdxcompiler.so" >&2
    exit 1
fi

apply_patch_once() {
    local directory=$1 patch=$2
    if git -C "$directory" apply --ignore-space-change --reverse --check "$patch" 2>/dev/null; then
        return
    fi
    git -C "$directory" apply --ignore-space-change --check "$patch"
    git -C "$directory" apply "$patch"
}
# Prepare the repository's XenonRecomp/PPC sources before this target build,
# following tools/patches/README.md and the normal host code-generation flow.
if git -C "$repo/thirdparty/plume" apply --ignore-space-change --reverse --check "$repo/tools/patches/plume-android.patch" 2>/dev/null; then
    # The Android overlay changes context in the base patch's window typedef.
    # Check the other base-patch files without undoing the user's overlay.
    git -C "$repo/thirdparty/plume" apply --ignore-space-change --reverse --check --exclude=plume_render_interface_types.h \
        "$repo/tools/patches/plume-lostodyssey.patch"
else
    apply_patch_once "$repo/thirdparty/plume" "$repo/tools/patches/plume-lostodyssey.patch"
fi
apply_patch_once "$repo/thirdparty/plume" "$repo/tools/patches/plume-sdl3.patch"
apply_patch_once "$repo/thirdparty/plume" "$repo/tools/patches/plume-android.patch"
apply_patch_once "$repo/thirdparty/SDL" "$repo/tools/patches/sdl-android-surface-lock.patch"

cmake -S "$repo" -B "$build" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$ndk/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 \
    -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE=Release \
    -DLO_BUILD_ANDROID_RUNTIME=ON -DLO_BUILD_RUNTIME=ON \
    -DLO_BUILD_TOOLS=OFF -DLO_BUILD_RECOMP_LIB=ON -DLO_BUILD_GPU=ON "$@"
cmake --build "$build" --target LostOdysseyRecomp -j "${LO_BUILD_JOBS:-4}"

stage="$repo/packaging/android/build/runtime-jni/arm64-v8a"
mkdir -p "$stage"
cp "$build/LostOdysseyRecomp/libmain.so" "$stage/"
cp "$build/thirdparty/SDL/libSDL3.so" "$stage/"
cp "$ndk/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so" "$stage/"
cp "$dxc" "$stage/libdxcompiler.so"
# libadrenotools hook libraries (custom Vulkan driver loading), when built.
for hook in "$build"/adrenotools/src/hook/lib*.so; do
    [ -f "$hook" ] && cp "$hook" "$stage/"
done
for library in "$stage"/*.so; do
    "$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf" --file-header "$library" | grep -q 'AArch64'
    "$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip" --strip-unneeded "$library"
done
cd "$repo/packaging/android"
"$gradle" :runtime:assembleDebug :runtime:lintDebug
