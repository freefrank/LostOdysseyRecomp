#include "standalone.h"
#ifdef _WIN32
#include "settings/restart.h"

#include <tlhelp32.h>
#include <fstream>
#include <iterator>

namespace updater
{
namespace
{
bool Chinese()
{
    return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_CHINESE;
}

int Notify(const wchar_t *english, const wchar_t *chinese, bool failed, std::string_view detail = {})
{
    wchar_t silent[2]{};
    if (!GetEnvironmentVariableW(L"LO_UPDATER_SILENT", silent, DWORD(std::size(silent))))
    {
        std::wstring message = Chinese() ? chinese : english;
        if (!detail.empty())
        {
            const int size = MultiByteToWideChar(CP_UTF8, 0, detail.data(), int(detail.size()), nullptr, 0);
            std::wstring wide(size, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, detail.data(), int(detail.size()), wide.data(), size);
            message += L"\n\n" + wide;
        }
        MessageBoxW(nullptr, message.c_str(), L"Lost Odyssey Updater",
                    MB_OK | (failed ? MB_ICONERROR : MB_ICONINFORMATION));
    }
    return failed ? 1 : 0;
}

bool SamePath(const std::filesystem::path &a, const std::filesystem::path &b)
{
    std::error_code error;
    if (std::filesystem::equivalent(a, b, error)) return true;
    return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
}
} // namespace

bool ConfigureStandalone(const std::filesystem::path &helper, StartupOptions &options, std::string &error)
{
    options = {};
    std::error_code ec;
    options.installRoot = std::filesystem::canonical(helper, ec).parent_path();
    if (ec || options.installRoot.empty()) { error = "Cannot locate the updater installation folder."; return false; }
    options.executable = options.installRoot / "LostOdysseyRecomp.exe";
    if (!std::filesystem::is_regular_file(options.executable, ec))
    {
        error = "Place LostOdysseyUpdater.exe beside LostOdysseyRecomp.exe and manifest.json.";
        return false;
    }
    const auto manifestPath = options.installRoot / "manifest.json";
    const auto size = std::filesystem::file_size(manifestPath, ec);
    if (ec || size > 4 * 1024 * 1024) { error = "The installed release manifest is missing or too large."; return false; }
    std::ifstream input(manifestPath, std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    auto manifest = ParsePackageManifest(text, error);
    if (!manifest) return false;
    if (manifest->developmentBuild || !ParseVersion(manifest->version))
    {
        error = "Standalone updates require an installed release package, not a development build.";
        return false;
    }
    bool verified = false;
    for (const auto &file : manifest->files)
    {
        if (!SamePath(options.installRoot / file.path, options.executable)) continue;
        const auto digest = Sha256File(options.executable, error);
        if (digest.empty() || digest != file.sha256)
        {
            error = "The installed game executable does not match manifest.json.";
            return false;
        }
        verified = true;
        break;
    }
    if (!verified) { error = "The release manifest does not identify the game executable."; return false; }
    options.currentVersion = manifest->version;
    // A direct user request is independent of the saved automatic-check opt-out.
    // PrepareAtStartup still respects the explicit LO_NO_UPDATE environment switch.
    options.automaticUpdates = true;
    options.uiLanguage = Chinese() ? 4 : 0;
    return true;
}

bool StandaloneGameClosed(const std::filesystem::path &executable, std::string &error)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) { error = "Could not check running game processes."; return false; }
    PROCESSENTRY32W entry{sizeof(entry)};
    bool closed = true;
    if (!Process32FirstW(snapshot, &entry))
    {
        CloseHandle(snapshot);
        error = "Could not read running game processes.";
        return false;
    }
    do
    {
        if (CompareStringOrdinal(entry.szExeFile, -1, executable.filename().c_str(), -1, TRUE) != CSTR_EQUAL) continue;
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
        if (!process)
        {
            if (GetLastError() == ERROR_INVALID_PARAMETER) continue; // Process already exited.
            error = "Close the running game before updating (its location could not be inspected).";
            closed = false;
            break;
        }
        std::wstring path(32768, L'\0');
        DWORD length = DWORD(path.size());
        const bool found = QueryFullProcessImageNameW(process, 0, path.data(), &length) != 0;
        CloseHandle(process);
        path.resize(length);
        if (!found || SamePath(path, executable))
        {
            error = "Close Lost Odyssey before running the standalone updater.";
            closed = false;
            break;
        }
    } while (Process32NextW(snapshot, &entry));
    CloseHandle(snapshot);
    return closed;
}

int RunStandalone(const std::filesystem::path &helper, PrepareUpdate prepare)
{
    StartupOptions options;
    std::string error;
    if (!ConfigureStandalone(helper, options, error))
        return Notify(L"The installed release could not be identified.", L"无法识别已安装的发布版本。", true, error);
    if (!StandaloneGameClosed(options.executable, error))
        return Notify(L"Close the game and try again.", L"请先关闭游戏，再运行更新器。", true, error);
    const auto result = prepare(options);
    if (result.status == StartupStatus::Ready && result.update)
    {
        if (!StandaloneGameClosed(options.executable, error))
            return Notify(L"Close the game and try again.", L"请先关闭游戏，再运行更新器。", true, error);
        // The helper preserves its caller's cwd. Standalone launches must use the
        // installation directory even when Explorer/a shortcut supplied another cwd.
        std::error_code ec;
        std::filesystem::current_path(options.installRoot, ec);
        if (ec || !settings::restart::LaunchWaitingProcess(result.update->runnerPath.wstring(),
                                                          ApplyHelperArguments(result.update->planPath)))
            return Notify(L"Could not start the update installer.", L"无法启动更新安装程序。", true);
        return 0; // Release this EXE before the existing helper replaces it.
    }
    switch (result.status)
    {
    case StartupStatus::UpToDate:
        return Notify(L"This installation is already up to date.", L"当前安装已是最新版本。", false, result.detail);
    case StartupStatus::Cancelled:
        return 0;
    case StartupStatus::Disabled:
        return Notify(L"Update checks are disabled by LO_NO_UPDATE.", L"LO_NO_UPDATE 已禁用更新检查。", false);
    default:
        return Notify(L"The update check could not complete. Please try again later.",
                      L"更新检查未能完成，请稍后重试。", true, result.detail);
    }
}
} // namespace updater
#endif
