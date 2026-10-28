#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <mutex>

// One continuous guest clock for mftb and KeTimeStampBundle. Host rendering,
// UI deadlines and I/O waits keep their own unscaled clocks. The audio driver
// follows AudioRate(): it mixes that many guest frames per output frame.
namespace debug_menu::fast_forward {
inline constexpr unsigned Rates[] = {2, 3, 4, 6, 8};
inline constexpr uint64_t InputLeaseNs = 250000000;
enum class Mode { Hold, Toggle };
enum class Button { LT = 0, LB = 1 };
struct Buttons { uint8_t lt = 0, rt = 0, lb = 0, rb = 0; };
struct ButtonInput { uint8_t button, pair; };
inline ButtonInput SelectButton(Button button, const Buttons& buttons) {
    switch (button) {
    case Button::LB: return {buttons.lb, buttons.rb};
    default: return {buttons.lt, buttons.rt};
    }
}
inline uint64_t NowNs() {
    return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
// The guest clock (mftb, KeTimeStampBundle) starts here, like a console a
// minute after power-on, instead of at the host's steady_clock value. The game
// converts the timebase to seconds and keeps them as 32-bit floats (the
// rendering thread's loading-screen tick in RenderingThreadMain 0x824856A0,
// for one). libnx's CLOCK_MONOTONIC is Unix time (~1.8e9 s), where a float
// step is 128 s: the 1/30 s tick interval vanished, so for 64 s of every 128
// the rendering thread drew a loading-screen frame before every queued
// command (loads of 20-67 s), and for the other 64 s none at all. Seen on the
// Switch on 2026-10-14; a PC up for more than a few days loses precision too.
inline constexpr uint64_t GuestClockStartNs = 60ull * 1000000000ull;
struct Clock {
    uint64_t last = 0, value = 0, lease = 0;
    uint64_t origin = 0; // host steady_clock value at GuestClockStartNs
    unsigned rate = 1;
    bool initialized = false, paused = false;
    void Add(uint64_t elapsed, unsigned factor) {
        const auto room = std::numeric_limits<uint64_t>::max() - value;
        value += elapsed > room / factor ? room : elapsed * factor;
    }
    uint64_t Read(uint64_t now) {
        if (!initialized) { initialized = true; last = origin = now; value = GuestClockStartNs; }
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
struct Status { bool enabled = false, active = false; unsigned multiplier = 2; Mode mode = Mode::Hold; bool keepAudioPitch = true; Button button = Button::LT; };
struct Control {
    Clock clock;
    bool enabled = false, armed = false, held = false;
    unsigned multiplier = 2;
    Mode mode = Mode::Hold;
    bool keepAudioPitch = true;
    Button button = Button::LT;
    void Stop(uint64_t now) {
        clock.Read(now); clock.rate = 1; clock.lease = now;
        held = armed = false;
    }
    void Enable(bool value, uint64_t now) { Stop(now); enabled = value; }
    void SetMode(Mode value, uint64_t now) { Stop(now); mode = value; }
    void SetButton(Button value, uint64_t now) {
        Stop(now); button = value >= Button::LT && value <= Button::LB ? value : Button::LT;
    }
    void SetRate(unsigned value, uint64_t now) {
        if (std::find(std::begin(Rates), std::end(Rates), value) == std::end(Rates)) return;
        clock.Read(now); multiplier = value;
        if (clock.rate > 1) clock.rate = multiplier;
    }
    void Pause(bool value, uint64_t now) {
        Stop(now); clock.paused = value;
    }
    void Sample(uint8_t button, uint8_t pair, bool allowed, uint64_t now) {
        clock.Read(now);
        if (!allowed || clock.paused || !enabled) { Stop(now); return; }
        // Require a release after focus/menu/enable/binding changes. Hysteresis
        // prevents trigger noise; paired buttons remain available for shortcuts.
        if (button <= 32) {
            if (mode == Mode::Hold) held = false;
            armed = true;
        } else if (button >= 64 && armed) {
            if (mode == Mode::Hold) held = true;
            else if (pair <= 32) held = !held;
            armed = false;
        }
        clock.rate = held && pair <= 32 ? multiplier : 1;
        clock.lease = now > std::numeric_limits<uint64_t>::max() - InputLeaseNs
            ? std::numeric_limits<uint64_t>::max() : now + InputLeaseNs;
    }
    Status Get(uint64_t now) {
        clock.Read(now);
        return {enabled, clock.rate > 1 && !clock.paused, multiplier, mode, keepAudioPitch, button};
    }
    unsigned ActiveRate() const { return clock.rate > 1 && !clock.paused ? clock.rate : 1; }
};
inline std::mutex mutex;
inline Control control;
// The clock's current rate for the audio driver, read without the lock.
inline std::atomic<unsigned> audioRate{1};
inline std::atomic<bool> audioKeepPitch{true};
// Caller holds mutex.
inline void PublishRate() {
    const unsigned rate = control.ActiveRate();
    if (audioRate.load(std::memory_order_relaxed) != rate) audioRate.store(rate, std::memory_order_relaxed);
}
inline unsigned AudioRate() { return audioRate.load(std::memory_order_relaxed); }
inline bool KeepAudioPitch() { return audioKeepPitch.load(std::memory_order_relaxed); }
inline uint64_t GameTimeNs() {
    std::lock_guard lock(mutex);
    const uint64_t value = control.clock.Read(NowNs());
    PublishRate();
    return value;
}
// Host steady_clock minus guest clock, without paused or fast-forwarded time.
inline int64_t GuestClockShiftNs() {
    std::lock_guard lock(mutex);
    control.clock.Read(NowNs());
    return int64_t(control.clock.origin) - int64_t(GuestClockStartNs);
}
inline Status GetStatus() { std::lock_guard lock(mutex); const Status status = control.Get(NowNs()); PublishRate(); return status; }
inline void Enable(bool enabled) { std::lock_guard lock(mutex); control.Enable(enabled, NowNs()); PublishRate(); }
inline void SetMode(Mode mode) { std::lock_guard lock(mutex); control.SetMode(mode, NowNs()); PublishRate(); }
inline void SetButton(Button button) { std::lock_guard lock(mutex); control.SetButton(button, NowNs()); PublishRate(); }
inline void SetRate(unsigned rate) { std::lock_guard lock(mutex); control.SetRate(rate, NowNs()); PublishRate(); }
inline void SetKeepAudioPitch(bool keep) {
    std::lock_guard lock(mutex);
    control.keepAudioPitch = keep;
    audioKeepPitch.store(keep, std::memory_order_relaxed);
}
inline void SetPaused(bool paused) { std::lock_guard lock(mutex); control.Pause(paused, NowNs()); PublishRate(); }
inline void Release() { std::lock_guard lock(mutex); control.Stop(NowNs()); PublishRate(); }
inline void Sample(uint8_t button, uint8_t pair, bool allowed) {
    std::lock_guard lock(mutex); control.Sample(button, pair, allowed, NowNs()); PublishRate();
}
} // namespace debug_menu::fast_forward
