#!/bin/zsh
# Build the iOS runtime, sign it through packaging/ios/xcode and install it.
#   tools/ios_deploy.sh <TEAMID> <bundle id> <device UDID> [--launch]
# Configure out/build/ios first (docs/IOS.md). --launch streams the console.
set -euo pipefail
cd "$(dirname "$0")/.."
team=$1 bundle=$2 device=$3
cmake --build out/build/ios --target LostOdysseyRecomp
# The wrapper compiles nothing; a clean build makes Xcode sign the new binary.
rm -rf packaging/ios/xcode/build
(cd packaging/ios/xcode && { [[ -d LostOdysseyRecompIOS.xcodeproj ]] || xcodegen; } &&
  xcodebuild -quiet -project LostOdysseyRecompIOS.xcodeproj -target LostOdysseyRecomp -configuration Release \
    -sdk iphoneos DEVELOPMENT_TEAM="$team" LO_IOS_BUNDLE_ID="$bundle" -allowProvisioningUpdates build)
xcrun devicectl device install app --device "$device" packaging/ios/xcode/build/Release-iphoneos/LostOdysseyRecomp.app >/dev/null
echo "installed $bundle"
if [[ ${4:-} == --launch ]]; then
  xcrun devicectl device process launch --console --terminate-existing --device "$device" "$bundle"
fi
