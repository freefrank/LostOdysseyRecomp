#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <charconv>
#include <string_view>
#include <type_traits>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <mutex>
#include <span>
#include <string>
#include <system_error>
#include <thread>
#include <os/thread_name.h>
#include <utility>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#elif defined(__unix__)
#include <unistd.h>
#endif

namespace xenos::preparation
{
    struct Cancelled : std::exception {
        const char* what() const noexcept override { return "shader preparation cancelled"; }
    };

    inline uint64_t HostPhysicalMemoryBytes()
    {
#ifdef _WIN32
        MEMORYSTATUSEX status{sizeof(status)};
        if (GlobalMemoryStatusEx(&status)) return status.ullTotalPhys;
#elif defined(__APPLE__)
        uint64_t bytes = 0;
        size_t size = sizeof(bytes);
        if (sysctlbyname("hw.memsize", &bytes, &size, nullptr, 0) == 0) return bytes;
#elif defined(__unix__) && defined(_SC_PHYS_PAGES)
        const long pages = sysconf(_SC_PHYS_PAGES), pageSize = sysconf(_SC_PAGESIZE);
        if (pages > 0 && pageSize > 0) return uint64_t(pages) * uint64_t(pageSize);
#endif
        return 0; // Unknown memory is not an artificial low-memory machine.
    }

    inline size_t DefaultWorkerCap(unsigned logicalThreads, uint64_t physicalBytes)
    {
        const size_t available = logicalThreads > 1 ? logicalThreads - 1 : 1u;
        return physicalBytes && physicalBytes < (8ull << 30)
            ? std::min<size_t>(available, 4) : available;
    }

    inline size_t HostWorkerCap(unsigned logicalThreads)
    {
        return DefaultWorkerCap(logicalThreads, HostPhysicalMemoryBytes());
    }

    inline size_t WorkerCount(unsigned logicalThreads, size_t jobs, bool forceSerial,
        unsigned cap = 4u, const char* overrideName = "LO_SHADER_WORKERS")
    {
        if (!jobs) return 0;
        if (forceSerial) return 1; // Safety/diagnostic serial always wins.
        if (const char* env = std::getenv(overrideName)) {
            const std::string_view value(env);
            if (value == "0" || value == "max" || value == "all")
                return std::min<size_t>(std::max(1u, logicalThreads), jobs);
            size_t parsed = 0;
            const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
            if (result.ec == std::errc{} && result.ptr == value.data() + value.size() && parsed)
                return std::min(parsed, jobs);
        }
        const unsigned requested = logicalThreads > 1 ? logicalThreads - 1 : 1u;
        return std::min<size_t>(cap ? std::min(requested, cap) : requested, jobs);
    }

    struct QueueStats
    {
        size_t startedWorkers = 0;
        size_t startFailures = 0;
        size_t consumed = 0;
        size_t maxQueued = 0;
        bool cancelled = false;
        std::string startError;
    };

    struct ThreadLauncher
    {
        std::jthread operator()(std::function<void()> work) const
        {
            return std::jthread(std::move(work));
        }
    };

