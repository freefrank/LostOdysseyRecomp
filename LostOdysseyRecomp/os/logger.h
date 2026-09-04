#pragma once

#include <fmt/core.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <thread>

enum class LogType
{
    Info,
    Warning,
    Error,
    Utility,
    Kernel,
    Verbose,   // per-frame chatter (interrupts, scratch writebacks...): LO_VERBOSE=1 to see it
};

namespace os::logger
{
    inline std::mutex g_mutex;
    inline bool g_kernelTrace = true;
    inline const bool g_verbose = getenv("LO_VERBOSE") != nullptr;

    // Every line carries the seconds since the process started and a short
    // thread tag, so a log can be read as a timeline ("what was it doing at
    // 72 s, and on which thread") rather than an unordered pile of messages.
    inline const std::chrono::steady_clock::time_point g_start = std::chrono::steady_clock::now();

    inline double ElapsedSeconds()
    {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - g_start).count();
    }

    inline uint32_t ThreadTag()
    {
        static thread_local const uint32_t tag = uint32_t(std::hash<std::thread::id>{}(std::this_thread::get_id()) & 0xFFFF);
        return tag;
    }

    inline const char* Prefix(LogType type)
    {
        switch (type)
        {
        case LogType::Info: return "[info] ";
        case LogType::Warning: return "[warn] ";
        case LogType::Error: return "[error]";
        case LogType::Utility: return "[util] ";
        case LogType::Kernel: return "[krnl] ";
        case LogType::Verbose: return "[verb] ";
        }
        return "";
    }

    template<typename... Args>
    inline void Log(LogType type, const char* func, fmt::format_string<Args...> format, Args&&... args)
    {
        if (type == LogType::Kernel && !g_kernelTrace)
            return;
        if (type == LogType::Verbose && !g_verbose)
            return;

        std::lock_guard lock(g_mutex);
        std::string msg = fmt::format(format, std::forward<Args>(args)...);
        if (func)
            fmt::print(stderr, "[{:9.3f} t{:04x}] {} {}: {}\n", ElapsedSeconds(), ThreadTag(), Prefix(type), func, msg);
        else
            fmt::print(stderr, "[{:9.3f} t{:04x}] {} {}\n", ElapsedSeconds(), ThreadTag(), Prefix(type), msg);
        fflush(stderr);
    }
}

#define LOG_IMPL(type, ...) os::logger::Log(LogType::type, nullptr, __VA_ARGS__)
#define LOGFN_IMPL(type, ...) os::logger::Log(LogType::type, __FUNCTION__, __VA_ARGS__)

#define LOG_INFO(...)      LOG_IMPL(Info, __VA_ARGS__)
#define LOG_WARNING(...)   LOG_IMPL(Warning, __VA_ARGS__)
#define LOG_ERROR(...)     LOG_IMPL(Error, __VA_ARGS__)
#define LOG_VERBOSE(...)   LOG_IMPL(Verbose, __VA_ARGS__)
#define LOGFN_INFO(...)    LOGFN_IMPL(Info, __VA_ARGS__)
#define LOGFN_WARNING(...) LOGFN_IMPL(Warning, __VA_ARGS__)
#define LOGFN_ERROR(...)   LOGFN_IMPL(Error, __VA_ARGS__)
#define LOG_UTILITY(...)   LOGFN_IMPL(Utility, __VA_ARGS__)
#define LOGF_UTILITY(...)  LOGFN_IMPL(Utility, __VA_ARGS__)
#define LOG_KERNEL(...)    LOGFN_IMPL(Kernel, __VA_ARGS__)
#define LOG_STUB()         LOGFN_IMPL(Kernel, "!!! STUB !!!")
