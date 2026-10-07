# Android ARM64 development builds

This directory builds the Android development probe and experimental full
runtime for the Lost Odyssey Recomp Android port. The probe is a diagnostic APK
that reports host memory-page behavior and Vulkan device limits, formats and
heaps, and exercises one clear/present frame per check run. The runtime is an
arm64 development APK path and is not a published Android release.

The targets use **arm64-v8a** with API 26+, compile/target SDK
35, Android Gradle Plugin 8.9.3, Gradle 8.11.1 and NDK 28.2.13676358. The
source CMake entry accepts `Android` for the probe and for the explicitly
opt-in `LO_BUILD_ANDROID_RUNTIME=ON` runtime path. The runtime remains
experimental and requires host-generated PPC sources plus the Android FFmpeg,
DXC and staged native-library inputs described below.

## Prerequisites

Install these SDK packages with `sdkmanager` (or select the same versions in
Android Studio):

```sh
sdkmanager "platform-tools" "platforms;android-35" \
  "build-tools;35.0.0" "cmake;3.22.1" \
  "ndk;28.2.13676358"
```

Set `ANDROID_HOME` or `ANDROID_SDK_ROOT` to the SDK directory. The Gradle
wrapper downloads the pinned Gradle 8.11.1 distribution and verifies its
SHA-256 checksum from `gradle-wrapper.properties`. Use JDK 17 for the Android
Gradle Plugin 8.9.3 build.

## Build and lint

From this directory, run:

```sh
./gradlew assembleDebug lintDebug
```

The probe and runtime are separate Gradle modules. Build the probe with
`:app:assembleDebug`; build the runtime shell with `:runtime:assembleDebug`
after staging its native libraries with `tools/android/build-runtime.sh`.
The runtime Gradle task is a packaging step. The current runtime APK has passed
the debug build, lint, v2 signature and 16 KB zip-alignment checks and has been
installed with ADB. On the development tablet, the four-disc resources are in
the app's external files directory with readable permissions; the runtime has
loaded the XEX, created the Vulkan device and swapchain, and entered real DXC
shader preparation. The [Android DXC build note](../../docs/notes/android-dxc-build-2026-10-02.md)
records the native compiler staging details. The current development APK has loaded the XEX, played the opening video,
reached the first battle and completed two touch-driven attacks with visible
damage on the development tablet. One initial-battle shader preparation pause
of about 40 seconds was observed. Longer play, audio beyond native queue
evidence, other GPUs, 16 KB devices and physical-controller validation remain
open; these checks do not establish complete-game support.

On Windows PowerShell use:

```powershell
.\gradlew.bat assembleDebug lintDebug
```

The debug APK is written to
`app/build/outputs/apk/debug/app-debug.apk`. The current host build has passed
`assembleDebug` and `lintDebug`; APK signature and 16 KB zip alignment checks
also pass. These checks prove packaging and static toolchain output only; they
do not establish device compatibility or full-game support.

The build copies the pinned SDL Java sources into Gradle's generated sources
and applies a narrow local shim to guard malformed USB broadcast intents and
validate USB permission responses. SDL's own API-guarded receiver helper uses
`RECEIVER_NOT_EXPORTED`. The lint baseline contains exactly 26 upstream SDL
`MissingPermission` findings (Bluetooth, audio and vibration paths). It does
not suppress application or receiver errors introduced by the probe.

## Install and collect a report

With an ARM64 Android device connected and visible to ADB:

```sh
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n io.github.freefrank.lostodyssey.probe/.ProbeActivity
adb exec-out run-as io.github.freefrank.lostodyssey.probe \
  cat files/probe-report.txt
adb logcat -d -s LOAndroidProbe
```

The probe Activity provides **Run checks**, **Test audio** and **Copy report**
controls. The native report is stored at
`files/probe-report.txt`; native diagnostics use the `LOAndroidProbe` logcat
tag. Capture the device model, SDK, ABI, page size, Vulkan features and limits
alongside the report. No game files or storage permissions are required.

