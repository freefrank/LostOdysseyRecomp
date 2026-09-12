#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace gpu::render_timing
{
    struct CpuSegments
    {
        uint64_t draws = 0, shaders = 0, pipelines = 0, textures = 0, resolves = 0;
        double drawMs = 0, constantsMs = 0, setsMs = 0, vertexMs = 0;
        double bindMs = 0, indexMs = 0, recordMs = 0;
        double shaderMs = 0, pipelineMs = 0, textureMs = 0, resolveMs = 0, fenceWaitMs = 0;
        double rtAcquireMs = 0, taaMs = 0, nestedFlushMs = 0;
    };

    inline constexpr std::string_view kCpuFieldKeys[] = {
        "draw_ms=",
        "constants_ms=",
        "sets_ms=",
        "vertex_ms=",
        "bind_ms=",
        "index_ms=",
        "record_ms=",
        "shader_ms=",
        "pipeline_ms=",
        "texture_ms=",
        "resolve_ms=",
        "fence_wait_ms=",
        "rt_acquire_ms=",
        "taa_ms=",
        "nested_flush_ms=",
    };
}
