#pragma once
#include <cstdint>

namespace gpu::temporal {
// Nanoseconds are normalized by Plume on both backends. These are instrumented
// queue intervals, not CPU recording time, display latency or whole-frame time.
struct GpuPassTimingStats {
    uint64_t samples = 0, unavailable = 0, queryPoolAllocations = 0;
    double totalMilliseconds = 0, lastMilliseconds = -1;
};
}

#ifdef LO_GPU_PLUME
#include <plume_render_interface.h>
#include <algorithm>
#include <array>
#include <memory>
#include <vector>
namespace gpu::temporal {
// Each pool belongs to exactly one submission. Only the existing fence callback
// can read or reuse it. Disabled instrumentation allocates nothing. Multi-pair
// clients MUST Seal before ending a command list, even on an aborted MV frame.
template<uint32_t Pairs = 1, size_t MaxPools = 16>
class GpuPassTimer {
    static_assert(Pairs > 0 && MaxPools > 0);
    struct Block {
        std::unique_ptr<plume::RenderQueryPool> pool;
        uint64_t serial = 0, lastEnd = 0;
        uint32_t used = 0;
    };
    bool enabled_ = false, unavailableDevice_ = false;
    std::unique_ptr<Block> active_;
    std::vector<std::unique_ptr<Block>> pending_, free_;
    GpuPassTimingStats stats_;
public:
    void Enable(bool enabled) { enabled_ = enabled; }
    const GpuPassTimingStats& Stats() const { return stats_; }
    size_t PendingCount() const { return pending_.size() + bool(active_); }
    // Begin/End are paired with no early return between them. A disabled/failed
    // Begin returns false; End must then be skipped. Failure affects timing only.
    bool Begin(plume::RenderDevice* device, plume::RenderCommandList* list) {
        if (!enabled_) return false;
        if (!device || !list || unavailableDevice_) { ++stats_.unavailable; return false; }
        if (!active_) {
            if (!free_.empty()) { active_ = std::move(free_.back()); free_.pop_back(); }
            else {
                if (pending_.size() >= MaxPools) { ++stats_.unavailable; return false; }
                auto block = std::make_unique<Block>();
                block->pool = device->createQueryPool(Pairs * 2);
                if (!block->pool || block->pool->getCount() != Pairs * 2) {
                    unavailableDevice_ = true; ++stats_.unavailable; return false;
                }
                ++stats_.queryPoolAllocations; active_ = std::move(block);
            }
            active_->used = 0; active_->serial = 0;
            // Plume does not end a render pass in resetQueryPool. Vulkan
            // requires an outside-pass reset; every caller binds its draw
            // framebuffer after Begin, including query-block rollover.
            if (device->getCapabilities().shaderFormat == plume::RenderShaderFormat::SPIRV)
                list->setFramebuffer(nullptr);
            list->resetQueryPool(active_->pool.get(), 0, Pairs * 2);
        }
        list->writeTimestamp(active_->pool.get(), active_->used * 2);
        return true;
    }
    void End(plume::RenderCommandList* list, uint64_t recordedSerial) {
        list->writeTimestamp(active_->pool.get(), active_->used * 2 + 1);
        active_->serial = recordedSerial; ++active_->used;
        if (active_->used == Pairs) Seal(list);
    }
    void Seal(plume::RenderCommandList* list) {
        if (!active_) return;
        // Plume reads its entire allocation, so never read unwritten queries.
        // Padding is outside all measured intervals and never causes a CPU wait.
        for (uint32_t i = active_->used * 2; i < Pairs * 2; ++i)
            list->writeTimestamp(active_->pool.get(), i);
        pending_.push_back(std::move(active_));
    }
    void ReleaseCompletedThrough(uint64_t serial) {
        for (auto it = pending_.begin(); it != pending_.end();) {
            auto& b = **it;
            if (b.serial > serial) { ++it; continue; }
            b.pool->queryResults(); const uint64_t* values = b.pool->getResults();
            uint64_t last = b.lastEnd;
            for (uint32_t i = 0; i < b.used; ++i) {
                const uint64_t begin = values ? values[2*i] : 0, end = values ? values[2*i+1] : 0;
                // Reject stale results after a failed backend readback as well as
                // unsupported/nonmonotonic clocks; zero is not a fabricated time.
                if (!begin || end < begin || (last && (begin < last || end <= last))) {
                    ++stats_.unavailable; continue;
                }
                stats_.lastMilliseconds = double(end - begin) / 1e6;
                stats_.totalMilliseconds += stats_.lastMilliseconds; ++stats_.samples;
                last = end;
            }
            b.lastEnd = last; free_.push_back(std::move(*it)); it = pending_.erase(it);
        }
    }
};
}
#endif
