#pragma once

#include <fmt/core.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <thread>
#ifdef __ANDROID__
#include <android/log.h>
#endif

enum class LogType
{
    Info,
    Warning,
    Error,
    Utility,
    Kernel,
    Verbose,   // per-frame chatter (interrupts, scratch writebacks...): LO_VERBOSE=1 to see it
    Notice,    // always written: build, device, map and rendering mismatches; read by log collection
};

namespace os::logger
{
    inline std::mutex g_mutex;
    // Info and Kernel lines need the Debug log setting; main applies it once
    // the settings file has been read, the settings menu when it is saved.
    inline std::atomic<bool> g_infoTrace{true};
    inline std::atomic<bool> g_kernelTrace{true};
    inline bool g_quietKernel = false; // --quiet-kernel

    // LO_DEBUG_LOG=1 forces the debug log on (test harnesses), =0 off.
    inline void SetDebugLog(bool enabled)
    {
        if (const char* forced = getenv("LO_DEBUG_LOG"); forced && *forced)
            enabled = forced[0] != '0';
        g_infoTrace.store(enabled, std::memory_order_relaxed);
        g_kernelTrace.store(enabled && !g_quietKernel, std::memory_order_relaxed);
    }
    // Opened once by main; intentionally kept open until process termination.
    inline FILE* g_file = nullptr;
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
        case LogType::Notice: return "[note] ";
        }
        return "";
    }

    template<typename... Args>
    inline void Log(LogType type, const char* func, fmt::format_string<Args...> format, Args&&... args)
    {
        if (type == LogType::Kernel && !g_kernelTrace.load(std::memory_order_relaxed))
            return;
        if (type == LogType::Info && !g_infoTrace.load(std::memory_order_relaxed))
            return;
        if (type == LogType::Verbose && !g_verbose)
            return;

        std::lock_guard lock(g_mutex);
        std::string msg = fmt::format(format, std::forward<Args>(args)...);
        const auto line = func
            ? fmt::format("[{:9.3f} t{:04x}] {} {}: {}\n", ElapsedSeconds(), ThreadTag(), Prefix(type), func, msg)
            : fmt::format("[{:9.3f} t{:04x}] {} {}\n", ElapsedSeconds(), ThreadTag(), Prefix(type), msg);
#if defined(__SWITCH__)
        // Each flush is an SD card write taken under g_mutex, which every
        // logging thread waits on: buffer, flush warnings and errors at once and
        // the rest at least every second. The crash handlers flush on exit.
        // No stderr copy once the runtime log is open (stderr.log keeps the
        // driver's, profiler's and crash output).
        if (g_file)
        {
            fwrite(line.data(), 1, line.size(), g_file);
            static auto lastFlush = std::chrono::steady_clock::now();
            const auto now = std::chrono::steady_clock::now();
            if (type == LogType::Error || type == LogType::Warning || now - lastFlush >= std::chrono::seconds(1))
            {
                fflush(g_file);
                lastFlush = now;
            }
            return;
        }
#else
        if (g_file)
        {
            fwrite(line.data(), 1, line.size(), g_file);
            fflush(g_file);
        }
#endif
#ifdef __ANDROID__
        // Android stderr is a file (native-stderr.log); logcat replaces the
        // terminal copy, and stderr keeps only lines without a log file.
        const int priority = type == LogType::Error ? ANDROID_LOG_ERROR
            : type == LogType::Warning ? ANDROID_LOG_WARN
            : type == LogType::Info || type == LogType::Notice ? ANDROID_LOG_INFO
            : type == LogType::Verbose ? ANDROID_LOG_VERBOSE : ANDROID_LOG_DEBUG;
        __android_log_write(priority, "LostOdyssey", line.c_str());
        if (g_file)
            return;
#endif
        fwrite(line.data(), 1, line.size(), stderr);
        fflush(stderr);
    }
}

#define LOG_IMPL(type, ...) os::logger::Log(LogType::type, nullptr, __VA_ARGS__)
#define LOGFN_IMPL(type, ...) os::logger::Log(LogType::type, __FUNCTION__, __VA_ARGS__)

#define LOG_INFO(...)      LOG_IMPL(Info, __VA_ARGS__)
#define LOG_NOTICE(...)    LOG_IMPL(Notice, __VA_ARGS__)
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
