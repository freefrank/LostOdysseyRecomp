#pragma once

#include "presence.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace discord_rpc
{
// Public application ID for Lost Odyssey Recomp; LO_DISCORD_APP_ID overrides it.
inline constexpr char kApplicationId[] = "1560871080275742851";

// Internal options also let CPU tests use a private fake IPC endpoint and clock scale.
struct ClientOptions
{
    std::string applicationId = kApplicationId;
    std::vector<std::string> endpoints;
    std::chrono::milliseconds reconnect{30000};
    std::chrono::milliseconds regularUpdate{15000};
    std::chrono::milliseconds meaningfulUpdate{5000};
    std::chrono::milliseconds handshakeTimeout{5000};
    std::chrono::milliseconds ioTimeout{200};
    std::chrono::milliseconds diagnosticInterval{300000};
    bool dryRun = false;
};

class Client
{
public:
    Client() = default;
    ~Client();
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    void Start(ClientOptions options, LogCallback log = nullptr);
    void Stop();
    void SetEnabled(bool enabled);
    bool Enabled() const noexcept { return enabled_.load(std::memory_order_relaxed); }
    void Publish(const Snapshot& snapshot);

private:
    void Run();
    bool WaitToRetry();
    bool Active() const;

    ClientOptions options_;
    LogCallback log_ = nullptr;
    int64_t startTime_ = 0;
    std::thread worker_;
    std::mutex lifecycleMutex_;
    std::mutex mutex_;
    std::mutex wakeMutex_;
    std::condition_variable changed_;
    Snapshot latest_;
    bool hasSnapshot_ = false;
    std::atomic<bool> enabled_{true};
    std::atomic<bool> stopping_{false};
};
}
