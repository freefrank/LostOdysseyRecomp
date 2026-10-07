#pragma once
// Startup watchdog (#282). While a thread runs labelled startup steps, a
// helper thread waits; when a step passes its deadline it logs the thread's
// stack, caller-supplied context and the modules loaded from outside Windows
// and the game folder (overlays and hooks), then repeats every 30 s. It never
// touches the watched calls and is silent unless a step is late. Windows x64
// only; elsewhere Watch does nothing.
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cwctype>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <os/logger.h>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif
#if defined(_WIN32) && defined(_M_X64)
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

namespace os::hang_watch
{
// LO_TRACE_STARTUP (any value but empty or 0) adds the verbose startup lines:
// adapters, foreign modules, window state and swap chain timing.
inline bool StartupTrace()
{
    static const bool enabled = [] {
        const char *value = std::getenv("LO_TRACE_STARTUP");
        return value && *value && std::strcmp(value, "0") != 0;
    }();
    return enabled;
}

#if defined(_WIN32) && defined(_M_X64)
// Return addresses of a suspended thread, by the x64 unwind tables. No heap
// use or locks while the thread is suspended; dynamic code without unwind
// data ends the walk.
inline unsigned CaptureFrames(HANDLE thread, DWORD64 *frames, unsigned capacity)
{
    if (SuspendThread(thread) == DWORD(-1)) return 0;
    unsigned count = 0;
    CONTEXT context{};
    context.ContextFlags = CONTEXT_FULL;
    __try
    {
        if (GetThreadContext(thread, &context))
        {
            while (count < capacity && context.Rip)
            {
                frames[count++] = context.Rip;
                DWORD64 imageBase = 0;
                const auto function = RtlLookupFunctionEntry(context.Rip, &imageBase, nullptr);
                if (!function)
                {
                    context.Rip = *reinterpret_cast<const DWORD64 *>(context.Rsp);
                    context.Rsp += 8;
                    continue;
                }
                void *handlerData = nullptr;
                DWORD64 establisher = 0;
                RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, context.Rip, function, &context, &handlerData,
                                 &establisher, nullptr);
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    ResumeThread(thread);
    return count;
}

inline std::string ModuleOf(DWORD64 address, DWORD64 &offset)
{
    HMODULE module = nullptr;
    offset = address;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(address), &module) || !module)
        return "?";
    offset = address - reinterpret_cast<DWORD64>(module);
    wchar_t path[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
    std::wstring name(path, length);
    name = name.substr(name.find_last_of(L"\\/") + 1);
    return std::string(name.begin(), name.end());
}

inline std::string StackOf(HANDLE thread)
{
    DWORD64 frames[32]{};
    const unsigned count = CaptureFrames(thread, frames, 32);
    if (!count) return "unavailable";
    const HANDLE process = GetCurrentProcess();
    static const bool symbols = [process] {
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS);
        return SymInitialize(process, "", TRUE) != FALSE;
    }();
    alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + 256]{};
    auto *symbol = reinterpret_cast<SYMBOL_INFO *>(buffer);
    std::string text;
    for (unsigned i = 0; i < count; ++i)
    {
        DWORD64 offset = 0;
        text += fmt::format("{}#{} {}+{:#x}", i ? " | " : "", i, ModuleOf(frames[i], offset), offset);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 255;
        DWORD64 displacement = 0;
        if (symbols && SymFromAddr(process, frames[i], &displacement, symbol))
            text += fmt::format(" {}+{:#x}", symbol->Name, displacement);
    }
    return text;
}

#endif

