#include <gpu/render_timing_contract.h>

#include <array>
#include <cassert>
#include <cstring>
#include <string_view>

int main()
{
    using gpu::render_timing::CpuSegments;
    using gpu::render_timing::kCpuFieldKeys;

    CpuSegments cpu{};
    assert(cpu.draws == 0);
    assert(cpu.shaders == 0);
    assert(cpu.pipelines == 0);
    assert(cpu.textures == 0);
    assert(cpu.resolves == 0);
    assert(cpu.drawMs == 0);
    assert(cpu.constantsMs == 0);
    assert(cpu.setsMs == 0);
    assert(cpu.vertexMs == 0);
    assert(cpu.bindMs == 0);
    assert(cpu.indexMs == 0);
    assert(cpu.recordMs == 0);
    assert(cpu.shaderMs == 0);
    assert(cpu.pipelineMs == 0);
    assert(cpu.textureMs == 0);
    assert(cpu.resolveMs == 0);
    assert(cpu.fenceWaitMs == 0);
    assert(cpu.rtAcquireMs == 0);
    assert(cpu.taaMs == 0);
    assert(cpu.nestedFlushMs == 0);

    constexpr std::string_view required[] = {
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
    static_assert(std::size(kCpuFieldKeys) == std::size(required));
    for (std::size_t i = 0; i < std::size(required); ++i)
        assert(kCpuFieldKeys[i] == required[i]);

    cpu.rtAcquireMs = 1.25;
    cpu.taaMs = 2.5;
    cpu.nestedFlushMs = 3.75;
    assert(cpu.rtAcquireMs == 1.25);
    assert(cpu.taaMs == 2.5);
    assert(cpu.nestedFlushMs == 3.75);
    return 0;
}
