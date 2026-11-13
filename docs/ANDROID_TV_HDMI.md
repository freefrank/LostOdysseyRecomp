# Android TV output: implementation and device validation

This experimental branch adds independent external output using public Android
APIs. It is **not yet verified on a Xiaomi 14 or a physical HDMI TV**. No device
was attached to the development host. The standalone diagnostic APK is a canvas
calibration app, not a rebuilt game and not a Vulkan performance benchmark.

## Output ownership and black bars

System mirroring fits the phone's entire display into the TV. Mirroring a wider
phone display into 16:9 can produce bars above and below the picture. The game's
own `aspect_ratio::Fit` and `upscaling::ResolveOutputRegion` can independently
introduce bars to preserve a fixed content aspect. A 16:9 setting inside a phone
surface cannot remove bars Android adds outside that surface. This is an
explanation consistent with the reported symptom, not a measured diagnosis of
HyperOS on this device.

`DisplayManager.DISPLAY_CATEGORY_PRESENTATION` identifies eligible external
displays. The list does not prove HDMI, physical connectivity, or that an OEM
will accept a presentation. HDMI audio availability is recorded separately.
`Presentation.show()` is the actual public-API capability test. Rejection keeps
the phone surface in place and suppresses retries until reconnect or Apply.

The runtime moves its single existing SDL SurfaceView into a non-focusable,
fullscreen Presentation. There is no second SDLActivity, guest runtime,
Vulkan renderer, game swapchain, or mirror-copy pass. SDL's cached surface
display is updated after attachment. Surface destruction/recreation follows
SDL's existing callbacks and the renderer's native-window swapchain recovery.
Android can still spend resources on system composition. Savings and latency
have not been measured.

`ActivityOptions.setLaunchDisplayId` requires device support and moving or
launching an Activity. Creating a second SDLActivity is unsuitable for SDL's
static singleton and this one-shot guest runtime. `SurfaceControl.Transaction`
can manipulate app-owned layers, but its public API does not grant control of
system display composition or panel power. Neither is required by this path.

## Settings

Open native Settings → System → TV mode (Android), or CTRL → TV mode on the
phone. Native menu access remains available with a controller. Android TV
settings themselves appear on the phone.

The Android TV page currently includes English and Italian. Other locales use
English fallback. Only this new resource file opts out of missing-translation
lint errors while translations remain incomplete.

* **Automatic**: use an eligible external display whose current mode is 16:9,
  within 2%. This also includes eligible wireless/virtual displays.
* **TV 16:9**: use another eligible presentation display too. Fit without
  stretching or cropping. Bars are necessary when its actual shape is not 16:9.
* **Original mirroring** and **Off**: do not create independent output or change
  the system's mirroring policy. Off cannot disable HDMI at system level.
* **720p / 1080p**: request a 1280×720 / 1920×1080 game buffer and render height.
  Request a matching supported physical mode, preferring 60 Hz for 30/60 FPS.
  If no matching physical mode exists, leave signal selection to Android.
  A request is not proof of the negotiated HDMI signal or scanout resolution.

Preferences live in app-private `tv_output` SharedPreferences. Docking only
overrides the frame planner's local config snapshot. `settings.ini` is not
rewritten, including the user's phone render resolution and FSR preferences.
Undocking clears the override and returns surface sizing to Android layout.
The existing aspect and resolution planners are reused.

## Console behavior

The phone Activity remains the focused lifecycle and input owner. The TV
Presentation cannot take focus and freeze SDL/HID. Android gamepad buttons and
joystick motion are forwarded to SDL's existing handlers while the surface is
on TV. USB HID still uses SDL's existing manager. Disconnecting restores the
ordinary input path. Bluetooth and USB must each be tested on real hardware.

The touch gamepad can be hidden temporarily, independently of its saved touch
preference. CTRL stays available. Phone blackout is optional, defaults off, and
uses a black view plus the public per-window minimum brightness. Touching the
phone wakes it. Pause, removal and teardown restore brightness. **The panel is
still physically on.** Locking the phone may pause rendering or cut HDMI.
This implementation does not bypass normal Android pause/stop behavior.

The game keeps SDL's default media audio routing. Public communication-routing
APIs do not select arbitrary game audio, so the app does not misuse them or
override global audio settings. The optional left/right tone requests a HDMI
device for its own AudioTrack and records `getRoutedDevice()` during playback.
This verifies only the probe track. Listen to the actual game and inspect the
audio dump to verify its separate SDL route. Audio fallback depends on Android.

## HDMI-specific renderer changes

* Requested resolution changes and mode preferences are idempotent. Display
  notifications do not repeatedly detach the surface or recreate swapchains.
