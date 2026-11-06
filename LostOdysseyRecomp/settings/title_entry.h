#pragma once

#include <cstdint>
#include <vector>

namespace settings::title_entry
{
// The title menu's free-port credit and Settings / Quit legends, or its modal, drawn
// by presentation over the game frame. Shown while the title menu is idle and shortcuts are available,
// not otherwise (Settings open, transitions, Press START); its opacity starts
// after a short delay, in which presentation can prepare it, and fades in.
bool HintShown();
float HintOpacity();
// The legend (or full-output title confirmation) as straight-alpha RGBA at
// x, y. Rasterized again, with a new revision, only when the size, language,
// button style or menu assets change, and during modal fades; null while a new size settles (window
// drag-resize). Presentation thread.
struct Hint
{
    std::vector<uint32_t> pixels;
    uint32_t x = 0, y = 0, width = 0, height = 0;
    uint64_t revision = 0;
};
const Hint *DrawHint(uint32_t outputWidth, uint32_t outputHeight);
} // namespace settings::title_entry
