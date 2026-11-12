#if !defined(__ANDROID__) && !defined(__SWITCH__) && !defined(__NX__)

#include "client.h"
#include "ipc_transport.h"
#include "protocol.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <map>
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
    LogFailure(log_, options_.dryRun ? "client started (dry run)" :
        (Enabled() ? "client started (enabled)" : "client started (disabled)"));
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
    // Repeated absence, rejections, or reconnects must not fill player logs.
    // The worker owns this bounded cache; the engine never pays for diagnostics.
    std::map<std::string, UpdatePolicy::Clock::time_point> logged;
    auto notice = [&](const std::string& key, const std::string& message) {
        const auto now = UpdatePolicy::Clock::now();
        const auto previous = logged.find(key);
        if (previous != logged.end() && now - previous->second < options_.diagnosticInterval) return;
        if (previous == logged.end() && logged.size() >= 64) logged.erase(logged.begin());
        logged[key] = now;
        LogFailure(log_, message.c_str());
    };
    bool receivedSnapshot = false;
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
        auto connectResult = [&](const std::string& endpoint, uint32_t error) {
            const auto key = error ? "connect-error-" + std::to_string(error) : "connected";
            notice(key, std::string(error ? "connect failed" : "connected") + " endpoint=" + endpoint +
                " error=" + std::to_string(error));
        };
        if (!options_.dryRun && !ValidApplicationId(options_.applicationId))
        {
            notice("invalid-id", "connect skipped: invalid application ID");
            if (!WaitToRetry()) break;
            continue;
        }
        if (!options_.dryRun && !transport.Connect(options_.endpoints, connectResult))
        {
            if (!WaitToRetry()) break;
            continue;
        }

        auto send = [&](Opcode opcode, const std::string& payload) {
            return transport.Write(EncodeFrame(opcode, payload), options_.ioTimeout);
        };
        if (!options_.dryRun && !send(Opcode::Handshake, BuildHandshake(options_.applicationId)))
        {
            notice("handshake-write", "disconnected reason=handshake write failed error=" + std::to_string(transport.LastError()));
            transport.Close();
            if (!WaitToRetry()) break;
            continue;
        }
        if (!options_.dryRun) notice("handshake", "handshake sent");
        const auto handshakeDeadline = UpdatePolicy::Clock::now() + options_.handshakeTimeout;
        bool ready = options_.dryRun;
        bool image = true;
        bool sentActivity = false;
        bool acceptedActivity = false;
        uint64_t activityNonce = 0;
        UpdatePolicy policy(options_.regularUpdate, options_.meaningfulUpdate);
        FrameDecoder decoder;
        std::array<uint8_t, 4096> incoming{};
        bool healthy = true;
        std::string disconnectReason;
        while (Active() && healthy)
        {
            std::optional<Snapshot> snapshot;
            {
                std::lock_guard lock(mutex_);
                if (hasSnapshot_) snapshot = latest_;
            }
            if (snapshot && !receivedSnapshot)
            {
                notice("snapshot", "first game snapshot received");
                receivedSnapshot = true;
            }
            const auto now = UpdatePolicy::Clock::now();
            if (ready && snapshot && policy.Due(*snapshot, now))
            {
                const auto activity = BuildActivity(*snapshot, startTime_, image);
                activityNonce = ++nonce;
                const auto command = BuildSetActivity(pid, activity, activityNonce);
                if (options_.dryRun) LogActivity(log_, command);
                else if (!send(Opcode::Frame, command))
                {
                    disconnectReason = "activity write failed error=" + std::to_string(transport.LastError());
                    break;
                }
                if (!sentActivity && !options_.dryRun) notice("activity-sent", "first activity sent");
                policy.Sent(*snapshot, now);
                sentActivity = true;
            }
            if (options_.dryRun)
            {
                std::unique_lock lock(wakeMutex_);
                changed_.wait_for(lock, options_.ioTimeout, [this] { return !Active(); });
                continue;
            }
            if (!ready && now >= handshakeDeadline) { disconnectReason = "READY timeout"; break; }
            if (ready && !snapshot && now >= handshakeDeadline)
                notice("no-snapshot", "READY but waiting for a game snapshot");
            const auto count = transport.Read(incoming, options_.ioTimeout);
            if (count < 0) { disconnectReason = "pipe read failed or closed error=" + std::to_string(transport.LastError()); break; }
            if (count > 0 && !decoder.Append(std::span(incoming.data(), size_t(count))))
            { disconnectReason = "invalid frame"; break; }
            while (auto frame = decoder.Pop())
            {
                if (frame->opcode == Opcode::Close)
                {
                    disconnectReason = "Discord CLOSE " + DescribeError(frame->payload);
                    healthy = false;
                    break;
                }
                if (frame->opcode == Opcode::Ping)
                {
                    healthy = send(Opcode::Pong, frame->payload);
                    if (!healthy) disconnectReason = "PONG write failed error=" + std::to_string(transport.LastError());
                    if (!healthy) break;
                }
                else if (frame->opcode == Opcode::Frame)
                {
                    const auto response = ParseResponse(frame->payload);
                    if (response.kind == Reply::Invalid) { disconnectReason = "invalid response JSON"; healthy = false; break; }
                    if (response.kind == Reply::Ready && !ready)
                    {
                        ready = true;
                        notice("ready", "READY received");
                        policy.Reset();
                    }
                    if (response.kind == Reply::Error)
                    {
                        const auto error = DescribeError(frame->payload);
                        notice("error-" + error, "Discord ERROR " + error);
                        // A missing uploaded logo must not suppress text presence.
                        if (ready && response.activity && image && response.invalidAsset && response.nonce == activityNonce)
                        {
                            image = false;
                            policy.Reset();
                        }
                        else { disconnectReason = "Discord rejected request " + error; healthy = false; break; }
                    }
                    else if (ready && sentActivity && !acceptedActivity && response.activity && response.nonce == activityNonce)
                    {
                        acceptedActivity = true;
                        notice("activity-accepted", "activity accepted");
                    }
                }
                else if (frame->opcode != Opcode::Pong)
                {
                    disconnectReason = "unexpected opcode";
                    healthy = false;
                    break;
                }
            }
            if (!decoder.IsValid()) { disconnectReason = "invalid frame"; healthy = false; }
        }

        // Best effort, bounded, and entirely off the engine thread.
        if (ready && sentActivity && !Active())
        {
            const auto clear = BuildClearActivity(pid, ++nonce);
            if (options_.dryRun) LogActivity(log_, clear);
            else if (transport.IsOpen())
            {
                if (send(Opcode::Frame, clear)) notice("clear", "activity clear sent");
                else notice("clear-failed", "activity clear failed error=" + std::to_string(transport.LastError()));
            }
        }
        if (disconnectReason.empty()) disconnectReason = stopping_.load(std::memory_order_relaxed) ? "shutdown" : "setting disabled";
        notice("disconnect-" + disconnectReason, "disconnected reason=" + disconnectReason);
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
