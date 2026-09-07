#pragma once
#include <chrono>
#include <cstdint>

namespace gpu
{
// Pure deadline calculation; caller owns sleeping and the instance's thread.
class FramePacer
{
public:
    using Clock = std::chrono::steady_clock;
    Clock::time_point Schedule(Clock::time_point now, uint32_t fps)
    {
        if (!fps) { m_fps = 0; m_next = now; return now; }
        const auto period = std::chrono::nanoseconds(1000000000ull / fps);
        const auto candidate = m_next + period;
        if (fps != m_fps || candidate > now + period)
            m_next = now + period;
        else
            // The current frame is already late: release it immediately and
            // start a fresh period here. Do not add another period of delay,
            // or retain an overdue anchor that allows catch-up bursts.
            m_next = candidate < now ? now : candidate;
        m_fps = fps;
        return m_next;
    }
private:
    uint32_t m_fps = 0;
    Clock::time_point m_next{};
};

// Only the known interval-2 path is changed. Preserve immediate, interval-1,
// interval-3, flags and every caller other than the identified present site.
constexpr uint32_t MapPresentInterval(uint32_t value, uint32_t caller,
    uint32_t fps, bool experimental120)
{
    if (caller != 0x827B4A4C || (value & 0xFF00u) != 0x200u || fps < 60)
        return value;
    return (value & ~0xFF00u) | ((fps == 120 && experimental120) ? 0u : 0x100u);
}
}
