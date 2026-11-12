#pragma once
#include <SDL3/SDL.h>
#include <host_ui/rasterizer.h>
#include <cstdlib>
#include <string_view>
#include <vector>

namespace host_ui
{
// A window like the update prompt's. The renderer has not started yet, so SDL
// is initialized here and shut down again before video::Init.
class PromptWindow
{
public:
    bool Open()
    {
        const char *driver = std::getenv("SDL_VIDEODRIVER");
        if (driver && std::string_view(driver) == "dummy") return false;
        constexpr uint32_t subsystems = SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS;
        if (!SDL_InitSubSystem(subsystems)) return false;
        initialized_ = subsystems;
        if (const char *current = SDL_GetCurrentVideoDriver(); current && std::string_view(current) == "dummy")
            return false;
        window_ = SDL_CreateWindow("Lost Odyssey Recomp", 1280, 720,
                                   SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        if (!window_) return false;
        SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        renderer_ = SDL_CreateRenderer(window_, nullptr);
        if (!renderer_) renderer_ = SDL_CreateRenderer(window_, "software");
        if (renderer_) SDL_SetRenderVSync(renderer_, 1);
        // SDL2 sampled textures with nearest filtering by default; SDL3 smooths them.
        if (renderer_) SDL_SetDefaultTextureScaleMode(renderer_, SDL_SCALEMODE_NEAREST);
        texture_ = renderer_ ? SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, 1280, 720)
                             : nullptr;
        if (!texture_) return false;
        SDL_SetRenderLogicalPresentation(renderer_, 1280, 720, SDL_LOGICAL_PRESENTATION_LETTERBOX);
        int count = 0;
        if (SDL_JoystickID *ids = SDL_GetGamepads(&count))
        {
            for (int i = 0; i < count; ++i)
                if (auto *controller = SDL_OpenGamepad(ids[i])) controllers_.push_back(controller);
            SDL_free(ids);
        }
        pixels_.Resize(1280, 720);
        return true;
    }
    ~PromptWindow()
    {
        for (auto *controller : controllers_) SDL_CloseGamepad(controller);
        if (texture_) SDL_DestroyTexture(texture_);
        if (renderer_) SDL_DestroyRenderer(renderer_);
        if (window_) SDL_DestroyWindow(window_);
        if (initialized_) SDL_QuitSubSystem(initialized_);
        if (initialized_ && !SDL_WasInit(0)) SDL_Quit();
    }
    uint32_t Id() const { return SDL_GetWindowID(window_); }
    void Size(int &width, int &height) const { SDL_GetWindowSize(window_, &width, &height); }
    host_ui::PixelBuffer &Pixels() { return pixels_; }
    void Present()
    {
        SDL_UpdateTexture(texture_, nullptr, pixels_.pixels.data(), 1280 * sizeof(uint32_t));
        SDL_RenderClear(renderer_);
        SDL_RenderTexture(renderer_, texture_, nullptr, nullptr);
        SDL_RenderPresent(renderer_);
    }

private:
    uint32_t initialized_ = 0;
    SDL_Window *window_ = nullptr;
    SDL_Renderer *renderer_ = nullptr;
    SDL_Texture *texture_ = nullptr;
    std::vector<SDL_Gamepad *> controllers_;
    host_ui::PixelBuffer pixels_;
};

}
