#pragma once

namespace hid
{
    void Init();
    void Poll();
    // When another thread owns the SDL event loop (the video thread), it
    // forwards controller hot-plug events here and Poll() stops pumping.
    void SetExternalEventPump(bool external);
    void HandleControllerEvent(uint32_t eventType, int32_t deviceIndexOrInstance);

    uint32_t GetState(uint32_t dwUserIndex, XAMINPUT_STATE* pState);
    uint32_t SetState(uint32_t dwUserIndex, XAMINPUT_VIBRATION* pVibration);
    uint32_t GetCapabilities(uint32_t dwUserIndex, XAMINPUT_CAPABILITIES* pCaps);
}
