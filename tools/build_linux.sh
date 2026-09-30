#!/usr/bin/env bash
set -euo pipefail

# Build the Linux runtime with clang/clang++ and Ninja (preset linux-clang).
# Usage: ./tools/build_linux.sh [target] [--deploy]

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

TARGET="LostOdysseyRecomp"
DEPLOY=0

for arg in "$@"; do
    if [ "$arg" = "--deploy" ]; then
        DEPLOY=1
    elif [[ "$arg" != -* ]]; then
        TARGET="$arg"
    fi
done

BUILD_DIR="out/build/linux-clang"
if [ ! -f "$BUILD_DIR/build.ninja" ]; then
    echo "==> Configuring linux-clang preset..."
    cmake --preset linux-clang
fi

echo "==> Building $TARGET with linux-clang..."
cmake --build --preset linux-clang --target "$TARGET"

if [ "$DEPLOY" -eq 1 ] || [ -n "${LO_DEPLOY_TO_GAME:-}" ]; then
    GAME_DIR="/mnt/d/Mihoyo/LostOdysseyRecomp-windows-x64"
    if [ -d "$GAME_DIR" ]; then
        echo "==> Deploying to $GAME_DIR..."
        if [ -f "$BUILD_DIR/LostOdysseyRecomp/$TARGET" ]; then
            cp -f "$BUILD_DIR/LostOdysseyRecomp/$TARGET" "$GAME_DIR/$TARGET"
            chmod +x "$GAME_DIR/$TARGET"
            echo "Deployed $TARGET -> $GAME_DIR/$TARGET"
        fi
        if [ -f "$BUILD_DIR/LostOdysseyRecomp/LoShaderPackTool" ]; then
            cp -f "$BUILD_DIR/LostOdysseyRecomp/LoShaderPackTool" "$GAME_DIR/LoShaderPackTool"
            chmod +x "$GAME_DIR/LoShaderPackTool"
        fi
    fi
fi

echo "==> Linux build complete: $BUILD_DIR/LostOdysseyRecomp/$TARGET"
