# SDL3 migration

LostOdysseyRecomp uses one vendored SDL 3.4.18 revision on Windows x64, Linux
x64, macOS arm64, and Android arm64. SDL2 and an SDL2 compatibility layer are
not part of the runtime.

The migration covers gamepad enumeration, hotplug, prompts and rumble; queued
audio streams; window, display, focus, keyboard and mouse events; native window
properties; Vulkan and Metal presentation; installer and updater interfaces;
and the Android SDLActivity/JNI package.

## Required release verification

For every release candidate, build and package all four targets. A successful
build is not runtime proof. Record the hardware and connection used for each
controller check.

| Platform | Required runtime checks |
| --- | --- |
| Windows x64 | Gameplay and setup screens, windowed/fullscreen transitions, focus recovery, stereo and 5.1 audio, USB and Bluetooth gamepad hotplug and rumble |
| Linux x64 | Gameplay and setup screens under the supported display backends, fullscreen/focus, PipeWire audio, USB and Bluetooth gamepad hotplug and rumble, AppImage and Flatpak contents |
| macOS arm64 | Gameplay and setup screens, Metal surface lifecycle, fullscreen/focus, audio, Bluetooth and USB gamepads, DMG contents |
| Android arm64 | Install the APK; check foreground return, Vulkan surface recreation, touch, audio, and controller hotplug over the available USB and Bluetooth paths |

Checks that cannot be performed must be listed in the release evidence instead
of being inferred from compilation or from SDL's controller database.
