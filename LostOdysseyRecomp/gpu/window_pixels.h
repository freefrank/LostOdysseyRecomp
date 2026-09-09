#pragma once

#ifdef _WIN32
#include <windows.h>
#include <SDL.h>

namespace gpu::video::window_pixels
{
// Keep this scope alive through SDL window creation, events and destruction.
// Awareness belongs to the window-owning thread: setting it on the first-run
// dialog's thread does not establish it on a subsequently created thread.
class Context final
{
    DPI_AWARENESS_CONTEXT previous_ = nullptr;
    bool ready_ = false;

public:
    Context() noexcept
    {
        previous_ = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        if (!previous_) return;

        // SDL window sizes and mouse coordinates must stay in physical pixels.
        // Set this before SDL's first video initialization, including retries.
        // It is an application-wide SDL policy, not a change to Windows scaling
        // or to the native settings, installer, updater and Debug Menu layouts.
        ready_ = SDL_SetHintWithPriority(SDL_HINT_WINDOWS_DPI_SCALING, "0", SDL_HINT_OVERRIDE) == SDL_TRUE;
    }

    ~Context()
    {
        if (previous_) SetThreadDpiAwarenessContext(previous_);
    }

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    bool Ready() const noexcept { return ready_; }
};
}
#endif
