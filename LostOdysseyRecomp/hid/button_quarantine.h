#pragma once

#include <atomic>
#include <cstdint>

namespace hid
{
// The host input dispatcher owns ObserveRelease/Consume. Guest readers never
// retire bits: a stale guest sample must not overwrite a newer close action.
// A generation also invalidates samples spanning a complete close/release cycle.
class ButtonQuarantine
{
    static constexpr uint64_t kButtons = 0xFFFF;
    static constexpr uint64_t kGeneration = uint64_t(1) << 16;
    std::atomic<uint64_t> state_{0};

public:
    using Snapshot = uint64_t;

    Snapshot Capture() const { return state_.load(std::memory_order_acquire); }

    void ObserveRelease(uint16_t physicallyHeld)
    {
        state_.fetch_and(~kButtons | uint64_t(physicallyHeld), std::memory_order_acq_rel);
    }

    // Must happen BEFORE any callback which hides the overlay or resumes guests.
    void Consume(uint16_t buttons)
    {
        auto old = state_.load(std::memory_order_relaxed);
        for (;;)
        {
            const auto next = ((old & ~kButtons) + kGeneration) | ((old & kButtons) | buttons);
            if (state_.compare_exchange_weak(old, next, std::memory_order_release,
                                            std::memory_order_relaxed)) return;
        }
    }

    // Capture before sampling the device. Keep both masks so a release during
    // sampling cannot turn an already-consumed held button into a fresh press.
    uint16_t Filter(uint16_t buttons, Snapshot beforeSample) const
    {
        const auto now = Capture();
        if ((beforeSample & ~kButtons) != (now & ~kButtons)) return 0;
        return uint16_t(buttons & ~uint16_t(beforeSample | now));
    }
};
}
