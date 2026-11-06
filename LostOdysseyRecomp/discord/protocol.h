#pragma once

#include "presence.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace discord_rpc
{
enum class Opcode : uint32_t { Handshake = 0, Frame = 1, Close = 2, Ping = 3, Pong = 4 };
inline constexpr size_t kMaxPayload = 64 * 1024;

struct Frame
{
    Opcode opcode;
    std::string payload;
};

std::vector<uint8_t> EncodeFrame(Opcode opcode, std::string_view payload);

class FrameDecoder
{
public:
    bool Append(std::span<const uint8_t> bytes);
    std::optional<Frame> Pop();
    void Reset();
    bool IsValid() const { return valid_; }

private:
    std::vector<uint8_t> bytes_;
    bool valid_ = true;
};

std::string BuildHandshake(std::string_view applicationId);
std::string BuildActivity(const Snapshot& snapshot, int64_t startTime, bool image = true);
std::string BuildSetActivity(uint32_t pid, std::string_view activity, uint64_t nonce);
std::string BuildClearActivity(uint32_t pid, uint64_t nonce);

enum class Reply { Other, Ready, Error, Invalid };
struct Response
{
    Reply kind = Reply::Other;
    bool activity = false;
    bool invalidAsset = false;
    uint64_t nonce = 0;
};
// Reads only protocol metadata; user data in READY is neither retained nor logged.
Response ParseResponse(std::string_view payload);
// Log only bounded protocol error fields, never READY user data.
std::string DescribeError(std::string_view payload);

class UpdatePolicy
{
public:
    using Clock = std::chrono::steady_clock;
    UpdatePolicy(std::chrono::milliseconds regular = std::chrono::seconds(15),
        std::chrono::milliseconds meaningful = std::chrono::seconds(5));
    bool Due(const Snapshot& snapshot, Clock::time_point now) const;
    void Sent(const Snapshot& snapshot, Clock::time_point now);
    void Reset();

private:
    std::chrono::milliseconds regular_;
    std::chrono::milliseconds meaningful_;
    std::optional<Snapshot> last_;
    Clock::time_point sent_{};
};
}
