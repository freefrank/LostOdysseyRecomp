#include "gpu/shader/preparation_queue.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

namespace
{
    void Require(bool value, const char* message)
    {
        if (!value) throw std::runtime_error(message);
    }

    std::vector<uint8_t> Read(const fs::path& path)
    {
        std::ifstream in(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(in), {}};
    }

    struct FailingLauncher
    {
        std::atomic<unsigned>* attempts;
        unsigned successfulStarts;

        std::jthread operator()(std::function<void()> work) const
        {
            if (attempts->fetch_add(1) >= successfulStarts)
                throw std::system_error(std::make_error_code(std::errc::resource_unavailable_try_again));
            return std::jthread(std::move(work));
        }
    };

    struct ThrowingLauncher
    {
        std::atomic<unsigned>* attempts;
        unsigned successfulStarts;

        std::jthread operator()(std::function<void()> work) const
        {
            if (attempts->fetch_add(1) >= successfulStarts) {
                // Give the started producers time to fill the two-item queue
                // and block before launch unwinds the worker vector.
                std::this_thread::sleep_for(50ms);
                throw std::runtime_error("injected non-system launcher failure");
            }
            return std::jthread(std::move(work));
        }
    };

    int RunLaunchExceptionOnly(const fs::path& output)
    {
        Require(!fs::exists(output), "output directory already exists");
        fs::create_directories(output);
        std::atomic<unsigned> attempts{0};
        const auto started = std::chrono::steady_clock::now();
        bool observed = false;
        try {
            xenos::preparation::RunBounded<size_t>(200, 7, 2,
                [](size_t index) {
                    std::this_thread::sleep_for(1ms);
                    return index;
                }, [](size_t) { return true; }, [] {}, ThrowingLauncher{&attempts, 2});
        } catch (const std::runtime_error& e) {
            observed = std::string(e.what()) == "injected non-system launcher failure";
        }
        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count();
        Require(observed && attempts == 3 && elapsedMs < 2000,
            "non-system launcher failure did not cancel and join started producers");
        std::ofstream summary(output / "summary.json", std::ios::binary);
        summary << "{\n  \"result\": \"PASS\",\n  \"startedBeforeFailure\": 2,\n"
            << "  \"launcherAttempts\": " << attempts << ",\n"
            << "  \"elapsedMs\": " << elapsedMs << "\n}\n";
        summary.close();
        Require(bool(summary), "cannot write launch-exception summary");
        std::printf("non-system launcher failure wakeup: PASS (2 workers, %lld ms)\n",
            static_cast<long long>(elapsedMs));
        return 0;
    }
}

