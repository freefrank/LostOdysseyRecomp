// Standalone Windows/SDL fixture; link the existing SDL2 library and Win32 libs.
// It never shows a window, changes a display mode/DPI, or launches the game.
// Actual display DPI observations and synthetic DPI messages are reported
// separately: sending a DPI message does not change a monitor's real DPI.
#define SDL_MAIN_HANDLED
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
// Match the runtime PCH's intrinsic declarations before SDL's Clang shim.
#include <x86intrin.h>
#include "../../LostOdysseyRecomp/gpu/window_pixels.h"
#include "../../LostOdysseyRecomp/gpu/window_mode.h"
#include "../../LostOdysseyRecomp/gpu/display_change.h"
#include "../../thirdparty/plume/plume_render_interface_types.h"
#include <SDL_syswm.h>
#include <array>
#include <cstdio>
#include <exception>
#include <memory>
#include <stdexcept>
#include <thread>
#include <string_view>

namespace
{
void Check(bool condition, const char* description)
{
    if (!condition) throw std::runtime_error(description);
}

struct ThreadAwareness
{
    DPI_AWARENESS_CONTEXT previous;
    explicit ThreadAwareness(DPI_AWARENESS_CONTEXT value) : previous(SetThreadDpiAwarenessContext(value))
    {
        Check(previous != nullptr, "SetThreadDpiAwarenessContext failed");
    }
    ~ThreadAwareness() { SetThreadDpiAwarenessContext(previous); }
};

void Pump()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {}
}

void CheckPhysicalSize(SDL_Window* window, int expectedWidth, int expectedHeight)
{
    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
    Check(SDL_GetWindowWMInfo(window, &info) == SDL_TRUE, "SDL native window lookup failed");
    const HWND native = info.info.win.window;
    Check(!IsWindowVisible(native), "fixture window became visible");
    Check(GetForegroundWindow() != native, "fixture window took focus");
    Check(AreDpiAwarenessContextsEqual(GetWindowDpiAwarenessContext(native),
              DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2), "game window is not PMv2 aware");

    int sdlWidth = 0, sdlHeight = 0, drawableWidth = 0, drawableHeight = 0;
    SDL_GetWindowSize(window, &sdlWidth, &sdlHeight);
    SDL_GetWindowSizeInPixels(window, &drawableWidth, &drawableHeight);
    Check(sdlWidth == expectedWidth && sdlHeight == expectedHeight, "SDL coordinate size differs from the selected resolution");
    Check(drawableWidth == expectedWidth && drawableHeight == expectedHeight, "SDL drawable size differs from the selected resolution");

    // The presentation thread is independent of the window thread. Exercise
    // the same GetClientRect input used by both Plume Windows swap chains,
    // then measure screen-space physical client corners in a PMv2 context.
    std::exception_ptr error;
    std::thread inspector([&] {
        try {
            const ThreadAwareness unaware(DPI_AWARENESS_CONTEXT_UNAWARE);
            RECT presentation{};
            Check(GetClientRect(native, &presentation), "cross-thread GetClientRect failed");
            uint32_t fixedWidth = 0, fixedHeight = 0;
            Check(plume::GetWindowClientPixels(native, fixedWidth, fixedHeight), "physical presentation size lookup failed");
            Check(fixedWidth == uint32_t(expectedWidth) && fixedHeight == uint32_t(expectedHeight),
                  "presentation thread received incorrect physical dimensions");
            Check(AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(), DPI_AWARENESS_CONTEXT_UNAWARE),
                  "presentation query leaked its thread DPI context");
            std::printf("presentation dpi=%u raw=%ldx%ld corrected=%ux%u restored=1\n", GetDpiForWindow(native),
                presentation.right - presentation.left, presentation.bottom - presentation.top, fixedWidth, fixedHeight);
            const ThreadAwareness physical(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
            RECT client{};
            Check(GetClientRect(native, &client), "physical GetClientRect failed");
            POINT first{client.left, client.top}, last{client.right, client.bottom};
            Check(ClientToScreen(native, &first) && ClientToScreen(native, &last), "physical client corner lookup failed");
            Check(last.x - first.x == expectedWidth && last.y - first.y == expectedHeight,
                  "actual physical client size differs from the selected resolution");
        } catch (...) { error = std::current_exception(); }
    });
    inspector.join();
    if (error) std::rethrow_exception(error);
    std::printf("physical display=%d dpi=%u client=%dx%d drawable=%dx%d hidden=1\n",
        SDL_GetWindowDisplayIndex(window), GetDpiForWindow(native), expectedWidth, expectedHeight,
        drawableWidth, drawableHeight);
}

