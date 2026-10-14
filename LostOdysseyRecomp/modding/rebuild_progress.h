#pragma once

#include <atomic>
#include <cstdint>

// The rebuild of a folder's files for the mods (modding/text_overlay.cpp), which
// the game waits for on the first start after the mods change: its worker
// threads count packages here and the window shows the count (gpu/video.cpp).
namespace modding::rebuild_progress
{
    // total << 32 | done; 0 while no rebuild runs.
    inline std::atomic<uint64_t> g_progress{0};

    inline void AddTotal(uint32_t packages) { g_progress.fetch_add(uint64_t(packages) << 32, std::memory_order_relaxed); }
    inline void Advance() { g_progress.fetch_add(1, std::memory_order_relaxed); }
    inline void Finish() { g_progress.store(0, std::memory_order_relaxed); }

    inline uint32_t Done(uint64_t progress) { return uint32_t(progress); }
    inline uint32_t Total(uint64_t progress) { return uint32_t(progress >> 32); }
}
