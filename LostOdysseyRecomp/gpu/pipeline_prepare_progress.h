#pragma once

#include <atomic>
#include <cstdint>

// Startup pipeline preparation that runs in the background: the renderer publishes
// its progress once a frame and presentation draws a bar at the bottom of the screen.
namespace gpu::pipeline_prepare
{
    // done << 32 | total; total 0 when nothing is being prepared.
    inline std::atomic<uint64_t> g_progress{0};
    // Set before renderer initialization when startup must finish preparing before
    // it returns (--prepare-shaders-only); LO_PIPELINE_PREPARE_SYNC=1 does the same.
    inline std::atomic<bool> g_synchronous{false};

    inline void Publish(uint32_t done, uint32_t total)
    {
        g_progress.store(uint64_t(done) << 32 | total, std::memory_order_relaxed);
    }

    inline uint32_t Done(uint64_t progress) { return uint32_t(progress >> 32); }
    inline uint32_t Total(uint64_t progress) { return uint32_t(progress); }
}
