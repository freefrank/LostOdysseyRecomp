#if !defined(__ANDROID__) && !defined(__SWITCH__) && !defined(__NX__)

#include "protocol.h"

#include "os/json.h"

#include <algorithm>
#include <charconv>

namespace discord_rpc
{
namespace
{
uint32_t Read32(const uint8_t* bytes)
{
    return uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8) |
        (uint32_t(bytes[2]) << 16) | (uint32_t(bytes[3]) << 24);
}

void Write32(std::vector<uint8_t>& bytes, uint32_t value)
{
    for (unsigned shift = 0; shift < 32; shift += 8)
        bytes.push_back(uint8_t(value >> shift));
}

std::string DisplayText(std::string_view text)
{
    // Discord limits details/state to 128 bytes. Keep whole UTF-8 characters.
    size_t length = std::min(text.size(), size_t(128));
    while (length < text.size() && length && (uint8_t(text[length]) & 0xc0) == 0x80)
        --length;
    std::string result(text.substr(0, length));
    std::erase_if(result, [](unsigned char c) { return c < 0x20 || c == 0x7f; });
    return result;
}

const char* ModeText(Mode mode)
{
    switch (mode)
    {
    case Mode::Battle: return "In battle";
    case Mode::Cutscene: return "Cutscene";
    case Mode::Movie: return "Watching a movie";
    case Mode::Menu: return "In the menu";
    case Mode::Title: return "At the title screen";
    default: return "Exploring";
    }
}
}

std::vector<uint8_t> EncodeFrame(Opcode opcode, std::string_view payload)
{
    if (payload.size() > kMaxPayload)
        return {};
    std::vector<uint8_t> bytes;
    bytes.reserve(8 + payload.size());
    Write32(bytes, uint32_t(opcode));
    Write32(bytes, uint32_t(payload.size()));
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    return bytes;
}

bool FrameDecoder::Append(std::span<const uint8_t> bytes)
{
    if (!valid_ || bytes_.size() + bytes.size() > 2 * (kMaxPayload + 8))
        return valid_ = false;
    bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
    if (bytes_.size() >= 8 && (Read32(bytes_.data()) > uint32_t(Opcode::Pong) ||
        Read32(bytes_.data() + 4) > kMaxPayload))
        return valid_ = false;
    return true;
}

std::optional<Frame> FrameDecoder::Pop()
{
    if (!valid_ || bytes_.size() < 8)
        return std::nullopt;
    const auto length = Read32(bytes_.data() + 4);
    if (length > kMaxPayload || Read32(bytes_.data()) > uint32_t(Opcode::Pong))
    {
        valid_ = false;
        return std::nullopt;
    }
    if (bytes_.size() < 8 + length)
        return std::nullopt;
    Frame result{Opcode(Read32(bytes_.data())), std::string(bytes_.begin() + 8, bytes_.begin() + 8 + length)};
    bytes_.erase(bytes_.begin(), bytes_.begin() + 8 + length);
    return result;
}

void FrameDecoder::Reset()
{
    bytes_.clear();
    valid_ = true;
}

std::string BuildHandshake(std::string_view applicationId)
{
    return nlohmann::json{{"v", 1}, {"client_id", applicationId}}.dump();
}

std::string BuildActivity(const Snapshot& snapshot, int64_t startTime, bool image)
{
    std::string state;
    if (snapshot.disc >= 1 && snapshot.disc <= 4)
        state = "Disc " + std::to_string(snapshot.disc) + " - ";
    state += ModeText(snapshot.mode);
    std::string details = snapshot.mode == Mode::Title ? "At the title screen" : DisplayText(snapshot.location);
    if (details.empty())
        details = "Lost Odyssey Recomp";
    nlohmann::json activity{{"type", 0}, {"details", details}, {"state", state},
        {"timestamps", {{"start", startTime}}}};
    if (image)
        activity["assets"] = {{"large_image", "logo"}, {"large_text", "Lost Odyssey Recomp"}};
    return activity.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

std::string BuildSetActivity(uint32_t pid, std::string_view activity, uint64_t nonce)
{
    return nlohmann::json{{"cmd", "SET_ACTIVITY"}, {"args", {{"pid", pid},
        {"activity", nlohmann::json::parse(activity)}}}, {"nonce", std::to_string(nonce)}}.dump();
}

std::string BuildClearActivity(uint32_t pid, uint64_t nonce)
{
    return nlohmann::json{{"cmd", "SET_ACTIVITY"}, {"args", {{"pid", pid}, {"activity", nullptr}}},
        {"nonce", std::to_string(nonce)}}.dump();
}

Response ParseResponse(std::string_view payload)
{
    const auto json = nlohmann::json::parse(payload, nullptr, false);
    if (!json.is_object())
        return {Reply::Invalid};
    Response result;
    const auto event = json.find("evt");
    if (event != json.end() && event->is_string())
    {
        const auto& value = event->get_ref<const std::string&>();
        if (value == "READY") result.kind = Reply::Ready;
        if (value == "ERROR") result.kind = Reply::Error;
    }
    const auto command = json.find("cmd");
    result.activity = command != json.end() && command->is_string() && *command == "SET_ACTIVITY";
    const auto nonce = json.find("nonce");
    if (nonce != json.end() && nonce->is_string())
    {
        const auto& value = nonce->get_ref<const std::string&>();
        std::from_chars(value.data(), value.data() + value.size(), result.nonce);
    }
    if (result.kind == Reply::Error)
    {
        const auto data = json.find("data");
        if (data != json.end() && data->is_object())
        {
            const auto message = data->find("message");
            if (message != data->end() && message->is_string())
            {
                auto text = message->get<std::string>();
                std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
                    return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : char(c);
                });
                result.invalidAsset = text.find("image") != std::string::npos ||
                    text.find("asset") != std::string::npos;
            }
        }
    }
    return result;
}

std::string DescribeError(std::string_view payload)
{
    const auto json = nlohmann::json::parse(payload, nullptr, false);
    if (!json.is_object()) return "invalid JSON";
    const auto data = json.find("data");
    const auto& fields = data != json.end() && data->is_object() ? *data : json;
    nlohmann::json safe = nlohmann::json::object();
    const auto code = fields.find("code");
    if (code != fields.end() && code->is_number_integer()) safe["code"] = *code;
    const auto message = fields.find("message");
    if (message != fields.end() && message->is_string())
        safe["message"] = DisplayText(message->get_ref<const std::string&>());
    return safe.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

UpdatePolicy::UpdatePolicy(std::chrono::milliseconds regular, std::chrono::milliseconds meaningful)
    : regular_(regular), meaningful_(meaningful) {}

bool UpdatePolicy::Due(const Snapshot& snapshot, Clock::time_point now) const
{
    if (!last_) return true;
    if (snapshot == *last_) return false;
    const bool meaningful = snapshot.disc != last_->disc || snapshot.mode != last_->mode;
    return now - sent_ >= (meaningful ? meaningful_ : regular_);
}

void UpdatePolicy::Sent(const Snapshot& snapshot, Clock::time_point now)
{
    last_ = snapshot;
    sent_ = now;
}

void UpdatePolicy::Reset() { last_.reset(); }
}

#endif