int main(int argc, char** argv)
try
{
    if (argc == 3 && std::string(argv[1]) == "--launch-exception-only")
        return RunLaunchExceptionOnly(fs::absolute(argv[2]));
    Require(argc == 2, "usage: shader_preparation_queue_test <new-output-directory>");
    const fs::path output = fs::absolute(argv[1]);
    Require(!fs::exists(output), "output directory already exists");
    fs::create_directories(output);

    using xenos::preparation::WorkerCount;
    Require(WorkerCount(0, 10, false) == 1, "unknown logical count must use one worker");
    Require(WorkerCount(1, 10, false) == 1, "single logical thread must use one worker");
    Require(WorkerCount(2, 10, false) == 1, "two logical threads must leave one free");
    Require(WorkerCount(8, 10, false) == 7, "multi-core count must use logical minus one");
    Require(WorkerCount(8, 3, false) == 3, "worker count must not exceed jobs");
    Require(WorkerCount(8, 10, true) == 1, "serial control must use one worker");
    Require(WorkerCount(8, 0, false) == 0, "empty input must start no workers");

    constexpr size_t jobs = 97;
    std::array<std::atomic<unsigned>, jobs> prepared{};
    std::array<unsigned, jobs> consumed{};
    auto coverage = xenos::preparation::RunBounded<size_t>(jobs, 7, 3,
        [&](size_t index) {
            ++prepared[index];
            if ((index & 7) == 0) std::this_thread::sleep_for(1ms);
            return index;
        },
        [&](size_t index) {
            ++consumed[index];
            return true;
        }, [] {});
    Require(coverage.startedWorkers == 7, "coverage worker count mismatch");
    Require(coverage.consumed == jobs, "not all jobs consumed");
    Require(coverage.maxQueued > 0 && coverage.maxQueued <= 3, "queue exceeded its item bound");
    for (size_t i = 0; i < jobs; ++i)
        Require(prepared[i] == 1 && consumed[i] == 1, "job was lost or duplicated");

    std::atomic<unsigned> allFailAttempts{0};
    size_t serialConsumed = 0;
    auto serialFallback = xenos::preparation::RunBounded<size_t>(11, 4, 2,
        [](size_t index) { return index; },
        [&](size_t) { ++serialConsumed; return true; }, [] {},
        FailingLauncher{&allFailAttempts, 0});
    Require(serialFallback.startedWorkers == 0 && serialFallback.startFailures == 1,
        "complete thread-start failure did not select serial fallback");
    Require(serialConsumed == 11 && serialFallback.consumed == 11,
        "serial fallback did not cover every job");

    std::atomic<unsigned> partialAttempts{0};
    size_t partialConsumed = 0;
    auto partialFallback = xenos::preparation::RunBounded<size_t>(41, 6, 2,
        [](size_t index) { return index; },
        [&](size_t) { ++partialConsumed; return true; }, [] {},
        FailingLauncher{&partialAttempts, 2});
    Require(partialFallback.startedWorkers == 2 && partialFallback.startFailures == 1,
        "partial thread-start failure count mismatch");
    Require(partialConsumed == 41 && partialFallback.consumed == 41,
        "remaining workers did not cover every job");

    std::atomic<size_t> cancellationPrepared{0};
    size_t cancellationCalls = 0;
    const auto cancellationStarted = std::chrono::steady_clock::now();
    auto cancellation = xenos::preparation::RunBounded<size_t>(200, 7, 2,
        [&](size_t index) {
            ++cancellationPrepared;
            std::this_thread::sleep_for(1ms);
            return index;
        },
        [&](size_t) { return ++cancellationCalls < 5; }, [] {});
    const auto cancellationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - cancellationStarted).count();
    Require(cancellation.cancelled && cancellationCalls == 5,
        "consumer cancellation was not reported at the requested boundary");
    Require(cancellation.maxQueued <= 2 && cancellationMs < 2000,
        "consumer cancellation exceeded the queue bound or failed to wake producers");

    bool workerFailureObserved = false;
    const auto failureStarted = std::chrono::steady_clock::now();
    try {
        xenos::preparation::RunBounded<size_t>(40, 4, 2,
            [](size_t index) -> size_t {
                if (index == 3) throw std::runtime_error("injected prepare failure");
                std::this_thread::sleep_for(1ms);
                return index;
            }, [](size_t) { return true; }, [] {});
    } catch (const std::runtime_error& e) {
        workerFailureObserved = std::string(e.what()) == "injected prepare failure";
    }
    const auto failureMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - failureStarted).count();
    Require(workerFailureObserved && failureMs < 2000,
        "worker failure did not cancel and join the bounded queue");

    bool consumerFailureObserved = false;
    size_t consumerFailureCalls = 0;
    const auto consumerFailureStarted = std::chrono::steady_clock::now();
    try {
        xenos::preparation::RunBounded<size_t>(200, 7, 2,
            [](size_t index) {
                std::this_thread::sleep_for(1ms);
                return index;
            },
            [&](size_t) {
                if (++consumerFailureCalls == 5) throw std::runtime_error("injected consumer failure");
                return true;
            }, [] {});
    } catch (const std::runtime_error& e) {
        consumerFailureObserved = std::string(e.what()) == "injected consumer failure";
    }
    const auto consumerFailureMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - consumerFailureStarted).count();
    Require(consumerFailureObserved && consumerFailureMs < 2000,
        "consumer failure did not wake and join blocked producers");

    const auto cache = output / "shader.cache";
    const std::array<uint8_t, 4> oldBytes{1, 2, 3, 4};
    const std::array<uint8_t, 5> newBytes{9, 8, 7, 6, 5};
    {
        std::ofstream out(cache, std::ios::binary);
        out.write(reinterpret_cast<const char*>(oldBytes.data()), oldBytes.size());
    }
    const auto skipped = xenos::preparation::WriteCompiledCache(cache, newBytes, false);
    Require(skipped.status == xenos::preparation::CacheWriteStatus::Skipped,
        "failed compilation was not skipped");
    Require(Read(cache) == std::vector<uint8_t>(oldBytes.begin(), oldBytes.end()),
        "failed compilation modified an existing cache file");
    const auto saved = xenos::preparation::WriteCompiledCache(cache, newBytes, true);
    Require(saved.status == xenos::preparation::CacheWriteStatus::Saved,
        "successful compilation was not persisted");
    Require(Read(cache) == std::vector<uint8_t>(newBytes.begin(), newBytes.end()),
        "persisted cache bytes differ from the compiled binary");
    const auto empty = xenos::preparation::WriteCompiledCache(cache, {}, true);
    Require(empty.status == xenos::preparation::CacheWriteStatus::Failed,
        "empty successful binary was accepted");
    Require(Read(cache) == std::vector<uint8_t>(newBytes.begin(), newBytes.end()),
        "rejected empty binary modified the valid cache file");

    std::ofstream summary(output / "summary.json", std::ios::binary);
    summary << "{\n"
        << "  \"result\": \"PASS\",\n"
        << "  \"jobs\": " << jobs << ",\n"
        << "  \"workers\": " << coverage.startedWorkers << ",\n"
        << "  \"queueCapacity\": 3,\n"
        << "  \"maxQueued\": " << coverage.maxQueued << ",\n"
        << "  \"serialFallbackJobs\": " << serialConsumed << ",\n"
        << "  \"partialFallbackWorkers\": " << partialFallback.startedWorkers << ",\n"
        << "  \"cancelAfterConsumeCalls\": " << cancellationCalls << ",\n"
        << "  \"cancelElapsedMs\": " << cancellationMs << ",\n"
        << "  \"workerFailureElapsedMs\": " << failureMs << ",\n"
        << "  \"consumerFailureElapsedMs\": " << consumerFailureMs << ",\n"
        << "  \"failedCompilationCacheUntouched\": true\n"
        << "}\n";
    summary.close();
    Require(bool(summary), "cannot write summary");

    std::printf("shader preparation queue: PASS (%zu jobs, %zu workers, max queue %zu)\n",
        jobs, coverage.startedWorkers, coverage.maxQueued);
    std::printf("thread start fallback: PASS (serial %zu jobs, partial %zu jobs)\n",
        serialConsumed, partialConsumed);
    std::printf("cancellation/failure wakeup: PASS (%lld/%lld/%lld ms)\n",
        static_cast<long long>(cancellationMs), static_cast<long long>(failureMs),
        static_cast<long long>(consumerFailureMs));
    std::printf("failed compilation persistence: PASS\n");
    return 0;
}
catch (const std::exception& e)
{
    std::fprintf(stderr, "shader preparation queue: FAIL: %s\n", e.what());
    return 1;
}
