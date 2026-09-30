#pragma once
#include "frame_rate.h"
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

// Pace application frames using the number of frames the FG SDK says it
// presented since the previous application frame. The SDK's dynamic FPS target
// is advisory, so the host must also account for generated output.
class OutputFramePacer
{
public:
    using Clock = FramePacer::Clock;
    Clock::time_point Schedule(Clock::time_point now, uint32_t outputFps, uint64_t actualPresents)
    {
        if (!outputFps || !actualPresents)
        {
            m_fps = 0;
            m_presented = 0;
            m_next = now;
            return now;
        }
        if (outputFps != m_fps || actualPresents < m_presented || !m_presented)
        {
            m_fps = outputFps;
            m_presented = actualPresents;
            m_next = now;
            return now;
        }
        const auto delta = actualPresents - m_presented;
        m_presented = actualPresents;
        if (!delta) return now;
        // A skipped SDK query can combine several presents. A long stall is a
        // fresh cadence, not a debt that should freeze gameplay on recovery.
        if (delta > 16 || now > m_next + std::chrono::seconds(1))
        {
            m_next = now;
            return now;
        }
        m_next += std::chrono::nanoseconds(1000000000ull / outputFps) * int64_t(delta);
        if (m_next < now) m_next = now;
        return m_next;
    }
private:
    uint32_t m_fps = 0;
    uint64_t m_presented = 0;
    Clock::time_point m_next{};
};

// Only the known interval-2 path is changed. Preserve immediate, interval-1,
// interval-3, flags and every caller other than the identified present site.
// The virtual display stays at 60 Hz. Above 60 FPS, wait on the host's native
// frame deadline instead of an integer number of guest vblanks (90 cannot be
// represented that way). Zero is the existing LO_FPS uncapped diagnostic.
// Do not scale the PPC timebase, engine delta, audio or virtual vblank clocks.
constexpr uint32_t MapPresentInterval(uint32_t value, uint32_t caller, uint32_t fps, bool vrrRequested = false)
{
    if (caller != 0x827B4A4C || (value & 0xFF00u) != 0x200u)
        return value;
    // VRR owns its deadline even below 60; integer guest vblanks would quantize
    // a 57 FPS ceiling. Other callers/intervals and every clock are unchanged.
    if (vrrRequested) return value & ~0xFF00u;
    if (fps == frame_rate::kGuestRefreshHz)
        return (value & ~0xFF00u) | 0x100u;
    if (frame_rate::NeedsImmediate(fps))
        return value & ~0xFF00u;
    return value;
}
}
