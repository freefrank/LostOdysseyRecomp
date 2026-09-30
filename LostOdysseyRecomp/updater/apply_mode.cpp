#include "apply_mode.h"

#ifdef _WIN32
#include "apply_arguments.h"
#include "settings/restart.h"
#include "updater/update.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <system_error>
#include <string>
#include <string_view>

namespace updater
{
namespace
{
int Fail(const std::wstring &message)
{
    wchar_t silent[2]{};
    if (!GetEnvironmentVariableW(L"LO_UPDATER_SILENT", silent, DWORD(std::size(silent))))
        MessageBoxExW(nullptr, message.c_str(), L"Lost Odyssey update", MB_OK | MB_ICONERROR,
                      MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
    return 1;
}

bool SamePath(const std::filesystem::path &left, const std::filesystem::path &right)
{
    std::error_code error;
    if (std::filesystem::equivalent(left, right, error)) return true;
    const auto a = std::filesystem::absolute(left, error).lexically_normal();
    if (error) return false;
    const auto b = std::filesystem::absolute(right, error).lexically_normal();
    return !error && CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_EQUAL;
}

bool IsWithin(const std::filesystem::path &child, const std::filesystem::path &parent)
{
    const auto normalizedChild = child.lexically_normal();
    const auto normalizedParent = parent.lexically_normal();
    auto childIt = normalizedChild.begin();
    auto parentIt = normalizedParent.begin();
    for (; parentIt != normalizedParent.end(); ++parentIt, ++childIt)
    {
        if (childIt == normalizedChild.end() ||
            CompareStringOrdinal(childIt->c_str(), -1, parentIt->c_str(), -1, TRUE) != CSTR_EQUAL)
            return false;
    }
    return true;
}

bool ParseApplyArguments(std::filesystem::path &planPath, DWORD &parentId, std::wstring &readyEvent,
                         bool &applyRequested)
{
    int count = 0;
    auto arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    bool malformed = arguments == nullptr;
    for (int i = 1; arguments && i < count; ++i)
    {
        const std::wstring_view argument(arguments[i]);
        if (IsApplyPlanArgument(argument) && planPath.empty())
        {
            applyRequested = true;
            if (i + 1 < count) planPath = arguments[++i];
            else malformed = true;
        }
        else if (argument == L"--wait-process" && i + 1 < count && !parentId)
        {
            wchar_t *end = nullptr;
            const auto parsed = std::wcstoul(arguments[++i], &end, 10);
            if (!parsed || !end || *end) malformed = true;
            else parentId = DWORD(parsed);
        }
        else if (argument == L"--restart-ready" && i + 1 < count && readyEvent.empty())
        {
            readyEvent = arguments[++i];
        }
        else
        {
            malformed = true;
        }
    }
    if (arguments) LocalFree(arguments);
    return !malformed;
}

bool Launch(const ApplyPlan &plan, std::string &error)
{
    std::wstring command = settings::restart::QuoteArgument(plan.executable.wstring());
    for (const auto &argument : plan.launchArguments)
        command += L" " + settings::restart::QuoteArgument(argument);
    command.push_back(L'\0');
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(plan.executable.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        nullptr, &startup, &process))
    {
        const auto win32Error = GetLastError();
        error = "could not launch updated game: Win32 error " + std::to_string(win32Error);
        return false;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}

int Apply(const std::filesystem::path &planPath, DWORD parentId, const std::wstring &readyEvent)
{
    std::string error;
    auto plan = ReadApplyPlan(planPath, error);
    const auto self = CurrentExecutablePath();
    if (!plan || !ValidateApplyPlan(*plan, error) || self.empty() || SamePath(self, plan->executable) ||
        !IsWithin(planPath, plan->installRoot / ".update") ||
        !SamePath(planPath.parent_path(), plan->stageRoot.parent_path()))
    {
        if (error.empty()) error = "apply plan does not describe a staged operation";
        return Fail(L"The staged update is invalid. The existing installation was not changed.\n\n" +
                    std::filesystem::path(error).wstring());
    }

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

    if (!ApplyWithRollback(*plan, {}, error))
    {
        std::ofstream result(plan->installRoot / ".update" / "last-result.txt", std::ios::trunc);
        result << "failed=" << error << "\n";
        return Fail(L"The update could not be installed. Previous files were restored where possible.\n\n" +
                    std::filesystem::path(error).wstring());
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
    wchar_t module[32768]{};
    if (GetModuleFileNameW(nullptr, module, DWORD(std::size(module))))
        MoveFileExW(module, nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);

    wchar_t silent[2]{};
    const bool ask = !plan->launchAfterApply &&
                     !GetEnvironmentVariableW(L"LO_UPDATER_SILENT", silent, DWORD(std::size(silent)));
    if (plan->launchAfterApply || (ask && MessageBoxExW(nullptr, L"The update is complete. Open Lost Odyssey now?",
                             L"Lost Odyssey Updater", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2,
                              MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US)) == IDYES))
    {
        if (!Launch(*plan, error))
            return Fail(L"The update was installed, but the game could not be opened.\n\n" +
                        std::filesystem::path(error).wstring());
    }
    return 0;
}
} // namespace

std::optional<int> TryRunApplyMode()
{
    std::filesystem::path planPath;
    DWORD parentId = 0;
    std::wstring readyEvent;
    bool applyRequested = false;
    const bool valid = ParseApplyArguments(planPath, parentId, readyEvent, applyRequested);
    if (!applyRequested) return std::nullopt;
    if (!valid || planPath.empty() || !parentId || readyEvent.empty())
        return Fail(L"The update helper received an invalid startup request. The existing installation was not changed.");
    std::error_code error;
    planPath = std::filesystem::absolute(planPath, error).lexically_normal();
    if (error) return Fail(L"The update helper received an invalid startup request. The existing installation was not changed.");
    return Apply(planPath, parentId, readyEvent);
}
} // namespace updater
#endif

#ifndef _WIN32
#include "update.h"
#include "../os/user_paths.h"

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>

namespace updater
{
std::optional<int> TryRunApplyMode()
{
    std::ifstream commandLine("/proc/self/cmdline", std::ios::binary);
    std::vector<std::string> arguments;
    std::string argument;
    while (std::getline(commandLine, argument, '\0')) arguments.push_back(argument);
    auto findArgument = [&](std::string_view name) -> std::optional<std::string> {
        for (size_t index = 1; index + 1 < arguments.size(); ++index)
            if (arguments[index] == name) return arguments[index + 1];
        return std::nullopt;
    };
    const auto planArgument = findArgument("--apply-plan");
    if (!planArgument)
    {
        // Only the successfully restarted image may discard the rollback copy.
        // This also handles updates applied by versions predating cleanup.
        const char *current = std::getenv("APPIMAGE");
        if (current && *current)
        {
            const auto currentPath = std::filesystem::path(current).lexically_normal();
            if (currentPath.is_absolute())
            {
                std::error_code cleanupError;
                const auto previous = std::filesystem::path(currentPath.string() + ".previous");
                if (std::filesystem::is_regular_file(std::filesystem::symlink_status(previous, cleanupError)))
                    std::filesystem::remove(previous, cleanupError);
                if (cleanupError == std::errc::no_such_file_or_directory) cleanupError.clear();
                if (cleanupError)
                    std::cerr << "Lost Odyssey update: could not remove previous AppImage: "
                              << cleanupError.message() << "\n";
            }
        }
        return std::nullopt;
    }

    std::optional<std::filesystem::path> failureOperationRoot;
    auto failWithReason = [&](std::string_view reason) -> int {
        if (failureOperationRoot && std::filesystem::is_directory(*failureOperationRoot))
        {
            std::ofstream result(*failureOperationRoot / "last-result.txt", std::ios::trunc);
            if (result.is_open())
            {
                result << "failed=" << reason << "\n";
            }
        }
        std::cerr << "Lost Odyssey update: " << reason << "\n";
        return 1;
    };

    const auto waitArgument = findArgument("--wait-process");
    if (!waitArgument) return failWithReason("missing --wait-process argument; existing installation not changed");
    char *endPtr = nullptr;
    errno = 0;
    const long parsedParent = std::strtol(waitArgument->c_str(), &endPtr, 10);
    if (errno != 0 || endPtr == waitArgument->c_str() || *endPtr != '\0' || parsedParent <= 0 ||
        parsedParent > static_cast<long>(std::numeric_limits<pid_t>::max()))
    {
        return failWithReason("invalid --wait-process PID; existing installation not changed");
    }
    const auto parent = static_cast<pid_t>(parsedParent);
    while (parent > 1)
    {
        if (kill(parent, 0) != 0)
        {
            if (errno == ESRCH)
            {
                break;
            }
            return failWithReason("cannot check parent process status; existing installation not changed");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::string error;
    const auto plan = ReadApplyPlan(*planArgument, error);
    if (!plan)
    {
        return failWithReason("cannot read apply plan; existing installation not changed");
    }
    const auto operationRoot = plan->stageRoot.parent_path();
    std::error_code operationError;
    const auto updateRoot = os::user_paths::StateDir() / ".update";
    if (operationRoot.filename().string().rfind("operation-", 0) != 0 ||
        !std::filesystem::equivalent(operationRoot.parent_path(), updateRoot, operationError) ||
        operationError || plan->stageRoot.filename() != "stage" ||
        std::filesystem::absolute(*planArgument).lexically_normal() !=
            (operationRoot / "apply-plan.json").lexically_normal())
    {
        return failWithReason("apply plan is outside the update state directory; existing installation not changed");
    }
    if (std::filesystem::is_directory(operationRoot))
    {
        failureOperationRoot = operationRoot;
    }

    const char *appImage = std::getenv("APPIMAGE");
    if (!appImage || *appImage == '\0' || plan->files.size() != 1)
    {
        return failWithReason("invalid AppImage environment or payload file count; existing installation not changed");
    }

    std::error_code envPathError;
    std::error_code planPathError;
    const auto appImagePath = std::filesystem::absolute(appImage, envPathError).lexically_normal();
    const auto planExecPath = std::filesystem::absolute(plan->executable, planPathError).lexically_normal();
    if (envPathError || planPathError || appImagePath != planExecPath)
    {
        return failWithReason("AppImage path does not match plan executable; existing installation not changed");
    }

    std::string pathError;
    if (!IsSafePayloadPath(plan->files.front().path, pathError))
        return failWithReason("invalid staged AppImage path; existing installation not changed");
    const auto staged = plan->stageRoot / plan->files.front().path;
    std::error_code stagedError;
    if (!std::filesystem::is_regular_file(staged, stagedError) || stagedError)
    {
        return failWithReason("staged AppImage is missing; existing installation not changed");
    }

    // Copy onto the target filesystem, then swap with a same-directory backup
    // so EXDEV cannot replace the only copy.
    const auto incoming = appImagePath.parent_path() / (appImagePath.filename().string() + ".new");
    const auto previous = appImagePath.parent_path() / (appImagePath.filename().string() + ".previous");
    std::error_code filesystemError;
    std::filesystem::remove(incoming, filesystemError);
    std::filesystem::copy_file(staged, incoming, std::filesystem::copy_options::overwrite_existing, filesystemError);
    if (filesystemError)
    {
        std::filesystem::remove(incoming, filesystemError);
        return failWithReason("failed to copy staged AppImage onto the target filesystem; existing installation not changed");
    }
    if (chmod(incoming.c_str(), 0755) != 0)
    {
        std::filesystem::remove(incoming, filesystemError);
        return failWithReason("failed to set executable permissions on staged AppImage; existing installation not changed");
    }

    std::error_code existsError;
    const bool hadExisting = std::filesystem::exists(appImagePath, existsError);
    if (hadExisting)
    {
        std::filesystem::rename(appImagePath, previous, filesystemError);
        if (filesystemError)
        {
            std::filesystem::remove(incoming, filesystemError);
            return failWithReason("failed to preserve existing AppImage; existing installation not changed");
        }
    }

    std::filesystem::rename(incoming, appImagePath, filesystemError);
    if (filesystemError)
    {
        std::filesystem::remove(incoming, filesystemError);
        if (hadExisting)
        {
            std::error_code restoreError;
            std::filesystem::rename(previous, appImagePath, restoreError);
            if (restoreError)
                return failWithReason("failed to replace AppImage and failed to restore the previous installation");
            return failWithReason("failed to replace AppImage; existing installation restored");
        }
        return failWithReason("failed to replace AppImage; existing installation not changed");
    }

    std::vector<char *> execArguments;
    const auto executable = appImagePath.string();
    execArguments.push_back(const_cast<char *>(executable.c_str()));
    std::vector<std::string> launchUtf8;
    launchUtf8.reserve(plan->launchArguments.size());
    for (const auto &launchArgument : plan->launchArguments)
    {
        const auto utf8 = std::filesystem::path(launchArgument).u8string();
        launchUtf8.emplace_back(reinterpret_cast<const char *>(utf8.data()), utf8.size());
    }
    for (auto &launchArgument : launchUtf8)
        execArguments.push_back(launchArgument.data());
    execArguments.push_back(nullptr);

    // The new image is already in place. Neither the downloaded archive nor
    // the staged copy is needed by the process we are about to execute.
    std::error_code cleanupError;
    std::filesystem::remove_all(operationRoot, cleanupError);
    if (cleanupError)
        std::cerr << "Lost Odyssey update: could not remove temporary update files: "
                  << cleanupError.message() << "\n";

    execv(executable.c_str(), execArguments.data());
    const auto launchError = errno;
    std::error_code restoreError;
    if (hadExisting)
    {
        std::filesystem::rename(previous, appImagePath, restoreError);
        if (!restoreError)
            return failWithReason("failed to execute updated AppImage (errno " +
                                  std::to_string(launchError) + "); previous installation restored");
    }
    if (!hadExisting)
        return failWithReason("failed to execute updated AppImage (errno " +
                              std::to_string(launchError) + "); no previous installation is available");
    return failWithReason("failed to execute updated AppImage (errno " +
                          std::to_string(launchError) + "); could not restore previous installation");
}
} // namespace updater
#endif
