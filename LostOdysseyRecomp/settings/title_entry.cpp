#include <stdafx.h>
#include <bit>
#include <os/logger.h>
#include <debug/auto_continue.h>
#include "menu.h"
#include "quit_action_hook.h"

extern "C" PPC_FUNC(__imp__sub_8234B9B8);
extern "C" PPC_FUNC(__imp__sub_8288AC10);
extern "C" PPC_FUNC(__imp__sub_824C0658);
extern "C" PPC_FUNC(__imp__sub_82B617B8);

namespace
{
// 8234B9B8 ticks the title object ([832644EC]): state at +0x14 and a queue of
// next states at +0x18 (data, count), which 8282B180 pops from the front.
// State 8 polls the title menu (New Game / Continue / A Thousand Years of
// Dreams). New Game queues state 42, which opens Settings in mode 1 after
// restoring the game defaults; state 43 waits until Settings is idle again,
// then pops the next state.
constexpr uint32_t TitleMenuState = 8;
constexpr uint32_t WaitForSettingsState = 43;
// 8288AC10 opens the Settings task from its idle state 1. Mode 0 keeps the
// current options, as the camp System menu (8287C9D0) does with its 0.8 s.
constexpr uint32_t CampOpenTime = 0x82000D50;
// The title actions (8282B540) confirm with this sound.
constexpr uint32_t SoundPlayer = 0x832D268C;
constexpr uint32_t ConfirmSound = 0x10000001;
// Nonzero sends state 43 to the profile error path (state 80) instead.
constexpr uint32_t ProfileError = 0x832631D0;
// All state belongs to the title guest thread.
bool entered = false;

void OpenSettings(PPCContext& ctx, uint8_t* base, uint32_t title)
{
    using settings::quit_action::SettingsMenu;
    if (PPC_LOAD_U32(SettingsMenu + 4) != 1 || PPC_LOAD_U32(title + 0x1C) != 0)
    {
        LOG_INFO("settings: title entry ignored (Settings state {}, queued title states {})",
            PPC_LOAD_U32(SettingsMenu + 4), PPC_LOAD_U32(title + 0x1C));
        return;
    }
    const PPCContext saved = ctx;
    ctx.r3.u64 = SettingsMenu;
    ctx.r4.u64 = 0;
    ctx.f1.f64 = std::bit_cast<float>(PPC_LOAD_U32(CampOpenTime));
    __imp__sub_8288AC10(ctx, base);
    ctx = saved;
    if (PPC_LOAD_U32(SettingsMenu + 4) == 1)
    {
        LOG_WARNING("settings: title entry could not open Settings");
        return;
    }
    settings::MarkTitleEntry();
    // Back to the title menu once Settings closes, through New Game's wait state.
    ctx.r3.u64 = 4;
    ctx.r4.u64 = title + 0x18;
    __imp__sub_824C0658(ctx, base);
    const uint32_t next = ctx.r3.u32;
    ctx = saved;
    if (next)
    {
        PPC_STORE_U32(next, TitleMenuState);
        PPC_STORE_U32(title + 0x14, WaitForSettingsState);
        entered = true;
    }
    else
        LOG_WARNING("settings: title entry could not queue the title menu; it stays in state {}", TitleMenuState);
    ctx.r3.u64 = PPC_LOAD_U32(SoundPlayer);
    ctx.r4.u64 = ConfirmSound;
    ctx.r5.u64 = 0;
    __imp__sub_82B617B8(ctx, base);
    ctx = saved;
    LOG_INFO("settings: title entry opened Settings (profile error {})", PPC_LOAD_U32(ProfileError));
}
}

PPC_FUNC(sub_8234B9B8)
{
    debug_menu::AutoContinueAdvance(ctx, base);
    const uint32_t title = ctx.r3.u32;
    const uint32_t state = PPC_LOAD_U32(title + 0x14);
    if (entered && state != WaitForSettingsState)
    {
        entered = false;
        LOG_INFO("settings: title entry closed, title state {}", state);
    }
    if (settings::ConsumeTitleShortcut(state == TitleMenuState))
        OpenSettings(ctx, base, title);
    __imp__sub_8234B9B8(ctx, base);
}
