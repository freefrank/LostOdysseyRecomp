#pragma once

#include <cstdint>

struct PPCContext;

namespace save_menu_holds
{
// Runs the title tick (sub_8234B9B8, r3 = title object) without its fixed
// 1.0 s wait between the two save scans after Continue.
void TitleTick(PPCContext& ctx, uint8_t* base);
}
