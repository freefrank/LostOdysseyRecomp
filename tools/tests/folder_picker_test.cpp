// Native shell-dialog fixture on a private, non-input desktop. It invokes the
// production chooser and the dialog's own accept/cancel commands, never SendInput.
#include "settings/folder_picker.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>
#include <thread>

namespace
{
bool cancelDialog = false;
bool activatedDialog = false;
unsigned dialogsShown = 0;
unsigned timeoutTicks = 0;
HHOOK hook = nullptr;
void CALLBACK DialogTimer(HWND dialog, UINT, UINT_PTR timer, DWORD)
{
    KillTimer(dialog, timer);
    if (++timeoutTicks > 8) { PostMessageW(dialog, WM_CLOSE, 0, 0); return; }
    const HWND button = GetDlgItem(dialog, cancelDialog ? IDCANCEL : IDOK);
    if (button && IsWindowEnabled(button))
    {
        wchar_t label[160]{};
        GetWindowTextW(button, label, int(std::size(label)));
        std::wprintf(L"dialog button id=%d label=%ls\n", cancelDialog ? IDCANCEL : IDOK, label);
        // Dispatch the real shell dialog command; GetResult remains authoritative.
        SendMessageW(button, BM_CLICK, 0, 0);
        if (IsWindow(dialog)) SetTimer(dialog, timer, 250, DialogTimer);
    }
    else SetTimer(dialog, timer, 250, DialogTimer);
}
LRESULT CALLBACK Activated(int code, WPARAM wparam, LPARAM lparam)
{
    if (code == HCBT_ACTIVATE)
    {
        HWND window = reinterpret_cast<HWND>(wparam);
        wchar_t type[64]{};
        GetClassNameW(window, type, int(std::size(type)));
        if (std::wstring(type) == L"#32770" && !activatedDialog)
        {
            activatedDialog = true;
            ++dialogsShown;
            timeoutTicks = 0;
            SetTimer(window, 88, 250, DialogTimer);
        }
    }
    return CallNextHookEx(hook, code, wparam, lparam);
}
}

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const auto initialDesktop = GetThreadDesktop(GetCurrentThreadId());
    HDESK desktop = CreateDesktopW(L"LO_FolderPicker_Fixture", nullptr, nullptr, 0, GENERIC_ALL, nullptr);
    assert(desktop && SetThreadDesktop(desktop));
    const auto work = std::filesystem::absolute(L"out/v0.5.0/startup/picker");
    const auto selected = work / L"選択_´_′_中文" / std::wstring(80, L'a') /
                          std::wstring(80, L'b') / std::wstring(80, L'c');
    std::filesystem::create_directories(L"\\\\?\\" + selected.wstring());
    assert(selected.wstring().size() > MAX_PATH);
    assert(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)));
    hook = SetWindowsHookExW(WH_CBT, Activated, nullptr, GetCurrentThreadId());
    assert(hook);
    auto result = settings::folder_picker::Choose(nullptr, L"Folder picker fixture — real selection", selected);
    std::wprintf(L"accept status=0x%08lx length=%zu shown=%u path=%ls\n", static_cast<unsigned long>(result.status),
                 result.path.wstring().size(), dialogsShown, result.path.c_str());
    // Shell can return a valid 8.3 alias for long paths in applications without
    // longPathAware manifests. Verify the selected directory, not its spelling.
    std::error_code equivalenceError;
    const bool equivalent = std::filesystem::equivalent(result.path, L"\\\\?\\" + selected.wstring(), equivalenceError);
    std::printf("physical selection equivalent=%d error=%d\n", equivalent, equivalenceError.value());
    assert(result.status == S_OK && equivalent && dialogsShown == 1);
    // The chooser must preserve the caller's apartment. Shell extensions may
    // retain their own apartment references, so zero global COM refs is no oracle.
    APTTYPE apartment; APTTYPEQUALIFIER qualifier;
    const HRESULT afterAccept = CoGetApartmentType(&apartment, &qualifier);
    std::printf("after accept apartment=0x%08lx\n", static_cast<unsigned long>(afterAccept));
    assert(SUCCEEDED(afterAccept) && (apartment == APTTYPE_STA || apartment == APTTYPE_MAINSTA));
    cancelDialog = true;
    activatedDialog = false;
    result = settings::folder_picker::Choose(nullptr, L"Folder picker fixture — real cancellation", selected);
    assert(result.status == HRESULT_FROM_WIN32(ERROR_CANCELLED) && result.path.empty() && dialogsShown == 2);
    assert(SUCCEEDED(CoGetApartmentType(&apartment, &qualifier)));
    cancelDialog = false;
    activatedDialog = false;
    const auto unicode = work / L"選択_´_′_中文";
    result = settings::folder_picker::Choose(nullptr, L"Folder picker fixture — Unicode selection", unicode);
    std::wprintf(L"Unicode accept status=0x%08lx shown=%u path=%ls expected=%ls\n",
                 static_cast<unsigned long>(result.status), dialogsShown, result.path.c_str(), unicode.c_str());
    assert(result.status == S_OK && std::filesystem::equivalent(result.path, unicode) && dialogsShown == 3);
    assert(SUCCEEDED(CoGetApartmentType(&apartment, &qualifier)));
    UnhookWindowsHookEx(hook);
    std::thread incompatible([&] {
        assert(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)));
        const auto unavailable = settings::folder_picker::Choose(nullptr,
            L"Must not create a dialog in an incompatible apartment", selected);
        assert(unavailable.status == RPC_E_CHANGED_MODE && unavailable.path.empty());
        APTTYPE workerApartment; APTTYPEQUALIFIER workerQualifier;
        assert(SUCCEEDED(CoGetApartmentType(&workerApartment, &workerQualifier)) && workerApartment == APTTYPE_MTA);
        CoUninitialize();
    });
    incompatible.join();
    assert(dialogsShown == 3);
    CoUninitialize();
    // The shell may retain helper windows until process shutdown; the test never
    // switches the interactive desktop and the OS closes this private one on exit.
    (void)initialDesktop;
    (void)desktop;
    std::printf("PASS actual shell accept (>MAX_PATH and Unicode directory identities), cancel, preserved caller COM and incompatible apartment error\n");
}
