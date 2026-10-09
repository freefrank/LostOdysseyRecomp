#pragma once

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

    uint32_t GetState(uint32_t dwUserIndex, XAMINPUT_STATE* pState);
    uint32_t SetState(uint32_t dwUserIndex, XAMINPUT_VIBRATION* pVibration);
    uint32_t GetCapabilities(uint32_t dwUserIndex, XAMINPUT_CAPABILITIES* pCaps);
}
