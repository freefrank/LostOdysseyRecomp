#include "updater/update.h"
#include "updater/standalone.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include <charconv>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace
{
std::wstring Quote(std::wstring_view value)
{
    std::wstring result = L"\"";
    size_t slashes = 0;
    for (const auto character : value)
    {
        if (character == L'\\') { ++slashes; continue; }
        if (character == L'\"') result.append(slashes * 2 + 1, L'\\');
        else result.append(slashes, L'\\');
        slashes = 0;
        result.push_back(character);
    }
    result.append(slashes * 2, L'\\');
    result.push_back(L'\"');
    return result;
}

bool Launch(const updater::ApplyPlan &plan, std::string &error)
{
    std::wstring command = Quote(plan.executable.wstring());
    for (const auto &argument : plan.launchArguments) command += L" " + Quote(argument);
    command.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(plan.executable.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        nullptr, &startup, &process))
    {
        error = "could not launch updated game: Win32 error " + std::to_string(GetLastError());
        return false;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}

int Fail(const std::wstring &message)
{
    wchar_t silent[2]{};
    if (!GetEnvironmentVariableW(L"LO_UPDATER_SILENT", silent, DWORD(std::size(silent))))
        MessageBoxExW(nullptr, message.c_str(), L"Lost Odyssey update", MB_OK | MB_ICONERROR,
                      MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
    return 1;
}
} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    int count = 0;
    auto arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (arguments && count == 1)
    {
        LocalFree(arguments);
        return updater::RunStandalone(updater::CurrentExecutablePath());
    }
    std::filesystem::path planPath;
    DWORD parentId = 0;
    std::wstring readyEvent;
    bool malformed = false;
    for (int i = 1; arguments && i < count; ++i)
    {
        const std::wstring_view argument(arguments[i]);
        if (argument == L"--apply-plan" && i + 1 < count) planPath = arguments[++i];
        else if (argument == L"--wait-process" && i + 1 < count)
        {
            wchar_t *end = nullptr;
            const auto parsed = std::wcstoul(arguments[++i], &end, 10);
            if (!parsed || !end || *end) malformed = true;
            else parentId = DWORD(parsed);
        }
        else if (argument == L"--restart-ready" && i + 1 < count) readyEvent = arguments[++i];
        else malformed = true;
    }
    if (arguments) LocalFree(arguments);
    if (malformed || planPath.empty() || !parentId || readyEvent.empty())
        return Fail(L"The update helper received an invalid startup request. The existing installation was not changed.");
    std::string error;
    auto plan = updater::ReadApplyPlan(planPath, error);
    if (!plan || !updater::ValidateApplyPlan(*plan, error))
        return Fail(L"The staged update is invalid. The existing installation was not changed.\n\n" +
                    std::filesystem::path(error).wstring());
    HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, parentId);
    HANDLE ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, readyEvent.c_str());
    if (!parent || !ready)
    {
        if (ready) CloseHandle(ready);
        if (parent) CloseHandle(parent);
        return Fail(L"The update helper could not establish the restart handshake. The existing installation was not changed.");
    }
    if (!SetEvent(ready))
    {
        CloseHandle(ready);
        CloseHandle(parent);
        return Fail(L"The update helper could not confirm readiness. The existing installation was not changed.");
    }
    CloseHandle(ready);
    if (WaitForSingleObject(parent, INFINITE) != WAIT_OBJECT_0)
    {
        CloseHandle(parent);
        return Fail(L"The update helper could not wait for the game to exit. The existing installation was not changed.");
    }
    CloseHandle(parent);
    if (!updater::ApplyWithRollback(*plan, {}, error))
    {
        std::ofstream result(plan->installRoot / ".update" / "last-result.txt", std::ios::trunc);
        result << "failed=" << error << "\n";
        std::string launchError;
        Launch(*plan, launchError);
        return Fail(L"The update could not be installed. The previous files were restored and restarted.\n\n" +
                    std::filesystem::path(error).wstring());
    }
    if (!Launch(*plan, error))
    {
        const auto updateLaunchError = error;
        std::string rollbackError, oldLaunchError;
        const bool rolledBack = updater::RollbackInstalledFiles(*plan, rollbackError);
        const bool relaunched = rolledBack && Launch(*plan, oldLaunchError);
        std::string detail = updateLaunchError;
        if (!rolledBack) detail += "; rollback failed: " + rollbackError;
        else if (!relaunched) detail += "; previous version relaunch failed: " + oldLaunchError;
        std::ofstream result(plan->installRoot / ".update" / "last-result.txt", std::ios::trunc);
        result << "failed=" << detail << "\n";
        return Fail(L"The updated game could not be launched. The updater restored the previous version.\n\n" +
                    std::filesystem::path(detail).wstring());
    }
    std::ofstream result(plan->installRoot / ".update" / "last-result.txt", std::ios::trunc);
    result << "updated=" << plan->version << "\n";
    result.close();
    std::error_code filesystemError;
    const auto operationRoot = plan->stageRoot.parent_path();
    std::filesystem::remove(operationRoot / "download.zip", filesystemError);
    std::filesystem::remove(operationRoot / "apply-plan.json", filesystemError);
    std::filesystem::remove_all(operationRoot / "stage", filesystemError);
    std::filesystem::remove_all(operationRoot / "rollback", filesystemError);
    wchar_t module[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, module, DWORD(std::size(module))))
        MoveFileExW(module, nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
    return 0;
}
