#pragma once

#ifdef _WIN32
#include <windows.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <wrl/client.h>
#include <filesystem>
#include <memory>

namespace settings::folder_picker
{
struct Result
{
    HRESULT status = E_FAIL;
    std::filesystem::path path;
};

// The caller owns the modal window thread. Balance S_OK and S_FALSE, and report
// an incompatible pre-existing apartment without uninitializing someone else's COM.
inline Result Choose(HWND owner, const wchar_t *title, const std::filesystem::path &initial)
{
    struct Apartment
    {
        HRESULT status = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        ~Apartment() { if (SUCCEEDED(status)) CoUninitialize(); }
    } apartment;
    if (FAILED(apartment.status)) return {apartment.status, {}};

    Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
    HRESULT status = CoCreateInstance(__uuidof(FileOpenDialog), nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&dialog));
    if (FAILED(status)) return {status, {}};
    FILEOPENDIALOGOPTIONS options{};
    if (FAILED(status = dialog->GetOptions(&options)) ||
        FAILED(status = dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM |
                                           FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR | FOS_DONTADDTORECENT)) ||
        FAILED(status = dialog->SetTitle(title)))
        return {status, {}};

    // A stale configured path must not prevent opening a usable picker.
    Microsoft::WRL::ComPtr<IShellItem> initialFolder;
    if (!initial.empty() && SUCCEEDED(SHCreateItemFromParsingName(initial.c_str(), nullptr,
                                                                 IID_PPV_ARGS(&initialFolder))))
        dialog->SetFolder(initialFolder.Get());

    if (FAILED(status = dialog->Show(owner))) return {status, {}};
    Microsoft::WRL::ComPtr<IShellItem> selected;
    if (FAILED(status = dialog->GetResult(&selected))) return {status, {}};
    wchar_t *path = nullptr;
    status = selected->GetDisplayName(SIGDN_FILESYSPATH, &path);
    const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> ownedPath(path, CoTaskMemFree);
    if (FAILED(status)) return {status, {}};
    if (!path || !*path) return {E_UNEXPECTED, {}};
    return {S_OK, std::filesystem::path(path)};
}
} // namespace settings::folder_picker
#endif
