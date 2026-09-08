#include "updater/progress.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <iostream>

int main()
{
    updater::ProgressWindow progress(4);
    progress.SetProgress(4 * 1024 * 1024, 16 * 1024 * 1024, L"正在下载更新… 4.0 / 16.0 MiB");
    HWND window = FindWindowW(L"LostOdysseyUpdateProgress", nullptr);
    if (!window)
    {
        std::cerr << "FAIL: native update progress window was not created\n";
        return 1;
    }
    HWND cancel = GetDlgItem(window, IDCANCEL);
    if (!cancel || !IsWindowEnabled(cancel))
    {
        std::cerr << "FAIL: cancel-update control is not available\n";
        return 1;
    }
    SendMessageW(window, WM_COMMAND, IDCANCEL, 0);
    if (!progress.Cancelled() || IsWindowEnabled(cancel))
    {
        std::cerr << "FAIL: cancel did not stop the update UI boundary\n";
        return 1;
    }
    std::cout << "PASS: native updater progress and cancel boundary\n";
    return 0;
}