void RunRenderingFixes()
{
    using namespace gpu::video;
    SDL_KeyboardEvent key{};
    key.type = SDL_KEYDOWN; key.keysym.sym = SDLK_RETURN; key.keysym.mod = KMOD_LALT;
    Check(window_mode::IsToggleChord(key), "Alt+Enter rejected");
    key.keysym.sym = SDLK_KP_ENTER;
    Check(window_mode::IsToggleChord(key), "Alt+keypad Enter rejected");
    key.repeat = 1;
    Check(!window_mode::IsToggleChord(key), "repeat toggles fullscreen");
    key.repeat = 0; key.type = SDL_KEYUP;
    Check(!window_mode::IsToggleChord(key), "key release toggles fullscreen");
    key.type = SDL_KEYDOWN;
    for (auto extra : {KMOD_RALT, KMOD_CTRL, KMOD_SHIFT, KMOD_GUI, KMOD_MODE}) {
        key.keysym.mod = Uint16(KMOD_LALT | extra);
        Check(!window_mode::IsToggleChord(key), "extra modifier toggles fullscreen");
    }
    key.keysym.mod = KMOD_RALT;
    Check(!window_mode::IsToggleChord(key), "AltGr toggles fullscreen");
    key.keysym.mod = Uint16(KMOD_LALT | KMOD_CAPS | KMOD_NUM);
    Check(window_mode::IsToggleChord(key), "lock keys suppress shortcut");
    DisplayChangeTracker tracker;
    const auto menu = tracker.Begin(1280, 720, 1);
    Check(!tracker.TryBegin(1280, 720, 0), "shortcut replaced pending menu window change");
    tracker.WindowComplete(menu, true);
    Check(!tracker.TryBegin(1280, 720, 0), "shortcut replaced pending menu presentation");
    tracker.Complete(menu, true);
    const auto shortcut = tracker.TryBegin(1280, 720, 0);
    Check(shortcut > menu, "shortcut did not obtain a new ticket");
    tracker.Complete(menu, false);
    Check(tracker.Query(shortcut) == DisplayChangeResult::Pending, "old result completed shortcut");
    tracker.WindowComplete(shortcut, true); tracker.Complete(shortcut, true);
    Check(tracker.Query(shortcut) == DisplayChangeResult::Applied, "shortcut did not finish");
    std::puts("shortcut: modifier/repeat/AltGr guards and pending display transaction passed");

    const ThreadAwareness inherited(DPI_AWARENESS_CONTEXT_UNAWARE);
    const window_pixels::Context pixels;
    Check(pixels.Ready(), "physical-pixel context failed");
    Check(SDL_InitSubSystem(SDL_INIT_VIDEO) == 0, SDL_GetError());
    struct Cleanup { ~Cleanup() { SDL_QuitSubSystem(SDL_INIT_VIDEO); } } cleanup;
    const int count = SDL_GetNumVideoDisplays();
    Check(count > 0, "no displays available");
    for (int display = 0; display < count; ++display) {
        const auto destroy = [](SDL_Window* window) { SDL_DestroyWindow(window); };
        std::unique_ptr<SDL_Window, decltype(destroy)> window(SDL_CreateWindow("Lost Odyssey rendering window fixture",
            SDL_WINDOWPOS_CENTERED_DISPLAY(display), SDL_WINDOWPOS_CENTERED_DISPLAY(display),
            1280, 720, SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE), destroy);
        Check(window != nullptr, SDL_GetError()); Pump();
        CheckPhysicalSize(window.get(), 1280, 720);
        SDL_SetWindowSize(window.get(), 3840, 2160); Pump();
        CheckPhysicalSize(window.get(), 3840, 2160);
        SDL_SetWindowSize(window.get(), 1136, 684); Pump();
        window_mode::Placement original; original.Capture(window.get());
        SDL_SysWMinfo info{}; SDL_VERSION(&info.version);
        Check(SDL_GetWindowWMInfo(window.get(), &info), "native window lookup failed");
        SDL_SetWindowBordered(window.get(), SDL_FALSE); Pump();
        Check(window_mode::FitBorderless(info.info.win.window), "borderless bounds correction failed"); Pump();
        MONITORINFO monitor{}; monitor.cbSize = sizeof(monitor); RECT bounds{};
        Check(GetMonitorInfoW(MonitorFromWindow(info.info.win.window, MONITOR_DEFAULTTONEAREST), &monitor), "monitor lookup failed");
        Check(GetWindowRect(info.info.win.window, &bounds) && EqualRect(&bounds, &monitor.rcMonitor), "borderless window does not fill its monitor");
        CheckPhysicalSize(window.get(), monitor.rcMonitor.right - monitor.rcMonitor.left, monitor.rcMonitor.bottom - monitor.rcMonitor.top);
        Check(window_mode::FitBorderless(info.info.win.window), "repeated borderless correction failed");
        SDL_SetWindowBordered(window.get(), SDL_TRUE); original.Restore(window.get()); Pump();
        window_mode::Placement restored; restored.Capture(window.get());
        Check(restored.x == original.x && restored.y == original.y && restored.width == original.width && restored.height == original.height,
              "windowed position or size was not restored");
        CheckPhysicalSize(window.get(), 1136, 684);
        if (count > 1) {
            const int next = (display + 1) % count;
            SDL_SetWindowPosition(window.get(), SDL_WINDOWPOS_CENTERED_DISPLAY(next), SDL_WINDOWPOS_CENTERED_DISPLAY(next)); Pump();
            CheckPhysicalSize(window.get(), 1136, 684);
        }
        std::printf("borderless display=%d physical=%ldx%ld windowed-placement-restored=1\n", display,
            monitor.rcMonitor.right - monitor.rcMonitor.left, monitor.rcMonitor.bottom - monitor.rcMonitor.top);
    }
}