The probe's memory check is deliberately limited: it uses a fixed 4 GiB
virtual-address reservation with small aliases. The current experiment marks
the **E alias** path unsupported on 16 KiB hosts; its A/C checks can still run.
This is not evidence for full guest mapping, protection, or runtime
compatibility. Vulkan coverage stops at capability discovery and one
clear/present frame per check run; shaders, pipelines and game rendering remain
future work.

## Android ARM64 recompiled library

The library build separates host code generation from Android target
compilation. XenonRecomp and its tools run on the Windows host; the generated
PPC sources are then compiled by the Android NDK into the PIC static target
`LostOdysseyRecompLib`. The repository's generated `LostOdysseyRecompLib/ppc`
sources are private build inputs and must not be added to a commit.

Use a WSL system CMake **3.28 or newer** for this slice. The Gradle project's
CMake 3.22.1 requirement is for the probe APK and is separate from this
cross-build. From the repository root, with `ANDROID_NDK_ROOT` pointing to the
pinned NDK directory:

```sh
cmake --version
cmake -S . -B out/build/android-ppc -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 \
  -DLO_BUILD_RUNTIME=OFF -DLO_BUILD_GPU=OFF \
  -DLO_BUILD_TOOLS=OFF -DLO_BUILD_RECOMP_LIB=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/android-ppc --target LostOdysseyRecompLib -j 4
```

The Release build passed with NDK 28.2: the archive contains 247 AArch64 ELF
objects (246 generated files plus the function mapping), all compiled with
PIC. This remains a separate library-only target. The opt-in full runtime
target links `libmain.so` for the Android shell after staging the Android
FFmpeg, DXC and other native libraries; the development build has passed the
runtime library link and focused host checks. It does not yet establish APK
installation, resource loading or a playable game flow.

## Experimental full runtime

From the repository root, generate PPC sources on the host and stage the
Android native dependencies before invoking the runtime CMake path:

```sh
tools/android/build-runtime.sh
```

The Gradle shell can then be packaged from this directory:

```sh
./gradlew :runtime:assembleDebug
```

