#pragma once

#include <cstdint>

namespace hid::android_touch
{
struct State
{
    uint16_t buttons = 0;
    uint8_t leftTrigger = 0;
    uint8_t rightTrigger = 0;
    int16_t leftX = 0;
    int16_t leftY = 0;
    int16_t rightX = 0;
    int16_t rightY = 0;
};

// The Android UI publishes a complete snapshot after every touch change.
void Update(int32_t buttons, int32_t leftTrigger, int32_t rightTrigger,
            int32_t leftX, int32_t leftY, int32_t rightX, int32_t rightY);
// Merged on-screen state: the main-screen touch controls plus the buttons
// held on the dual-screen companion (second display).
State Snapshot();
void Clear();
// Buttons held on the dual-screen companion. Kept apart from Update() so the
// main-screen controls never release a button the companion is holding.
void SetCompanion(int32_t buttons, int32_t leftTrigger, int32_t rightTrigger);
}
