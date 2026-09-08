#include "progress.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "settings/desktop_ui.h"

#include <algorithm>
#include <array>
#include <cwchar>
#include <string>

namespace updater
{
struct ProgressWindow::Impl
{
    HWND window{}, title{}, status{}, cancel{};
    HFONT font{}, titleFont{};
    uint32_t language = 0;
    double fraction = 0.0;
    bool cancelled = false;

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
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
    {
        auto self = reinterpret_cast<Impl *>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE)
        {
            self = static_cast<Impl *>(reinterpret_cast<CREATESTRUCTW *>(lparam)->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (!self) return DefWindowProcW(window, message, wparam, lparam);
        if (message == WM_COMMAND && LOWORD(wparam) == IDCANCEL)
        {
            self->cancelled = true;
            EnableWindow(self->cancel, FALSE);
            SetWindowTextW(self->status, self->Pick(L"Cancelling update…", L"正在取消更新…", L"更新をキャンセルしています…",
                                                   L"업데이트 취소 중…", L"正在取消更新…"));
            return 0;
        }
        if (message == WM_CLOSE)
        {
            SendMessageW(window, WM_COMMAND, IDCANCEL, 0);
            return 0;
        }
        if (message == WM_DRAWITEM)
        {
            settings::desktop_ui::DrawButton(*reinterpret_cast<DRAWITEMSTRUCT *>(lparam));
            return TRUE;
        }
        if (message == WM_CTLCOLORSTATIC)
            return reinterpret_cast<LRESULT>(settings::desktop_ui::ColorControl(reinterpret_cast<HDC>(wparam)));
        if (message == WM_ERASEBKGND)
        {
            RECT rect{};
            GetClientRect(window, &rect);
            FillRect(reinterpret_cast<HDC>(wparam), &rect, settings::desktop_ui::SurfaceBrush());
            return TRUE;
        }
        if (message == WM_PAINT)
        {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(window, &paint);
            RECT track{settings::desktop_ui::Px(window, 28), settings::desktop_ui::Px(window, 112),
                       settings::desktop_ui::Px(window, 492), settings::desktop_ui::Px(window, 124)};
            settings::desktop_ui::Fill(dc, track, settings::desktop_ui::Raised);
            RECT fill = track;
            fill.right = fill.left + LONG(double(fill.right - fill.left) * std::clamp(self->fraction, 0.0, 1.0));
            settings::desktop_ui::Fill(dc, fill, settings::desktop_ui::Accent);
            EndPaint(window, &paint);
            return 0;
        }
        return DefWindowProcW(window, message, wparam, lparam);
    }

    void Create()
    {
        static const wchar_t className[] = L"LostOdysseyUpdateProgress";
        static bool registered = false;
        if (!registered)
        {
            WNDCLASSEXW type{};
            type.hIcon = LoadIconW(GetModuleHandleW(nullptr), L"IDI_LOST_ODYSSEY_RECOMP");
            type.cbSize = sizeof(type);
            type.lpfnWndProc = WindowProc;
            type.hInstance = GetModuleHandleW(nullptr);
            type.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
            type.hbrBackground = settings::desktop_ui::SurfaceBrush();
            type.lpszClassName = className;
            registered = RegisterClassExW(&type) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
        }
        if (!registered) return;
        window = CreateWindowExW(WS_EX_APPWINDOW, className, L"Lost Odyssey — Update",
                                 WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                                 CW_USEDEFAULT, CW_USEDEFAULT, 540, 220, nullptr, nullptr,
                                 GetModuleHandleW(nullptr), this);
        if (!window) return;
        settings::desktop_ui::EnableDarkFrame(window);
        font = settings::desktop_ui::Font(window, 15);
        titleFont = settings::desktop_ui::Font(window, 22, FW_SEMIBOLD);
        auto control = [&](const wchar_t *kind, const wchar_t *text, DWORD style, int x, int y, int width, int height, int id = 0) {
            HWND child = CreateWindowExW(0, kind, text, WS_CHILD | WS_VISIBLE | style,
                settings::desktop_ui::Px(window, x), settings::desktop_ui::Px(window, y),
                settings::desktop_ui::Px(window, width), settings::desktop_ui::Px(window, height), window,
                reinterpret_cast<HMENU>(static_cast<intptr_t>(id)), GetModuleHandleW(nullptr), nullptr);
            settings::desktop_ui::StyleControl(child, font);
            return child;
        };
        title = control(L"STATIC", Pick(L"Updating Lost Odyssey", L"正在更新 Lost Odyssey", L"Lost Odyssey を更新中",
                                        L"Lost Odyssey 업데이트", L"正在更新 Lost Odyssey"),
                        0, 28, 22, 464, 34);
        SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(titleFont), TRUE);
        status = control(L"STATIC", Pick(L"Preparing download…", L"正在準備下載…", L"ダウンロードを準備中…",
                                         L"다운로드 준비 중…", L"正在准备下载…"),
                         SS_LEFT | SS_NOPREFIX, 28, 68, 464, 28);
        cancel = control(L"BUTTON", Pick(L"Cancel update", L"取消更新", L"更新をキャンセル", L"업데이트 취소", L"取消更新"),
                         WS_TABSTOP | BS_OWNERDRAW, 352, 140, 140, 36, IDCANCEL);
        ShowWindow(window, SW_SHOWNORMAL);
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
    if (impl_->font) DeleteObject(impl_->font);
    if (impl_->titleFont) DeleteObject(impl_->titleFont);
}

void ProgressWindow::SetProgress(uint64_t completed, uint64_t total, std::wstring_view detail)
{
    impl_->fraction = total ? double(completed) / double(total) : 0.0;
    if (impl_->status) SetWindowTextW(impl_->status, std::wstring(detail).c_str());
    if (impl_->window) InvalidateRect(impl_->window, nullptr, FALSE);
    impl_->Pump();
}

void ProgressWindow::SetDownloadProgress(uint64_t completed, uint64_t total)
{
    wchar_t amount[96]{};
    std::swprintf(amount, std::size(amount), L"%.1f / %.1f MiB", double(completed) / (1024.0 * 1024.0),
                  double(total) / (1024.0 * 1024.0));
    std::wstring detail = impl_->Pick(L"Downloading update… ", L"正在下載更新… ", L"更新をダウンロード中… ",
                                     L"업데이트 다운로드 중… ", L"正在下载更新… ");
    detail += amount;
    SetProgress(completed, total, detail);
}

void ProgressWindow::SetPhase(ProgressPhase phase)
{
    switch (phase)
    {
    case ProgressPhase::Verifying:
        SetPhase(impl_->Pick(L"Verifying downloaded update…", L"正在驗證下載的更新…", L"ダウンロードした更新を検証中…",
                             L"다운로드한 업데이트 확인 중…", L"正在验证下载的更新…"));
        break;
    case ProgressPhase::CheckingPackage:
        SetPhase(impl_->Pick(L"Checking package files…", L"正在檢查套件檔案…", L"パッケージファイルを確認中…",
                             L"패키지 파일 확인 중…", L"正在检查程序包文件…"));
        break;
    case ProgressPhase::Ready:
        SetProgress(1, 1, impl_->Pick(L"Update ready. Restarting…", L"更新已就緒，正在重新啟動…", L"更新の準備ができました。再起動中…",
                                      L"업데이트 준비 완료. 다시 시작하는 중…", L"更新已就绪，正在重新启动…"));
        break;
    }
}

void ProgressWindow::SetPhase(std::wstring_view detail)
{
    if (impl_->status) SetWindowTextW(impl_->status, std::wstring(detail).c_str());
    impl_->Pump();
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
