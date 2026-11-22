#!/usr/bin/env bash
# iPhone build, sign and install run by Sessionist's Recomp Hub. Run from the repo root.
# Env from Sessionist: PORT_DIR, TEAM, BUNDLE, DEVICE.
# Needs the host shader store from a Mac run (see docs/IOS.md).
set -e -o pipefail
cd "$(dirname "$0")/.."
. sessionist/common.sh

if [ ! -f out/build/ios/build.ninja ]; then
  prepare_dependencies
  STORE=$(ls -t "${PORT_DIR:-.}"/ios-assets/builtin_v*_spirv.lostore "$HOME/Library/Application Support/LostOdysseyRecomp/shaders"/builtin_v*_spirv.lostore 2>/dev/null | head -1 || true)
  [ -n "$STORE" ] || { echo "[RED] Missing host shader store (builtin_v*_spirv.lostore). Run the Mac source build once to the title screen with LO_SHADER_CACHE_DIR=${PORT_DIR:-.}/ios-assets (see docs/IOS.md)."; exit 1; }
  cmake -S . -B out/build/ios -G Ninja \
    -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
    -DLO_BUILD_AUDIO_TESTS=OFF \
    -DLO_IOS_BUNDLE_ID="$BUNDLE" \
    -DLO_IOS_HOST_SHADER_STORE="$STORE"
fi
tools/ios_deploy.sh "$TEAM" "$BUNDLE" "$DEVICE"
