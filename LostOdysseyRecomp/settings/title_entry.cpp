#include <stdafx.h>
#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <os/logger.h>
#include <debug/auto_continue.h>
#include <hid/hid.h>
#include <kernel/io/file_system.h>
#include "config.h"
#include "menu.h"
#include "menu_assets.h"
#include "menu_render.h"
#include "quit_action_hook.h"
#include "title_entry.h"
#include "translations.h"

extern "C" PPC_FUNC(__imp__sub_8234B9B8);
extern "C" PPC_FUNC(__imp__sub_8288AC10);
extern "C" PPC_FUNC(__imp__sub_824C0658);
extern "C" PPC_FUNC(__imp__sub_82B617B8);

namespace
{
using settings::quit_action::SettingsMenu;
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

// The menu waits for input and Y would open Settings now.
bool MenuIdle(uint8_t* base, uint32_t title)
{
    return PPC_LOAD_U32(title + 0x14) == TitleMenuState && PPC_LOAD_U32(SettingsMenu + 4) == 1 &&
           PPC_LOAD_U32(title + 0x1C) == 0;
}

void OpenSettings(PPCContext& ctx, uint8_t* base, uint32_t title)
{
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

// The legend appears a moment after the menu settles and fades in; the title
// tick refreshes it every frame, so a stopped tick (loading) hides it too.
using Clock = std::chrono::steady_clock;
constexpr auto HintDelay = std::chrono::milliseconds(200);
constexpr auto HintFade = std::chrono::milliseconds(250);
constexpr auto HintStale = std::chrono::milliseconds(250);
std::atomic<Clock::rep> hintIdleSince{0}, hintTick{0};

void ReportHint(bool idle)
{
    const auto now = Clock::now().time_since_epoch().count();
    if (!idle)
        hintIdleSince = 0;
    else if (!hintIdleSince.load())
        hintIdleSince = now;
    hintTick = now;
}
}

float settings::title_entry::HintOpacity()
{
    const auto since = hintIdleSince.load(), tick = hintTick.load();
    if (!since || settings::IsOpen())
        return 0.0f;
    const auto now = Clock::now().time_since_epoch().count();
    if (Clock::duration(now - tick) > HintStale)
        return 0.0f;
    const double shown = std::chrono::duration<double>(Clock::duration(now - since) - HintDelay) /
                         std::chrono::duration<double>(HintFade);
    return float(std::clamp(shown, 0.0, 1.0));
}

const settings::title_entry::Hint* settings::title_entry::DrawHint(uint32_t outputWidth, uint32_t outputHeight)
{
    static Hint hint;
    static uint32_t cachedWidth = 0, cachedHeight = 0, cachedLanguage = ~0u;
    static int cachedPrompts = -1;
    static std::shared_ptr<const menu_assets::Assets> cachedAssets;
    const uint32_t language = GetConfig().uiLanguage;
    const bool keyboard = hid::UsesKeyboardPrompts();
    const int prompts = keyboard ? 2 : hid::UsesPlayStationPrompts() ? 1 : 0;
    auto assets = menu_assets::Cached(FileSystem::GetGameRoot(), language);
    if (outputWidth != cachedWidth || outputHeight != cachedHeight || language != cachedLanguage ||
        prompts != cachedPrompts || assets != cachedAssets)
    {
        cachedWidth = outputWidth;
        cachedHeight = outputHeight;
        cachedLanguage = language;
        cachedPrompts = prompts;
        cachedAssets = assets;
        hint.pixels.clear();
        hint.x = hint.y = hint.width = hint.height = 0;
        ++hint.revision;
        MenuSnapshot snapshot;
        snapshot.titleHint = true;
        snapshot.language = language;
        snapshot.playStationPrompts = prompts == 1;
        snapshot.assets = assets;
        // The menu's help line convention; keyboard players see the S key.
        snapshot.help = Translate(language, L"Y: Settings", L"Y：設定");
        if (keyboard)
            snapshot.help[0] = L'S';
        std::vector<uint32_t> full;
        if (RasterizeMenu(snapshot, outputWidth, outputHeight, full))
        {
            uint32_t x0 = outputWidth, y0 = outputHeight, x1 = 0, y1 = 0;
            for (uint32_t y = 0; y < outputHeight; ++y)
                for (uint32_t x = 0; x < outputWidth; ++x)
                    if (full[size_t(y) * outputWidth + x] >> 24)
                    {
                        x0 = std::min(x0, x);
                        y0 = std::min(y0, y);
                        x1 = std::max(x1, x + 1);
                        y1 = std::max(y1, y + 1);
                    }
            if (x0 < x1)
            {
                hint.x = x0;
                hint.y = y0;
                hint.width = x1 - x0;
                hint.height = y1 - y0;
                hint.pixels.resize(size_t(hint.width) * hint.height);
                for (uint32_t y = 0; y < hint.height; ++y)
                    std::copy_n(&full[size_t(y0 + y) * outputWidth + x0], hint.width, &hint.pixels[size_t(y) * hint.width]);
            }
        }
    }
    return hint.width ? &hint : nullptr;
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
    if (settings::ConsumeTitleShortcut(MenuIdle(base, title)))
        OpenSettings(ctx, base, title);
    ReportHint(MenuIdle(base, title));
    __imp__sub_8234B9B8(ctx, base);
}
