#pragma once
#include "config.h"
#include <atomic>
#if defined(__linux__) && !defined(_WIN32)
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
extern char **environ;
#endif
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <cwchar>
#include <string>
#include <string_view>
#endif

namespace settings::restart
{
enum class State : uint32_t
{
    Idle,
    Requested,
    InstallRequested,
    LaunchFailed
};

inline std::atomic<State> state{State::Idle};

inline bool Required(const Config &before, const Config &after)
{
    return before.gameLanguage != after.gameLanguage || before.graphicsBackend != after.graphicsBackend;
}

inline void Request() { state.store(State::Requested); }
inline void RequestInstall() { state.store(State::InstallRequested); }
inline bool Requested() { return state.load() == State::Requested || state.load() == State::InstallRequested; }
inline bool InstallRequested() { return state.load() == State::InstallRequested; }
inline void ReportLaunchFailure() { state.store(State::LaunchFailed); }
inline bool ConsumeLaunchFailure()
{
    State expected = State::LaunchFailed;
    return state.compare_exchange_strong(expected, State::Idle);
}
inline void Cancel() { state.store(State::Idle); }

#ifdef _WIN32
inline std::wstring QuoteArgument(std::wstring_view value)
{
    std::wstring result = L"\"";
    size_t slashes = 0;
    for (wchar_t character : value)
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

inline std::wstring CurrentArgumentsWithoutHandshake()
{
    int count = 0;
    auto arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    std::wstring command;
    for (int i = 1; arguments && i < count; ++i)
    {
        if ((std::wstring_view(arguments[i]) == L"--wait-process" ||
             std::wstring_view(arguments[i]) == L"--restart-ready") && i + 1 < count)
        {
            ++i;
            continue;
        }
        if (!command.empty()) command.push_back(L' ');
        command += QuoteArgument(arguments[i]);
    }
    if (arguments) LocalFree(arguments);
    return command;
}

inline std::wstring LaunchArguments(bool install)
{
    std::wstring arguments = CurrentArgumentsWithoutHandshake();
    if (install)
    {
        if (!arguments.empty()) arguments.push_back(L' ');
        arguments += L"--install";
    }
    return arguments;
}

// Creates a child which proves it holds a SYNCHRONIZE handle to this process,
// signals readiness, then waits for this process to exit. Return true only when
// the child is parked, so the caller can safely take the established shutdown
// path without overlapping guest save/profile/cache access.
inline bool LaunchWaitingProcess(const std::wstring &executable, const std::wstring &arguments,
                                 uint32_t readyTimeoutMs = 10000)
{
    const std::wstring eventName = L"Local\\LostOdysseyRestart-" + std::to_wstring(GetCurrentProcessId()) +
                                   L"-" + std::to_wstring(GetTickCount64());
    HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, eventName.c_str());
    if (!ready) { ReportLaunchFailure(); return false; }
    std::wstring command = QuoteArgument(executable);
    if (!arguments.empty()) command += L" " + arguments;
    command += L" --wait-process " + std::to_wstring(GetCurrentProcessId());
    command += L" --restart-ready " + QuoteArgument(eventName);
    command.push_back(L'\0');
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, 0,
                        nullptr, nullptr, &startup, &process))
    {
        CloseHandle(ready);
        ReportLaunchFailure();
        return false;
    }
    const DWORD wait = WaitForSingleObject(ready, readyTimeoutMs);
    CloseHandle(ready);
    if (wait != WAIT_OBJECT_0)
    {
        // This is our confirmed child and it has not completed the pre-init
        // handshake. Stop it so it cannot initialize later behind the user.
        TerminateProcess(process.hProcess, ERROR_TIMEOUT);
        WaitForSingleObject(process.hProcess, 2000);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        ReportLaunchFailure();
        return false;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}

inline bool LaunchWaitingChild(uint32_t readyTimeoutMs = 10000)
{
    std::wstring executable(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(), DWORD(executable.size()));
    if (!length || length >= executable.size())
    {
        ReportLaunchFailure();
        return false;
    }
    executable.resize(length);
    return LaunchWaitingProcess(executable, LaunchArguments(InstallRequested()), readyTimeoutMs);
}

// Call at the very beginning of main, before log/settings/profile/cache/guest
// initialization. A validated restart child signals the parent and remains
// parked until the old process is fully gone.
enum class ChildHandshake
{
    NotChild,
    Waited,
    Invalid
};

