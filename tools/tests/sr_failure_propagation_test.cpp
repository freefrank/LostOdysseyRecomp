// Focused D3D12 owner/replay fault propagation; no guest executable or game.
#include <gpu/temporal_history.h>
#include <gpu/motion_replay_gpu.h>
#include <gpu/sr_scene_input_policy.h>
#include "motion_replay_fixture.h"

#include <bit>
#include <cstdio>
#include <stdexcept>

namespace plume { std::unique_ptr<RenderInterface> CreateD3D12Interface(); }
namespace {
using namespace plume;
using namespace gpu::temporal;
void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
    std::printf("PASS: %s\n", message);
}

SceneObservation Scene(uint64_t frame) {
    SceneObservation scene;
    scene.Reset(frame);
    SceneAnchor anchor;
    anchor.depthAllocation = 7;
    anchor.viewport = {0, 0, 8, 8};
    const Matrix identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    for (unsigned i = 0; i < 16; ++i)
        anchor.vpBits[i] = std::bit_cast<uint32_t>(float(identity[i]));
    scene.ObserveCamera(anchor);
    scene.ObserveDepth(7, {frame, frame * 2 + 1, 0x1000, 24, 8, 8, true});
    scene.ObserveColor({frame, frame * 2 + 2, 0x2000, 6, 8, 8, true});
    return scene;
}

void CheckOwnedAllocation(plume::RenderDevice& device, plume::RenderCommandList& commands,
    plume::RenderTexture* sourceDepth, plume::RenderTexture* sourceColor) {
    HistoryOwner owner;
    Check(owner.Init(&device), "real history owner initialized");
    gpu::frame_plan::FramePlan plan{};
    plan.width = 8; plan.height = 8; plan.consumer = gpu::upscaling::TemporalConsumer::DlssSr;
    const auto assertFailure = [&](uint64_t frame, uint32_t injection, InputCaptureFailure expected) {
        owner.BeginFrame(frame, 1);
        const auto scene = Scene(frame);
        Check(scene.Ready(), "scene inputs ready before injected allocation failure");
        owner.InjectOwnedCaptureAllocationFailureForTest(injection);
        Check(!owner.CaptureDepth(&commands, sourceDepth, scene), "real CaptureDepth allocation path fails");
        Check(!owner.CaptureColorInputs(&commands, sourceColor, scene, plan, {}, ColorEncoding::Sdr),
            "real CaptureColorInputs refuses SDK handoff after owned allocation failure");
        Check(owner.LastInputCaptureFailure() == expected &&
            ClassifySrCaptureFailure(owner.LastInputCaptureFailure()) == SrSceneInputFailure::RequestFailure,
            "typed owned allocation cause reaches hard request classifier");
        owner.Reset();
        Check(!owner.CaptureDepth(&commands, sourceDepth, scene) &&
            !owner.CaptureColorInputs(&commands, sourceColor, scene, plan, {}, ColorEncoding::Sdr) &&
            owner.LastInputCaptureFailure() == expected,
            "same-frame Reset and repeated capture cannot erase resource cause");
    };
    commands.begin();
    assertFailure(1, 1, InputCaptureFailure::OwnedDepthAllocationFailed);
    owner.BeginFrame(1, 2);
    Check(owner.LastInputCaptureFailure() == InputCaptureFailure::OwnedDepthAllocationFailed &&
        !owner.CaptureDepth(&commands, sourceDepth, Scene(1)),
        "same frame with a new epoch cannot clear owned allocation failure");
    owner.BeginFrame(2, 1);
    Check(owner.LastInputCaptureFailure() == InputCaptureFailure::None,
        "only a new frame clears typed allocation failure");
    assertFailure(2, 3, InputCaptureFailure::OwnedColorAllocationFailed);
    commands.end(); // No GPU copy is recorded after either failed allocation.
    Check(ClassifySrCaptureFailure(InputCaptureFailure::MissingCamera) ==
        SrSceneInputFailure::FrameFallback, "camera missing without a resource cause remains transient");
}

