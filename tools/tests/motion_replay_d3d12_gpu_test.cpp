#include <gpu/motion_replay_gpu.h>
#include <plume_d3d12.h>
#include "motion_replay_fixture.h"
#include <cstdio>
#include <stdexcept>

namespace plume { std::unique_ptr<RenderInterface> CreateD3D12Interface(); }

int main() {
    try {
        auto api = plume::CreateD3D12Interface();
        auto device = api ? api->createDevice() : nullptr;
        if (!device) { std::puts("SKIP: D3D12 device unavailable"); return 77; }

        plume::RenderDescriptorSetBuilder sets[4];
        for (unsigned i = 0; i < 4; ++i) {
            sets[i].begin();
            if (i == 0) sets[i].addByteAddressBuffer(0);
            else sets[i].addTexture(0);
            sets[i].end();
        }
        gpu::temporal::MotionReplayGPU replay;
        if (!replay.Init(device.get(), sets, 4))
            throw std::runtime_error("motion replay init failed: " + replay.LastError());

        const auto vs = motion_fixture::Vertex(false).Guest();
        const auto ps = motion_fixture::Pixel().Guest();
        plume::RenderGraphicsPipelineDesc desc;
        desc.depthEnabled = true;
        desc.depthWriteEnabled = true;
        desc.depthFunction = plume::RenderComparisonFunction::GREATER_EQUAL;
        desc.depthTargetFormat = plume::RenderFormat::D32_FLOAT_S8_UINT;
        desc.renderTargetCount = 1;
        desc.renderTargetFormat[0] = plume::RenderFormat::R8G8B8A8_UNORM;
        desc.renderTargetBlend[0] = plume::RenderBlendDesc::Copy();

        // Real translated PS coverage and the depth-only fallback both need
        // TEXCOORD16/17 linked to the replay VS on D3D12.
        for (bool coverage : {true, false}) {
            gpu::pipeline_cache::Key key{};
            key.vs = 1; key.ps = coverage ? 2 : 3; key.depthControl = 6;
            auto* pipeline = replay.PreparePipeline(key, desc, vs.data(), uint32_t(vs.size()),
                coverage ? ps.data() : nullptr, coverage ? uint32_t(ps.size()) : 0, true);
            if (!pipeline)
                throw std::runtime_error(std::string(coverage ? "coverage" : "depth-only") +
                    " motion replay PSO failed: " + replay.LastError());
            std::printf("PASS: D3D12 %s motion replay PSO created\n", coverage ? "coverage" : "depth-only");
        }
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
