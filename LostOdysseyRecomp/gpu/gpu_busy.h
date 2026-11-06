#pragma once
#include <atomic>
#include <cstdint>

// GPU busy time for the Detailed performance overlay (ui/performance_overlay.cpp).
// While the overlay asks for it, timestamp queries bracket each renderer queue
// batch and the presentation command list; their results are read after the
// fences the renderer and presentation already wait for, never with an extra
// GPU wait. The command processor thread records and reads both.
namespace gpu::gpu_busy
{
inline std::atomic<bool> g_requested{false};
// Dynamic resolution (gpu/dynamic_resolution.h) needs the measurement for as long as it runs, whatever the overlay does.
inline std::atomic<bool> g_dynamicResolution{false};
// So does the perf line (debug/perf_stats.cpp, LO_PERF_GPU), whatever the overlay does.
inline std::atomic<bool> g_perf{false};
inline std::atomic<uint64_t> g_busy{0};

inline bool Requested()
{
    return g_requested.load(std::memory_order_relaxed) || g_dynamicResolution.load(std::memory_order_relaxed) ||
        g_perf.load(std::memory_order_relaxed);
}
inline void Request(bool requested) { g_requested.store(requested, std::memory_order_relaxed); }
// One completed batch's begin and end timestamps, as plume returns them
// (nanoseconds on Vulkan and D3D12). Unwritten or reordered results add nothing.
inline void AddBatch(uint64_t begin, uint64_t end)
{
    if (begin && end > begin) g_busy.fetch_add(end - begin, std::memory_order_relaxed);
}
// Running total of AddBatch.
inline uint64_t Total() { return g_busy.load(std::memory_order_relaxed); }
}
