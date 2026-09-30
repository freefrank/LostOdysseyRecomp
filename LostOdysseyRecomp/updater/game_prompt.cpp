#include "game_prompt.h"

#include <SDL.h>
#include <host_ui/rasterizer.h>
#include <host_ui/widgets.h>

#include <algorithm>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <string>
#include <vector>

namespace updater::game_prompt
{
namespace
{
enum class Phase { Hidden, Checking, Offer };
struct State
{
    std::mutex mutex;
    std::condition_variable decision;
    Phase phase = Phase::Hidden;
    bool accepted = false;
    bool chinese = false;
    int selected = 0;
    int scroll = 0;
    std::string version;
    std::vector<std::string> lines;
};
State state;
constexpr int visibleLines = 17;

std::vector<std::string> Wrap(std::string_view text)
{
    host_ui::Rasterizer measure(nullptr, 1280, 720);
    std::vector<std::string> lines;
    std::string line;
    for (size_t offset = 0; offset < text.size(); )
    {
        const size_t start = offset;
        const auto point = host_ui::font::DecodeUtf8(text, offset);
        if (!point) break;
        if (point == '\r') continue;
        if (point == '\n')
        {
            lines.push_back(std::move(line));
            line.clear();
            continue;
        }
        std::string glyph(text.substr(start, offset - start));
        if (!line.empty() && measure.MeasureString(line + glyph) > 1040)
        {
            lines.push_back(std::move(line));
            line.clear();
        }
        line += glyph;
    }
    if (!line.empty()) lines.push_back(std::move(line));
    if (lines.empty()) lines.emplace_back();
    return lines;
}

int MaxScroll()
{
    return std::max(0, int(state.lines.size()) - visibleLines);
}

void Resolve(bool accepted)
{
    state.accepted = accepted;
    state.phase = Phase::Hidden;
    state.decision.notify_all();
}

void BeginOffer(std::string_view version, std::string_view changelog, uint32_t uiLanguage)
{
    state.chinese = uiLanguage == 4;
    state.version = version;
    state.lines = Wrap(changelog);
    state.selected = 0;
    state.scroll = 0;
    state.accepted = false;
    state.phase = Phase::Offer;
}
}

void ShowChecking(uint32_t uiLanguage)
{
    std::lock_guard lock(state.mutex);
    state.chinese = uiLanguage == 4;
    state.phase = Phase::Checking;
}

void HideChecking()
{
    std::lock_guard lock(state.mutex);
    if (state.phase == Phase::Checking) state.phase = Phase::Hidden;
}

bool Confirm(std::string_view version, std::string_view changelog, uint32_t uiLanguage)
{
    std::unique_lock lock(state.mutex);
    BeginOffer(version, changelog, uiLanguage);
    state.decision.wait(lock, [] { return state.phase != Phase::Offer; });
    return state.accepted;
}

bool ConfirmBeforeImport(std::string_view version, std::string_view changelog, uint32_t uiLanguage)
{
    const char* requestedDriver = std::getenv("SDL_VIDEODRIVER");
    if (requestedDriver && std::string_view(requestedDriver) == "dummy")
        return false;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0)
        return false;
    if (const char* driver = SDL_GetCurrentVideoDriver();
        driver && std::string_view(driver) == "dummy")
    {
        SDL_Quit();
        return false;
    }
    SDL_Window* window = SDL_CreateWindow("Lost Odyssey Recomp",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Renderer* renderer = window
        ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC)
        : nullptr;
    if (!renderer && window)
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    SDL_Texture* texture = renderer
        ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
              SDL_TEXTUREACCESS_STREAMING, 1280, 720)
        : nullptr;
    if (!texture)
    {
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }
    SDL_RenderSetLogicalSize(renderer, 1280, 720);
    std::vector<SDL_GameController*> controllers;
    for (int i = 0; i < SDL_NumJoysticks(); ++i)
        if (SDL_IsGameController(i))
            if (auto* controller = SDL_GameControllerOpen(i)) controllers.push_back(controller);

    {
        std::lock_guard lock(state.mutex);
        BeginOffer(version, changelog, uiLanguage);
    }
    host_ui::PixelBuffer pixels;
    pixels.Resize(1280, 720);
    host_ui::Rasterizer rasterizer(pixels);
    const uint32_t windowId = SDL_GetWindowID(window);
    while (Visible())
    {
        SDL_Event event;
        if (SDL_WaitEventTimeout(&event, 16))
        {
            do
            {
                if (event.type == SDL_QUIT ||
                    (event.type == SDL_WINDOWEVENT && event.window.windowID == windowId &&
                     event.window.event == SDL_WINDOWEVENT_CLOSE))
                {
                    std::lock_guard lock(state.mutex);
                    Resolve(false);
                    break;
                }
                int width = 0, height = 0;
                SDL_GetWindowSize(window, &width, &height);
                HandleEvent(event, windowId, width, height);
            } while (Visible() && SDL_PollEvent(&event));
        }
        if (!Visible()) break;
        Render(rasterizer);
        SDL_UpdateTexture(texture, nullptr, pixels.pixels.data(), 1280 * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }
    bool accepted = false;
    {
        std::lock_guard lock(state.mutex);
        accepted = state.accepted;
    }
    for (auto* controller : controllers) SDL_GameControllerClose(controller);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return accepted;
}

