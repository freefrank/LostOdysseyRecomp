#include "progress.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "settings/window_chrome.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>
#include <string>

namespace updater
{
namespace ui = settings::window_chrome;

struct ProgressWindow::Impl
{
    static constexpr UINT_PTR AnimationTimer = 1;
    HWND window{}, title{}, status{}, amount{}, percentage{}, cancel{};
    HFONT font{}, titleFont{};
    ui::State chrome;
    uint32_t language = 0;
    double fraction = 0.0;
    bool cancelled = false, cancellable = true, indeterminate = true;
    bool animating = false, animationsEnabled = true;
    std::wstring statusText, amountText, percentText;

    const wchar_t *Pick(const wchar_t *en, const wchar_t *tw, const wchar_t *jp,
                        const wchar_t *kr, const wchar_t *sc) const
    {
        const wchar_t *values[] = {en, tw, jp, kr, sc};
        return values[std::min(language, 4u)];
    }

    void Pump()
    {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                PostQuitMessage(int(message.wParam));
                break;
            }
            if (!window || !IsDialogMessageW(window, &message))
            {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }
    }

    RECT Track() const
    {
        RECT client{};
        GetClientRect(window, &client);
        return {ui::Px(window, 28), ui::Px(window, ui::TitleHeight + 130),
                client.right - ui::Px(window, 28), ui::Px(window, ui::TitleHeight + 136)};
    }

    static void Text(HWND control, std::wstring &previous, std::wstring_view value)
    {
        if (previous == value) return;
        previous = value;
        if (control) SetWindowTextW(control, previous.c_str());
    }

    void UpdateAnimation()
    {
        const bool needed = window && indeterminate && !cancelled && animationsEnabled &&
                            IsWindowVisible(window) && !IsIconic(window);
        if (needed == animating) return;
        animating = needed;
        if (needed) SetTimer(window, AnimationTimer, 66, nullptr);
        else if (window) KillTimer(window, AnimationTimer);
    }

    void SetCancellable(bool value)
    {
        if (cancellable == value) return;
        cancellable = value;
        const BOOL enabled = value && !cancelled;
        if (cancel) EnableWindow(cancel, enabled);
        if (chrome.close) EnableWindow(chrome.close, enabled);
    }

    void RequestCancel()
    {
        // The transaction only observes cancellation in its download loop.
        // Verification/package checking must finish through the existing owner.
        if (!cancellable || cancelled) return;
        cancelled = true;
        SetCancellable(false);
        Text(status, statusText, Pick(L"Cancelling…", L"正在取消…", L"キャンセル中…",
                                     L"취소 중…", L"正在取消…"));
        Text(amount, amountText, L"");
        Text(percentage, percentText, L"");
        UpdateAnimation();
        const RECT track = Track();
        InvalidateRect(window, &track, FALSE);
    }

