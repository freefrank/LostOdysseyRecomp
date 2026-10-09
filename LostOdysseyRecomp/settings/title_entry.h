#pragma once

#include <cstdint>
#include <vector>

namespace settings::title_entry
{
// The title menu's Settings legend ("Y: Settings"), drawn by presentation over
// the game frame. Opacity fades in while the title menu is idle and Y would
// open Settings, and is 0 otherwise (Settings open, transitions, Press START).
float HintOpacity();
// The legend for an output size as straight-alpha RGBA, cropped to its own
// rectangle at x, y. Rasterized again, with a new revision, only when the
// size, language, button style or menu assets change. Presentation thread.
struct Hint
{
    std::vector<uint32_t> pixels;
    uint32_t x = 0, y = 0, width = 0, height = 0;
    uint64_t revision = 0;
};
const Hint *DrawHint(uint32_t outputWidth, uint32_t outputHeight);
} // namespace settings::title_entry
