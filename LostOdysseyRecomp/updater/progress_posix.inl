#include "progress.h"
#include "posix_ui.h"
#include "external_update_notice.h"
#include "../install/installer_colors.h"
#include "../install/installer_font.h"
#include "../install/installer_navigation.h"

#ifndef _WIN32
#include <SDL3/SDL.h>
#include <hid/face_buttons.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace updater
{
using namespace install::ui;

namespace
{
std::vector<SDL_Gamepad*> OpenProgressGamepads()
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

SDL_Window* CreateProgressWindow(const char* title, int width, int height)
{
    SDL_Window* window = SDL_CreateWindow(title, width, height, 0);
    if (window) SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    return window;
}

SDL_Renderer* CreateProgressRenderer(SDL_Window* window)
{
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) renderer = SDL_CreateRenderer(window, "software");
    if (renderer) SDL_SetRenderVSync(renderer, 1);
    // SDL2 sampled textures with nearest filtering by default; SDL3 smooths them.
    if (renderer) SDL_SetDefaultTextureScaleMode(renderer, SDL_SCALEMODE_NEAREST);
    return renderer;
}

std::string FormatBytes(uint64_t bytes)
{
    double gib = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
    double mib = static_cast<double>(bytes) / (1024.0 * 1024.0);
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);
    if (gib >= 1.0)
        ss << gib << " GiB";
    else
        ss << mib << " MiB";
    return ss.str();
}

std::string Utf8FromWide(std::wstring_view wide)
{
    std::string result;
    result.reserve(wide.size());
    for (wchar_t c : wide)
    {
        if (c < 0x80)
            result.push_back(static_cast<char>(c));
        else
            result.push_back('?'); // Sufficient for ascii/fallback numbers/labels
    }
    return result;
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

        // Word-wrap paragraph
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
    const char* driver = SDL_GetCurrentVideoDriver();
    const bool dummy = (driver && std::string_view(driver) == "dummy") ||
                       (std::getenv("SDL_VIDEODRIVER") && std::string_view(std::getenv("SDL_VIDEODRIVER")) == "dummy");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        return false;
    }

    // In dummy mode, don't block user forever in headless environment.
    if (dummy)
    {
        return false;
    }

    std::vector<SDL_Gamepad*> controllers = OpenProgressGamepads();

    constexpr int WIN_WIDTH = 640;
    constexpr int WIN_HEIGHT = 460;

    SDL_Window* window = CreateProgressWindow("Lost Odyssey Recomp - Update Available", WIN_WIDTH, WIN_HEIGHT);

    if (!window)
    {
        for (auto* pad : controllers) SDL_CloseGamepad(pad);
        return false;
    }

    SDL_Renderer* renderer = CreateProgressRenderer(window);
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
            hid::face_buttons::Normalize(event);
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
                if (event.wheel.integer_y > 0 && scrollOffset > 0) scrollOffset--;
                else if (event.wheel.integer_y < 0 && scrollOffset + MAX_VISIBLE_LINES < static_cast<int>(logLines.size())) scrollOffset++;
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

        // Install button
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

        // Later button
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
    const char* driver = SDL_GetCurrentVideoDriver();
    const bool dummy = (driver && std::string_view(driver) == "dummy") ||
                       (std::getenv("SDL_VIDEODRIVER") && std::string_view(std::getenv("SDL_VIDEODRIVER")) == "dummy");

    if (dummy || !SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        return;
    }

    std::vector<SDL_Gamepad*> controllers = OpenProgressGamepads();

    constexpr int WIN_WIDTH = 700;
    constexpr int WIN_HEIGHT = 340;

    const auto title = std::string(text.title) + ": " + std::string(version);

    SDL_Window* window = CreateProgressWindow(title.c_str(), WIN_WIDTH, WIN_HEIGHT);

    if (!window)
    {
        for (auto* pad : controllers) SDL_CloseGamepad(pad);
        return;
    }

    SDL_Renderer* renderer = CreateProgressRenderer(window);
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

struct ProgressWindow::Impl
{
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    std::vector<SDL_Gamepad*> controllers;
    std::string phaseText;
    std::string detailText;
    std::string amountText;
    uint64_t completed = 0;
    uint64_t total = 0;
    bool cancelled = false;
    bool cancellable = true;
    bool dummy = false;

    explicit Impl(uint32_t language)
    {
        const char* driver = SDL_GetCurrentVideoDriver();
        dummy = (driver && std::string_view(driver) == "dummy") ||
                (std::getenv("SDL_VIDEODRIVER") && std::string_view(std::getenv("SDL_VIDEODRIVER")) == "dummy");

        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
        {
            return;
        }

        controllers = OpenProgressGamepads();

        constexpr int WIN_WIDTH = 540;
        constexpr int WIN_HEIGHT = 220;

        window = CreateProgressWindow("Lost Odyssey Recomp - Updating", WIN_WIDTH, WIN_HEIGHT);

        if (window)
        {
            renderer = CreateProgressRenderer(window);
        }
    }

