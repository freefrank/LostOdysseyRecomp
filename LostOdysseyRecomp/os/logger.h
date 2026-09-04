#pragma once

#include <fmt/core.h>
#include <cstdio>
#include <mutex>

enum class LogType
{
    Info,
    Warning,
    Error,
    Utility,
    Kernel,
};

namespace os::logger
{
    inline std::mutex g_mutex;
    inline bool g_kernelTrace = true;

    inline const char* Prefix(LogType type)
    {
        switch (type)
        {
        case LogType::Info: return "[info] ";
        case LogType::Warning: return "[warn] ";
        case LogType::Error: return "[error]";
        case LogType::Utility: return "[util] ";
        case LogType::Kernel: return "[krnl] ";
        }
        return "";
    }

    template<typename... Args>
    inline void Log(LogType type, const char* func, fmt::format_string<Args...> format, Args&&... args)
    {
        if (type == LogType::Kernel && !g_kernelTrace)
            return;

        std::lock_guard lock(g_mutex);
        std::string msg = fmt::format(format, std::forward<Args>(args)...);
        if (func)
            fmt::print(stderr, "{} {}: {}\n", Prefix(type), func, msg);
        else
            fmt::print(stderr, "{} {}\n", Prefix(type), msg);
        fflush(stderr);
    }
}

#define LOG_IMPL(type, ...) os::logger::Log(LogType::type, nullptr, __VA_ARGS__)
#define LOGFN_IMPL(type, ...) os::logger::Log(LogType::type, __FUNCTION__, __VA_ARGS__)

#define LOG_INFO(...)      LOG_IMPL(Info, __VA_ARGS__)
#define LOG_WARNING(...)   LOG_IMPL(Warning, __VA_ARGS__)
#define LOG_ERROR(...)     LOG_IMPL(Error, __VA_ARGS__)
#define LOGFN_INFO(...)    LOGFN_IMPL(Info, __VA_ARGS__)
#define LOGFN_WARNING(...) LOGFN_IMPL(Warning, __VA_ARGS__)
#define LOGFN_ERROR(...)   LOGFN_IMPL(Error, __VA_ARGS__)
#define LOG_UTILITY(...)   LOGFN_IMPL(Utility, __VA_ARGS__)
#define LOGF_UTILITY(...)  LOGFN_IMPL(Utility, __VA_ARGS__)
#define LOG_KERNEL(...)    LOGFN_IMPL(Kernel, __VA_ARGS__)
#define LOG_STUB()         LOGFN_IMPL(Kernel, "!!! STUB !!!")
