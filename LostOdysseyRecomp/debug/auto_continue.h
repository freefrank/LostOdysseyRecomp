#pragma once

#include <cstdint>

struct PPCContext;

namespace debug_menu
{
    // Opt-in, once per process: use the game's native newest-save Continue flow.
    // Does not select an arbitrary slot or persist a setting.
    bool AutoContinueEnabled();
    // One step on the title tick (sub_8234B9B8, settings/title_entry.cpp), before
    // the original runs. ctx.r3 is the title object.
    void AutoContinueAdvance(PPCContext& ctx, uint8_t* base);
}
