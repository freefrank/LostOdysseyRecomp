#include <stdafx.h>
#include <algorithm>
#include <iterator>
#include "save_menu_holds.h"

extern "C" PPC_FUNC(__imp__sub_822ED938);
extern "C" PPC_FUNC(__imp__sub_8234B9B8);
extern "C" PPC_FUNC(__imp__sub_8285FD88);
extern "C" PPC_FUNC(__imp__sub_8285FDA8);

namespace debug_menu::menu_animations
{
// debug/menu_animations.cpp: the save page's animation scope around the original.
void TickSaveLoadMenu(PPCContext& ctx, uint8_t* base);
}

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

// Title tick states that wait until a time has passed since the start time
// at +164 (float seconds), with nothing else to wait for:
// 27: after Continue, once the first save scan is done, 1.0 s (0x82000F28)
//     before the second scan.
// 85: after START, state 84 shows the profile check message, stores the
//     start time and holds 0.5 s (0x82000F70) before 86 requests the profile
//     data (achievements and DLC, on the XDK thread) and 87 waits for it.
// 83: after that request, holds until 1.0 s (0x82000F28) since state 84, then
//     hides the message and opens the title menu.
// Later states read +164 too, so the start moves back only for the call. A
// minute back clears every hold, also at the float's coarse resolution after
// a long uptime.
constexpr uint32_t TitleStateOffset = 20;
constexpr uint32_t TitleHoldStates[] = {27, 83, 85};
constexpr uint32_t TitleStartOffset = 164;
constexpr float TitleHoldSkip = 60.0f;

// Message 6 of 8285FD88 is the profile check ("Updating gamer profile...",
// text 12139). Without the holds it would only flash while 87 waits, so it
// is not shown, and 83 does not hide it.
constexpr uint32_t ProfileCheckMessage = 6;
constexpr uint32_t ShowProfileCheckState = 85;
constexpr uint32_t HideProfileCheckState = 83;
// Set on the title thread while the title tick runs.
thread_local uint32_t tickingTitle = 0;
bool profileMessageSkipped = false;
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
    debug_menu::menu_animations::TickSaveLoadMenu(ctx, base);
}

PPC_FUNC(sub_8285FD88)
{
    if (tickingTitle)
    {
        profileMessageSkipped = ctx.r4.u32 == ProfileCheckMessage &&
                                PPC_LOAD_U32(tickingTitle + TitleStateOffset) == ShowProfileCheckState;
        if (profileMessageSkipped)
            return;
    }
    __imp__sub_8285FD88(ctx, base);
}

PPC_FUNC(sub_8285FDA8)
{
    if (tickingTitle && profileMessageSkipped && PPC_LOAD_U32(tickingTitle + TitleStateOffset) == HideProfileCheckState)
    {
        profileMessageSkipped = false;
        return;
    }
    __imp__sub_8285FDA8(ctx, base);
}

void save_menu_holds::TitleTick(PPCContext& ctx, uint8_t* base)
{
    const uint32_t title = ctx.r3.u32;
    const uint32_t state = PPC_LOAD_U32(title + TitleStateOffset);
    const bool hold = std::find(std::begin(TitleHoldStates), std::end(TitleHoldStates), state) != std::end(TitleHoldStates);
    PPCRegister start{}, earlier{};
    if (hold)
    {
        start.u32 = PPC_LOAD_U32(title + TitleStartOffset);
        earlier.f32 = start.f32 - TitleHoldSkip;
        PPC_STORE_U32(title + TitleStartOffset, earlier.u32);
    }
    tickingTitle = title;
    __imp__sub_8234B9B8(ctx, base);
    tickingTitle = 0;
    if (hold && PPC_LOAD_U32(title + TitleStartOffset) == earlier.u32)
        PPC_STORE_U32(title + TitleStartOffset, start.u32);
}
