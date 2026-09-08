#pragma once
#include "config.h"
#include <atomic>
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
    LaunchFailed
};

inline std::atomic<State> state{State::Idle};

inline bool Required(const Config &before, const Config &after)
{
    return before.gameLanguage != after.gameLanguage || before.graphicsBackend != after.graphicsBackend;
}

inline void Request() { state.store(State::Requested); }
inline bool Requested() { return state.load() == State::Requested; }
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
    return LaunchWaitingProcess(executable, CurrentArgumentsWithoutHandshake(), readyTimeoutMs);
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
} // namespace settings::restart