inline ChildHandshake WaitForParentIfRestartChild()
{
    int count = 0;
    auto arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    DWORD parentId = 0;
    std::wstring eventName;
    bool sawRestartFlag = false, sawParent = false, sawEvent = false, malformed = false;
    for (int i = 1; arguments && i < count; ++i)
    {
        const std::wstring_view argument(arguments[i]);
        if (argument == L"--wait-process")
        {
            sawRestartFlag = true;
            if (sawParent) malformed = true;
            sawParent = true;
            if (i + 1 >= count) { malformed = true; break; }
            wchar_t *end = nullptr;
            const unsigned long parsed = std::wcstoul(arguments[++i], &end, 10);
            if (parsed && end && !*end) parentId = DWORD(parsed);
            else malformed = true;
        }
        else if (argument == L"--restart-ready")
        {
            sawRestartFlag = true;
            if (sawEvent) malformed = true;
            sawEvent = true;
            if (i + 1 >= count) { malformed = true; break; }
            eventName = arguments[++i];
            if (eventName.empty()) malformed = true;
        }
    }
    if (arguments) LocalFree(arguments);
    if (!sawRestartFlag) return ChildHandshake::NotChild;
    if (malformed || !parentId || eventName.empty()) return ChildHandshake::Invalid;
    HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, parentId);
    HANDLE ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, eventName.c_str());
    if (!parent || !ready)
    {
        if (ready) CloseHandle(ready);
        if (parent) CloseHandle(parent);
        return ChildHandshake::Invalid;
    }
    if (!SetEvent(ready))
    {
        CloseHandle(ready);
        CloseHandle(parent);
        return ChildHandshake::Invalid;
    }
    CloseHandle(ready);
    const DWORD wait = WaitForSingleObject(parent, INFINITE);
    CloseHandle(parent);
    return wait == WAIT_OBJECT_0 ? ChildHandshake::Waited : ChildHandshake::Invalid;
}
#endif

#if defined(__linux__) && !defined(_WIN32)
// The parent keeps the pidfd until it sees the child's pre-init acknowledgement.
// The child inherits only two designated descriptors; no PID reuse or polling a
// process name is involved in deciding when it is safe to initialize the game.
inline constexpr int ParentFd = 100;
inline constexpr int ReadyFd = 101;

inline std::vector<std::string> LaunchArguments(bool install)
{
    std::ifstream command("/proc/self/cmdline", std::ios::binary);
    std::vector<std::string> arguments;
    std::string argument;
    while (std::getline(command, argument, '\0')) arguments.push_back(argument);
    if (!command.eof() || arguments.empty()) return {};
    std::vector<std::string> result{arguments.front()};
    bool hasInstall = false;
    for (size_t i = 1; i < arguments.size(); ++i)
    {
        const std::string_view current(arguments[i]);
        if (current == "--restart-parent-fd" || current == "--restart-ready-fd" ||
            current == "--wait-process" || current == "--restart-ready")
        {
            if (i + 1 < arguments.size()) ++i;
            continue;
        }
        if (current == "--install") { hasInstall = true; continue; }
        result.push_back(arguments[i]);
    }
    if (install || hasInstall) result.emplace_back("--install");
    return result;
}

inline bool LaunchWaitingProcess(const std::string &executable, std::vector<std::string> arguments,
                                 uint32_t readyTimeoutMs = 10000)
{
    auto fail = [] { ReportLaunchFailure(); return false; };
    if (executable.empty() || arguments.empty()) return fail();
#if defined(SYS_pidfd_open) && defined(SYS_pidfd_send_signal)
    int parent = static_cast<int>(syscall(SYS_pidfd_open, getpid(), 0));
#else
    int parent = -1;
#endif
    if (parent < 0) return fail(); // No unsafe fallback when pidfds are unavailable.
    int ready[2];
    if (pipe2(ready, O_CLOEXEC) != 0) { close(parent); return fail(); }

    // The source FDs are strictly above the child destinations, so dup2 in the
    // file actions cannot overwrite the other source. dup2 clears CLOEXEC.
    const int parentSource = fcntl(parent, F_DUPFD_CLOEXEC, ReadyFd + 1);
    const int readySource = fcntl(ready[1], F_DUPFD_CLOEXEC, ReadyFd + 1);
    close(parent);
    close(ready[1]);
    if (parentSource < 0 || readySource < 0)
    {
        if (parentSource >= 0) close(parentSource);
        if (readySource >= 0) close(readySource);
        close(ready[0]);
        return fail();
    }

    arguments[0] = executable;
    arguments.emplace_back("--restart-parent-fd");
    arguments.push_back(std::to_string(ParentFd));
    arguments.emplace_back("--restart-ready-fd");
    arguments.push_back(std::to_string(ReadyFd));
    std::vector<char *> argv;
    for (auto &argument : arguments) argv.push_back(argument.data());
    argv.push_back(nullptr);

    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attributes;
    const bool actionsReady = posix_spawn_file_actions_init(&actions) == 0;
    const bool attributesReady = posix_spawnattr_init(&attributes) == 0;
    bool configured = actionsReady && attributesReady;
    if (configured)
        configured = posix_spawn_file_actions_addclose(&actions, ready[0]) == 0 &&
                     posix_spawn_file_actions_adddup2(&actions, parentSource, ParentFd) == 0 &&
                     posix_spawn_file_actions_adddup2(&actions, readySource, ReadyFd) == 0 &&
                     posix_spawn_file_actions_addclose(&actions, parentSource) == 0 &&
                     posix_spawn_file_actions_addclose(&actions, readySource) == 0 &&
                     posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP) == 0 &&
                     posix_spawnattr_setpgroup(&attributes, 0) == 0;
    pid_t child = -1;
    const int spawnError = configured
        ? posix_spawn(&child, executable.c_str(), &actions, &attributes, argv.data(), environ)
        : EINVAL;
    if (actionsReady) posix_spawn_file_actions_destroy(&actions);
    if (attributesReady) posix_spawnattr_destroy(&attributes);
    close(parentSource);
    close(readySource);
    if (spawnError != 0) { close(ready[0]); return fail(); }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(readyTimeoutMs);
    bool acknowledged = false;
    while (true)
    {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();
        pollfd fd{ready[0], POLLIN, 0};
        const int result = poll(&fd, 1, remaining > 0 ? static_cast<int>(std::min<long long>(remaining, INT_MAX)) : 0);
        if (result < 0 && errno == EINTR) continue;
        if (result > 0 && (fd.revents & POLLIN))
        {
            char token = 0;
            acknowledged = read(ready[0], &token, 1) == 1 && token == 'R';
        }
        break;
    }
    close(ready[0]);
    if (acknowledged) return true;

    // AppImage launchers may fork before executing the game. Kill their entire
    // private process group, including late children, before returning control
    // to the still-running parent. Reap the direct child to avoid a zombie.
    kill(-child, SIGKILL);
    int status;
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
    return fail();
}

