#include "updater/game_prompt.h"
#include "host_ui/rasterizer.h"
#include <SDL.h>

#include <chrono>
#include <future>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

bool WaitForOffer()
{
    for (int i = 0; i < 100; ++i)
    {
        if (updater::game_prompt::Visible()) return true;
        std::this_thread::sleep_for(10ms);
    }
    return false;
}

int main()
{
    updater::game_prompt::ShowChecking(4);
    if (!updater::game_prompt::Visible()) return 1;
    host_ui::PixelBuffer pixels;
    pixels.Resize(1280, 720);
    host_ui::Rasterizer rasterizer(pixels);
    updater::game_prompt::Render(rasterizer);
    if (pixels.pixels[0] == 0) return 2;
    updater::game_prompt::HideChecking();
    if (updater::game_prompt::Visible()) return 3;

    auto accepted = std::async(std::launch::async, [] {
        return updater::game_prompt::Confirm("v0.7.0", "中文更新内容\nA longer English line", 4);
    });
    if (!WaitForOffer()) return 4;
    updater::game_prompt::Render(rasterizer);
    SDL_Event click{};
    click.type = SDL_MOUSEBUTTONDOWN;
    click.button.windowID = 7;
    click.button.x = 1400; // 1920x1080 letterboxed scaling of Install button.
    click.button.y = 960;
    if (!updater::game_prompt::HandleEvent(click, 7, 1920, 1080)) return 5;
    if (accepted.wait_for(1s) != std::future_status::ready || !accepted.get()) return 6;
    if (updater::game_prompt::Visible()) return 7;

    auto declined = std::async(std::launch::async, [] {
        return updater::game_prompt::Confirm("v0.7.0", "Later", 0);
    });
    if (!WaitForOffer()) return 8;
    SDL_Event escape{};
    escape.type = SDL_KEYDOWN;
    escape.key.windowID = 7;
    escape.key.keysym.sym = SDLK_ESCAPE;
    if (!updater::game_prompt::HandleEvent(escape, 7, 1280, 720)) return 9;
    if (declined.wait_for(1s) != std::future_status::ready || declined.get()) return 10;
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    if (updater::game_prompt::ConfirmBeforeImport("v0.7.0", "No display", 0)) return 11;
    std::cout << "startup update prompt: render, install, later and headless guard passed\n";
    return 0;
}
