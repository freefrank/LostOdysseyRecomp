// Direct presentation checks only. Creates an inactive desktop; never switches it.
#include "updater/progress.h"
#include "settings/window_chrome.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
void Check(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

std::wstring Text(HWND window)
{
    std::wstring value(size_t(GetWindowTextLengthW(window)) + 1, L'\0');
    GetWindowTextW(window, value.data(), int(value.size()));
    value.resize(value.size() - 1);
    return value;
}

std::wstring InputDesktop()
{
    HDESK desktop = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    Check(desktop != nullptr, "input desktop handle");
    wchar_t name[256]{};
    DWORD needed{};
    const bool read = GetUserObjectInformationW(desktop, UOI_NAME, name, sizeof(name), &needed) != FALSE;
    CloseDesktop(desktop);
    Check(read, "input desktop name");
    return name;
}

void Capture(HWND window, const std::filesystem::path &path)
{
    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    RECT rect{};
    GetWindowRect(window, &rect);
    const int width = rect.right - rect.left, height = rect.bottom - rect.top;
    HDC source = GetWindowDC(window), memory = CreateCompatibleDC(source);
    HBITMAP bitmap = CreateCompatibleBitmap(source, width, height);
    HGDIOBJ previous = SelectObject(memory, bitmap);
    // WM_PRINT explicitly asks the real window and its native child controls to
    // render; PrintWindow may reuse stale backing surfaces on inactive desktops.
    SendMessageW(window, WM_PRINT, reinterpret_cast<WPARAM>(memory),
                 PRF_CLIENT | PRF_CHILDREN | PRF_ERASEBKGND);
    SelectObject(memory, previous);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    std::vector<unsigned char> pixels(size_t(width) * height * 4);
    const int lines = GetDIBits(memory, bitmap, 0, UINT(height), pixels.data(), &info, DIB_RGB_COLORS);
    DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(window, source);
    Check(lines == height, "actual window capture");
    BITMAPFILEHEADER file{};
    file.bfType = 0x4D42;
    file.bfOffBits = sizeof(file) + sizeof(info.bmiHeader);
    file.bfSize = file.bfOffBits + DWORD(pixels.size());
    std::ofstream stream(path, std::ios::binary);
    stream.write(reinterpret_cast<const char *>(&file), sizeof(file));
    stream.write(reinterpret_cast<const char *>(&info.bmiHeader), sizeof(info.bmiHeader));
    stream.write(reinterpret_cast<const char *>(pixels.data()), std::streamsize(pixels.size()));
    Check(bool(stream), "write capture");
}

struct TextSpy
{
    HWND window;
    WNDPROC previous;
    int writes = 0;
    explicit TextSpy(HWND target) : window(target)
    {
        SetPropW(window, L"LoUpdaterUiTestSpy", this);
        previous = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(Proc)));
    }
    ~TextSpy()
    {
        SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(previous));
        RemovePropW(window, L"LoUpdaterUiTestSpy");
    }
    static LRESULT CALLBACK Proc(HWND window, UINT message, WPARAM wp, LPARAM lp)
    {
        auto self = static_cast<TextSpy *>(GetPropW(window, L"LoUpdaterUiTestSpy"));
        if (message == WM_SETTEXT) ++self->writes;
        return CallWindowProcW(self->previous, window, message, wp, lp);
    }
};

void Bounds(HWND window)
{
    RECT client{};
    GetClientRect(window, &client);
    for (int id : {101, 102, 103, 104, IDCANCEL, settings::window_chrome::CloseId,
                   settings::window_chrome::MinimizeId})
    {
        HWND child = GetDlgItem(window, id);
        Check(child != nullptr, "expected control");
        RECT rect{};
        GetWindowRect(child, &rect);
        MapWindowPoints(nullptr, window, reinterpret_cast<POINT *>(&rect), 2);
        Check(rect.left >= 0 && rect.top >= 0 && rect.right <= client.right && rect.bottom <= client.bottom,
              "control inside client bounds");
    }
}

