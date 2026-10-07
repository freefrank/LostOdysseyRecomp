#pragma once

#ifdef _WIN32
#include <windows.h>
#include <SDL3/SDL.h>

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

        // SDL3 handles DPI awareness internally once the owner thread is
        // per-monitor aware. Pixel-sensitive paths query the drawable size.
        ready_ = true;
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
