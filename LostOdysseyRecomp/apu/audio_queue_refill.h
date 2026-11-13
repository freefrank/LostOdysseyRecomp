#pragma once
#include <cstdint>

namespace apu::detail
{
// Recover the existing queue reserve after a late wakeup. A clock reset alone
// discards overdue deadlines but leaves only one frame ahead of the device.
// Never call the guest on SDL's real-time callback and never grow the queue cap.
// Stop on no progress (unregistered client, failed device, or slow producer).
template<class Queued, class CanRefill, class Dispatch>
uint32_t RefillAudioQueue(uint32_t target, Queued queued, CanRefill canRefill, Dispatch dispatch)
{
    uint32_t refills = 0;
    for (uint32_t budget = 1; budget < target && canRefill(); ++budget)
    {
        const uint32_t before = queued();
        if (before >= target) break;
        dispatch();
        ++refills;
        if (queued() <= before) break;
    }
    return refills;
}
}
