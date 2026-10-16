#pragma once
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace gpu::loading {
// Published by the guest loader hooks. This state must never change a clock,
// engine delta, audio rate, or the player's saved frame-rate setting.
inline std::atomic<bool> inProgress{false};
#if defined(__ANDROID__) || defined(__SWITCH__)
inline std::atomic<bool> portable{true};
#else
inline std::atomic<bool> portable{false};
#endif
inline void SetLoading(bool value) { inProgress.store(value, std::memory_order_release); }
inline bool Enabled() {
    static const bool enabled = [] {
        const char* value = std::getenv("LO_FAST_LOADING");
        return !value || std::strcmp(value, "0") != 0;
    }();
    return enabled;
}
inline bool Active() { return Enabled() && inProgress.load(std::memory_order_acquire); }
constexpr uint32_t Target(uint32_t configured, bool loading, bool batteryDevice) {
    if (!loading) return configured;
    // Zero is uncapped. Battery devices keep a bounded CPU/GPU budget even
    // with LO_FPS=0; at 30/60 FPS they load at up to 120/240 FPS.
    return batteryDevice ? (configured ? std::min(configured, 60u) * 4 : 240u) : 0u;
}
inline uint32_t Target(uint32_t configured) {
    return Target(configured, Active(), portable.load(std::memory_order_relaxed));
}

// The guest still completes every swap. Only host presentation is decimated,
// before acquiring a swapchain image or waiting on its previous submission.
// This gate never sleeps and never accumulates catch-up presentation debt.
class PresentGate {
    uint64_t next_ = 0;
public:
    bool Accept(bool loading, uint64_t nowNs, uint32_t refreshHz) {
        if (!loading) { next_ = 0; return true; }
        if (nowNs < next_) return false;
        const auto hz = refreshHz >= 24 && refreshHz <= 1000 ? refreshHz : 60u;
        next_ = nowNs + (1000000000ull + hz - 1) / hz;
        return true;
    }
};
}