#ifdef _WIN32
// Modules outside the Windows folder (the display driver store excepted) and
// the game folder: overlays, capture tools and other injected hooks.
inline std::string ForeignModules()
{
    HMODULE modules[1024]{};
    DWORD bytes = 0;
    if (!K32EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &bytes)) return "unavailable";
    auto lower = [](std::wstring text) {
        for (auto &c : text) c = towlower(c);
        return text;
    };
    wchar_t windows[MAX_PATH]{}, exe[MAX_PATH]{};
    GetWindowsDirectoryW(windows, MAX_PATH);
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    const auto windowsDir = lower(windows) + L"\\";
    auto gameDir = lower(exe);
    gameDir = gameDir.substr(0, gameDir.find_last_of(L"\\/") + 1);
    std::string text;
    for (DWORD i = 0; i < bytes / sizeof(HMODULE) && i < 1024; ++i)
    {
        wchar_t path[MAX_PATH]{};
        const DWORD length = GetModuleFileNameW(modules[i], path, MAX_PATH);
        const auto name = lower(std::wstring(path, length));
        const bool driverStore = name.find(L"\\driverstore\\") != std::wstring::npos;
        if ((name.rfind(windowsDir, 0) == 0 && !driverStore) || name.rfind(gameDir, 0) == 0) continue;
        int size = WideCharToMultiByte(CP_UTF8, 0, path, int(length), nullptr, 0, nullptr, nullptr);
        std::string utf8(size_t(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, path, int(length), utf8.data(), size, nullptr, nullptr);
        text += (text.empty() ? "" : "; ") + utf8;
    }
    return text.empty() ? std::string("none") : text;
}
#endif

// Watches the constructing thread through a sequence of labelled steps. Any
// step that runs past `firstReport` (10 s) is reported, then every
// `repeatReport` (30 s) while it lasts; a reported step logs its total time
// when the next step starts.
class Watch
{
    std::mutex mutex;
    std::condition_variable wake;
    bool done = false;
    const char *step = "";
    std::chrono::steady_clock::time_point stepStart = std::chrono::steady_clock::now();
    uint64_t stepSerial = 0;
    bool stepReported = false;
    std::thread worker;

public:
    Watch(const char *first, std::function<std::string()> context,
          std::chrono::milliseconds firstReport = std::chrono::seconds(10),
          std::chrono::milliseconds repeatReport = std::chrono::seconds(30)) : step(first)
    {
#if defined(_WIN32) && defined(_M_X64)
        HANDLE thread = nullptr;
        if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &thread,
                             THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, 0))
            return;
        worker = std::thread([this, context = std::move(context), thread, firstReport, repeatReport] {
            bool modulesLogged = false;
            std::unique_lock lock(mutex);
            while (!done)
            {
                const uint64_t serial = stepSerial;
                auto deadline = stepStart + firstReport;
                while (!wake.wait_until(lock, deadline, [&] { return done || serial != stepSerial; }))
                {
                    stepReported = true;
                    const char *label = step;
                    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::steady_clock::now() - stepStart).count();
                    lock.unlock();
                    LOG_WARNING("hang watch: {} still running after {} s; {}", label, seconds,
                                context ? context() : std::string{});
                    LOG_WARNING("hang watch: {} stack: {}", label, StackOf(thread));
                    if (!std::exchange(modulesLogged, true))
                        LOG_WARNING("hang watch: modules from outside Windows and the game folder: {}", ForeignModules());
                    lock.lock();
                    deadline = std::chrono::steady_clock::now() + repeatReport;
                }
            }
            CloseHandle(thread);
        });
#else
        (void)context;
        (void)firstReport;
        (void)repeatReport;
#endif
    }
    // The next step; the previous one is complete.
    void Step(const char *label)
    {
        const char *previous = nullptr;
        long long seconds = 0;
        {
            std::lock_guard lock(mutex);
            const auto now = std::chrono::steady_clock::now();
            if (stepReported)
            {
                previous = step;
                seconds = std::chrono::duration_cast<std::chrono::seconds>(now - stepStart).count();
            }
            step = label;
            stepStart = now;
            stepReported = false;
            ++stepSerial;
        }
        wake.notify_all();
        if (previous) LOG_WARNING("hang watch: {} finished after {} s", previous, seconds);
    }
    ~Watch()
    {
        Step("");
        {
            std::lock_guard lock(mutex);
            done = true;
        }
        wake.notify_all();
        if (worker.joinable()) worker.join();
    }
    Watch(const Watch &) = delete;
    Watch &operator=(const Watch &) = delete;
};
} // namespace os::hang_watch