void CheckModuleFailure(plume::RenderDevice& device) {
    RenderDescriptorSetBuilder sets[4];
    for (auto& set : sets) { set.begin(); set.end(); }
    MotionReplayGPU replay;
    Check(replay.Init(&device, sets, 4), "real motion replay pipeline initialized");
    const auto guestVertex = motion_fixture::Vertex(false).Guest();
    const auto guestPixel = motion_fixture::Pixel().Guest();
    gpu::pipeline_cache::Key key{};
    key.vs = 71; key.ps = 72; key.depthControl = 6; key.prim = 4;
    RenderGraphicsPipelineDesc desc{};
    desc.depthEnabled = true;
    TemporalFrameInputs incomplete;
    incomplete.currentInputsComplete = false;
    // Region::Complete examines only pointer identity and bounds in this
    // CPU decision check; the module path above uses an actual D3D12 device.
    auto* nonOwning = reinterpret_cast<RenderTexture*>(uintptr_t(0x1000));
    incomplete.color = {nonOwning, {8, 8}, 0, 0, 8, 8};
    incomplete.depth = {nonOwning, {8, 8}, 0, 0, 8, 8};
    incomplete.depthConvention = DepthConvention::Reversed;
    incomplete.plan.consumer = gpu::upscaling::TemporalConsumer::DlssSr;
    MotionReplayGPU::PipelinePrepareStatus status{};
    replay.BeginFrame(21, 1);
    replay.InjectNextModuleAllocationFailureForTest();
    auto* first = replay.PreparePipeline(key, desc, guestVertex.data(), uint32_t(guestVertex.size()),
        guestPixel.data(), uint32_t(guestPixel.size()), true, &status);
    Check(!first && status == MotionReplayGPU::PipelinePrepareStatus::Failed &&
        replay.ResourceFailedThisFrame() && replay.LastError() == "Replay shader module allocation failed",
        "actual translated/compiled module path propagates injected createShader allocation failure");
    Check(ClassifySrIncomplete(incomplete, replay.ResourceFailedThisFrame()) == SrSceneInputFailure::RequestFailure,
        "first module GPU allocation failure reaches hard request classifier");
    replay.BeginFrame(22, 1);
    Check(!replay.ResourceFailedThisFrame(), "new frame initially clears replay resource flag");
    auto* cached = replay.PreparePipeline(key, desc, guestVertex.data(), uint32_t(guestVertex.size()),
        guestPixel.data(), uint32_t(guestPixel.size()), true, &status);
    Check(!cached && status == MotionReplayGPU::PipelinePrepareStatus::Failed &&
        replay.ResourceFailedThisFrame() && replay.LastError() == "Replay shader module allocation failed",
        "cached module allocation failure propagates again in next frame");
    Check(ClassifySrIncomplete(incomplete, replay.ResourceFailedThisFrame()) == SrSceneInputFailure::RequestFailure,
        "cached module GPU allocation failure reaches hard request classifier");
    // A missing guest shader is typed as unsupported, not GPU allocation.
    gpu::pipeline_cache::Key unknown{};
    unknown.vs = 73; unknown.ps = 74;
    replay.BeginFrame(23, 1);
    Check(!replay.PreparePipeline(unknown, desc, nullptr, 0, nullptr, 0, true, &status) &&
        status == MotionReplayGPU::PipelinePrepareStatus::Failed && !replay.ResourceFailedThisFrame(),
        "unsupported microcode does not masquerade as device allocation failure");
    replay.BeginFrame(24, 1);
    replay.AbortFrame("first_visibility_writer");
    replay.AbortFrame("later_suppressed_writer");
    Check(replay.LastError() == "first_visibility_writer",
        "later aborted draw cannot overwrite first abort reason");
}
} // namespace

int main() {
    try {
        auto api = plume::CreateD3D12Interface();
        Check(bool(api), "D3D12 interface");
        auto device = api->createDevice();
        Check(bool(device), "device");
        auto queue = device->createCommandQueue(RenderCommandListType::DIRECT);
        auto commands = queue->createCommandList();
        auto depth = device->createTexture(RenderTextureDesc::Texture2D(8, 8, 1, RenderFormat::R32_FLOAT));
        auto color = device->createTexture(RenderTextureDesc::Texture2D(8, 8, 1, RenderFormat::R8G8B8A8_UNORM));
        Check(queue && commands && depth && color, "real owner source resources");
        CheckOwnedAllocation(*device, *commands, depth.get(), color.get());
        CheckModuleFailure(*device);
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "sr failure propagation: %s\n", e.what());
        return 1;
    }
}