bool Visible()
{
    std::lock_guard lock(state.mutex);
    return state.phase != Phase::Hidden;
}

bool HandleEvent(const SDL_Event &event, uint32_t windowId, int windowWidth, int windowHeight)
{
    std::lock_guard lock(state.mutex);
    if (state.phase == Phase::Hidden) return false;
    if (state.phase == Phase::Checking)
        return event.type == SDL_KEYDOWN || event.type == SDL_KEYUP ||
               event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEWHEEL ||
               event.type == SDL_CONTROLLERBUTTONDOWN;

    if (event.type == SDL_KEYDOWN)
    {
        if (event.key.windowID != windowId) return false;
        if (event.key.repeat) return true;
        switch (event.key.keysym.sym)
        {
        case SDLK_UP: state.scroll = std::max(0, state.scroll - 1); break;
        case SDLK_DOWN: state.scroll = std::min(MaxScroll(), state.scroll + 1); break;
        case SDLK_PAGEUP: state.scroll = std::max(0, state.scroll - visibleLines); break;
        case SDLK_PAGEDOWN: state.scroll = std::min(MaxScroll(), state.scroll + visibleLines); break;
        case SDLK_LEFT: state.selected = 0; break;
        case SDLK_RIGHT: state.selected = 1; break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_SPACE: Resolve(state.selected == 0); break;
        case SDLK_ESCAPE: Resolve(false); break;
        default: break;
        }
        return true;
    }
    if (event.type == SDL_KEYUP && event.key.windowID == windowId) return true;
    if (event.type == SDL_MOUSEWHEEL && event.wheel.windowID == windowId)
    {
        state.scroll = std::clamp(state.scroll - event.wheel.y * 3, 0, MaxScroll());
        return true;
    }
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.windowID == windowId)
    {
        const double scale = std::min(windowWidth / 1280.0, windowHeight / 720.0);
        const int x = scale > 0 ? int((event.button.x - (windowWidth - 1280 * scale) / 2) / scale) : 0;
        const int y = scale > 0 ? int((event.button.y - (windowHeight - 720 * scale) / 2) / scale) : 0;
        if (y >= 618 && y < 664)
        {
            if (x >= 866 && x < 1006) Resolve(true);
            else if (x >= 1022 && x < 1162) Resolve(false);
        }
        return true;
    }
    if (event.type == SDL_CONTROLLERBUTTONDOWN)
    {
        switch (event.cbutton.button)
        {
        case SDL_CONTROLLER_BUTTON_DPAD_UP: state.scroll = std::max(0, state.scroll - 1); break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: state.scroll = std::min(MaxScroll(), state.scroll + 1); break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: state.selected = 0; break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: state.selected = 1; break;
        case SDL_CONTROLLER_BUTTON_A: Resolve(state.selected == 0); break;
        case SDL_CONTROLLER_BUTTON_B: Resolve(false); break;
        default: break;
        }
        return true;
    }
    return false;
}

void Render(host_ui::Rasterizer &r)
{
    std::lock_guard lock(state.mutex);
    if (state.phase == Phase::Hidden) return;
    r.FillRect(0, 0, 1280, 720, host_ui::MakeColor(255, 15, 20, 29));
    host_ui::DrawPanel(r, 80, 50, 1120, 620);
    host_ui::DrawHeader(r, 80, 50, 1120, 62,
        state.chinese ? L"Lost Odyssey 更新" : L"Lost Odyssey Update");
    const auto white = host_ui::MakeColor(255, 230, 235, 240);
    const auto muted = host_ui::MakeColor(255, 160, 173, 187);
    if (state.phase == Phase::Checking)
    {
        r.DrawString(118, 168, state.chinese ? "正在检查更新..." : "Checking for updates...", white, 1.5f);
        return;
    }

    r.DrawString(112, 132, state.chinese ? "发现新版本：" : "New version: ", white, 1.2f);
    r.DrawString(300, 132, state.version, white, 1.2f);
    r.DrawString(112, 170, state.chinese ? "更新内容" : "Changes", muted);
    r.FillRect(108, 200, 1064, 390, host_ui::MakeColor(255, 22, 28, 38));
    r.DrawRect(108, 200, 1064, 390, host_ui::MakeColor(255, 65, 75, 88));
    const int last = std::min(int(state.lines.size()), state.scroll + visibleLines);
    for (int i = state.scroll; i < last; ++i)
        r.DrawString(122, 212 + (i - state.scroll) * 22, state.lines[i], white);
    if (MaxScroll())
        r.DrawString(112, 594, state.chinese ? "上下滚动查看全部内容" : "Scroll to read all changes", muted);
    host_ui::DrawButton(r, 866, 618, 140, 46,
        state.chinese ? L"安装 (A)" : L"Install (A)", state.selected == 0);
    host_ui::DrawButton(r, 1022, 618, 140, 46,
        state.chinese ? L"稍后 (B)" : L"Later (B)", state.selected == 1);
}
}
