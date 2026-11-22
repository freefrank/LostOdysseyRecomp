#!/usr/bin/env bash
# Mac build run by Sessionist's Recomp Hub. Run from the repo root.
# Env from Sessionist: ICLOUD_CONTAINER (iCloud.<bundle id>), INPUT (optional:
# the user's Lost Odyssey folder, used to find Disc 1).
# Output: out/releases/LostOdysseyRecomp.app
set -e -o pipefail
cd "$(dirname "$0")/.."
. sessionist/common.sh

prepare_dependencies

if ! ls LostOdysseyRecompLib/ppc/*.cpp >/dev/null 2>&1; then
  GAME_INPUT="${INPUT:-}"
  if [ ! -f LostOdysseyRecompLib/private/disc1/default.xex ] && [ -n "$GAME_INPUT" ]; then
    DISC1=""
    for cand in "$GAME_INPUT" "$GAME_INPUT/disc1" "$GAME_INPUT/Disc 1" "$GAME_INPUT/Disc1" "$GAME_INPUT/game/disc1" "$(dirname "$GAME_INPUT")"; do
      if [ -f "$cand/default.xex" ]; then DISC1="$cand"; break; fi
    done
    if [ -n "$DISC1" ]; then
      mkdir -p LostOdysseyRecompLib/private
      rm -rf LostOdysseyRecompLib/private/disc1
      ln -s "$DISC1" LostOdysseyRecompLib/private/disc1
      echo "Linked Disc 1 from $DISC1"
    fi
  fi
  [ -f LostOdysseyRecompLib/private/disc1/default.xex ] || { echo "[RED] Choose your Lost Odyssey folder (containing disc1/default.xex) in 'Your game' above, or place Disc 1 files in $PWD/LostOdysseyRecompLib/private/disc1/ and build again."; exit 1; }
  cmake -S tools/xexdump -B out/build/tools -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
  cmake --build out/build/tools --target XenonRecomp XenonAnalyse xexdump
  out/build/tools/xexdump LostOdysseyRecompLib/private/disc1/default.xex LostOdysseyRecompLib/private/image_disc1.bin
  python3 -B tools/ppc_codegen.py generate
fi

cmake -S . -B out/build/macos-gpu -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DLO_ICLOUD_CONTAINER="${ICLOUD_CONTAINER:-}"
cmake --build out/build/macos-gpu --target LostOdysseyRecomp
# Package into a scratch folder: package_macos.py refuses to overwrite a zip
# for the same commit, which would break a rebuild without new commits.
rm -rf out/sessionist-package
python3 tools/package_macos.py --build out/build/macos-gpu --output out/sessionist-package
ZIP=$(ls -t out/sessionist-package/LostOdysseyRecomp-macos-arm64-*.zip | head -1)
mkdir -p out/releases
rm -rf out/releases/LostOdysseyRecomp.app out/releases/unpacked
ditto -x -k "$ZIP" out/releases/unpacked
mv out/releases/unpacked/LostOdysseyRecomp.app out/releases/LostOdysseyRecomp.app
rm -rf out/releases/unpacked
