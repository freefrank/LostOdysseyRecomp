#include "menu.h"
#include "menu_render.h"
#include "config.h"
#include "restart.h"
#include "translations.h"
#include <gpu/video.h>
#include <os/logger.h>
#include <stdafx.h>
extern "C" PPC_FUNC(__imp__sub_822F19B0);
extern "C" PPC_FUNC(__imp__sub_82481BE8);
extern "C" PPC_FUNC(__imp__sub_82870E38);
extern "C" PPC_FUNC(__imp__sub_828710A0);
namespace settings
{
namespace
{
std::atomic<bool> active{false};
std::atomic<uint16_t> pending{0};
std::atomic<unsigned> cancelPolls{0};
std::atomic<uint16_t> cancelButton{0x2000};
std::atomic<bool> swapConfirm{false};
std::atomic<bool> waitForRelease{true};
std::atomic<int> mouseTab{-1}, mouseRow{-1};
std::atomic<int> mouseDialog{-1};
std::atomic<uint16_t> mouseAction{0};
std::mutex snapshotMutex;
using Row = MenuRow;
using Snapshot = MenuSnapshot;
Snapshot snapshot;
Config edit;
Config previousDisplay;
bool displayPreview = false;
bool restartPrompt = false, restartRollbackPreview = false, restartSaveFailed = false;
int restartChoice = 0;
Config restartBefore, restartAfter;
std::chrono::steady_clock::time_point previewDeadline{};
int tab = 0, row = 0;
bool bypass = false, sawModal = false;
uint32_t lastMenu = 0;
std::wstring status;
constexpr uint32_t resolutions[][2] = {{1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160}};
constexpr int internalResolutions[] = {0, 720, 1080, 1440, 2160};
const wchar_t *Tr(const wchar_t *en, const wchar_t *zh)
{
    return Translate(edit.uiLanguage, en, zh);
}
uint32_t ConfigAddress(uint8_t *base)
{
    uint32_t object = PPC_LOAD_U32(0x8326A068);
    if (!object)
        return 0;
    uint32_t storage = PPC_LOAD_U32(object + 4);
    return storage ? PPC_LOAD_U32(storage + 0x18) : 0;
}
uint32_t VoiceCount(uint8_t *base)
{
    // The original 82482028/82482038 access this resource-populated list.
    return std::clamp(uint32_t(PPC_LOAD_U8(0x8336A5F0 + 419)), 1u, 10u);
}
uint32_t VoiceLanguage(uint8_t *base, uint32_t index)
{
    return PPC_LOAD_U16(0x8336A5F0 + 288 + std::min(index, VoiceCount(base) - 1) * 2);
}
const wchar_t *VoiceName(uint8_t *base, uint32_t index)
{
    constexpr const wchar_t *names[] = {L"English", L"English", L"日本語", L"Deutsch", L"Français",
                                       L"Español", L"Italiano", L"한국어", L"繁體中文", L"简体中文"};
    const auto language = VoiceLanguage(base, index);
    return language < std::size(names) ? names[language] : L"Unknown";
}
void Publish(uint8_t *base, uint32_t config)
{
    Snapshot next;
    next.tab = tab;
    next.row = row;
    next.language = edit.uiLanguage;
    const uint32_t flags = PPC_LOAD_U32(config + 4);
    auto addChoices = [&](const wchar_t *en, const wchar_t *zh, std::vector<std::wstring> choices,
                          uint32_t selected, bool enabled = true) {
        selected = choices.empty() ? 0 : std::min(selected, uint32_t(choices.size() - 1));
        const std::wstring value = choices.empty() ? std::wstring{} : choices[selected];
        next.rows.push_back({Tr(en, zh), value, enabled, std::move(choices), int(selected)});
    };
    auto addAction = [&](const wchar_t *en, const wchar_t *zh, const wchar_t *value) {
        addChoices(en, zh, {value}, 0);
    };
    auto addSlider = [&](const wchar_t *en, const wchar_t *zh, uint32_t percent) {
        next.rows.push_back({Tr(en, zh), std::to_wstring(percent) + L"%", true, {}, 0,
                             int(std::min(percent, 100u))});
    };
    auto onOff = [&]() {
        return std::vector<std::wstring>{Tr(L"On", L"開"), Tr(L"Off", L"關")};
    };
    if (tab == 0)
    {
        addChoices(L"Text speed", L"文字速度", {Tr(L"Fast", L"快"), Tr(L"Normal", L"正常"),
                                                    Tr(L"Slow", L"慢")},
                   std::min(PPC_LOAD_U32(config), 2u));
        addChoices(L"Captions", L"字幕", onOff(), (flags & 0x40000000) ? 0 : 1);
        addChoices(L"Remember battle cursor", L"記住戰鬥游標", onOff(),
                   (flags & 0x10000000) ? 0 : 1);
        addChoices(L"Automatic back-row input", L"後排自動輸入", onOff(),
                   (flags & 0x00800000) ? 0 : 1);
        addChoices(L"Invert camera vertically", L"反轉鏡頭上下", onOff(),
                   (flags & 0x08000000) ? 0 : 1);
        addChoices(L"Invert camera horizontally", L"反轉鏡頭左右", onOff(),
                   (flags & 0x04000000) ? 0 : 1);
        addChoices(L"Confirmation button", L"確認按鍵", {L"A / B", L"B / A"},
                   (flags & 0x02000000) ? 1 : 0);
        next.rows.back().controllerButtons = true;
        addAction(L"Restore game defaults", L"恢復遊戲預設設定", Tr(L"Restore", L"恢復"));
    }
    else if (tab == 1)
    {
        std::vector<std::wstring> voices;
        for (uint32_t i = 0; i < VoiceCount(base); ++i)
            voices.emplace_back(VoiceName(base, i));
        addChoices(L"Voice language", L"語音語言", std::move(voices), PPC_LOAD_U32(config + 24));
        addSlider(L"Music", L"音樂音量", PPC_LOAD_U32(config + 8));
        addSlider(L"Sound effects", L"音效音量", PPC_LOAD_U32(config + 12));
    }
    else if (tab == 2)
    {
        addChoices(L"Graphics backend", L"圖形後端", {L"Direct3D 12", L"Vulkan"},
                   uint32_t(edit.graphicsBackend));
        addChoices(L"Display mode", L"顯示模式",
                   {Tr(L"Windowed", L"視窗"), Tr(L"Borderless fullscreen", L"無邊框全螢幕"),
                    Tr(L"Exclusive fullscreen", L"獨占全螢幕")},
                   uint32_t(edit.windowMode));
        std::vector<std::wstring> outputChoices;
        uint32_t outputChoice = 0;
        for (uint32_t i = 0; i < std::size(resolutions); ++i)
        {
            outputChoices.push_back(std::to_wstring(resolutions[i][0]) + L" × " +
                                    std::to_wstring(resolutions[i][1]));
            if (edit.width == resolutions[i][0]) outputChoice = i;
        }
        addChoices(L"Output resolution", L"輸出解析度", std::move(outputChoices), outputChoice);
        std::vector<std::wstring> internalChoices{Tr(L"Auto (match output)", L"自動（跟隨輸出）")};
        uint32_t internalChoice = 0;
        for (uint32_t i = 1; i < std::size(internalResolutions); ++i)
        {
            internalChoices.push_back(std::to_wstring(internalResolutions[i] * 16 / 9) + L" × " +
                                      std::to_wstring(internalResolutions[i]));
            if (edit.internalResolution == internalResolutions[i]) internalChoice = i;
        }
        addChoices(L"Internal resolution", L"內部解析度", std::move(internalChoices), internalChoice);
        addChoices(L"Anti-aliasing", L"抗鋸齒",
                   {Tr(L"Off", L"關"), L"FXAA", L"SMAA", Tr(L"TAA (Experimental)", L"TAA（實驗性）")},
                   std::min(edit.antialiasing, 3u));
        addChoices(L"Upscaling quality", L"縮放品質",
                   {Tr(L"Standard", L"標準"), Tr(L"High", L"高")},
                   std::min(edit.scalingQuality, 1u));
        addChoices(L"Frame rate", L"影格率",
                   {L"30 FPS", std::wstring(L"60 FPS") + Tr(L" (experimental)", L"（實驗性）"),
                    std::wstring(L"120 FPS") + Tr(L" (experimental)", L"（實驗性）")},
                   edit.frameRate == 120 ? 2 : edit.frameRate == 60 ? 1 : 0);
        addAction(L"Brightness calibration", L"亮度校準", Tr(L"Open", L"開啟"));
        addAction(L"Apply display settings", L"套用顯示設定",
                  displayPreview ? Tr(L"Keep changes", L"保留更改") : Tr(L"Apply", L"套用"));
    }
    else
    {
        std::vector<std::wstring> uiLanguages(std::begin(UiLanguageNames), std::end(UiLanguageNames));
        addChoices(L"Settings language", L"設定界面語言", std::move(uiLanguages), edit.uiLanguage);
        std::vector<std::wstring> gameLanguages;
        for (const auto name : GameLanguageNames) gameLanguages.emplace_back(name);
        addChoices(L"Game language", L"遊戲語言", std::move(gameLanguages), GameLanguageIndex(edit.gameLanguage));
        addAction(L"Save settings", L"儲存設定", Tr(L"Save", L"儲存"));
    }
    next.help = status.empty() ? Tr(L"LB / RB: category     D-pad: select / change     A: select     B: back",
                                    L"LB / RB：分類     方向鍵：選擇 / 調整     A：確認     B：返回")
                               : status;
    if (status.empty() && (flags & 0x02000000))
        next.help = Tr(L"LB / RB: category     D-pad: select / change     B: select     A: back",
                       L"LB / RB：分類     方向鍵：選擇 / 調整     B：確認     A：返回");
    if (tab == 3 && row == 1)
        next.help = Tr(L"Game language takes effect after restarting. Requires matching language assets.",
                       L"遊戲語言重新啟動後生效，需要對應語言資源。中文遊戲文本需要亞洲版資源。");
    if (tab == 2 && row == 0)
        next.help = Tr(L"The graphics backend is changed after restarting. LO_GRAPHICS_API remains a diagnostic override.",
                       L"圖形後端重新啟動後變更；LO_GRAPHICS_API 仍可作為診斷覆寫。 ");
    if (tab == 2 && row == 2)
        next.help = Tr(L"Sets the output size. Borderless fullscreen uses the desktop size.",
                       L"設定輸出尺寸；無邊框全螢幕使用桌面尺寸。");
    if (tab == 2 && row == 3)
        next.help = Tr(L"Scene detail up to 4K. Auto follows output; higher values use more GPU power.",
                       L"場景細節最高 4K。自動跟隨輸出；較高解析度需要更多 GPU 效能。");
    if (tab == 2 && row == 4 && edit.antialiasing == 3)
        next.help = Tr(L"Camera-based TAA; moving effects may trail. Unsupported scenes use SMAA.",
                       L"以相機重投影的 TAA；動態特效可能拖影。不支援的場景使用 SMAA。");
    if (tab == 2 && row == 5)
        next.help = Tr(L"Controls filtering when internal and output sizes differ. Scene detail uses Internal resolution.",
                       L"控制內部與輸出尺寸不同時的取樣濾鏡。場景細節由內部解析度決定。");
    if (tab == 2 && row == 6)
        next.help = edit.frameRate == 120
            ? Tr(L"120 FPS is experimental and requires LO_EXPERIMENTAL_120; otherwise runs at 60 FPS.",
                 L"120 FPS 為實驗性功能，需啟用 LO_EXPERIMENTAL_120，否則以 60 FPS 執行。")
            : Tr(L"60/120 FPS are experimental. Verify game speed, audio and battle timing.",
                 L"60/120 FPS 為實驗性功能，請確認遊戲速度、音訊與戰鬥時序。");
    if (displayPreview)
        next.help = Tr(L"Keep changes? A: keep, B: revert. Reverting automatically in 15 seconds.",
                       L"保留顯示更改？A：保留，B：還原。15 秒後自動還原。");
    if (displayPreview && (flags & 0x02000000))
        next.help = Tr(L"Keep changes? B: keep, A: revert. Reverting automatically in 15 seconds.",
                       L"保留顯示更改？B：保留，A：還原。15 秒後自動還原。");
    if (restartPrompt)
    {
        next.dialogTitle = Tr(L"Restart required", L"需要重新啟動");
        next.dialogMessage = restartSaveFailed
            ? Tr(L"Settings could not be saved. Check settings.ini permissions, then retry or cancel.",
                 L"無法儲存設定。請檢查 settings.ini 權限後重試或取消。")
            : Tr(L"Save these settings and restart now?", L"儲存這些設定並立即重新啟動嗎？");
        next.dialogChoices = {Tr(L"Restart now", L"立即重新啟動"), Tr(L"Later", L"稍後"), Tr(L"Cancel", L"取消")};
        next.dialogSelection = restartChoice;
    }
    std::lock_guard lock(snapshotMutex);
    if (next.tab == snapshot.tab && next.row == snapshot.row && next.language == snapshot.language &&
        next.rows == snapshot.rows && next.help == snapshot.help && next.dialogTitle == snapshot.dialogTitle &&
        next.dialogMessage == snapshot.dialogMessage && next.dialogChoices == snapshot.dialogChoices &&
        next.dialogSelection == snapshot.dialogSelection)
        return;
    next.revision = snapshot.revision + 1;
    snapshot = std::move(next);
}
} // namespace
bool FilterInput(uint16_t &buttons, int16_t x, int16_t y)
{
    if (unsigned polls = cancelPolls.load(); polls && cancelPolls.compare_exchange_strong(polls, polls - 1))
    {
        buttons = cancelButton.load();
        return false;
    }
    if (!active.load())
        return false;
    if (swapConfirm.load())
        buttons = (buttons & ~0x3000) | ((buttons & 0x1000) << 1) | ((buttons & 0x2000) >> 1);
    if (x < -16000)
        buttons |= 4;
    if (x > 16000)
        buttons |= 8;
    if (y < -16000)
        buttons |= 2;
    if (y > 16000)
        buttons |= 1;
    static uint16_t previous = 0;
    static auto repeat = std::chrono::steady_clock::time_point{};
    if (waitForRelease.load())
    {
        previous = buttons;
        if (!buttons)
            waitForRelease = false;
        buttons = 0;
        return true;
    }
    auto now = std::chrono::steady_clock::now();
    uint16_t edge = buttons & ~previous;
    if (buttons != previous)
        repeat = now + std::chrono::milliseconds(350);
    else if (now >= repeat)
    {
        edge |= buttons & 15;
        repeat = now + std::chrono::milliseconds(100);
    }
    previous = buttons;
    pending.fetch_or(edge);
    buttons = 0;
    return true;
}
void PointerClick(float x, float y, bool reverse)
{
    if (!active.load())
        return;
    std::lock_guard lock(snapshotMutex);
    if (!snapshot.dialogChoices.empty())
    {
        const int selected = int(y - 360) / 43;
        if (x >= 390 && x < 890 && y >= 360 && selected >= 0 && selected < int(snapshot.dialogChoices.size()))
        {
            mouseDialog = selected;
            mouseAction = 0x1000;
        }
        return;
    }
    if (x >= 386 && x < 1026 && y >= 110 && y < 142)
    {
        mouseTab = int(x - 386) / 160;
        return;
    }
    constexpr int height = 43;
    const int selected = int(y - 150) / height;
    if (x < 38 || x >= 1026 || y < 150 || selected < 0 || selected >= int(snapshot.rows.size()))
        return;
    mouseRow = selected;
    if (x >= 386)
        mouseAction = reverse ? 4 : 0x1000;
}
} // namespace settings

