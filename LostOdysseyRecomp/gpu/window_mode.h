#pragma once
#include <SDL.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace gpu::video::window_mode {
inline bool IsToggleChord(const SDL_KeyboardEvent& event) {
    // Right Alt may be AltGr even without a Ctrl event on some layouts.
    constexpr auto disallowed = KMOD_RALT | KMOD_CTRL | KMOD_SHIFT | KMOD_GUI | KMOD_MODE;
    return event.type == SDL_KEYDOWN && !event.repeat &&
        (event.keysym.sym == SDLK_RETURN || event.keysym.sym == SDLK_KP_ENTER) &&
        (event.keysym.mod & KMOD_LALT) && !(event.keysym.mod & disallowed);
}

struct Placement {
    int x = 0, y = 0, width = 0, height = 0;
    bool maximized = false;
    bool valid = false;
    void Capture(SDL_Window* window) {
        SDL_GetWindowPosition(window, &x, &y);
        SDL_GetWindowSize(window, &width, &height);
        maximized = (SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED) != 0;
        valid = width > 0 && height > 0;
    }
    void Restore(SDL_Window* window) const {
        if (!valid) return;
        // SDL retains the normal rectangle when it restores a maximized window.
        // Do not replace that rectangle with the maximized client dimensions.
        if (maximized) { SDL_MaximizeWindow(window); return; }
        SDL_SetWindowPosition(window, x, y);
        SDL_SetWindowSize(window, width, height);
    }
};

#ifdef _WIN32
// Called on the PMv2 window owner after SDL processes native DPI/display events.
// Repair only an incorrect outer rectangle; the renderer retains aspect ratio.
inline bool FitBorderless(HWND window) {
    if (!window || IsIconic(window)) return true;
    MONITORINFO monitor{};
    monitor.cbSize = sizeof(monitor);
    RECT current{};
    if (!GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor) ||
        !GetWindowRect(window, &current)) return false;
    const RECT& target = monitor.rcMonitor;
    if (EqualRect(&current, &target)) return true;
    return SetWindowPos(window, nullptr, target.left, target.top,
        target.right - target.left, target.bottom - target.top,
        SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER) != FALSE;
}
#endif
}
