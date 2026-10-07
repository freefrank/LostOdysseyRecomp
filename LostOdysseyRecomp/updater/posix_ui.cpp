#include "posix_ui.h"
#include "external_update_notice.h"
#include "../install/installer_colors.h"
#include "../install/installer_font.h"
#include "../install/installer_navigation.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace updater
{
using namespace install::ui;

namespace
{
std::vector<SDL_Gamepad*> OpenGamepads()
{
    std::vector<SDL_Gamepad*> result;
    int count = 0;
    if (SDL_JoystickID* ids = SDL_GetGamepads(&count))
    {
        for (int i = 0; i < count; ++i)
            if (auto* pad = SDL_OpenGamepad(ids[i])) result.push_back(pad);
        SDL_free(ids);
    }
    return result;
}

SDL_Window* CreateCenteredWindow(const char* title, int width, int height)
{
    SDL_Window* window = SDL_CreateWindow(title, width, height, 0);
    if (window) SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    return window;
}

SDL_Renderer* CreateVsyncRenderer(SDL_Window* window)
{
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) renderer = SDL_CreateRenderer(window, "software");
    if (renderer) SDL_SetRenderVSync(renderer, 1);
    return renderer;
}

std::vector<std::string> WrapText(std::string_view text, int maxWidth, float scale)
{
    std::vector<std::string> lines;
    std::string currentLine;

    size_t pos = 0;
    while (pos < text.size())
    {
        size_t nextNewline = text.find('\n', pos);
        std::string_view paragraph = (nextNewline == std::string_view::npos)
                                         ? text.substr(pos)
                                         : text.substr(pos, nextNewline - pos);

        size_t wordStart = 0;
        currentLine.clear();
        while (wordStart < paragraph.size())
        {
            size_t wordEnd = paragraph.find(' ', wordStart);
            if (wordEnd == std::string_view::npos) wordEnd = paragraph.size();
            std::string_view word = paragraph.substr(wordStart, wordEnd - wordStart);

            std::string testLine = currentLine.empty() ? std::string(word) : (currentLine + " " + std::string(word));
            if (MeasureTextWidth(testLine, scale) <= maxWidth)
            {
                currentLine = std::move(testLine);
            }
            else
            {
                if (!currentLine.empty())
                {
                    lines.push_back(currentLine);
                    currentLine = std::string(word);
                }
                else
                {
                    lines.push_back(TruncateTextWidth(word, maxWidth, scale));
                    currentLine.clear();
                }
            }

            wordStart = (wordEnd < paragraph.size() && paragraph[wordEnd] == ' ') ? wordEnd + 1 : wordEnd;
        }
        if (!currentLine.empty()) lines.push_back(currentLine);
        if (paragraph.empty()) lines.emplace_back("");

        if (nextNewline == std::string_view::npos) break;
        pos = nextNewline + 1;
    }
    return lines;
}
} // namespace