void Exercise(const std::filesystem::path &output)
{
    using settings::window_chrome::Px;
    constexpr uint64_t MiB = 1024 * 1024;
    {
        updater::ProgressWindow progress(0);
        HWND window = FindWindowW(L"LostOdysseyUpdateProgress", nullptr);
        Check(window != nullptr, "native progress window");
        Bounds(window);
        const LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
        Check((style & (WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_SYSMENU)) ==
              (WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_SYSMENU), "native window contract retained");
        progress.SetDownloadProgress(8 * MiB, 0);
        Check(Text(GetDlgItem(window, 103)) == L"8.0 MiB", "unknown total has no zero denominator");
        Check(Text(GetDlgItem(window, 104)).empty(), "unknown total has no made-up percentage");
        Capture(window, output / "updater-unknown.bmp");
        progress.SetDownloadProgress(8 * MiB, 32 * MiB);
        Check(Text(GetDlgItem(window, 104)) == L"25%", "known progress percentage");
        Check(IsWindowEnabled(GetDlgItem(window, IDCANCEL)), "download cancellation enabled");
        UpdateWindow(window);
        {
            TextSpy status(GetDlgItem(window, 102)), amount(GetDlgItem(window, 103)), percentage(GetDlgItem(window, 104));
            progress.SetDownloadProgress(8 * MiB, 32 * MiB);
            Check(status.writes == 0 && amount.writes == 0 && percentage.writes == 0, "unchanged values do not rewrite labels");
            Check(!GetUpdateRect(window, nullptr, FALSE), "unchanged determinate value does not invalidate track");
        }
        Capture(window, output / "updater-download.bmp");
        progress.SetPhase(updater::ProgressPhase::Verifying);
        Check(Text(GetDlgItem(window, 102)) == L"Verifying…", "verification state");
        Check(!IsWindowEnabled(GetDlgItem(window, IDCANCEL)), "verification cannot claim unsupported cancellation");
        SendMessageW(window, WM_CLOSE, 0, 0);
        Check(!progress.Cancelled() && IsWindow(window), "verification close keeps transaction owner intact");
        progress.SetPhase(updater::ProgressPhase::Ready);
        Check(Text(GetDlgItem(window, 104)) == L"100%", "ready is complete");
        Check(!IsWindowEnabled(GetDlgItem(window, settings::window_chrome::CloseId)), "ready close disabled");
        ShowWindow(window, SW_SHOWMINNOACTIVE);
        progress.Cancelled();
        Check(IsIconic(window), "native minimize");
    }
    Check(FindWindowW(L"LostOdysseyUpdateProgress", nullptr) == nullptr, "window released after owner destruction");
    {
        updater::ProgressWindow progress(4);
        HWND window = FindWindowW(L"LostOdysseyUpdateProgress", nullptr);
        SetWindowPos(window, nullptr, 100, 100, Px(window, 440), Px(window, 280), SWP_NOZORDER | SWP_NOACTIVATE);
        progress.SetPhase(updater::ProgressPhase::CheckingPackage);
        Check(Text(GetDlgItem(window, 102)) == L"Checking package…", "English package phase regardless of game language");
        Bounds(window);
        Capture(window, output / "updater-narrow-zh.bmp");
    }
    {
        updater::ProgressWindow progress(0);
        HWND window = FindWindowW(L"LostOdysseyUpdateProgress", nullptr);
        progress.SetDownloadProgress(4 * MiB, 16 * MiB);
        SendMessageW(window, WM_CLOSE, 0, 0);
        Check(progress.Cancelled(), "download close requests cancellation");
        Check(!IsWindowEnabled(GetDlgItem(window, IDCANCEL)), "cancel disables repeated requests");
        progress.SetDownloadProgress(5 * MiB, 16 * MiB);
        Check(Text(GetDlgItem(window, 102)) == L"Cancelling…", "late progress cannot overwrite cancelling state");
    }
}

void Render(const std::filesystem::path &output, bool narrowOnly)
{
    constexpr uint64_t MiB = 1024 * 1024;
    if (!narrowOnly)
    {
        updater::ProgressWindow progress(7);
        HWND window = FindWindowW(L"LostOdysseyUpdateProgress", nullptr);
        Check(Text(GetDlgItem(window, 101)) == L"Update", "English title with non-English language value");
        Check(Text(GetDlgItem(window, IDCANCEL)) == L"Cancel", "English Cancel button");
        Check(Text(GetDlgItem(window, 102)) == L"Preparing…", "English preparation text");
        Check(SendMessageW(GetDlgItem(window, 101), WM_GETFONT, 0, 0) != 0, "title font assigned");
        Check(SendMessageW(GetDlgItem(window, IDCANCEL), WM_GETFONT, 0, 0) != 0, "button font assigned");
        progress.SetDownloadProgress(8 * MiB, 0);
        Check(Text(GetDlgItem(window, 102)) == L"Downloading…", "English download text");
        Capture(window, output / "updater-unknown.bmp");
        progress.SetDownloadProgress(8 * MiB, 32 * MiB);
        Capture(window, output / "updater-download.bmp");
    }
    {
        updater::ProgressWindow progress(7);
        HWND window = FindWindowW(L"LostOdysseyUpdateProgress", nullptr);
        SetWindowPos(window, nullptr, 100, 100, settings::window_chrome::Px(window, 440),
                     settings::window_chrome::Px(window, 280), SWP_NOZORDER | SWP_NOACTIVATE);
        progress.SetPhase(updater::ProgressPhase::CheckingPackage);
        Check(Text(GetDlgItem(window, 102)) == L"Checking package…", "English narrow package text");
        Capture(window, output / "updater-narrow-zh.bmp");
    }
}
}

int wmain(int argc, wchar_t **argv)
{
    const bool narrowOnly = argc == 3 && std::wstring_view(argv[2]) == L"--render-narrow-only";
    const bool renderOnly = narrowOnly || (argc == 3 && std::wstring_view(argv[2]) == L"--render-only");
    if (argc != 2 && !renderOnly) return 2;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const auto originalInput = InputDesktop();
    HDESK previous = GetThreadDesktop(GetCurrentThreadId());
    const std::wstring name = L"LoUpdaterUi-" + std::to_wstring(GetCurrentProcessId());
    HDESK desktop = CreateDesktopW(name.c_str(), nullptr, nullptr, 0, GENERIC_ALL, nullptr);
    if (!desktop || !SetThreadDesktop(desktop)) return 3;
    int result = 0;
    try
    {
        std::filesystem::create_directories(argv[1]);
        if (renderOnly) Render(argv[1], narrowOnly);
        else Exercise(argv[1]);
        Check(InputDesktop() == originalInput, "user input desktop unchanged");
        if (renderOnly) std::cout << "PASS: updater actual UI rendering only; prior functional checks retained\n";
        else std::cout << "PASS: updater UI state, cancellation boundary, change cache, native sizing, localization and render\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        result = 1;
    }
    const bool restored = SetThreadDesktop(previous) != FALSE;
    const bool closed = CloseDesktop(desktop) != FALSE;
    std::wcout << L"desktop=" << name << L" thread_restored=" << restored << L" desktop_closed=" << closed << '\n';
    // If native theme helpers retain a window, the parent verifies exit cleanup.
    return result;
}
