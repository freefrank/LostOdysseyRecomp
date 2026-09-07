#pragma once
#include <cstdint>

namespace hid
{
// Opt-in test input only. Start on the next engine tick so a command received
// halfway through an update cannot appear on only some of that tick's reads.
// Zero-duration cancellation is intentionally immediate, including paused ticks.
struct TestInputPulse
{
    uint64_t start = 0;
    uint32_t duration = 0;
    void Set(uint64_t tick, uint32_t ticks)
    {
        start = tick + 1;
        duration = ticks;
    }
    bool Active(uint64_t tick) const
    {
        return duration && tick >= start && tick - start < duration;
    }
    bool Pending(uint64_t tick) const { return duration && tick < start; }
};
}
