# Multiple controllers and keyboard input

Status: **2026-09-06, released in v0.2.1; not included in v0.2**. The input update opens all SDL-mapped game controllers and keeps the keyboard active. It merges them into player 1, not separate multiplayer slots.

## Behavior

- All devices recognized by SDL GameController are opened, without Xbox-model preference. Stable instance IDs avoid duplicate opens and support removal/reconnection. Unmapped joystick devices still require an SDL controller mapping.
- Held buttons are combined; each trigger takes the maximum value. Each stick uses the strongest whole vector above its deadzone (left 7,849, right 8,689), rather than combining axes from different controllers.
- Keyboard events update a synchronized snapshot on the event-pump thread. Losing window focus clears it to prevent held keys remaining stuck. Keyboard remains usable with connected pads; held I/J/K/L keys override their corresponding left-stick axes.
- E/R provide full left/right trigger input. Existing arrow, Enter/Backspace, Z/X/A/S and Q/W mappings remain unchanged.
- Each `GetState` clears the caller state before constructing the current combined input. Rumble stays opt-in with `LO_CONTROLLER_RUMBLE=1`; when enabled, requests are sent to all opened pads.

Implementation: [hid.cpp](../../LostOdysseyRecomp/hid/hid.cpp), [hid.h](../../LostOdysseyRecomp/hid/hid.h) and [video event pump](../../LostOdysseyRecomp/gpu/video.cpp).

## Validation and boundaries

`LoHidTest` compiles the actual HID implementation and uses SDL virtual devices. `out/hid-test.log` passed two-controller input, keyboard merge/release, stick deadzones and strongest-vector selection, trigger values, unplug/replug, state reset and duplicate add events with an external event pump. The full build passed in `out/build-multi-controller.log`.

The combined capture/input preview at `out/render-capture-preview/LostOdysseyRecomp.exe` has SHA256 `F5563D97CD1A71DFDAA06A6CEEFD5D32771D9A00AFCF5ED604D9C53879CAB2E0`. Its isolated title smoke run stayed near 30 fps through 31 seconds; a targeted Win32 Z key event reached the SDL window loop (`video key:122 repeat0` at 32.449 seconds in `out/multi-input-smoke/runtime.log`). This confirms the event path, not physical keyboard or gameplay acceptance; the virtual-device GetState tests provide the direct merged-input assertions. Physical controller models, platform-specific device mappings, real rumble behavior and gameplay switching have not been established by the virtual-device test. This is not a claim that every joystick is supported. All owned test processes were stopped and the original executable baseline (`135DCA79` hash prefix) was restored. The published v0.2.1 passed hosted CI 34053765472, including LoHidTest. Its official package passed manifest hashes, installer self-test and an isolated 54-second cold-cache German title-menu run; this adds release-artifact evidence, not physical-controller or gameplay acceptance.

## IME interference: open TODO (2026-09-06)

On another PC, the user reported that Z/X/A/S barely operated the game, then explicitly confirmed that switching to an English input method restored controls. This is the current workaround; v0.2.1 has no fix for this issue. The Debug Menu binding is F1, not the initially reported F6; F6 is unbound. The user confirmed the symptom and requested a TODO, not implementation or fix acceptance.

The code inspection found SDL text input enabled by default (`SDL_StartTextInput`) and no `SDL_StopTextInput` call when `video.cpp` creates the game window. This is a proposed mechanism and repair direction, not a runtime-verified root cause. Planned work: disable SDL text input/IME while handling game controls and enable it only for actual text-entry scenes. Do not change the user's global input method. Validate Z/X/A/S and F1 under a Chinese IME, focus changes and keyboard/controller switching before accepting a fix. No runtime code was changed for this TODO.

See [controls](../../README.md) and [current status](../STATUS.md).
