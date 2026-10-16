#include "compaction_prompt.h"
#include "import_game.h"
#include <host_ui/prompt_window.h>
#include <host_ui/widgets.h>
#include <hid/face_buttons.h>
#include <os/logger.h>
#include <settings/config.h>
#include <settings/translations.h>
#if defined(__ANDROID__)
#include <hid/android_touch.h>
#endif
#include <atomic>
#include <cstdio>
#include <cwchar>
#include <thread>

namespace install::compaction
{
namespace
{
constexpr int ButtonX[] = {112, 466, 820};
constexpr int ButtonWidth = 340, ButtonTop = 594, ButtonHeight = 50;

std::wstring Amount(uint64_t bytes)
{
    wchar_t text[40];
    std::swprintf(text, std::size(text), L"%.1f", double(bytes) / 1'000'000'000.0);
    return text;
}

std::wstring FormatAmount(const wchar_t* format, uint64_t bytes)
{
    std::wstring text(format);
    if (const auto position = text.find(L"%s"); position != std::wstring::npos)
        text.replace(position, 2, Amount(bytes));
    return text;
}

Action ReadAction(const SDL_Event& event, const host_ui::PromptWindow& window)
{
    if (event.type == SDL_EVENT_QUIT ||
        (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == window.Id()))
        return Action::Close;
    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
        (event.key.windowID == window.Id() || !event.key.windowID))
    {
        switch (event.key.key)
        {
        case SDLK_LEFT: case SDLK_UP: return Action::Previous;
        case SDLK_RIGHT: case SDLK_DOWN: case SDLK_TAB: return Action::Next;
        case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE: return Action::Activate;
        case SDLK_ESCAPE: return Action::Later;
        default: break;
        }
    }
    if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
    {
        switch (hid::face_buttons::FromEvent(event.gbutton))
        {
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT: case SDL_GAMEPAD_BUTTON_DPAD_UP: return Action::Previous;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return Action::Next;
        case SDL_GAMEPAD_BUTTON_SOUTH: return Action::Activate;
        case SDL_GAMEPAD_BUTTON_EAST: return Action::Later;
        default: break;
        }
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT &&
        event.button.windowID == window.Id())
    {
        int width = 0, height = 0;
        window.Size(width, height);
        const double scale = std::min(width / 1280.0, height / 720.0);
        if (scale <= 0) return Action::None;
        const int x = int((event.button.x - (width - 1280 * scale) / 2) / scale);
        const int y = int((event.button.y - (height - 720 * scale) / 2) / scale);
        if (y >= ButtonTop && y < ButtonTop + ButtonHeight)
            for (int i = 0; i < 3; ++i)
                if (x >= ButtonX[i] && x < ButtonX[i] + ButtonWidth)
                    return Action(int(Action::Free) + i);
    }
    return Action::None;
}

#if defined(__ANDROID__)
Action TouchAction(uint16_t& previous)
{
    const uint16_t buttons = hid::android_touch::Snapshot().buttons;
    const uint16_t pressed = buttons & ~previous;
    previous = buttons;
    if (pressed & 0x1000) return Action::Activate;
    if (pressed & 0x2000) return Action::Later;
    if (pressed & (0x0001 | 0x0004)) return Action::Previous;
    if (pressed & (0x0002 | 0x0008)) return Action::Next;
    return Action::None;
}
#endif

struct Work
{
    std::atomic<uint64_t> completed{0}, total{0};
    std::atomic<bool> cancel{false}, finished{false};
    CompactResult result;
    std::string error;
    bool cancelled = false;