    ~Impl()
    {
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        for (auto* pad : controllers) SDL_CloseGamepad(pad);
    }

    void Pump()
    {
        if (!window) return;

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            hid::face_buttons::Normalize(event);
            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                if (cancellable) cancelled = true;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE && cancellable)
                    cancelled = true;
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                if (event.gbutton.button == SDL_GAMEPAD_BUTTON_EAST && cancellable)
                    cancelled = true;
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (cancellable && event.button.button == SDL_BUTTON_LEFT)
                {
                    int mx = event.button.x, my = event.button.y;
                    int btnX = 540 - 140;
                    int btnY = 220 - 55;
                    if (mx >= btnX && mx <= btnX + 110 && my >= btnY && my <= btnY + 34)
                        cancelled = true;
                }
                break;
            default:
                break;
            }
        }
        Render();
    }

    void Render()
    {
        if (!renderer) return;

        constexpr int WIN_WIDTH = 540;
        constexpr int WIN_HEIGHT = 220;

        FillRect(renderer, 0, 0, WIN_WIDTH, WIN_HEIGHT, COLOR_STEEL);
        DrawBevelPanel(renderer, 10, 10, WIN_WIDTH - 20, WIN_HEIGHT - 20, COLOR_STEEL_PANEL);

        // Title
        DrawString(renderer, 24, 24, "UPDATING LOST ODYSSEY RECOMP", COLOR_ACCENT_GOLD.r, COLOR_ACCENT_GOLD.g, COLOR_ACCENT_GOLD.b, 255, 1.15f);

        // Status text
        std::string status = detailText.empty() ? phaseText : detailText;
        if (!status.empty())
        {
            DrawString(renderer, 24, 60, status, COLOR_INK.r, COLOR_INK.g, COLOR_INK.b, 255, 1.0f);
        }

        // Progress bar
        int barX = 24;
        int barY = 92;
        int barW = WIN_WIDTH - 48;
        int barH = 26;
        FillRect(renderer, barX, barY, barW, barH, COLOR_RAIL);
        DrawRect(renderer, barX, barY, barW, barH, COLOR_BORDER_LINE);

        double pct = total > 0 ? std::clamp(static_cast<double>(completed) / total, 0.0, 1.0) : 0.0;
        int fillW = static_cast<int>(barW * pct);
        if (fillW > 0)
        {
            FillRect(renderer, barX + 2, barY + 2, fillW - 4, barH - 4, COLOR_CYAN);
        }

        // Amount text
        if (!amountText.empty())
        {
            DrawString(renderer, 24, 128, amountText, COLOR_MUTED.r, COLOR_MUTED.g, COLOR_MUTED.b, 255, 0.95f);
        }

        // Cancel button
        if (cancellable)
        {
            int btnX = WIN_WIDTH - 140;
            int btnY = WIN_HEIGHT - 55;
            DrawBevelPanel(renderer, btnX, btnY, 110, 34, COLOR_RAIL);
            DrawString(renderer, btnX + 18, btnY + 9, "Cancel (B)", COLOR_MUTED.r, COLOR_MUTED.g, COLOR_MUTED.b, 255, 0.95f);
        }

        SDL_RenderPresent(renderer);
    }
};

ProgressWindow::ProgressWindow(uint32_t language) : impl_(std::make_unique<Impl>(language)) {}
ProgressWindow::~ProgressWindow() = default;

void ProgressWindow::SetProgress(uint64_t completed, uint64_t total, std::wstring_view detail)
{
    impl_->completed = completed;
    impl_->total = total;
    impl_->detailText = Utf8FromWide(detail);
    impl_->Pump();
}

void ProgressWindow::SetDownloadProgress(uint64_t completed, uint64_t total)
{
    impl_->completed = completed;
    impl_->total = total;
    impl_->cancellable = true;
    if (total > 0)
    {
        double pct = (double(completed) / double(total)) * 100.0;
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << pct << "% ("
           << FormatBytes(completed) << " / " << FormatBytes(total) << ")";
        impl_->amountText = ss.str();
    }
    else
    {
        impl_->amountText = FormatBytes(completed);
    }
    impl_->phaseText = "Downloading update…";
    impl_->Pump();
}

void ProgressWindow::SetPhase(ProgressPhase phase)
{
    if (phase == ProgressPhase::Ready)
    {
        impl_->cancellable = false;
        impl_->amountText.clear();
        impl_->phaseText = "Ready to restart";
        impl_->completed = 1;
        impl_->total = 1;
    }
    else
    {
        impl_->cancellable = false;
        impl_->amountText.clear();
        impl_->phaseText = "Extracting…";
    }
    impl_->Pump();
}

void ProgressWindow::SetPhase(std::wstring_view detail)
{
    impl_->cancellable = false;
    impl_->amountText.clear();
    impl_->phaseText = Utf8FromWide(detail);
    impl_->Pump();
}

bool ProgressWindow::Cancelled()
{
    impl_->Pump();
    return impl_->cancelled;
}

} // namespace updater
#endif