void CheckDpiMessages(SDL_Window* window, int width, int height)
{
    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
    Check(SDL_GetWindowWMInfo(window, &info) == SDL_TRUE, "SDL native window lookup failed");
    const HWND native = info.info.win.window;
    const DWORD style = DWORD(GetWindowLongPtrW(native, GWL_STYLE));
    const DWORD extended = DWORD(GetWindowLongPtrW(native, GWL_EXSTYLE));
    const UINT actualDpi = GetDpiForWindow(native);
    for (const UINT nextDpi : std::array<UINT, 4>{96, 120, 144, 192})
    {
        RECT currentFrame{0, 0, width, height};
        Check(AdjustWindowRectExForDpi(&currentFrame, style, FALSE, extended, actualDpi), "current frame calculation failed");
        SIZE proposed{currentFrame.right - currentFrame.left, currentFrame.bottom - currentFrame.top};
        Check(SendMessageW(native, WM_GETDPISCALEDSIZE, nextDpi, LPARAM(&proposed)) == TRUE,
              "SDL did not handle WM_GETDPISCALEDSIZE");
        RECT nextFrame{0, 0, width, height};
        Check(AdjustWindowRectExForDpi(&nextFrame, style, FALSE, extended, nextDpi), "next frame calculation failed");
        Check(proposed.cx == nextFrame.right - nextFrame.left && proposed.cy == nextFrame.bottom - nextFrame.top,
              "DPI transition would scale the client area");
        std::printf("message-contract actual-dpi=%u requested-dpi=%u client=%dx%d\n", actualDpi, nextDpi, width, height);
    }
    // A same-DPI notification exercises the real WM_DPICHANGED handler without
    // pretending that a synthetic message changed this monitor's scaling.
    RECT unchanged{};
    Check(GetWindowRect(native, &unchanged), "window rectangle lookup failed");
    SendMessageW(native, WM_DPICHANGED, MAKEWPARAM(actualDpi, actualDpi), LPARAM(&unchanged));
    Pump();
    CheckPhysicalSize(window, width, height);
}