bool ConfirmUpdateSdl(std::string_view version, std::string_view changelog, uint32_t uiLanguage)
{
    (void)uiLanguage;
    const char* envDriver = std::getenv("SDL_VIDEODRIVER");
    const bool dummyEnv = envDriver && std::string_view(envDriver) == "dummy";

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        return false;
    }

    const char* driver = SDL_GetCurrentVideoDriver();
    const bool dummy = dummyEnv || (driver && std::string_view(driver) == "dummy");

    // In dummy mode or headless, do not hang waiting for human interaction
    if (dummy)
    {
        return false;
    }

    std::vector<SDL_Gamepad*> controllers = OpenGamepads();

    constexpr int WIN_WIDTH = 640;
    constexpr int WIN_HEIGHT = 460;

    SDL_Window* window = CreateCenteredWindow("Lost Odyssey Recomp - Update Available", WIN_WIDTH, WIN_HEIGHT);

    if (!window)
    {
        for (auto* pad : controllers) SDL_CloseGamepad(pad);
        return false;
    }

    SDL_Renderer* renderer = CreateVsyncRenderer(window);
    if (!renderer)
    {
        SDL_DestroyWindow(window);
        for (auto* pad : controllers) SDL_CloseGamepad(pad);
        return false;
    }

    int selectedButton = 0; // 0 = Install, 1 = Later
    bool done = false;
    bool accepted = false;

    StickNavigation stick;
    std::vector<std::string> logLines = WrapText(changelog, WIN_WIDTH - 60, 0.95f);
    int scrollOffset = 0;
    constexpr int MAX_VISIBLE_LINES = 14;

    while (!done)
    {
        uint64_t now = SDL_GetTicks();
        for (auto* pad : controllers)
        {
            int axisX = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
            Direction dir = stick.Update(axisX, 0, now);
            if (dir == Direction::Left) selectedButton = 0;
            else if (dir == Direction::Right) selectedButton = 1;
        }

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                done = true;
                accepted = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                switch (event.key.key)
                {
                case SDLK_ESCAPE:
                    done = true;
                    accepted = false;
                    break;
                case SDLK_LEFT:
                case SDLK_A:
                    selectedButton = 0;
                    break;
                case SDLK_RIGHT:
                case SDLK_D:
                    selectedButton = 1;
                    break;
                case SDLK_UP:
                case SDLK_W:
                    if (scrollOffset > 0) scrollOffset--;
                    break;
                case SDLK_DOWN:
                case SDLK_S:
                    if (scrollOffset + MAX_VISIBLE_LINES < static_cast<int>(logLines.size()))
                        scrollOffset++;
                    break;
                case SDLK_RETURN:
                case SDLK_SPACE:
                    accepted = (selectedButton == 0);
                    done = true;
                    break;
                default:
                    break;
                }
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                switch (event.gbutton.button)
                {
                case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
                    selectedButton = 0;
                    break;
                case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
                    selectedButton = 1;
                    break;
                case SDL_GAMEPAD_BUTTON_DPAD_UP:
                    if (scrollOffset > 0) scrollOffset--;
                    break;
                case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
                    if (scrollOffset + MAX_VISIBLE_LINES < static_cast<int>(logLines.size()))
                        scrollOffset++;
                    break;
                case SDL_GAMEPAD_BUTTON_SOUTH:
                    accepted = (selectedButton == 0);
                    done = true;
                    break;
                case SDL_GAMEPAD_BUTTON_EAST:
                    accepted = false;
                    done = true;
                    break;
                default:
                    break;
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    int mx = event.button.x, my = event.button.y;
                    int btnY = WIN_HEIGHT - 65;
                    int btnH = 38;
                    int btnW = 120;
                    if (my >= btnY && my <= btnY + btnH)
                    {
                        if (mx >= WIN_WIDTH - 280 && mx <= WIN_WIDTH - 280 + btnW)
                        {
                            accepted = true;
                            done = true;
                        }
                        else if (mx >= WIN_WIDTH - 140 && mx <= WIN_WIDTH - 140 + btnW)
                        {
                            accepted = false;
                            done = true;
                        }
                    }
                }
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                if (event.wheel.y > 0 && scrollOffset > 0) scrollOffset--;
                else if (event.wheel.y < 0 && scrollOffset + MAX_VISIBLE_LINES < static_cast<int>(logLines.size())) scrollOffset++;
                break;
            }
        }

        // Render
        FillRect(renderer, 0, 0, WIN_WIDTH, WIN_HEIGHT, COLOR_STEEL);
        DrawBevelPanel(renderer, 10, 10, WIN_WIDTH - 20, WIN_HEIGHT - 20, COLOR_STEEL_PANEL);

        std::string title = "Lost Odyssey " + std::string(version) + " Update";
        DrawString(renderer, 24, 24, title, COLOR_ACCENT_GOLD.r, COLOR_ACCENT_GOLD.g, COLOR_ACCENT_GOLD.b, 255, 1.25f);

        // Changelog box
        int boxX = 20;
        int boxY = 60;
        int boxW = WIN_WIDTH - 40;
        int boxH = WIN_HEIGHT - 140;
        FillRect(renderer, boxX, boxY, boxW, boxH, COLOR_RAIL);
        DrawRect(renderer, boxX, boxY, boxW, boxH, COLOR_BORDER_LINE);

        int lineY = boxY + 12;
        int lineHeight = TextLineHeight(0.95f) + 4;
        for (int i = scrollOffset; i < static_cast<int>(logLines.size()) && i < scrollOffset + MAX_VISIBLE_LINES; ++i)
        {
            DrawString(renderer, boxX + 12, lineY, logLines[i], COLOR_INK.r, COLOR_INK.g, COLOR_INK.b, 255, 0.95f);
            lineY += lineHeight;
        }

        // Buttons: Install / Later
        int btnY = WIN_HEIGHT - 65;
        int btnW = 120;
        int btnH = 38;

        int installX = WIN_WIDTH - 280;
        if (selectedButton == 0)
        {
            DrawSelectionBar(renderer, installX, btnY, btnW, btnH);
            DrawString(renderer, installX + 24, btnY + 10, "Install (A)", COLOR_SEL_INK.r, COLOR_SEL_INK.g, COLOR_SEL_INK.b, 255, 1.0f);
        }
        else
        {
            DrawBevelPanel(renderer, installX, btnY, btnW, btnH, COLOR_RAIL);
            DrawString(renderer, installX + 24, btnY + 10, "Install (A)", COLOR_INK.r, COLOR_INK.g, COLOR_INK.b, 255, 1.0f);
        }

        int laterX = WIN_WIDTH - 140;
        if (selectedButton == 1)
        {
            DrawSelectionBar(renderer, laterX, btnY, btnW, btnH);
            DrawString(renderer, laterX + 28, btnY + 10, "Later (B)", COLOR_SEL_INK.r, COLOR_SEL_INK.g, COLOR_SEL_INK.b, 255, 1.0f);
        }
        else
        {
            DrawBevelPanel(renderer, laterX, btnY, btnW, btnH, COLOR_RAIL);
            DrawString(renderer, laterX + 28, btnY + 10, "Later (B)", COLOR_INK.r, COLOR_INK.g, COLOR_INK.b, 255, 1.0f);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    for (auto* pad : controllers) SDL_CloseGamepad(pad);
    return accepted;
}

