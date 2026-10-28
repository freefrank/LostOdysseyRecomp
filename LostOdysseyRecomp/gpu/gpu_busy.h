#pragma once
#include <atomic>
#include <cstdint>
#if defined(__ANDROID__)
#include <diagnostics/android_tester.h>
#endif

// GPU busy time for the Detailed performance overlay (ui/performance_overlay.cpp).
// While the overlay asks for it, timestamp queries bracket each renderer queue
// batch and the presentation command list; their results are read after the
// fences the renderer and presentation already wait for, never with an extra
// GPU wait. The command processor thread records and reads both.
namespace gpu::gpu_busy
{
inline std::atomic<bool> g_requested{false};
inline std::atomic<uint64_t> g_busy{0};
inline std::atomic<uint64_t> g_batches{0};

inline bool Requested() {
    return g_requested.load(std::memory_order_relaxed)
#if defined(__ANDROID__)
        || ::diagnostics::android_tester::g_recorder.CollectGpu()
#endif
        ;
}
inline void Request(bool requested) { g_requested.store(requested, std::memory_order_relaxed); }
// One completed batch's begin and end timestamps, as plume returns them
// (nanoseconds on Vulkan and D3D12). Unwritten or reordered results add nothing.
inline bool AddBatch(uint64_t begin, uint64_t end, uint64_t& previousEnd)
{
    // Each query pool is read only after its existing fence. A failed backend
    // read may leave its previous results in place, so reject stale timestamps.
    if (!begin || end <= begin || (previousEnd && (begin < previousEnd || end <= previousEnd))) return false;
    previousEnd = end;
    g_busy.fetch_add(end - begin, std::memory_order_relaxed);
    g_batches.fetch_add(1, std::memory_order_relaxed);
    return true;
}
// Running total of AddBatch.
inline uint64_t Total() { return g_busy.load(std::memory_order_relaxed); }
inline uint64_t Batches() { return g_batches.load(std::memory_order_relaxed); }
}
