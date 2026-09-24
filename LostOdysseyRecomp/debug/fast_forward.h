#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>
#include <mutex>

// One continuous guest clock for mftb and KeTimeStampBundle. Host rendering,
// UI deadlines and I/O waits keep their own unscaled clocks.
namespace debug_menu::fast_forward {
inline constexpr unsigned Rates[] = {2, 3, 4, 6, 8};
inline constexpr uint64_t InputLeaseNs = 250000000;
inline uint64_t NowNs() {
    return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
struct Clock {
    uint64_t last = 0, value = 0, lease = 0;
    unsigned rate = 1;
    bool initialized = false, paused = false;
    void Add(uint64_t elapsed, unsigned factor) {
        const auto room = std::numeric_limits<uint64_t>::max() - value;
        value += elapsed > room / factor ? room : elapsed * factor;
    }
    uint64_t Read(uint64_t now) {
        if (!initialized) { initialized = true; last = value = now; }
        if (now < last) return value;
        if (!paused) {
            const auto boostedEnd = std::clamp(lease, last, now);
            Add(boostedEnd - last, rate);
            Add(now - boostedEnd, 1);
        }
        last = now;
        if (now >= lease) rate = 1;
        return value;
    }
};
struct Status { bool enabled = false, active = false; unsigned multiplier = 2; };
struct Control {
    Clock clock;
    bool enabled = false, armed = false, held = false;
    unsigned multiplier = 2;
    void Stop(uint64_t now) {
        clock.Read(now); clock.rate = 1; clock.lease = now;
        held = armed = false;
    }
    void Enable(bool value, uint64_t now) { Stop(now); enabled = value; }
    void SetRate(unsigned value, uint64_t now) {
        if (std::find(std::begin(Rates), std::end(Rates), value) == std::end(Rates)) return;
        clock.Read(now); multiplier = value;
        if (clock.rate > 1) clock.rate = multiplier;
    }
    void Pause(bool value, uint64_t now) {
        Stop(now); clock.paused = value;
    }
    void Sample(uint8_t lt, uint8_t rt, bool allowed, uint64_t now) {
        clock.Read(now);
        if (!allowed || clock.paused || !enabled) { Stop(now); return; }
        // Require a release after focus/menu/enable changes. Hysteresis prevents
        // trigger noise; LT+RT remains available to the retail editor.
        if (lt <= 32) { held = false; armed = true; }
        else if (lt >= 64 && armed) held = true;
        clock.rate = held && rt <= 32 ? multiplier : 1;
        clock.lease = now > std::numeric_limits<uint64_t>::max() - InputLeaseNs
            ? std::numeric_limits<uint64_t>::max() : now + InputLeaseNs;
    }
    Status Get(uint64_t now) {
        clock.Read(now);
        return {enabled, clock.rate > 1 && !clock.paused, multiplier};
    }
};
inline std::mutex mutex;
inline Control control;
inline uint64_t GameTimeNs() { std::lock_guard lock(mutex); return control.clock.Read(NowNs()); }
inline Status GetStatus() { std::lock_guard lock(mutex); return control.Get(NowNs()); }
inline void Enable(bool enabled) { std::lock_guard lock(mutex); control.Enable(enabled, NowNs()); }
inline void SetRate(unsigned rate) { std::lock_guard lock(mutex); control.SetRate(rate, NowNs()); }
inline void SetPaused(bool paused) { std::lock_guard lock(mutex); control.Pause(paused, NowNs()); }
inline void Release() { std::lock_guard lock(mutex); control.Stop(NowNs()); }
inline void Sample(uint8_t lt, uint8_t rt, bool allowed) {
    std::lock_guard lock(mutex); control.Sample(lt, rt, allowed, NowNs());
}
} // namespace debug_menu::fast_forward