void ShowExternalUpdateNoticeSdl(std::string_view version, uint32_t uiLanguage)
{
    const auto& text = FlatpakUpdateNoticeText(uiLanguage);
    const char* envDriver = std::getenv("SDL_VIDEODRIVER");
    const bool dummyEnv = envDriver && std::string_view(envDriver) == "dummy";

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        return;
    }

    const char* driver = SDL_GetCurrentVideoDriver();
    const bool dummy = dummyEnv || (driver && std::string_view(driver) == "dummy");
    if (dummy)
    {
        return;
    }

    std::vector<SDL_Gamepad*> controllers = OpenGamepads();

    constexpr int WIN_WIDTH = 700;
    constexpr int WIN_HEIGHT = 340;

    const auto title = std::string(text.title) + ": " + std::string(version);

    SDL_Window* window = CreateCenteredWindow(title.c_str(), WIN_WIDTH, WIN_HEIGHT);

    if (!window)
    {
        for (auto* pad : controllers) SDL_CloseGamepad(pad);
        return;
    }

    SDL_Renderer* renderer = CreateVsyncRenderer(window);
    if (!renderer)
    {
        SDL_DestroyWindow(window);
        for (auto* pad : controllers) SDL_CloseGamepad(pad);
        return;
    }

    bool done = false;
    while (!done)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT ||
                event.type == SDL_EVENT_KEY_DOWN ||
                event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ||
                event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
            {
                done = true;
                break;
            }
        }

        FillRect(renderer, 0, 0, WIN_WIDTH, WIN_HEIGHT, COLOR_STEEL);
        DrawBevelPanel(renderer, 10, 10, WIN_WIDTH - 20, WIN_HEIGHT - 20, COLOR_STEEL_PANEL);

        DrawString(renderer, 24, 24, TruncateTextWidth(title, WIN_WIDTH - 48, 1.1f), COLOR_ACCENT_GOLD.r, COLOR_ACCENT_GOLD.g, COLOR_ACCENT_GOLD.b, 255, 1.1f);

        DrawString(renderer, 24, 70, text.download, COLOR_INK.r, COLOR_INK.g, COLOR_INK.b, 255, 0.9f);
        DrawString(renderer, 24, 100, FlatpakReleaseUrl, COLOR_CYAN.r, COLOR_CYAN.g, COLOR_CYAN.b, 255, 0.9f);
        DrawString(renderer, 24, 145, text.install, COLOR_INK.r, COLOR_INK.g, COLOR_INK.b, 255, 0.9f);
        DrawString(renderer, 24, 175, FlatpakBundleInstallCommand, COLOR_CYAN.r, COLOR_CYAN.g, COLOR_CYAN.b, 255, 0.9f);
        DrawString(renderer, 24, 215, text.systemScope, COLOR_MUTED.r, COLOR_MUTED.g, COLOR_MUTED.b, 255, 0.9f);

        int btnX = (WIN_WIDTH - 120) / 2;
        int btnY = WIN_HEIGHT - 65;
        DrawSelectionBar(renderer, btnX, btnY, 120, 38);
        DrawString(renderer, btnX + (120 - MeasureTextWidth(text.close)) / 2, btnY + 10, text.close, COLOR_SEL_INK.r, COLOR_SEL_INK.g, COLOR_SEL_INK.b, 255, 1.0f);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    for (auto* pad : controllers) SDL_CloseGamepad(pad);
}
} // namespace updater