The runtime uses app-owned external files for game data and keeps physical SDL
controller input. Its source touch-controller implementation provides a
`Controller settings` dialog with **Show touch controls**, **Control size**
(60–140%), **Opacity** (25–100%), **Apply**, **Cancel** and **Edit layout**.
The editor provides **SAVE**, **CANCEL**, **RESET** and **HIDE**/**SHOW**;
controls can be dragged independently, and hidden controls remain selectable
in the editor. The pure layout model checks and the configurable editor flow
were verified on the development tablet; broader multitouch and physical
controller coverage remain pending. The
BDA vertex-fetch path avoids requiring the complete
1 GiB vertex arena as one storage-buffer descriptor on devices with a smaller
reported range. Host HLSL remains unchanged for the desktop path.

Android shows the update prompt at startup and opens the APK download in the
browser; desktop-style automatic install and restart and automatic tar
capture packaging are not supported by this development target. The app
creates `game/` with a `README.txt` under `Android/data/<package>/files/` on the
internal storage and every SD card when opened (the discs are copied in), and the game reads the first
storage with `game/disc1/default.xex`, else the internal one. A folder chosen on
the **Game folder** page (shown at launch when no game data is found, and from
**CTRL → Game folder**) is checked first; the game reads files by path, so it
needs "All files access" (storage permission on Android 8–10), and changing the
folder restarts the game. The page's **Import disc images…** button runs the
desktop importer screens on the device (disc images or extracted discs from any
storage; destination defaults to the chosen folder, else the app folder; a
folder outside the app folders is remembered as the chosen one); it needs the
same permission, starts the game when finished, returns to the page when
cancelled, and stops a running game after a confirmation. A successful
native link or Gradle package is not gameplay acceptance; install the APK and
record resource loading, shader compilation, input, audio, lifecycle and a
bounded game-flow test separately.

USB and Bluetooth controllers use the same touch visibility policy. Connecting
one automatically hides touch controls while retaining `CTRL`; enable **Show
touch controls** to use touch and physical input together. Disconnecting the
last controller restores the saved touch preference. Android input events and
SDL's connected-controller state cover framework and HIDAPI controller paths.
The SDL fallback is checked once a second while the Activity is resumed; the
watch stops in the background. Physical hot-plug verification remains pending.

Android's graphics menu omits desktop backend, window, output-size, aspect,
VRR and frame-generation controls. Render resolution, supported antialiasing,
filtering, RGB range, frame rate and brightness remain available. FSR choices
appear only in builds that include FSR; the current development build does not.
Returning through the launcher reuses the top runtime Activity, preventing the
duplicate SDL startup observed with the previous default launch mode. On
foreground return, the renderer recreates the Vulkan surface and swapchain
after draining queued GPU work instead of presenting to the abandoned Surface.

The Android build uses the repository's pinned SDL 3.4.18 revision. This brings
the current controller mappings and HIDAPI drivers to the APK, but does not
establish Android compatibility for every controller or USB/Bluetooth mode.
`tools/android/build-runtime.sh` also applies
`tools/patches/sdl-android-surface-lock.patch`: Vulkan surface creation reads
and retains the native window under SDL's Activity mutex while calling the
driver. Direct CMake invocations must apply this patch first.

## Shader pack

Android reads the same Vulkan shader pack as Windows, Linux and macOS: every
SPIR-V build fetches vertices through the arena's device address, because many
Android GPUs cap storage-buffer descriptors at 128 MiB. At startup the runtime
looks for `files/shaders/portable_vk.lospv` in app storage and, when it is
missing or made for another shader contract, offers the published pack from the
`shader-packs` release in the same window as the desktop (press A or B on the
on-screen controller, or use a physical controller). The download goes to `files/shaders/` and is checked
against the index size, SHA-256 and the contract before it replaces anything.
The pipeline recipe corpus from the same release goes to
`files/shaders/pipelines_corpus.bin` in the background, without a window, through
the same `HttpURLConnection` path.

Packs are built on Windows or Linux with one entry,
`tools/shader_pack/build_packs.py` ([portable shader packs](../../docs/PORTABLE_SHADER_PACK.md)).
A pack built that way can also be installed by hand while the game is stopped:

```sh
adb push portable_vk.lospv /data/local/tmp/lo-portable_vk.lospv
adb shell run-as io.github.freefrank.lostodyssey mkdir -p files/shaders
adb shell run-as io.github.freefrank.lostodyssey cp /data/local/tmp/lo-portable_vk.lospv files/shaders/portable_vk.lospv
adb shell rm /data/local/tmp/lo-portable_vk.lospv
```

The pack avoids on-device DXC work for covered shaders; driver pipeline
creation and shaders outside the pack may still need preparation.

## GPU driver (Qualcomm devices)

The Qualcomm proprietary Vulkan driver renders the text on the highlighted
menu row fully transparent (Adreno 750, every renderer input identical to
Windows). A Mesa Turnip driver renders it correctly, so the runtime can load a
driver package through [libadrenotools](../../thirdparty/libadrenotools)
(BSD-2, vendored at `8fae8ce`; its hook libraries are packaged and extracted
with the APK, which is why `useLegacyPackaging` is on).

On an arm64 device with Android 9+ and `/dev/kgsl-3d0` the launcher opens the
**GPU driver** page before the first game start; later starts go straight to
the game, and **CTRL → GPU driver** opens the page while playing (changing the
driver there restarts the process, because the driver is bound before the
Vulkan instance). The page lists the installed packages plus **System GPU
driver**, downloads packages from the same five GitHub release feeds as the
Eden emulator's driver fetcher (Mr. Purple Turnip, GameHub Adreno 8xx, KIMCHI
Turnip, Weab-Chan Freedreno, Whitebelyash Turnip; zip assets only, feeds cached
for an hour in `cache/gpu_driver_catalog/`), shows Eden's recommendation for
the Adreno model from `/sys/class/kgsl/kgsl-3d0/gpu_model`, and installs a zip
from the file picker. The menu-text fix was verified with KIMCHI
`Turnip_v26.0.0_R8.zip`. Other devices and non-Qualcomm GPUs never see the page.

Packages are the libadrenotools format (flat zip with `meta.json` naming the
`.so` in `libraryName`; `minApi` is checked). Each package is extracted into
`files/gpu_driver/<zip name>/`; the choice is stored in the `gpu_driver`
preferences (`selected`, empty for the system driver) and passed to the native
loader as `LO_CUSTOM_DRIVER_DIR` and `LO_VK_CUSTOM_DRIVER`. A package that
loads but cannot create a Vulkan instance or device falls back to the system
driver in-process; a start that never reaches the game (the process dies within
15 seconds) selects the system driver again and reopens the page with a notice.
When no driver gives the renderer a usable Vulkan device (for example a system
driver below Vulkan 1.2, like the Adreno 650's), `video.cpp` passes the reason
to `RuntimeActivity.reportGraphicsFailure` and the activity reopens the page
with it when the native main returns, instead of closing; otherwise the
launcher would start the same driver again on every start (#185). Devices
without the page (Mali, PowerVR) show the reason in a dialog instead. A start
on the system driver that dies within 15 seconds reopens the page too.
`adb shell am start -n io.github.freefrank.lostodyssey/.RuntimeActivity` still
starts the game directly, honouring the stored choice; debug builds also accept
`--es LO_VK_CUSTOM_DRIVER <file> --es LO_CUSTOM_DRIVER_DIR <dir>` to override it.

The catalog and package logic has JVM tests:

```sh
./gradlew :runtime:testDebugUnitTest
```

## Logs

`RuntimeActivity` sets `LO_LOG_DIR` to `getExternalFilesDir(null)/logs`, so the
runtime log, `shader-*.jsonl` and `native-stderr.log` land in
`Android/data/io.github.freefrank.lostodyssey/files/logs/`, which players can
copy over USB without adb. `PlayerLogs` media-scans that folder when the
launcher opens, 15 s after a start and when the game is paused, because MTP
lists only scanned files at their scanned size. Uncaught Java exceptions are
written to `java-crash-<ms>.txt` (newest five kept).

Every log line also goes to logcat with the tag `LostOdyssey`. Fatal native signals append a `[crash]` report (signal, fault
address, `pc`/`lr` as `libmain.so+0x…`, guest registers and a frame-pointer
backtrace) and then pass the signal on, so the system tombstone is still
written. Resolve the offsets with the matching unstripped `libmain.so`:

```sh
llvm-addr2line -Cfe libmain.so 0x1234
```

The startup lines `LO_ANDROID_DEVICE=…`, `vulkan gpu:` and `vulkan gpu driver:`
name the phone, every Vulkan device and its driver before device creation.

## Continuous integration

Two Gitea workflows build this APK through `tools/android/ci_build.sh` on
git.zkx.ca's privileged Linux runner, after the same host recompiler tools and
PPC code generation as the Linux release job. The script installs the Android
SDK packages with `sdkmanager`, seeds the Android DXC from the verified
prebuilt in the private build-inputs repository (`android/libdxcompiler.so`,
hash pinned in the script; `tools/android/build-dxc.sh` only compiles it as a
fallback, which exceeds the runner's memory), runs `tools/android/build-runtime.sh`
(assemble + lint) and `:runtime:testDebugUnitTest`. The SDK/NDK, the DXC build,
the Gradle home and ccache live on the runner's `LO_CI_CACHE` volume. Every
APK is signed with the project's debug keystore from the private build-inputs
repository (`android/debug.keystore`, the key that signed v0.8.0); the
maintainer decided on 2026-10-03 that the app stays debug-signed and never gets
a release keystore, so each release installs over the previous one.

- `.gitea/workflows/android-apk.yml` runs on pushes that touch
  `packaging/android`, `tools/android` or `thirdparty/libadrenotools`, and on
  manual dispatch; it uploads `LostOdysseyRecomp-android-arm64-debug` and
  mirrors the result to the GitHub commit as `gitea/android-apk`.
- `.gitea/workflows/release.yml` has an Android job for every `v*` tag (and
  build-only dispatches). It builds the release build type with the tag as
  `versionName` (`versionCode` = major×1000000 + minor×10000 + patch) and
  publishes `LostOdysseyRecomp-android-arm64-<tag>.apk` with the other
  packages, signed with the same debug keystore.

A green run proves packaging, lint and the JVM tests, not device behaviour.
