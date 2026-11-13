#pragma once
#include "aspect_ratio.h"
#include <atomic>
#include <cstdint>

// Runtime-only override. Never write the phone's settings.ini when docking.
namespace gpu::android_tv {
struct State { uint32_t height = 0, generation = 0; };
inline std::atomic<uint64_t> state{0};
inline State Snapshot() {
    const uint64_t value = state.load(std::memory_order_acquire);
    return {uint32_t(value), uint32_t(value >> 32)};
}
inline void SetOutput(bool external, int height) {
    const uint32_t next = external && (height == 720 || height == 1080) ? uint32_t(height) : 0;
    auto previous = state.load(std::memory_order_relaxed);
    while (uint32_t(previous) != next && !state.compare_exchange_weak(previous,
        ((uint64_t(uint32_t(previous >> 32) + 1)) << 32) | next,
        std::memory_order_release, std::memory_order_relaxed)) {}
}
inline aspect_ratio::Mode Aspect(State value, aspect_ratio::Mode configured) {
    return value.height ? aspect_ratio::Mode::Wide : configured;
}
} // namespace gpu::android_tv