void RunCycle(int cycle)
{
    const ThreadAwareness inherited(DPI_AWARENESS_CONTEXT_UNAWARE);
    // Exercise an inherited/env-like conflicting SDL preference. The game's
    // policy must win before video initialization, including a second lifecycle.
    Check(SDL_SetHintWithPriority(SDL_HINT_WINDOWS_DPI_SCALING, "1", SDL_HINT_OVERRIDE) == SDL_TRUE,
          "could not arrange SDL scaling conflict");
    {
        const gpu::video::window_pixels::Context pixels;
        Check(pixels.Ready(), "physical-pixel context initialization failed");
        Check(SDL_InitSubSystem(SDL_INIT_VIDEO) == 0, SDL_GetError());
        struct VideoCleanup { ~VideoCleanup() { SDL_QuitSubSystem(SDL_INIT_VIDEO); } } cleanup;
        const int count = SDL_GetNumVideoDisplays();
        Check(count > 0, "no displays available");
        for (int display = 0; display < count; ++display)
        {
            const auto destroy = [](SDL_Window* window) { SDL_DestroyWindow(window); };
            std::unique_ptr<SDL_Window, decltype(destroy)> window(SDL_CreateWindow("Lost Odyssey pixel fixture",
                SDL_WINDOWPOS_CENTERED_DISPLAY(display), SDL_WINDOWPOS_CENTERED_DISPLAY(display),
                640, 360, SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE), destroy);
            Check(window != nullptr, SDL_GetError());
            Pump();
            CheckPhysicalSize(window.get(), 640, 360);
            SDL_SetWindowSize(window.get(), 1280, 720);
            Pump();
            CheckPhysicalSize(window.get(), 1280, 720);
            CheckDpiMessages(window.get(), 1280, 720);
            if (count > 1)
            {
                const int next = (display + 1) % count;
                SDL_SetWindowPosition(window.get(), SDL_WINDOWPOS_CENTERED_DISPLAY(next), SDL_WINDOWPOS_CENTERED_DISPLAY(next));
                Pump();
                CheckPhysicalSize(window.get(), 1280, 720);
            }
        }
    }
    Check(AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(), DPI_AWARENESS_CONTEXT_UNAWARE),
          "window lifecycle did not restore its prior thread awareness");
    // Failure/early-return cleanup uses the same scope even without an HWND.
    { const gpu::video::window_pixels::Context pixels; Check(pixels.Ready(), "empty context initialization failed"); }
    Check(AreDpiAwarenessContextsEqual(GetThreadDpiAwarenessContext(), DPI_AWARENESS_CONTEXT_UNAWARE),
          "empty context did not restore its prior thread awareness");
    std::printf("lifecycle=%d restored=1\n", cycle);
}
}

int main(int argc, char** argv)
{
    SDL_SetMainReady();
    std::exception_ptr error;
    std::thread owner([&] {
        try {
            if (argc == 2 && std::string_view(argv[1]) == "--rendering-fixes-only") RunRenderingFixes();
            else { RunCycle(1); RunCycle(2); }
        }
        catch (...) { error = std::current_exception(); }
    });
    owner.join();
    try { if (error) std::rethrow_exception(error); }
    catch (const std::exception& failure)
    {
        std::fprintf(stderr, "FAIL: %s\n", failure.what());
        return 1;
    }
    std::puts("PASS: hidden physical window sizes, SDL DPI messages, cross-thread presentation dimensions and context restoration");
    return 0;
}
