#include <stdafx.h>
#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <os/logger.h>
#include <debug/auto_continue.h>
#include <hid/hid.h>
#include <kernel/io/file_system.h>
#include <patches/save_menu_holds.h>
#include "config.h"
#include "menu.h"
#include "menu_assets.h"
#include "menu_render.h"
#include "quit_action_hook.h"
#include "title_entry.h"
#include "title_input_gate.h"
#include "translations.h"

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
    // Queue the title menu first: once Settings closes, New Game's wait state
    // pops it. Without a slot, Settings stays closed.
    ctx.r3.u64 = 4;
    ctx.r4.u64 = title + 0x18;
    __imp__sub_824C0658(ctx, base);
    const uint32_t next = ctx.r3.u32;
    ctx = saved;
    if (!next)
    {
        LOG_WARNING("settings: title entry could not queue the title menu");
        return;
    }
    PPC_STORE_U32(next, TitleMenuState);
    ctx.r3.u64 = SettingsMenu;
    ctx.r4.u64 = 0;
    ctx.f1.f64 = std::bit_cast<float>(PPC_LOAD_U32(CampOpenTime));
    __imp__sub_8288AC10(ctx, base);
    ctx = saved;
    if (PPC_LOAD_U32(SettingsMenu + 4) == 1)
    {
        // Drop the queued title menu again (the array's count).
        PPC_STORE_U32(title + 0x1C, PPC_LOAD_U32(title + 0x1C) - 1);
        LOG_WARNING("settings: title entry could not open Settings");
        return;
    }
    settings::MarkTitleEntry();
    PPC_STORE_U32(title + 0x14, WaitForSettingsState);
    entered = true;
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
// A drag-resize changes the output size every frame: the legend waits until
// the size holds this long before it is rasterized again.
constexpr auto ResizeSettle = std::chrono::milliseconds(150);
std::atomic<Clock::rep> hintIdleSince{0}, hintTick{0};
// Presentation owns the closing fade after the guest has dismissed the modal.
std::atomic<bool> dialogDrawn{false};

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

bool settings::title_entry::HintShown()
{
    if (settings::IsTitleQuitOpen() || dialogDrawn.load())
        return true;
    const auto since = hintIdleSince.load(), tick = hintTick.load();
    return since && !settings::IsOpen() &&
           Clock::duration(Clock::now().time_since_epoch().count() - tick) <= HintStale;
}

float settings::title_entry::HintOpacity()
{
    // The modal's own straight-alpha raster carries its 100 ms fade, including
    // the dim. The footer keeps its original delayed opacity below.
    if (settings::IsTitleQuitOpen() || dialogDrawn.load())
        return 1.0f;
    if (!HintShown())
        return 0.0f;
    const auto since = hintIdleSince.load();
    const auto now = Clock::now().time_since_epoch().count();
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
    static uint32_t pendingWidth = 0, pendingHeight = 0;
    static Clock::time_point pendingSince;
    static uint64_t dialogRevision = 0, uploadedDialogRevision = 0;
    if (settings::DrawTitleQuit(hint.pixels, dialogRevision, outputWidth, outputHeight))
    {
        const bool opened = !dialogDrawn.exchange(true);
        if (opened || uploadedDialogRevision != dialogRevision)
            ++hint.revision;
        uploadedDialogRevision = dialogRevision;
        hint.x = hint.y = 0;
        hint.width = outputWidth;
        hint.height = outputHeight;
        return &hint;
    }
    if (dialogDrawn.exchange(false))
        cachedWidth = 0; // Restore the footer after its full-output modal raster.
    if (cachedWidth && (outputWidth != cachedWidth || outputHeight != cachedHeight))
    {
        const auto now = Clock::now();
        if (outputWidth != pendingWidth || outputHeight != pendingHeight)
        {
            pendingWidth = outputWidth;
            pendingHeight = outputHeight;
            pendingSince = now;
        }
        if (now - pendingSince < ResizeSettle)
            return nullptr;
    }
    else
        pendingWidth = pendingHeight = 0;
    const uint32_t language = GetConfig().uiLanguage;
    const bool keyboard = hid::UsesKeyboardPrompts();
    const int prompts = hid::PromptFamily(); // redrawn when the icon family changes
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
        snapshot.playStationPrompts = hid::UsesPlayStationPrompts();
        snapshot.promptFamily = hid::PromptFamily();
        snapshot.promptIcons = PromptIcons(snapshot.promptFamily);
        snapshot.assets = assets;
        // The menu's help line convention; keyboard players see the S key.
        snapshot.help = Translate(language, L"Y: Settings", L"Y：設定");
        if (keyboard)
            snapshot.help[0] = L'S';
        snapshot.titleQuitKey = keyboard ? L"Backspace" : L"Back";
        snapshot.titleQuitLabel = Translate(language, L"Quit game", L"退出遊戲");
        // Only the credit and legend's footer rectangle is rasterized.
        const auto bounds = TitleHintBounds(outputWidth, outputHeight);
        std::vector<uint32_t> pixels;
        if (RasterizeMenu(snapshot, outputWidth, outputHeight, pixels) &&
            pixels.size() == (bounds.x1 - bounds.x0) * (bounds.y1 - bounds.y0))
        {
            hint.x = uint32_t(bounds.x0);
            hint.y = uint32_t(bounds.y0);
            hint.width = uint32_t(bounds.x1 - bounds.x0);
            hint.height = uint32_t(bounds.y1 - bounds.y0);
            hint.pixels = std::move(pixels);
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
    const bool idle = MenuIdle(base, title);
    const bool quitOwnsInput = settings::UpdateTitleQuit(base, idle);
    if (settings::ConsumeTitleShortcut(idle && !quitOwnsInput))
        OpenSettings(ctx, base, title);
    ReportHint(MenuIdle(base, title) && !quitOwnsInput);
    settings::title_entry::TickWithMenuInputMuted(quitOwnsInput, ctx.r4.u32,
        [base](uint32_t address) { return PPC_LOAD_U32(address); },
        [base](uint32_t address, uint32_t mask) { PPC_STORE_U32(address, mask); },
        [&] { save_menu_holds::TitleTick(ctx, base); });
}
