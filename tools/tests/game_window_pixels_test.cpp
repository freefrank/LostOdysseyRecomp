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
#include <SDL_syswm.h>
#include <array>
#include <cstdio>
#include <exception>
#include <memory>
#include <stdexcept>
#include <thread>

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
            Check(presentation.right - presentation.left == expectedWidth &&
                  presentation.bottom - presentation.top == expectedHeight,
                  "presentation thread received virtualized dimensions");
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

int main()
{
    SDL_SetMainReady();
    std::exception_ptr error;
    std::thread owner([&] {
        try { RunCycle(1); RunCycle(2); }
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