    void Layout(bool dpiChanged = false)
    {
        if (!title) return;
        if (dpiChanged || !font)
        {
            if (font) DeleteObject(font);
            if (titleFont) DeleteObject(titleFont);
            font = ui::Font(window, 15);
            titleFont = ui::Font(window, 24, FW_SEMIBOLD);
            for (HWND control : {status, amount, percentage, cancel}) ui::StyleControl(control, font);
            ui::StyleControl(title, titleFont);
        }
        ui::Layout(window, chrome);
        RECT client{};
        GetClientRect(window, &client);
        const int margin = ui::Px(window, 28);
        const int width = std::max(1L, client.right - 2 * margin);
        auto position = [&](HWND control, int x, int y, int w, int h) {
            SetWindowPos(control, nullptr, x, ui::Px(window, y), w, ui::Px(window, h),
                         SWP_NOZORDER | SWP_NOACTIVATE);
        };
        position(title, margin, ui::TitleHeight + 20, width, 38);
        position(status, margin, ui::TitleHeight + 66, width, 26);
        position(amount, margin, ui::TitleHeight + 100, width - ui::Px(window, 90), 24);
        position(percentage, client.right - margin - ui::Px(window, 80), ui::TitleHeight + 100,
                 ui::Px(window, 80), 24);
        SetWindowPos(cancel, nullptr, client.right - margin - ui::Px(window, 132),
                     client.bottom - ui::Px(window, 60), ui::Px(window, 132), ui::Px(window, 36),
                     SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(window, nullptr, FALSE);
    }

    void Paint(HDC dc)
    {
        RECT client{};
        GetClientRect(window, &client);
        FillRect(dc, &client, ui::Brush());
        ui::Paint(window, dc, chrome);
        const RECT track = Track();
        ui::Fill(dc, track, ui::Raised);
        RECT fill = track;
        if (indeterminate && !cancelled)
        {
            const int width = track.right - track.left;
            const int segment = std::max(1, width / 4);
            const int offset = animating ? int((GetTickCount64() / 8) % (width + segment)) - segment : width / 3;
            fill.left = track.left + std::max(0, offset);
            fill.right = track.left + std::min(width, offset + segment);
        }
        else fill.right = fill.left + LONG(double(fill.right - fill.left) * fraction);
        if (fill.right > fill.left) ui::Fill(dc, fill, cancelled ? ui::Muted : ui::Accent);
    }

    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
    {
        auto self = reinterpret_cast<Impl *>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE)
        {
            self = static_cast<Impl *>(reinterpret_cast<CREATESTRUCTW *>(lparam)->lpCreateParams);
            self->window = window;
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (!self) return DefWindowProcW(window, message, wparam, lparam);
        LRESULT result{};
        if (ui::HandleMessage(window, message, wparam, lparam, self->chrome, result, 440, 280)) return result;
        switch (message)
        {
        case WM_COMMAND:
            if (LOWORD(wparam) == IDCANCEL) { self->RequestCancel(); return 0; }
            break;
        case WM_CLOSE:
            self->RequestCancel();
            return 0;
        case WM_DRAWITEM:
            ui::DrawButton(*reinterpret_cast<DRAWITEMSTRUCT *>(lparam));
            return TRUE;
        case WM_CTLCOLORSTATIC:
        {
            const HDC dc = reinterpret_cast<HDC>(wparam);
            HBRUSH brush = ui::ColorControl(dc);
            if (reinterpret_cast<HWND>(lparam) == self->amount) SetTextColor(dc, ui::Muted);
            return reinterpret_cast<LRESULT>(brush);
        }
        case WM_SIZE:
            self->Layout();
            self->UpdateAnimation();
            return 0;
        case WM_DPICHANGED:
        {
            const RECT &suggested = *reinterpret_cast<RECT *>(lparam);
            SetWindowPos(window, nullptr, suggested.left, suggested.top, suggested.right - suggested.left,
                         suggested.bottom - suggested.top, SWP_NOZORDER | SWP_NOACTIVATE);
            self->Layout(true);
            return 0;
        }
        case WM_SHOWWINDOW:
            if (!wparam && self->animating)
            {
                KillTimer(window, AnimationTimer);
                self->animating = false;
            }
            break;
        case WM_TIMER:
            if (wparam == AnimationTimer)
            {
                self->UpdateAnimation();
                if (self->animating)
                {
                    const RECT track = self->Track();
                    InvalidateRect(window, &track, FALSE);
                }
                return 0;
            }
            break;
        case WM_ERASEBKGND:
            return TRUE;
        case WM_PAINT:
        {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(window, &paint);
            self->Paint(dc);
            EndPaint(window, &paint);
            return 0;
        }
        case WM_PRINTCLIENT:
            self->Paint(reinterpret_cast<HDC>(wparam));
            return 0;
        case WM_DESTROY:
            KillTimer(window, AnimationTimer);
            self->animating = false;
            return 0;
        }
        return DefWindowProcW(window, message, wparam, lparam);
    }

    void Create()
    {
        static const wchar_t className[] = L"LostOdysseyUpdateProgress";
        WNDCLASSEXW type{};
        type.cbSize = sizeof(type);
        type.hIcon = LoadIconW(GetModuleHandleW(nullptr), L"IDI_LOST_ODYSSEY_RECOMP");
        type.lpfnWndProc = WindowProc;
        type.hInstance = GetModuleHandleW(nullptr);
        type.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        type.hbrBackground = ui::Brush();
        type.lpszClassName = className;
        if (!RegisterClassExW(&type) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;
        BOOL animate = TRUE;
        SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animate, 0);
        animationsEnabled = animate != FALSE;
        window = CreateWindowExW(WS_EX_APPWINDOW, className, L"Lost Odyssey — Update",
                                 WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                 CW_USEDEFAULT, CW_USEDEFAULT, 560, 310, nullptr, nullptr,
                                 GetModuleHandleW(nullptr), this);
        if (!window) return;
        ui::Create(window, chrome);
        auto control = [&](const wchar_t *kind, const wchar_t *text, DWORD style, int id) {
            return CreateWindowExW(0, kind, text, WS_CHILD | WS_VISIBLE | style,
                                   0, 0, 0, 0, window, reinterpret_cast<HMENU>(intptr_t(id)),
                                   GetModuleHandleW(nullptr), nullptr);
        };
        title = control(L"STATIC", Pick(L"Update", L"更新", L"アップデート", L"업데이트", L"更新"),
                        SS_LEFT | SS_NOPREFIX, 101);
        status = control(L"STATIC", L"", SS_LEFT | SS_NOPREFIX | SS_ENDELLIPSIS, 102);
        amount = control(L"STATIC", L"", SS_LEFT | SS_NOPREFIX, 103);
        percentage = control(L"STATIC", L"", SS_RIGHT | SS_NOPREFIX, 104);
        cancel = control(L"BUTTON", Pick(L"Cancel", L"取消", L"キャンセル", L"취소", L"取消"),
                         WS_TABSTOP | BS_OWNERDRAW, IDCANCEL);
        Text(status, statusText, Pick(L"Preparing…", L"準備中…", L"準備中…", L"준비 중…", L"准备中…"));
        SetWindowPos(window, nullptr, 0, 0, ui::Px(window, 560), ui::Px(window, 310),
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        Layout(true);
        ShowWindow(window, SW_SHOWNORMAL);
        UpdateAnimation();
        UpdateWindow(window);
        Pump();
    }
};

ProgressWindow::ProgressWindow(uint32_t language) : impl_(std::make_unique<Impl>())
{
    impl_->language = language;
    impl_->Create();
}

ProgressWindow::~ProgressWindow()
{
    if (impl_->window) DestroyWindow(impl_->window);
    ui::Destroy(impl_->chrome);
    if (impl_->font) DeleteObject(impl_->font);
    if (impl_->titleFont) DeleteObject(impl_->titleFont);
}

void ProgressWindow::SetProgress(uint64_t completed, uint64_t total, std::wstring_view detail)
{
    if (!impl_->cancelled)
    {
        const RECT track = impl_->Track();
        const int width = track.right - track.left;
        const double fraction = total ? std::clamp(double(completed) / double(total), 0.0, 1.0) : 0.0;
        const bool changed = impl_->indeterminate != (total == 0) ||
                             int(impl_->fraction * width) != int(fraction * width);
        impl_->fraction = fraction;
        impl_->indeterminate = total == 0;
        Impl::Text(impl_->status, impl_->statusText, detail);
        wchar_t percent[16]{};
        if (total) std::swprintf(percent, std::size(percent), L"%.0f%%", std::floor(fraction * 100));
        Impl::Text(impl_->percentage, impl_->percentText, percent);
        impl_->UpdateAnimation();
        if (changed && impl_->window) InvalidateRect(impl_->window, &track, FALSE);
    }
    impl_->Pump();
}

void ProgressWindow::SetDownloadProgress(uint64_t completed, uint64_t total)
{
    if (!impl_->cancelled)
    {
        wchar_t amount[96]{};
        if (total)
            std::swprintf(amount, std::size(amount), L"%.1f / %.1f MiB", double(completed) / (1024.0 * 1024.0),
                          double(total) / (1024.0 * 1024.0));
        else std::swprintf(amount, std::size(amount), L"%.1f MiB", double(completed) / (1024.0 * 1024.0));
        Impl::Text(impl_->amount, impl_->amountText, amount);
        impl_->SetCancellable(true);
    }
    SetProgress(completed, total, impl_->Pick(L"Downloading…", L"下載中…", L"ダウンロード中…",
                                            L"다운로드 중…", L"下载中…"));
}

void ProgressWindow::SetPhase(ProgressPhase phase)
{
    if (phase == ProgressPhase::Ready)
    {
        impl_->SetCancellable(false);
        Impl::Text(impl_->amount, impl_->amountText, L"");
        SetProgress(1, 1, impl_->Pick(L"Ready to restart", L"可以重新啟動", L"再起動の準備完了",
                                     L"다시 시작할 준비 완료", L"可以重启"));
    }
    else if (phase == ProgressPhase::Verifying)
        SetPhase(impl_->Pick(L"Verifying…", L"驗證中…", L"検証中…", L"확인 중…", L"验证中…"));
    else
        SetPhase(impl_->Pick(L"Checking package…", L"檢查套件中…", L"パッケージ確認中…", L"패키지 확인 중…", L"检查程序包…"));
}

void ProgressWindow::SetPhase(std::wstring_view detail)
{
    impl_->SetCancellable(false);
    Impl::Text(impl_->amount, impl_->amountText, L"");
    SetProgress(0, 0, detail);
}

bool ProgressWindow::Cancelled()
{
    impl_->Pump();
    return impl_->cancelled;
}
} // namespace updater
#else
namespace updater
{
struct ProgressWindow::Impl {};
ProgressWindow::ProgressWindow(uint32_t) : impl_(std::make_unique<Impl>()) {}
ProgressWindow::~ProgressWindow() = default;
void ProgressWindow::SetProgress(uint64_t, uint64_t, std::wstring_view) {}
void ProgressWindow::SetDownloadProgress(uint64_t, uint64_t) {}
void ProgressWindow::SetPhase(ProgressPhase) {}
void ProgressWindow::SetPhase(std::wstring_view) {}
bool ProgressWindow::Cancelled() { return false; }
}
#endif
