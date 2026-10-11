#include <stdafx.h>
#include <algorithm>
#include <iterator>
#include "save_menu_holds.h"

extern "C" PPC_FUNC(__imp__sub_822ED938);
extern "C" PPC_FUNC(__imp__sub_8234B9B8);

namespace
{
// The save/load menu state machine (r3 = menu object, stage at +20) keeps
// four message stages on screen until its stage timer (+96, float seconds,
// reset on entry) passes 3.0 s (0x82000DE8): 22 and 63 after the slot list
// is read, 38 after a save is written, 51 after a load. Each only compares
// the timer, closes the message and moves on; the reads and writes finished
// in the stages before, and the next stage waits for async loading itself.
constexpr uint32_t StageOffset = 20;
constexpr uint32_t TimerOffset = 96;
constexpr uint32_t HoldStages[] = {22, 38, 51, 63};
constexpr float ExpiredTimer = 4.0f;

// After Continue on the title, state 27 of the title tick waits for the first
// save scan to finish and then until 1.0 s (0x82000F28) has passed since the
// start time it stored at +164 (float seconds) before it starts the second
// scan. Later states read +164 too, so move the start back only for the call.
constexpr uint32_t TitleStateOffset = 20;
constexpr uint32_t TitleScanState = 27;
constexpr uint32_t TitleScanStartOffset = 164;
constexpr float TitleScanWait = 1.0f;
}

PPC_FUNC(sub_822ED938)
{
    const uint32_t menu = ctx.r3.u32;
    const uint32_t stage = PPC_LOAD_U32(menu + StageOffset);
    if (std::find(std::begin(HoldStages), std::end(HoldStages), stage) != std::end(HoldStages))
    {
        PPCRegister timer{};
        timer.f32 = ExpiredTimer;
        PPC_STORE_U32(menu + TimerOffset, timer.u32);
    }
    __imp__sub_822ED938(ctx, base);
}

void save_menu_holds::TitleTick(PPCContext& ctx, uint8_t* base)
{
    const uint32_t title = ctx.r3.u32;
    if (PPC_LOAD_U32(title + TitleStateOffset) != TitleScanState)
    {
        __imp__sub_8234B9B8(ctx, base);
        return;
    }
    PPCRegister start{}, earlier{};
    start.u32 = PPC_LOAD_U32(title + TitleScanStartOffset);
    earlier.f32 = start.f32 - TitleScanWait;
    PPC_STORE_U32(title + TitleScanStartOffset, earlier.u32);
    __imp__sub_8234B9B8(ctx, base);
    if (PPC_LOAD_U32(title + TitleScanStartOffset) == earlier.u32)
        PPC_STORE_U32(title + TitleScanStartOffset, start.u32);
}