// Resolve the explicit host choice instead of the retail language allowlist's
// default alias. Resource suffixes come from the original executable's table.
PPC_FUNC(sub_82481BE8)
{
    const uint32_t language = settings::GameLanguage();
    if (language == 9 && ctx.r3.u32 == 0x8336A5F0 && (ctx.r4.u32 == 0 || ctx.r4.u32 == language))
    {
        ctx.r3.u64 = PPC_LOAD_U32(0x832455F0 + language * 4);
        static const bool logged = [] {
            LOG_INFO("settings: explicit game resource language {}", settings::GameLanguage());
            return true;
        }();
        (void)logged;
        return;
    }
    __imp__sub_82481BE8(ctx, base);
}

PPC_FUNC(sub_822F19B0)
{
#if !defined(_WIN32) || !defined(LO_GPU_PLUME)
    __imp__sub_822F19B0(ctx, base);
    return;
#else
    if (!gpu::video::GetDevice())
    {
        __imp__sub_822F19B0(ctx, base);
        return;
    }
    using namespace settings;
    const uint32_t menu = ctx.r3.u32;
    const uint32_t state = PPC_LOAD_U32(menu + 4);
    const uint32_t modal = PPC_LOAD_U32(menu + 0x1804);
    if (menu != lastMenu)
    {
        lastMenu = menu;
        bypass = false;
        sawModal = false;
        active = false;
    }
    if (state != 4)
    {
        if (bypass)
            sawModal = true;
        active = false;
        __imp__sub_822F19B0(ctx, base);
        return;
    }
    if (bypass)
    {
        if (modal)
        {
            sawModal = true;
            __imp__sub_822F19B0(ctx, base);
            return;
        }
        if (!sawModal)
        {
            __imp__sub_822F19B0(ctx, base);
            return;
        }
        bypass = false;
        sawModal = false;
    }
    const uint32_t config = ConfigAddress(base);
    if (!config)
    {
        __imp__sub_822F19B0(ctx, base);
        return;
    }
    if (!active.exchange(true))
    {
        edit = GetConfig();
        pending = 0;
        waitForRelease = true;
        restartPrompt = false;
        restartSaveFailed = false;
        status.clear();
        Publish(base, config);
        LOG_INFO("settings: replacement opened at guest menu {:#x}", menu);
        for (uint32_t i = 0; i < VoiceCount(base); ++i)
            LOG_INFO("settings: voice option {} -> language {}", i, VoiceLanguage(base, i));
    }
    uint16_t input = pending.exchange(0);
    if (int selected = mouseTab.exchange(-1); selected >= 0)
    {
        tab = selected;
        row = 0;
        input |= 0x400;
    }
    if (int selected = mouseRow.exchange(-1); selected >= 0)
    {
        row = selected;
        input |= 0x400;
    }
    input |= mouseAction.exchange(0);
    swapConfirm = (PPC_LOAD_U32(config + 4) & 0x02000000) != 0;
    if (restart::ConsumeLaunchFailure())
    {
        status = Tr(L"Restart could not be started. This game is still running; your saved settings are safe.",
                    L"無法啟動重新啟動程序。本遊戲仍在執行，已儲存的設定安全保留。");
        Publish(base, config);
    }
    if (restartPrompt)
    {
        if (int selected = mouseDialog.exchange(-1); selected >= 0)
            restartChoice = selected;
        if (input & 1) restartChoice = (restartChoice + 2) % 3;
        if (input & 2) restartChoice = (restartChoice + 1) % 3;
        if (input & 0x2000) restartChoice = 2;
        if (input & 0x3000)
        {
            if (restartChoice == 2)
            {
                if (restartRollbackPreview)
                {
                    PreviewConfig(restartBefore);
                    edit = restartBefore;
                }
                restartPrompt = false;
                restartSaveFailed = false;
                status = Tr(L"Changes requiring restart were cancelled.", L"已取消需要重新啟動的變更。");
            }
            else if (SaveConfig(restartAfter))
            {
                edit = restartAfter;
                restartPrompt = false;
                restartSaveFailed = false;
                status = restartChoice == 0
                    ? Tr(L"Saved. Preparing a safe restart…", L"已儲存，正在準備安全重新啟動……")
                    : Tr(L"Saved. Changes take effect after restarting.", L"已儲存，重新啟動後套用變更。");
                if (restartChoice == 0) restart::Request();
            }
            else
                restartSaveFailed = true;
        }
        if (input) Publish(base, config);
        return;
    }
    if (displayPreview)
    {
        if (gpu::video::DisplayModeFailed() || (input & 0x2000) || std::chrono::steady_clock::now() >= previewDeadline)
        {
            PreviewConfig(previousDisplay);
            edit = previousDisplay;
            displayPreview = false;
            status = Tr(L"Display settings reverted.", L"顯示設定已還原。");
            if (gpu::video::DisplayModeFailed())
                status = Tr(L"Display mode unavailable; previous settings restored.",
                            L"此顯示模式不可用，已還原之前的設定。");
        }
        else if (input & 0x1000)
        {
            if (restart::Required(previousDisplay, edit))
            {
                displayPreview = false;
                restartPrompt = true;
                restartRollbackPreview = true;
                restartSaveFailed = false;
                restartChoice = 0;
                restartBefore = previousDisplay;
                restartAfter = edit;
            }
            else if (SaveConfig(edit))
            {
                displayPreview = false;
                status = Tr(L"Display settings saved.", L"顯示設定已儲存。");
            }
            else
                status = Tr(L"Could not save settings.", L"無法儲存設定。");
        }
        if (input || !displayPreview)
            Publish(base, config);
        return;
    }
    bool changed = false;
    if (input & 0x300)
    {
        tab = (tab + ((input & 0x200) ? 1 : 3)) % 4;
        row = 0;
        status.clear();
    }
    const int count = tab == 0 ? 8 : tab == 1 ? 3 : tab == 2 ? 9 : 3;
    if (input & 1)
        row = (row + count - 1) % count;
    if (input & 2)
        row = (row + 1) % count;
    const bool action = (tab == 0 && row == 7) || (tab == 2 && row >= 7) || (tab == 3 && row == 2);
    const int delta = (input & 4) ? -1 : ((input & 8) || ((input & 0x1000) && !action)) ? 1 : 0;
    auto cycle = [&](uint32_t value, uint32_t count) {
        return uint32_t((int(value) + int(count) + delta) % int(count));
    };
    if (delta)
    {
        if (tab == 0 && row < 7)
        {
            if (row == 0)
                PPC_STORE_U32(config, cycle(PPC_LOAD_U32(config), 3));
            else
            {
                constexpr uint32_t masks[] = {0,          0x40000000, 0x10000000, 0x00800000,
                                              0x08000000, 0x04000000, 0x02000000};
                PPC_STORE_U32(config + 4, PPC_LOAD_U32(config + 4) ^ masks[row]);
            }
            changed = true;
        }
        else if (tab == 1)
        {
            if (row == 0)
                PPC_STORE_U32(config + 24, cycle(PPC_LOAD_U32(config + 24), VoiceCount(base)));
            else
            {
                auto offset = row == 1 ? 8 : 12;
                PPC_STORE_U32(config + offset, std::clamp(int(PPC_LOAD_U32(config + offset)) + delta * 4, 0, 100));
            }
            changed = true;
        }
        else if (tab == 2)
        {
            if (row == 0)
                edit.graphicsBackend = GraphicsBackend(cycle(uint32_t(edit.graphicsBackend), 2));
            if (row == 1)
                edit.windowMode = WindowMode(cycle(uint32_t(edit.windowMode), 3));
            if (row == 2)
            {
                uint32_t index = 0;
                for (uint32_t i = 0; i < 5; i++)
                    if (edit.width == resolutions[i][0])
                        index = i;
                index = cycle(index, 5);
                edit.width = resolutions[index][0];
                edit.height = resolutions[index][1];
            }
            if (row == 3)
            {
                uint32_t index = 0;
                for (uint32_t i = 0; i < std::size(internalResolutions); ++i)
                    if (edit.internalResolution == internalResolutions[i])
                        index = i;
                edit.internalResolution = internalResolutions[cycle(index, uint32_t(std::size(internalResolutions)))];
            }
            if (row == 4)
            {
                edit.antialiasing = cycle(edit.antialiasing, 4);
                edit.fxaa = edit.antialiasing == 1;
            }
            if (row == 5)
                edit.scalingQuality = cycle(edit.scalingQuality, 2);
            if (row == 6)
            {
                constexpr uint32_t rates[] = {30, 60, 120};
                const uint32_t index = edit.frameRate == 120 ? 2u : edit.frameRate == 60 ? 1u : 0u;
                edit.frameRate = rates[cycle(index, 3)];
            }
        }
        else
        {
            if (row == 0)
                edit.uiLanguage = cycle(edit.uiLanguage, 5);
            if (row == 1)
                edit.gameLanguage = GameLanguageIds[cycle(GameLanguageIndex(edit.gameLanguage), uint32_t(GameLanguageIds.size()))];
        }
    }
    if (changed)
    {
        PPCContext call = ctx;
        call.r3.u32 = config;
        __imp__sub_82870E38(call, base);
    }
    if ((input & 0x1000) && tab == 0 && row == 7)
    {
        PPCContext call = ctx;
        call.r3.u32 = config;
        __imp__sub_828710A0(call, base);
        call = ctx;
        call.r3.u32 = config;
        __imp__sub_82870E38(call, base);
        status = Tr(L"Game defaults restored.", L"遊戲預設設定已恢復。");
    }
    if ((input & 0x1000) && tab == 2 && row == 8)
    {
        previousDisplay = GetConfig();
        PreviewConfig(edit);
        displayPreview = true;
        previewDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    }
    if ((input & 0x1000) && tab == 3 && row == 2)
    {
        Config languages = GetConfig();
        languages.uiLanguage = edit.uiLanguage;
        languages.gameLanguage = edit.gameLanguage;
        const Config before = GetConfig();
        if (restart::Required(before, languages))
        {
            restartPrompt = true;
            restartRollbackPreview = false;
            restartSaveFailed = false;
            restartChoice = 0;
            restartBefore = before;
            restartAfter = languages;
        }
        else
            status = SaveConfig(languages) ? Tr(L"Language settings saved.", L"語言設定已儲存。")
                                           : Tr(L"Could not save settings.", L"無法儲存設定。");
    }
    if ((input & 0x1000) && tab == 2 && row == 7)
    {
        // Hand the original calibration screen its own brightness row.
        const uint32_t list = menu + 0x558, table = PPC_LOAD_U32(list + 0x84);
        // Internal row 11 is screen position; the visible Brightness row is 12.
        for (uint32_t i = 0; i < 13; i++)
            if (PPC_LOAD_U32(table + i * 0x30 + 4) == 12)
            {
                PPC_STORE_U32(list + 0x38, i);
                break;
            }
        bypass = true;
        sawModal = false;
        active = false;
        cancelButton = swapConfirm.load() ? 0x2000 : 0x1000;
        cancelPolls = 6;
    }
    if (input & 0x2000)
    {
        bypass = true;
        sawModal = false;
        active = false;
        cancelButton = swapConfirm.load() ? 0x1000 : 0x2000;
        cancelPolls = 6;
    }
    if (input)
        Publish(base, config);
    // The host UI owns input while open; the original task resumes for save
    // confirmation and calibration. Its parent continues ticking throughout.
#endif
}

bool settings::DrawMenu(std::vector<uint32_t> &pixels, uint64_t &revision, uint32_t width, uint32_t height)
{
    if (!active.load())
        return false;
    MenuSnapshot current;
    {
        std::lock_guard lock(snapshotMutex);
        current = snapshot;
    }
    // This cache belongs to the sole presentation thread. Dimensions must be
    // checked independently: portrait and landscape buffers can have equal area.
    static uint32_t cachedWidth = 0, cachedHeight = 0;
    if (revision == current.revision && cachedWidth == width && cachedHeight == height && !pixels.empty())
        return true;
    if (!RasterizeMenu(current, width, height, pixels))
        return false;
    cachedWidth = width;
    cachedHeight = height;
    revision = current.revision;
    return true;
}
