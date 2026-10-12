#pragma once
#include <cstdint>
#include <string>
#include "input_map.h"

namespace hid
{
    void Init();
    void Poll();
    // When another thread owns the SDL event loop (the video thread), it
    // forwards controller hot-plug events here and Poll() stops pumping.
    void SetExternalEventPump(bool external);
    void HandleControllerEvent(uint32_t eventType, uint32_t joystickId);
    void HandleKeyboardEvent(int32_t scancode, bool pressed);
    void ClearKeyboardState();
    void PumpHostInput();
    // Atomic presentation hint for host UI; does not change the guest's buttons.
    bool UsesPlayStationPrompts();
    // Auto prompts after a key press: host UI names keyboard keys instead.
    bool UsesKeyboardPrompts();
    // Button icon style: 0 Auto (follow the last active controller), 1 Xbox, 2 PlayStation.
    void SetPromptStyle(uint32_t style);
    // Physical SDL controllers may be discovered through HIDAPI without an Android InputDevice.
    bool HasConnectedController();
    // Player rumble strength, 0-100 percent of the guest's motor speeds (100 = retail).
    // A change rescales a rumble that is already running.
    void SetVibrationStrength(uint32_t percent);
    // Short pulse at the strongest guest level and the current strength, so the
    // player can feel the setting. Skipped while the guest is rumbling.
    void PreviewVibration();
    // Settings → Gameplay → Controls layouts (input_map.h), applied from the next poll.
    void SetInputMaps(const input_map::ControllerMap& controller, const input_map::KeyboardMap& keyboard);
    // Bit s for each input_map source held on any controller: a capture on the
    // Controls page records the next one.
    uint32_t PressedSources();
    // The key's name in the current keyboard layout (SDL), for the Controls
    // page. The thread that owns the SDL event loop refreshes the names at
    // Init and on SDL_EVENT_KEYMAP_CHANGED.
    std::string KeyName(uint32_t scancode);
    void RefreshKeyNames();

    uint32_t GetState(uint32_t dwUserIndex, XAMINPUT_STATE* pState);
    uint32_t SetState(uint32_t dwUserIndex, XAMINPUT_VIBRATION* pVibration);
    uint32_t GetCapabilities(uint32_t dwUserIndex, XAMINPUT_CAPABILITIES* pCaps);
}
