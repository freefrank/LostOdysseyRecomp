#include "discord/client.h"
#include "discord/ipc_transport.h"

#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace
{
using Clock = std::chrono::steady_clock;
Clock::time_point started;
std::mutex outputMutex;
std::atomic<bool> ready{false};
std::atomic<bool> sent{false};
std::atomic<bool> accepted{false};
std::atomic<bool> cleared{false};

void Log(const char* message)
{
    const std::string_view value(message);
    if (value.find("READY received") != std::string_view::npos) ready.store(true);
    if (value.find("first activity sent") != std::string_view::npos) sent.store(true);
    if (value.find("activity accepted") != std::string_view::npos) accepted.store(true);
    if (value.find("activity clear sent") != std::string_view::npos) cleared.store(true);
    const auto elapsed = std::chrono::duration<double>(Clock::now() - started).count();
    std::lock_guard lock(outputMutex);
    std::printf("[%7.3f s] %s\n", elapsed, message);
    std::fflush(stdout);
}

bool Seconds(std::string_view text, int& result)
{
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && result >= 1 && result <= 90;
}

void Usage(const char* program)
{
    std::fprintf(stderr, "Usage: %s [seconds|--duration-seconds=1..90] [--wait-seconds=1..90] [--endpoint=path]\n"
        "Runs the production Discord client, publishes a test activity, then clears it.\n"
        "Defaults: 20 seconds visible, 40 seconds to await acknowledgment, public application ID and normal IPC endpoints.\n",
        program);
}
}

int main(int argc, char** argv)
{
    int durationSeconds = 20;
    int waitSeconds = 40;
    discord_rpc::ClientOptions options;
    options.endpoints = discord_rpc::IpcEndpoints();
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument(argv[index]);
        if (argument.starts_with("--endpoint=") && argument.size() > 11)
            options.endpoints = {std::string(argument.substr(11))};
        else if (argument.starts_with("--wait-seconds=") && Seconds(argument.substr(15), waitSeconds)) {}
        else if (argument.starts_with("--duration-seconds=") && Seconds(argument.substr(19), durationSeconds)) {}
        else if (!argument.starts_with("-") && Seconds(argument, durationSeconds)) {}
        else
        {
            Usage(argv[0]);
            return 2;
        }
    }

    started = Clock::now();
    std::printf("Application ID: %s; wait: %d s; active: %d s\n", discord_rpc::kApplicationId, waitSeconds, durationSeconds);
    std::fflush(stdout);
    discord_rpc::Client client;
    client.Start(std::move(options), Log);
    const discord_rpc::Snapshot snapshot{1, "Discord status test", discord_rpc::Mode::Exploring};
    const auto deadline = started + std::chrono::seconds(waitSeconds);
    auto nextPublish = started;
    while (!accepted.load() && Clock::now() < deadline)
    {
        if (Clock::now() >= nextPublish)
        {
            client.Publish(snapshot);
            nextPublish = Clock::now() + std::chrono::seconds(1);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (accepted.load())
    {
        const auto stopAt = Clock::now() + std::chrono::seconds(durationSeconds);
        while (Clock::now() < stopAt)
        {
            client.Publish(snapshot);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    client.Stop();
    const bool success = ready.load() && sent.load() && accepted.load() && cleared.load();
    std::printf("%s: READY=%d sent=%d accepted=%d clear_sent=%d\n", success ? "PASS" : "FAIL",
        int(ready.load()), int(sent.load()), int(accepted.load()), int(cleared.load()));
    return success ? 0 : 1;
}