    void Run(const std::filesystem::path& root)
    {
        try
        {
            result = CompactGameData(root, [&](uint64_t done, uint64_t all, std::string_view) {
                completed.store(done, std::memory_order_relaxed);
                total.store(all, std::memory_order_relaxed);
            }, [&] { return cancel.load(std::memory_order_relaxed); });
            if (!result.replaced && result.sharedBytes > 0)
                LOG_NOTICE("compact game data: Already done: these files are stored once (about {:.1f} GB saved); {} not shared",
                           double(result.sharedBytes) / 1'000'000'000.0, result.failed);
            else
                LOG_NOTICE("compact game data: freed {} bytes; {} copies replaced, {} archive bytes shared, {} not shared",
                           result.freedBytes, result.replaced, result.sharedBytes, result.failed);
        }
        catch (const Error& failure)
        {
            cancelled = failure.cancelled();
            error = failure.what();
            LOG_WARNING("compact game data: {}", error);
        }
        catch (const std::exception& failure)
        {
            error = failure.what();
            LOG_WARNING("compact game data failed: {}", error);
        }
        finished.store(true, std::memory_order_release);
    }
};
}

void PrepareAtStartup(const std::filesystem::path& gameFolder)
{
    const auto config = settings::GetConfig();
    const char* overrideValue = std::getenv("LO_COMPACT_GAME_DATA");
    const auto mode = StartupMode(std::getenv("LO_BACKGROUND") || std::getenv("LO_HEADLESS"),
                                  config.askCompactGameData, overrideValue ? overrideValue : "");
    if (mode == Mode::Skip) return;
    const uint64_t estimate = EstimateCompactGameDataBytes(gameFolder);
    if (!estimate) return;
    // Probe once for this start, only after finding candidates. Android shared
    // storage may expose ordinary file copies but support neither kind of link.
    const bool canShare = CanShareGameData(gameFolder);
    if (!canShare)
    {
        LOG_NOTICE("compact game data: skipped; game folder does not support sharing files");
        return;
    }
    LOG_NOTICE("compact game data: estimated {} duplicate bytes", estimate);
    Work work;
    if (mode == Mode::Run)
    {
        work.Run(gameFolder);
        return;
    }

    host_ui::PromptWindow window;
    if (!window.Open()) return;
    host_ui::Rasterizer rasterizer(window.Pixels());
    const auto tr = [&](const wchar_t* en, const wchar_t* zh) {
        return settings::Translate(config.uiLanguage, en, zh);
    };
    const auto white = host_ui::MakeColor(255, 230, 235, 240);
    const auto muted = host_ui::MakeColor(255, 160, 173, 187);
    Offer offer;
    std::thread worker;
    struct JoinWorker
    {
        std::thread& thread;
        ~JoinWorker() { if (thread.joinable()) thread.join(); }
    } joinWorker{worker};
    bool started = false;
#if defined(__ANDROID__)
    uint16_t touchButtons = hid::android_touch::Snapshot().buttons;
#endif
    for (;;)
    {
        const bool finished = work.finished.load(std::memory_order_acquire);
        const auto apply = [&](Action action) {
            if (!started) offer.Apply(action);
            else if (!finished && (action == Action::Later || action == Action::Close)) work.cancel = true;
        };
        bool dismiss = false;
        SDL_Event event;
        if (SDL_WaitEventTimeout(&event, 16))
        {
            do
            {
                const auto action = ReadAction(event, window);
                apply(action);
                if (finished && action != Action::None && action != Action::Previous && action != Action::Next)
                    dismiss = true;
            } while (SDL_PollEvent(&event));
        }
#if defined(__ANDROID__)
        const auto touch = TouchAction(touchButtons);
        apply(touch);
        dismiss |= finished && (touch == Action::Activate || touch == Action::Later);
#endif
        if (dismiss) break;
        if (!started && offer.finished)
        {
            if (offer.selected == Choice::Never)
            {
                auto saved = config;
                saved.askCompactGameData = false;
                if (!settings::SaveConfig(saved)) LOG_WARNING("compact game data: could not save Don't ask again");
            }
            if (offer.selected != Choice::Free) return;
            started = true;
            worker = std::thread([&] { work.Run(gameFolder); });
        }

        rasterizer.FillRect(0, 0, 1280, 720, host_ui::MakeColor(255, 15, 20, 29));
        host_ui::DrawPanel(rasterizer, 80, 50, 1120, 620);
        host_ui::DrawHeader(rasterizer, 80, 50, 1120, 62, tr(L"Free disk space", L"釋放磁碟空間"));
        if (!started)
        {
            rasterizer.DrawWString(112, 160, FormatAmount(tr(L"Free about %s GB?", L"釋放約 %s GB？"), estimate), white, 1.5f);
            rasterizer.DrawWString(112, 220, tr(L"Your discs store some files twice.", L"您的光碟中儲存了部分重複檔案。"), muted);
            rasterizer.DrawWString(112, 260, tr(L"This takes a few minutes and runs once.", L"這需要幾分鐘，只需執行一次。"), muted);
            host_ui::DrawButton(rasterizer, ButtonX[0], ButtonTop, ButtonWidth, ButtonHeight,
                               tr(L"Free space (A)", L"釋放空間 (A)"), offer.selected == Choice::Free);
            host_ui::DrawButton(rasterizer, ButtonX[1], ButtonTop, ButtonWidth, ButtonHeight,
                               tr(L"Not now (B)", L"暫時不要 (B)"), offer.selected == Choice::Later);
            host_ui::DrawButton(rasterizer, ButtonX[2], ButtonTop, ButtonWidth, ButtonHeight,
                               tr(L"Don't ask again", L"不再詢問"), offer.selected == Choice::Never);
        }
        else if (!finished)
        {
            rasterizer.DrawWString(112, 160, tr(L"Comparing disc archives", L"正在比對光碟檔案"), white, 1.2f);
            const uint64_t done = work.completed.load(std::memory_order_relaxed);
            const uint64_t total = work.total.load(std::memory_order_relaxed);
            const double fraction = total ? std::min(1.0, double(done) / double(total)) : 0;
            rasterizer.DrawRect(112, 220, 1048, 28, muted);
            rasterizer.FillRect(114, 222, int(1044 * fraction), 24, host_ui::MakeColor(255, 196, 160, 82));
            rasterizer.DrawWString(112, 270, std::to_wstring(int(fraction * 100)) + L"%", muted);
            host_ui::DrawButton(rasterizer, ButtonX[1], ButtonTop, ButtonWidth, ButtonHeight,
                               tr(L"Cancel (B)", L"取消 (B)"), true);
        }
        else
        {
            const auto title = work.cancelled ? std::wstring(tr(L"Compaction cancelled.", L"已取消空間整理。")) :
                !work.error.empty() ? std::wstring(tr(L"Game files were kept. You can try again later.", L"遊戲檔案已保留，可稍後再試。")) :
                !work.result.replaced && work.result.sharedBytes > 0 ?
                FormatAmount(tr(L"Already done: these files are stored once (about %s GB saved)", L"已完成：這些檔案只儲存一份（約節省 %s GB）"), work.result.sharedBytes) :
                FormatAmount(tr(L"Freed %s GB.", L"已釋放 %s GB。"), work.result.freedBytes);
            rasterizer.DrawWString(112, 160, title, white);
            if (!work.cancelled && work.error.empty())
#if defined(_WIN32)
                rasterizer.DrawWString(112, 220, tr(L"Explorer still shows full folder size; check free space on the drive.", L"檔案總管仍顯示完整資料夾大小，請查看磁碟可用空間。"), muted);
#else
                rasterizer.DrawWString(112, 220, tr(L"Folder size may stay the same; check free space on the drive.", L"資料夾大小可能不變，請查看磁碟可用空間。"), muted);
#endif
            if (work.result.failed)
                rasterizer.DrawWString(112, 260, tr(L"Some files could not be shared.", L"部分檔案無法共用。"), muted);
            host_ui::DrawButton(rasterizer, ButtonX[1], ButtonTop, ButtonWidth, ButtonHeight,
                               tr(L"Continue (A)", L"繼續 (A)"), true);
        }
        window.Present();
    }
}
}
