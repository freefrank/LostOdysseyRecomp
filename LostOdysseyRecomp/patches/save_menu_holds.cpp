#include <stdafx.h>
#include <algorithm>
#include <iterator>

extern "C" PPC_FUNC(__imp__sub_822ED938);

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
