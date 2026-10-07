#pragma once
#include <SDL3/SDL.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace gpu::video::window_mode {
#ifdef _WIN32
// VK_MENU is 18. Left and Right Alt both report that code; LM/RMENU can stay unset.
inline bool MenuAltHeld() {
    return (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
}
inline bool LeftAltHeld() {
    return (GetAsyncKeyState(VK_LMENU) & 0x8000) != 0;
}
inline bool RightAltHeld() {
    return (GetAsyncKeyState(VK_RMENU) & 0x8000) != 0;
}
#else
inline bool MenuAltHeld() { return false; }
inline bool LeftAltHeld() { return false; }
inline bool RightAltHeld() { return false; }
#endif

// Accept either the focused game window or a SYSKEY with no SDL focus yet.
inline bool TargetsGameWindow(const SDL_KeyboardEvent& event, uint32_t gameWindowID) {
    return event.windowID == 0 || event.windowID == gameWindowID;
}

inline bool IsToggleChord(const SDL_KeyboardEvent& event) {
    // Right Alt is often AltGr: fake Left Ctrl, MODE, and no KMOD_ALT on Enter.
    constexpr auto disallowed = SDL_KMOD_SHIFT | SDL_KMOD_GUI;
    const bool enter = event.scancode == SDL_SCANCODE_RETURN ||
        event.scancode == SDL_SCANCODE_KP_ENTER ||
        event.key == SDLK_RETURN || event.key == SDLK_KP_ENTER;
    const bool alt = (event.mod & (SDL_KMOD_ALT | SDL_KMOD_MODE)) ||
        MenuAltHeld() || LeftAltHeld() || RightAltHeld();
    return event.type == SDL_EVENT_KEY_DOWN && !event.repeat && enter && alt &&
        !(event.mod & disallowed);
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
