#pragma once
#include <cstdint>

namespace debug_menu::loading_state {
// The native manager at 0x832631F0: sub_8231F5E0 changes the selected
// request from queued (1) to accepted (2), and publishes stage 4 only after
// package loads/unloads settle. sub_8231FBF8 then activates the new scene.
// In particular a queued encounter may be held during ordinary field play.
constexpr bool Loading(uint8_t stage, uint8_t current, int32_t phase) {
    return current < 6 && phase == 2 && stage < 4;
}
}
