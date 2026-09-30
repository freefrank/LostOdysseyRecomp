#pragma once

#include <chrono>
#include <cstdint>

namespace apu::xma
{
struct DecodeFailureReport
{
    uint64_t ordinal;
    uint64_t suppressed;
    bool emit;
    bool capture;
};

// ProcessPacket is serialized by the XMA worker's g_mutex. Keep one reporter
// for the worker, not one per reused context. Initial detailed reports and
// private packet captures retain the original process-wide limit of 16.
// Later warnings remain observable without allowing an unbounded log burst.
class DecodeFailureReports
{
public:
    using Clock = std::chrono::steady_clock;
    static constexpr uint64_t InitialReports = 16;
    static constexpr auto RepeatInterval = std::chrono::seconds(10);

    DecodeFailureReport Observe(Clock::time_point now) noexcept
    {
        const uint64_t ordinal = ++total_;
        const bool capture = ordinal <= InitialReports;
        const bool emit = capture || now - lastReport_ >= RepeatInterval;
        const uint64_t suppressed = emit ? suppressed_ : 0;
        if (emit)
        {
            lastReport_ = now;
            suppressed_ = 0;
        }
        else
        {
            ++suppressed_;
        }
        return {ordinal, suppressed, emit, capture};
    }

private:
    uint64_t total_ = 0;
    uint64_t suppressed_ = 0;
    Clock::time_point lastReport_{};
};

// receiveResult is the existing combined result and may equal sendResult
// when send failed. Do not describe that value as a receive result in logs.
constexpr const char* DecodeFailureStage(int sendResult, int receiveResult) noexcept
{
    return sendResult < 0 ? "send" : receiveResult < 0 ? "receive" : "frame-layout";
}
}