    // prepare runs only on worker threads (or the serial fallback); consume runs
    // only on the caller. Returning false from consume cancels outstanding work.
    // The bounded queue and cancellation wakeups keep both sides from waiting on
    // each other during normal completion, early cancellation, or an exception.
    template<typename Result, typename Prepare, typename Consume, typename Idle, typename Launch = ThreadLauncher>
    QueueStats RunBounded(size_t jobs, size_t requestedWorkers, size_t capacity,
        Prepare&& prepare, Consume&& consume, Idle&& idle, Launch&& launch = {})
    {
        QueueStats stats;
        if (!jobs) return stats;
        requestedWorkers = std::min(requestedWorkers, jobs);
        capacity = std::max<size_t>(1, capacity);

        std::atomic<size_t> next{0};
        std::atomic<bool> cancelled{false};
        std::mutex mutex;
        std::condition_variable changed;
        std::deque<Result> ready;
        std::exception_ptr workerFailure;

        auto cancel = [&] {
            { std::lock_guard lock(mutex); cancelled = true; }
            changed.notify_all();
        };
        auto poll = [&] {
            if constexpr (std::is_convertible_v<std::invoke_result_t<Idle>, bool>) {
                if (!idle()) { stats.cancelled = true; cancel(); return false; }
            } else idle();
            return true;
        };
        auto fail = [&](std::exception_ptr failure) {
            std::lock_guard lock(mutex);
            if (!workerFailure) workerFailure = failure;
            cancelled = true;
            changed.notify_all();
        };
        auto worker = [&] {
            os::SetCurrentThreadName("Shader Worker");
            try {
                for (;;) {
                    if (cancelled.load()) return;
                    const size_t index = next.fetch_add(1);
                    if (index >= jobs) return;
                    auto result = prepare(index);
                    std::unique_lock lock(mutex);
                    changed.wait(lock, [&] { return cancelled.load() || ready.size() < capacity; });
                    if (cancelled.load()) return;
                    ready.emplace_back(std::move(result));
                    stats.maxQueued = std::max(stats.maxQueued, ready.size());
                    lock.unlock();
                    changed.notify_all();
                }
            } catch (...) {
                fail(std::current_exception());
            }
        };

        std::vector<std::jthread> workers;
        struct CancelBeforeWorkersJoin
        {
            std::atomic<bool>& cancelled;
            std::condition_variable& changed;
            std::mutex& mutex;
            bool armed = true;
            ~CancelBeforeWorkersJoin() noexcept
            {
                if (!armed) return;
                { std::lock_guard lock(mutex); cancelled = true; }
                changed.notify_all();
            }
        } cancelBeforeWorkersJoin{cancelled, changed, mutex};
        workers.reserve(requestedWorkers);
        try {
            for (size_t i = 0; i < requestedWorkers; ++i)
                workers.emplace_back(launch(std::function<void()>(worker)));
        } catch (const std::system_error& e) {
            ++stats.startFailures;
            stats.startError = e.what();
        }
        stats.startedWorkers = workers.size();

        try {
            if (workers.empty()) {
                for (size_t i = 0; i < jobs; ++i) {
                    if (!poll()) break;
                    if (!consume(prepare(i))) {
                        stats.cancelled = true;
                        break;
                    }
                    ++stats.consumed;
                }
            } else {
                while (stats.consumed < jobs && !cancelled.load()) {
                    if (!poll()) break;
                    Result result;
                    std::unique_lock lock(mutex);
                    if (!changed.wait_for(lock, std::chrono::milliseconds(10),
                        [&] { return cancelled.load() || !ready.empty(); })) {
                        lock.unlock();
                        if (!poll()) break;
                        continue;
                    }
                    if (ready.empty()) break;
                    result = std::move(ready.front());
                    ready.pop_front();
                    lock.unlock();
                    changed.notify_all();
                    if (!consume(std::move(result))) {
                        stats.cancelled = true;
                        cancel();
                        break;
                    }
                    ++stats.consumed;
                }
            }
        } catch (...) {
            cancel();
            for (auto& thread : workers) thread.join();
            throw;
        }

        for (auto& thread : workers) thread.join();
        cancelBeforeWorkersJoin.armed = false;
        if (workerFailure) std::rethrow_exception(workerFailure);
        return stats;
    }

    enum class CacheWriteStatus { Skipped, Saved, Failed };
    struct CacheWriteResult
    {
        CacheWriteStatus status = CacheWriteStatus::Skipped;
        std::string error;
    };

    // A failed compilation must leave the previous cache path untouched so the
    // shader remains retryable. A successful binary can still be used during
    // this launch when persistence fails; the caller reports that failure.
    inline CacheWriteResult WriteCompiledCache(const std::filesystem::path& path,
        std::span<const uint8_t> binary, bool compilationSucceeded)
    {
        if (!compilationSucceeded) return {};
        if (binary.empty()) return {CacheWriteStatus::Failed, "compiled shader binary is empty"};
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(binary.data()), binary.size());
        out.close();
        if (!out) return {CacheWriteStatus::Failed, "cannot write compiled shader cache"};
        return {CacheWriteStatus::Saved, {}};
    }
}
