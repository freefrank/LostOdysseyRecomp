#pragma once

#include <plume_render_interface.h>
#include <os/logger.h>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>

namespace gpu::draw_timing
{
    // A single opt-in frame, independent of RenderDoc injection. Intervals group
    // four draws and include intervening copies/barriers; they are not isolated
    // shader durations. Readback uses the renderer's existing slot fence.
    inline uint64_t TargetFrame()
    {
        static const uint64_t frame = [] {
            const char* value = std::getenv("LO_GPU_DRAW_TIMING_FRAME");
            return value ? std::strtoull(value, nullptr, 10) : 0ull;
        }();
        return frame;
    }

    // LO_GPU_DRAW_TIMING_EVERY=N: probe every Nth frame as well (N >= 2), for
    // scenes whose frame number is not known in advance (the console reads it
    // from env.txt). A probe still covers one frame and is read back before
    // the next one starts.
    inline uint64_t Every()
    {
        static const uint64_t every = [] {
            const char* value = std::getenv("LO_GPU_DRAW_TIMING_EVERY");
            const uint64_t n = value ? std::strtoull(value, nullptr, 10) : 0ull;
            return n >= 2 ? n : 0ull;
        }();
        return every;
    }

    inline bool IsTarget(uint64_t frame)
    {
        return (TargetFrame() && TargetFrame() == frame) || (Every() && frame && frame % Every() == 0);
    }

    class Probe
    {
        static constexpr uint32_t kQueryCount = 2048;
        struct Draw {
            uint64_t vs, ps;
            uint32_t ordinal, indices, width, height;
        };
        std::unique_ptr<plume::RenderQueryPool> pool;
        std::vector<Draw> draws;
        std::vector<uint32_t> groupEnds;
        uint64_t frame = 0;
        uint32_t written = 0;
        bool recording = false, pending = false;

        void Mark(plume::RenderCommandList* list)
        {
            list->writeTimestamp(pool.get(), written++);
            groupEnds.push_back(uint32_t(draws.size()));
        }

    public:
        void Begin(plume::RenderDevice* device, plume::RenderCommandList* list, uint64_t currentFrame)
        {
            if (pending || !IsTarget(currentFrame)) return;
            if (!pool) pool = device->createQueryPool(kQueryCount);
            if (!pool || pool->getCount() != kQueryCount) {
                LOG_WARNING("gpu draw probe: query pool unavailable frame={}", currentFrame);
                return;
            }
            draws.clear(); groupEnds.clear();
            frame = currentFrame; written = 1; recording = true;
            list->resetQueryPool(pool.get(), 0, kQueryCount);
            list->writeTimestamp(pool.get(), 0);
        }

        void Record(plume::RenderCommandList* list, uint32_t ordinal, uint64_t vs, uint64_t ps,
                    uint32_t indices, uint32_t width, uint32_t height)
        {
            if (!recording || written >= kQueryCount - 1) return;
            draws.push_back({vs, ps, ordinal, indices, width, height});
            if ((draws.size() % 4) == 0) Mark(list);
        }

        void End(plume::RenderCommandList* list)
        {
            if (!recording) return;
            if (groupEnds.empty() || groupEnds.back() != draws.size()) Mark(list);
            // RenderQueryPool reads its complete allocation. Populate unused
            // queries after the final measured interval, without another wait.
            for (uint32_t i = written; i < kQueryCount; ++i) list->writeTimestamp(pool.get(), i);
            recording = false; pending = true;
        }

        void ReadCompleted()
        {
            if (!pending) return;
            pending = false;
            pool->queryResults();
            const uint64_t* times = pool->getResults();
            bool valid = times && times[0];
            for (uint32_t i = 1; valid && i < written; ++i) valid = times[i] >= times[i - 1];
            LOG_INFO("gpu draw probe frame={} draws={} groups={} valid={} scope=instrumented_queue_intervals_not_isolated_shader_time",
                     frame, draws.size(), groupEnds.size(), valid);
            if (!valid) return;
            uint32_t first = 0;
            for (uint32_t group = 0; group < groupEnds.size(); ++group) {
                const uint32_t end = groupEnds[group];
                LOG_INFO("gpu draw group frame={} group={} count={} gpu_ms={:.6f}",
                         frame, group, end - first, double(times[group + 1] - times[group]) / 1000000.0);
                for (uint32_t i = first; i < end; ++i) {
                    const auto& draw = draws[i];
                    LOG_INFO("gpu draw member frame={} group={} ordinal={} vs={:016x} ps={:016x} indices={} target={}x{}",
                             frame, group, draw.ordinal, draw.vs, draw.ps, draw.indices, draw.width, draw.height);
                }
                first = end;
            }
        }
    };
}
