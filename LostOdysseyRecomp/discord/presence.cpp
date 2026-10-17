#if !defined(__ANDROID__) && !defined(__SWITCH__) && !defined(__NX__)

#include "client.h"
#include "ipc_transport.h"
#include "protocol.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <utility>

namespace discord_rpc
{
namespace
{
Client client;

void LogActivity(LogCallback callback, const std::string& activity)
{
    if (callback) callback(activity.c_str());
    else std::fprintf(stderr, "[discord] %s\n", activity.c_str());
}

void LogFailure(LogCallback callback, const char* message) noexcept
{
    try
    {
        if (callback) callback(message);
        else std::fprintf(stderr, "[discord] %s\n", message);
    }
    catch (...) { /* A logging failure must not escape the optional worker. */ }
}

bool ValidApplicationId(const std::string& value)
{
    if (value.size() < 17 || value.size() > 20) return false;
    for (const auto c : value)
        if (c < '0' || c > '9') return false;
    return true;
}
}

Client::~Client() { Stop(); }

void Client::Start(ClientOptions options, LogCallback log)
{
    std::lock_guard lifecycleLock(lifecycleMutex_);
    if (worker_.joinable()) return;
    options_ = std::move(options);
    log_ = log;
    stopping_.store(false, std::memory_order_relaxed);
    // Kept across reconnects and On/Off toggles for the entire runtime session.
    if (!startTime_)
        startTime_ = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    try
    {
        worker_ = std::thread([this] {
            try { Run(); }
            catch (...) { LogFailure(log_, "Discord status stopped after an unexpected error."); }
        });
    }
    catch (...) { LogFailure(log_, "Discord status could not start its background thread."); }
}

void Client::Stop()
{
    std::lock_guard lifecycleLock(lifecycleMutex_);
    {
        std::lock_guard wakeLock(wakeMutex_);
        stopping_.store(true, std::memory_order_relaxed);
    }
    changed_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void Client::SetEnabled(bool enabled)
{
    {
        // Pair predicate changes with the wait mutex to prevent a lost wakeup.
        // This mutex is never held during snapshot copies, logging, or IPC.
        std::lock_guard wakeLock(wakeMutex_);
        enabled_.store(enabled, std::memory_order_relaxed);
    }
    changed_.notify_all();
}

void Client::Publish(const Snapshot& snapshot)
{
    std::unique_lock lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock()) return;
    latest_ = snapshot;
    hasSnapshot_ = true;
    // Do not wake a sleeping absent/disabled client for game-state updates.
}

bool Client::Active() const
{
    return enabled_.load(std::memory_order_relaxed) && !stopping_.load(std::memory_order_relaxed);
}

bool Client::WaitToRetry()
{
    std::unique_lock lock(wakeMutex_);
    changed_.wait_for(lock, options_.reconnect, [this] { return !Active(); });
    return !stopping_.load(std::memory_order_relaxed);
}

void Client::Run()
{
    uint64_t nonce = 0;
    const auto pid = ProcessId();
    while (!stopping_.load(std::memory_order_relaxed))
    {
        {
            std::unique_lock lock(wakeMutex_);
            changed_.wait(lock, [this] {
                return stopping_.load(std::memory_order_relaxed) || enabled_.load(std::memory_order_relaxed);
            });
        }
        if (!Active()) continue;
        IpcTransport transport;
        if (!options_.dryRun && (!ValidApplicationId(options_.applicationId) || !transport.Connect(options_.endpoints)))
        {
            if (!WaitToRetry()) break;
            continue;
        }

        auto send = [&](Opcode opcode, const std::string& payload) {
            return transport.Write(EncodeFrame(opcode, payload), options_.ioTimeout);
        };
        if (!options_.dryRun && !send(Opcode::Handshake, BuildHandshake(options_.applicationId)))
        {
            transport.Close();
            if (!WaitToRetry()) break;
            continue;
        }
        const auto handshakeDeadline = UpdatePolicy::Clock::now() + options_.handshakeTimeout;
        bool ready = options_.dryRun;
        bool image = true;
        bool sentActivity = false;
        uint64_t activityNonce = 0;
        UpdatePolicy policy(options_.regularUpdate, options_.meaningfulUpdate);
        FrameDecoder decoder;
        std::array<uint8_t, 4096> incoming{};
        bool healthy = true;
        while (Active() && healthy)
        {
            std::optional<Snapshot> snapshot;
            {
                std::lock_guard lock(mutex_);
                if (hasSnapshot_) snapshot = latest_;
            }
            const auto now = UpdatePolicy::Clock::now();
            if (ready && snapshot && policy.Due(*snapshot, now))
            {
                const auto activity = BuildActivity(*snapshot, startTime_, image);
                activityNonce = ++nonce;
                const auto command = BuildSetActivity(pid, activity, activityNonce);
                if (options_.dryRun) LogActivity(log_, command);
                else if (!send(Opcode::Frame, command)) break;
                policy.Sent(*snapshot, now);
                sentActivity = true;
            }
            if (options_.dryRun)
            {
                std::unique_lock lock(wakeMutex_);
                changed_.wait_for(lock, options_.ioTimeout, [this] { return !Active(); });
                continue;
            }
            if (!ready && now >= handshakeDeadline) break;
            const auto count = transport.Read(incoming, options_.ioTimeout);
            if (count < 0 || (count > 0 && !decoder.Append(std::span(incoming.data(), size_t(count)))))
                break;
            while (auto frame = decoder.Pop())
            {
                if (frame->opcode == Opcode::Close)
                {
                    healthy = false;
                    break;
                }
                if (frame->opcode == Opcode::Ping)
                {
                    healthy = send(Opcode::Pong, frame->payload);
                    if (!healthy) break;
                }
                else if (frame->opcode == Opcode::Frame)
                {
                    const auto response = ParseResponse(frame->payload);
                    if (response.kind == Reply::Invalid) { healthy = false; break; }
                    if (response.kind == Reply::Ready && !ready)
                    {
                        ready = true;
                        policy.Reset();
                    }
                    if (response.kind == Reply::Error)
                    {
                        // A missing uploaded logo must not suppress text presence.
                        if (ready && response.activity && image && response.invalidAsset && response.nonce == activityNonce)
                        {
                            image = false;
                            policy.Reset();
                        }
                        else { healthy = false; break; }
                    }
                }
                else if (frame->opcode != Opcode::Pong)
                {
                    healthy = false;
                    break;
                }
            }
            if (!decoder.IsValid()) healthy = false;
        }

        // Best effort, bounded, and entirely off the engine thread.
        if (ready && sentActivity && !Active())
        {
            const auto clear = BuildClearActivity(pid, ++nonce);
            if (options_.dryRun) LogActivity(log_, clear);
            else if (transport.IsOpen()) send(Opcode::Frame, clear);
        }
        transport.Close();
        if (Active() && !WaitToRetry()) break;
    }
}

void Initialize(LogCallback log)
{
    ClientOptions options;
    if (const auto* value = std::getenv("LO_DISCORD_APP_ID"); value && *value)
        options.applicationId = value;
    if (const auto* value = std::getenv("LO_DISCORD_DRY_RUN"))
        options.dryRun = std::string_view(value) == "1";
    options.endpoints = IpcEndpoints();
    client.Start(std::move(options), log);
}

void Shutdown() { client.Stop(); }
void SetEnabled(bool enabled) { client.SetEnabled(enabled); }
bool Enabled() noexcept { return client.Enabled(); }
void Publish(const Snapshot& snapshot) { client.Publish(snapshot); }
}

#endif
