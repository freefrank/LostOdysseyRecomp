#pragma once
#include <cstdint>
#include <vector>

// The achievement-unlocked toast over game frames (achievements/achievements.h
// queues the unlocks, ui/achievement_toast.h draws one). Presentation thread.
namespace ui::achievement_overlay {
struct Image {
    std::vector<uint32_t> pixels; // straight-alpha RGBA8 (R in the low byte)
    uint32_t width = 0, height = 0;
    int x = 0, y = 0;             // top-left in the output
};
// Once per presented game frame: the toast to draw over an output of this
// size, or null while none shows. The toast's clock only runs while game
// frames are presented, so a host menu or a load holds it.
const Image* Update(uint32_t outputWidth, uint32_t outputHeight);
}
