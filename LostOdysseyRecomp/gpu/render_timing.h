#pragma once

#include <os/logger.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

// Opt-in diagnostics. The renderer owns these counters on its command thread.
// Query readback happens only after its existing queue fence; no extra GPU wait.
namespace gpu::render_timing
{
    inline bool Enabled()
    {
        static const bool enabled = [] {
            const char* value = std::getenv("LO_RENDER_TIMING");
            return value && *value && std::strcmp(value, "0") != 0;
        }();
        return enabled;
    }

    struct GpuBatches
    {
        double elapsedMs = 0;
        uint64_t count = 0, valid = 0, invalid = 0;
        // Keep timestamp order across per-frame counter resets. An unchanged
        // result after a failed readback must not count as this frame's work.
        uint64_t lastEndNs = 0;

        void AddBatch(uint64_t beginNs, uint64_t endNs)
        {
            ++count;
            if (!beginNs || endNs < beginNs || (lastEndNs && (beginNs < lastEndNs || endNs <= lastEndNs)))
            {
                ++invalid;
                return;
            }
            elapsedMs += static_cast<double>(endNs - beginNs) / 1000000.0;
            lastEndNs = endNs;
            ++valid;
        }
        void AddUnavailableBatch() { ++count; ++invalid; }
        void Reset() { elapsedMs = 0; count = valid = invalid = 0; }
        // Construct a new GpuBatches when the device/query clock is recreated.
        bool Complete() const { return valid > 0 && invalid == 0; }
    };

    struct CpuSegments
    {
        uint64_t draws = 0, shaders = 0, pipelines = 0, textures = 0, resolves = 0;
        double drawMs = 0, constantsMs = 0, setsMs = 0, vertexMs = 0;
        double bindMs = 0, indexMs = 0, recordMs = 0;
        double shaderMs = 0, pipelineMs = 0, textureMs = 0, resolveMs = 0, fenceWaitMs = 0;
    };

    inline void LogFrame(uint64_t frame, const CpuSegments& cpu, const GpuBatches& gpu,
        bool captureActive, bool geometryTraceActive)
    {
        if (!Enabled()) return;
        // CPU components are elapsed wall timers and may overlap: shader/cache,
        // texture work and substeps can occur inside Draw. Do not sum them as CPU
        // utilization. GPU is the sum of measured renderer queue batches, not
        // whole-frame GPU time, presentation, compositor time or display latency.
        LOG_INFO("render timing frame={} draws={} shaders={} pipelines={} textures={} resolves={} "
            "draw_ms={:.6f} constants_ms={:.6f} sets_ms={:.6f} vertex_ms={:.6f} bind_ms={:.6f} index_ms={:.6f} record_ms={:.6f} "
            "shader_ms={:.6f} pipeline_ms={:.6f} texture_ms={:.6f} resolve_ms={:.6f} fence_wait_ms={:.6f} "
            "gpu_queue_batches_elapsed_ms={} gpu_batches={} gpu_valid={} gpu_invalid={} gpu_complete={} "
            "capture_active={} geometry_trace_active={} cpu_scope=overlapping_wall_components gpu_scope=renderer_queue_batches_excludes_present_and_compositor",
            frame, cpu.draws, cpu.shaders, cpu.pipelines, cpu.textures, cpu.resolves,
            cpu.drawMs, cpu.constantsMs, cpu.setsMs, cpu.vertexMs, cpu.bindMs, cpu.indexMs, cpu.recordMs,
            cpu.shaderMs, cpu.pipelineMs, cpu.textureMs, cpu.resolveMs, cpu.fenceWaitMs,
            gpu.valid ? fmt::format("{:.6f}", gpu.elapsedMs) : std::string("unknown"),
            gpu.count, gpu.valid, gpu.invalid, gpu.Complete(), captureActive, geometryTraceActive);
    }
}