* Include SDL window-display changes in native surface recovery checks.
* Skip the phone's old-size resume hold on intentional dock/undock transitions.
* Suggest 60 Hz via the TV surface and a matching enumerated display mode.
  Existing game 30/60 FPS selection and VSync policy remain in charge.
* Movies use the same single surface and existing 16:9 presentation path.
  No decoder changes or claims about existing 60 FPS/audio bugs are made.

## Standalone diagnostic APK

No game files or root are needed. Package ID:
`io.github.freefrank.lostodyssey.tvprobe`, separate from the game. It displays a
circle, square grid, four numbered corners and a moving marker. The same display
controller, preference rules, audio test and phone-blackout code are compiled
into the probe. The phone's Choreographer drives its canvas pattern. Reported
canvas-post rates are neither HDMI scanout nor game/Vulkan FPS measurements.

Build with SDK 35/build-tools 35.0.0 and JDK 17:

```sh
python3 tools/android/build-tv-probe.py --sdk "$ANDROID_HOME" --jdk "$JAVA_HOME"
# Or with Gradle dependencies available:
cd packaging/android
./gradlew :tvprobe:assembleDebug :tvprobe:lintDebug
```

The direct build produces `out/tv-probe/LostOdyssey-TV-Probe.apk`, signed with a
local development key. It is intentionally debuggable for report collection.
The independent GitHub Actions diagnostic workflow needs no private game input.
The full runtime still requires its existing private XEX/PPC inputs and Android
native dependencies. Do not pair this changed Java shell with old native
libraries: the TV frame-planner JNI entry point must be built with it.

## Xiaomi 14 test procedure

1. Install and open the diagnostic APK without HDMI. Confirm normal phone view.
2. Connect USB-C/HDMI. Open TV mode, select Automatic and 1080p, then Apply.
   A separate image on TV and the controls on the phone establish independent
   output. If the TV still exactly mirrors the phone, collect logs.
3. Confirm all four corners and the full border are visible. The circle must
   stay round and grid cells square. Repeat at 720p. Disable TV overscan using
   its own picture-size setting if it clips the border.
4. Switch the pattern to 30 and 60 FPS. Record judder or freezes. This checks
   only the pattern, not the game's already known 60 FPS problems.
5. Test each controller separately, moving sticks and pressing buttons. The
   displayed text and log must change. START opens probe settings on the phone.
6. Play the audio tone. Confirm left then right through the TV, and inspect
   `audio-probe actual-route`. Test game audio separately in a complete build.
7. Enable phone blackout. Verify TV keeps updating. Touch phone to wake, open
   another app, return, disconnect/reconnect, then verify brightness restoration.
8. Repeat connect/disconnect at least 10 times and change resolution while
   connected. Original mirroring and Off must restore the phone surface.
9. In a fully rebuilt game, repeat at 30 FPS during gameplay and CGI, including
   connecting/disconnecting mid-movie. Test 60 FPS separately and compare an
   undocked run at the same game settings to distinguish existing bugs.

Collect evidence after reproducing, while HDMI remains connected:

```sh
python3 tools/android/collect-tv-diagnostics.py --out xiaomi14-tv-connected.zip
# For a complete rebuilt game instead of the standalone probe:
python3 tools/android/collect-tv-diagnostics.py --game --out xiaomi14-game-tv.zip
```

The app writes `Android/data/<package>/files/logs/tv-display.log`. The game log
export includes it. Capture one connected and one disconnected report, plus a
photo of the TV calibration pattern if bars or clipping remain.

## Privileged alternatives

ADB is optional for read-only diagnostics. No `wm size`, forced unsupported
display mode, global setting change, hidden power API, or root requirement is
introduced. Shizuku is not integrated: it does not establish OEM support for
separate display power or change this app's SDL lifecycle by itself. A device
specific power-control route needs separate evidence and an explicit opt-in
implementation. Ordinary screen lock is not a verified panel-off solution.

## API references

* https://developer.android.com/reference/android/app/Presentation
* https://developer.android.com/reference/android/hardware/display/DisplayManager
* https://developer.android.com/reference/android/app/ActivityOptions#setLaunchDisplayId(int)
* https://developer.android.com/reference/android/view/WindowManager.LayoutParams#preferredDisplayModeId
* https://developer.android.com/reference/android/view/SurfaceControl.Transaction
* https://developer.android.com/reference/android/media/AudioRouting

## Validation status

Host tests, Java compilation and APK packaging are reported separately in the
delivery report. No automated host result establishes HDMI detection on
HyperOS, TV scanout, controller transport compatibility, audible game routing,
physical panel power-off, CGI correctness, or stable gameplay FPS.