inline bool LaunchWaitingChild(uint32_t readyTimeoutMs = 10000)
{
    auto arguments = LaunchArguments(InstallRequested());
    const char *appImage = std::getenv("APPIMAGE");
    std::string executable;
    if (appImage)
        executable = appImage;
    else
    {
        char path[4096];
        const ssize_t length = readlink("/proc/self/exe", path, sizeof(path));
        if (length > 0 && length < static_cast<ssize_t>(sizeof(path)))
            executable.assign(path, static_cast<size_t>(length));
    }
    return LaunchWaitingProcess(executable, std::move(arguments), readyTimeoutMs);
}

enum class ChildHandshake { NotChild, Waited, Invalid };

inline ChildHandshake WaitForParentIfRestartChild(int argc, char *const argv[])
{
    bool sawParent = false, sawReady = false;
    int parent = -1, ready = -1;
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view flag(argv[i]);
        if (flag != "--restart-parent-fd" && flag != "--restart-ready-fd") continue;
        if (++i >= argc) return ChildHandshake::Invalid;
        const std::string_view value(argv[i]);
        if (value.empty() || value.find_first_not_of("0123456789") != std::string_view::npos)
            return ChildHandshake::Invalid;
        const int expected = flag == "--restart-parent-fd" ? ParentFd : ReadyFd;
        if (value != std::to_string(expected)) return ChildHandshake::Invalid;
        if (flag == "--restart-parent-fd")
        {
            if (sawParent) return ChildHandshake::Invalid;
            sawParent = true;
            parent = expected;
        }
        else
        {
            if (sawReady) return ChildHandshake::Invalid;
            sawReady = true;
            ready = expected;
        }
    }
    if (!sawParent && !sawReady) return ChildHandshake::NotChild;
    if (!sawParent || !sawReady) return ChildHandshake::Invalid;
#ifdef SYS_pidfd_send_signal
    if (syscall(SYS_pidfd_send_signal, parent, 0, nullptr, 0) != 0) return ChildHandshake::Invalid;
#else
    return ChildHandshake::Invalid;
#endif
    struct stat info{};
    if (fstat(ready, &info) != 0 || !S_ISFIFO(info.st_mode) ||
        (fcntl(ready, F_GETFL) & O_ACCMODE) != O_WRONLY)
        return ChildHandshake::Invalid;
    pollfd parentPoll{parent, POLLIN, 0};
    if (poll(&parentPoll, 1, 0) != 0) return ChildHandshake::Invalid;
    const char token = 'R';
    if (write(ready, &token, 1) != 1) return ChildHandshake::Invalid;
    close(ready);
    while (true)
    {
        parentPoll = {parent, POLLIN, 0};
        const int result = poll(&parentPoll, 1, -1);
        if (result < 0 && errno == EINTR) continue;
        close(parent);
        return result > 0 && (parentPoll.revents & POLLIN) ? ChildHandshake::Waited
                                                             : ChildHandshake::Invalid;
    }
}
#endif
} // namespace settings::restart
