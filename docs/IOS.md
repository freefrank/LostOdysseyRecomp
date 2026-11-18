# iOS (experimental)

[Overview](../README.md) · [Building](BUILDING.md)

An iPhone/iPad build of the macOS port: the same recompiled guest code, plume's
Metal backend and SDL 3 (UIKit). It needs an Apple developer team to sign, a
device with 8 GB of memory or more (iPhone 15 Pro and later, M-series iPad) and
iOS 18. A game controller is required: there are no touch controls yet.

## What differs from macOS

| Area | iOS |
|---|---|
| Platform macros | `LO_PLATFORM_IOS`; `LO_PLATFORM_APPLE` covers Darwin code shared with macOS. `LO_PLATFORM_MACOS` is macOS only. |
| Guest memory | Same layout as macOS; the C view alias uses `vm_remap`. Needs the `extended-virtual-addressing` and `increased-memory-limit` entitlements. |
| Shader compiler | None (DXC cannot be loaded on iOS). Guest shaders come from the distribution pack `shaders/portable_vk.lospv`; host shaders from a store compiled on a Mac and bundled in the app (`LO_IOS_HOST_SHADER_STORE`). |
| Files | Everything lives in the app's Documents folder, visible in Finder: `game/disc1`–`disc4`, `settings.ini`, `save/`, `cache/`, `shaders/`, `logs/`. `game/`, `cache/`, `shaders/` and `logs/` are excluded from iCloud backup. |
| HTTP | `updater/ios_http.mm` (NSURLSession) instead of libcurl. |
| Window | Full screen, high pixel density; `os::ios::FitMetalView` keeps SDL's Metal view the size of the landscape window. |
| Off | MetalFX frame generation, touch controls, Steam, app self-update, process restart. Discord is built but never connects. |

## Build

On a Mac with Xcode and the macOS prerequisites from [BUILDING.md](BUILDING.md#building-on-macos):

1. Apply the four plume patches in order, ending with `tools/patches/plume-ios.patch`
   (see [tools/patches/README.md](../tools/patches/README.md#ios-plume-patch)).
   Initialize plume's own submodules (`git -C thirdparty/plume submodule update --init --recursive`).
1. Generate the guest code as for macOS (Disc 1 in `LostOdysseyRecompLib/private/disc1`).
2. Get a host shader store. Run a macOS build of the same tree once to the title
   screen with `LO_SHADER_CACHE_DIR=<folder>`; the store is
   `<folder>/builtin_v<N>_spirv.lostore`. The DXC identity it was made with is
   `dxc-<major>.<minor>` of the bundled `libdxcompiler.dylib` (`dxc-1.8`).
3. Configure and build:

```bash
cmake -S . -B out/build/ios -G Ninja -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY -DLO_BUILD_AUDIO_TESTS=OFF \
  -DLO_IOS_BUNDLE_ID=<your.bundle.id> -DLO_IOS_HOST_SHADER_STORE=/path/to/builtin_v28_spirv.lostore
cmake --build out/build/ios --target LostOdysseyRecomp
```

4. Sign and install through the wrapper project (`tools/ios_deploy.sh <TEAMID> <bundle id> <UDID> [--launch]`
   does the build, signing, install and launch in one go), which copies the CMake bundle
   and lets Xcode provision it (automatic signing creates the App ID with both
   memory entitlements):

```bash
cd packaging/ios/xcode && xcodegen
xcodebuild -project LostOdysseyRecompIOS.xcodeproj -target LostOdysseyRecomp -configuration Release \
  -sdk iphoneos DEVELOPMENT_TEAM=<TEAMID> LO_IOS_BUNDLE_ID=<your.bundle.id> -allowProvisioningUpdates build
xcrun devicectl device install app --device <UDID> build/Release-iphoneos/LostOdysseyRecomp.app
```

## Game data

Copy an installed game folder (the `game` folder holding `disc1`–`disc4` that the
desktop importer writes) into the app's Documents: Finder → the device → Files →
Lost Odyssey, or `xcrun devicectl device copy to --domain-type appDataContainer
--domain-identifier <bundle id> --destination Documents/game/disc1 --source <disc1>`.
Copy `shaders/portable_vk.lospv` the same way, or let the app download it.
