#!/usr/bin/env bash
# CI: Android SDK packages, Android DXC, runtime build, APK packaging and the
# runtime module's JVM tests. Shared by .github/workflows/android-apk.yml
# (push / dispatch check) and the Android job of release.yml. Expects the host
# recompiler tools to have generated the PPC sources already (same steps as the
# Linux release job), JDK 17, Ninja, ccache, a CMake >= 3.28 on PATH, and
# dependency directories restored by actions/cache under LO_ANDROID_CI_CACHE.
#
# Outputs out/apk/LostOdysseyRecomp-android-arm64-<RELEASE_TAG|debug>.apk.
# With RELEASE_TAG set the APK is the release build type. Every APK is signed
# with the project's debug keystore from the build-inputs checkout (the key
# that signed v0.8.0); there is no release keystore by decision.
set -euo pipefail

repo=$(cd "$(dirname "$0")/../.." && pwd)
cache=${LO_ANDROID_CI_CACHE:-${RUNNER_TEMP:-$repo/out}/lo-android-ci}
ndk_version=28.2.13676358
cmdline_tools_url=${LO_ANDROID_CMDLINE_TOOLS:-https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip}
jobs=${LO_BUILD_JOBS:-$(nproc)}

export ANDROID_HOME="$cache/android-sdk"
# Hosted Ubuntu defines ANDROID_SDK_ROOT for its preinstalled SDK. Keep both
# variables on the SDK restored above so Gradle and sdkmanager agree.
export ANDROID_SDK_ROOT="$ANDROID_HOME"
export LO_DXC_CACHE_DIR="$cache/android-dxc"
export GRADLE_USER_HOME="$cache/gradle"
export ANDROID_USER_HOME="$cache/android-user-home"
export CCACHE_DIR="$cache/ccache"
export CCACHE_BASEDIR="$repo"
export CCACHE_NOHASHDIR=1
export CCACHE_MAXSIZE=${CCACHE_MAXSIZE:-20G}
mkdir -p "$ANDROID_HOME" "$LO_DXC_CACHE_DIR" "$GRADLE_USER_HOME" "$ANDROID_USER_HOME" "$CCACHE_DIR"
ccache --zero-stats >/dev/null || true

echo "== Signing key"
# The project's debug keystore (build-inputs android/debug.keystore, the key
# that signed v0.8.0) is the only signing key. Without it, a keystore kept on
# the cache volume is used (generated once), so CI APKs still match each other.
keystore="$cache/android-user-home/lostodyssey-debug.keystore"
keystore_input="$repo/out/build-input/android/debug.keystore"
if [[ -f "$keystore_input" ]]; then
    cp "$keystore_input" "$keystore"
    echo "Using the build-inputs debug keystore"
elif [[ ! -f "$keystore" ]]; then
    echo "No build-inputs debug keystore; generating a cache-local one" >&2
    keytool -genkeypair -keystore "$keystore" -storepass android -keypass android \
        -alias androiddebugkey -dname "CN=Android Debug,O=Android,C=US" \
        -keyalg RSA -keysize 2048 -validity 10000
fi
keytool -list -keystore "$keystore" -storepass android -alias androiddebugkey | grep -i fingerprint || true
export LO_ANDROID_DEBUG_KEYSTORE="$keystore"

echo "== Android SDK packages"
sdkmanager="$ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager"
if [[ ! -x "$sdkmanager" ]]; then
    tmp=$(mktemp -d)
    wget -q -O "$tmp/cmdline-tools.zip" "$cmdline_tools_url"
    sha256sum "$tmp/cmdline-tools.zip"
    unzip -q "$tmp/cmdline-tools.zip" -d "$tmp"
    mkdir -p "$ANDROID_HOME/cmdline-tools"
    rm -rf "$ANDROID_HOME/cmdline-tools/latest"
    mv "$tmp/cmdline-tools" "$ANDROID_HOME/cmdline-tools/latest"
    rm -rf "$tmp"
fi
yes | "$sdkmanager" --licenses >/dev/null 2>&1 || true
"$sdkmanager" --install "platforms;android-35" "build-tools;35.0.0" "platform-tools" \
    "ndk;$ndk_version" "cmake;3.22.1" >/dev/null
"$sdkmanager" --list_installed

echo "== Android DXC"
# The verified prebuilt from the private build-inputs checkout seeds the cache;
# build-dxc.sh then reuses it. Compiling DXC is only the fallback: its link
# step is memory intensive, so keep it narrow on hosted runners.
prebuilt="$repo/out/build-input/android/libdxcompiler.so"
prebuilt_sha256=924fb3d8695676ed59ee1aaa2c73e1853f77972a8320824d93fe52bf77c2b6a8
artifact="$LO_DXC_CACHE_DIR/artifacts/libdxcompiler.so"
if [[ -f "$prebuilt" ]] && echo "$prebuilt_sha256  $prebuilt" | sha256sum -c --quiet; then
    mkdir -p "$(dirname "$artifact")" "$repo/out/android-dxc"
    cp "$prebuilt" "$artifact"
    cp "$prebuilt" "$repo/out/android-dxc/libdxcompiler.so"
    # The hash is the stronger check; build-dxc.sh's `file`-based test depends
    # on the host's magic database and sent run 581 into a source build.
    echo "Seeded the Android DXC from the build-inputs prebuilt ($prebuilt_sha256)"
    file "$repo/out/android-dxc/libdxcompiler.so" || true
else
    if [[ -f "$prebuilt" ]]; then
        echo "build-inputs libdxcompiler.so does not match the pinned hash; falling back to a source build" >&2
    else
        echo "No build-inputs libdxcompiler.so; falling back to a source build" >&2
    fi
    LO_DXC_JOBS=${LO_DXC_JOBS:-$jobs} bash "$repo/tools/android/build-dxc.sh"
fi

echo "== Runtime build, debug APK and lint"
export LO_ANDROID_BUILD_DIR="$repo/out/build/android-runtime"
export LO_ANDROID_DXC="$repo/out/android-dxc/libdxcompiler.so"
export LO_BUILD_JOBS=$jobs
bash "$repo/tools/android/build-runtime.sh" \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
ccache --show-stats || true

echo "== JVM tests"
(cd "$repo/packaging/android" && ./gradlew :runtime:testDebugUnitTest)

mkdir -p "$repo/out/apk"
rm -f "$repo/out/apk"/*.apk
if [[ -n "${RELEASE_TAG:-}" ]]; then
    echo "== Release APK for $RELEASE_TAG"
    (cd "$repo/packaging/android" && ./gradlew :runtime:assembleRelease -PloVersionName="$RELEASE_TAG")
    cp "$repo/packaging/android/runtime/build/outputs/apk/release/runtime-release.apk" \
        "$repo/out/apk/LostOdysseyRecomp-android-arm64-$RELEASE_TAG.apk"
else
    cp "$repo/packaging/android/runtime/build/outputs/apk/debug/runtime-debug.apk" \
        "$repo/out/apk/LostOdysseyRecomp-android-arm64-debug.apk"
fi
sha256sum "$repo/out/apk"/*.apk
