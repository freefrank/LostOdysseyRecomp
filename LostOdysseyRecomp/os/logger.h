#pragma once

#include <fmt/core.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include <os/netlog.h>
#if defined(__SWITCH__)
#include <os/switch_platform.h>
#include <os/switch_cpu_profiler.h>
#endif
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

#if defined(__SWITCH__)
    // SD card writes take tens of milliseconds (60-110 ms seen for a 64 KiB
    // buffer). Taken by whichever thread logged the line that filled the
    // buffer or crossed the one-second flush, that was usually the GPU command
    // processor, which logs every frame with LO_RENDER_TIMING: a hitch once a
    // second. Lines are now appended to memory under g_mutex and a writer
    // thread puts them on the card; warnings and errors still go out at once.
    inline std::string g_pending;              // under g_mutex
    inline std::mutex g_writeMutex;            // file writes, in order; taken before g_mutex
    inline std::condition_variable g_writerWake;
    inline bool g_writerStarted = false;       // under g_mutex
    inline constexpr size_t kWriterWakeBytes = 128 * 1024;

    // Writes everything queued so far. Lock order: g_writeMutex, then g_mutex.
    inline void DrainPending()
    {
        std::lock_guard write(g_writeMutex);
        std::string out;
        {
            std::lock_guard lock(g_mutex);
            out.swap(g_pending);
        }
        if (!out.empty() && g_file)
        {
            fwrite(out.data(), 1, out.size(), g_file);
            fflush(g_file);
        }
    }

    inline void StartWriter() // caller holds g_mutex
    {
        g_writerStarted = true;
        // Never joined or detached: libnx has no pthread_detach.
        (void)new std::thread([] {
            // Above the game's threads (pthreads get 0x3B, the game's own run
            // at 0x2C and up), so a freeze that keeps the cores busy still
            // gets its last lines to the card. It sleeps between writes.
            os::switch_platform::SetCurrentThreadPriority(0x2A);
            os::switch_cpu_profiler::RegisterCurrentThread("Log Writer"); // listed in hang.log
            for (;;)
            {
                {
                    std::unique_lock lock(g_mutex);
                    g_writerWake.wait_for(lock, std::chrono::milliseconds(500),
                        [] { return g_pending.size() >= kWriterWakeBytes; });
                }
                DrainPending();
            }
        });
    }
#endif

    template<typename... Args>
    inline void Log(LogType type, const char* func, fmt::format_string<Args...> format, Args&&... args)
    {
        if (type == LogType::Kernel && !g_kernelTrace.load(std::memory_order_relaxed))
            return;
        if (type == LogType::Info && !g_infoTrace.load(std::memory_order_relaxed))
            return;
        if (type == LogType::Verbose && !g_verbose)
            return;

        std::unique_lock lock(g_mutex);
        std::string msg = fmt::format(format, std::forward<Args>(args)...);
        const auto line = func
            ? fmt::format("[{:9.3f} t{:04x}] {} {}: {}\n", ElapsedSeconds(), ThreadTag(), Prefix(type), func, msg)
            : fmt::format("[{:9.3f} t{:04x}] {} {}\n", ElapsedSeconds(), ThreadTag(), Prefix(type), msg);
#if defined(__SWITCH__)
        // Queued for the writer thread (see DrainPending); warnings and errors
        // are written before returning. Crash and exit paths drain the queue
        // (os::logger::DrainForExit). No stderr copy once the runtime log is
        // open (stderr.log keeps the driver's, profiler's and crash output).
        os::netlog::Write(line.data(), line.size());
        if (g_file)
        {
            g_pending += line;
            if (!g_writerStarted)
                StartWriter();
            if (type == LogType::Error || type == LogType::Warning)
            {
                lock.unlock();
                DrainPending();
            }
            else if (g_pending.size() >= kWriterWakeBytes)
                g_writerWake.notify_one();
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
