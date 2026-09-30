// BR-01 owner sequence. HistoryOwner::CaptureColorInputs sets inputsComplete and
// reset reasons. Motion textures are a stand-in so that capture can run without
// NGX or a game. A gap's renderer epoch change is not the number of
// HistoryOwner::Reset calls: BeginFrame resets again when the epoch differs.
#include <plume_render_interface.h>
#include <gpu/temporal_history.h>
#include <gpu/temporal_lifecycle.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace plume {
std::unique_ptr<RenderInterface> CreateD3D12Interface();
}

namespace {

unsigned checks = 0;

void Check(bool condition, const char* description) {
    ++checks;
    if (!condition) throw std::runtime_error(description);
    std::printf("PASS: %s\n", description);
}

using Clock = std::chrono::steady_clock;
constexpr uint32_t W = 8, H = 8;

struct OwnerRun {
    plume::RenderCommandList* commands = nullptr;
    plume::RenderCommandQueue* queue = nullptr;
    plume::RenderCommandFence* fence = nullptr;
    plume::RenderTexture* color = nullptr;
    plume::RenderTexture* depth = nullptr;
    plume::RenderTexture* velocity = nullptr;
    plume::RenderTexture* reactive = nullptr;
    gpu::temporal::HistoryOwner owner;
    gpu::frame_plan::FramePlan plan{};
    gpu::temporal::FrameStartPolicy policy{};
    uint64_t epoch = 1;
    uint64_t supportedFrame = ~0ull;
    uint64_t gapResetFrame = ~0ull;
    bool jitter = false;
    Clock::time_point frameTime{};

    struct Captured {
        bool longReset = false;
        bool frameEndReset = false;
        bool resetInitialization = false;
        bool inputsComplete = false;
        bool completed = false;
        bool jitter = false;
        uint64_t epoch = 0;
        uint64_t epochDelta = 0;
        gpu::temporal::TemporalFrameInputs inputs{};
    };

    void Submit() {
        const auto serial = owner.RecordedSerial();
        commands->end();
        const plume::RenderCommandList* lists[] = {commands};
        queue->executeCommandLists(lists, 1, nullptr, 0, nullptr, 0, fence);
        queue->waitForCommandFence(fence);
        owner.ReleaseCompletedThrough(serial);
    }

    Captured Frame(uint64_t frame, Clock::time_point start, Clock::time_point end, bool repeatColor) {
        policy.frame = frame;
        policy.supportedFrame = supportedFrame;
        const auto started = gpu::temporal::ResolveFrameStartConsumers(policy);
        jitter = started.jitter;
        const uint64_t epochBefore = epoch;
        const bool longReset = gpu::temporal::ApplyTemporalLongInterval(&owner, start, frame,
            started.inputProbe, started.dlssSr, policy.diagnosticJitter, policy.forcedJitter, frameTime,
            jitter, supportedFrame, epoch, gapResetFrame);
        owner.BeginFrame(frame, epoch);
        const bool resetInitialization = owner.ResetInitializationRequired();

        gpu::temporal::SceneObservation scene;
        scene.Reset(frame);
        gpu::temporal::SceneAnchor anchor;
        anchor.depthAllocation = 7;
        anchor.viewport = {0, 0, double(W), double(H)};
        const gpu::temporal::Matrix identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        for (unsigned i = 0; i < 16; ++i) anchor.vpBits[i] = std::bit_cast<uint32_t>(float(identity[i]));
        scene.ObserveCamera(anchor);
        scene.ObserveDepth(7, {frame, frame * 2 + 1, 0x1000, 24, W, H, true});
        scene.ObserveColor({frame, frame * 2 + 2, 0x2000, 6, W, H, true});

        commands->begin();
        commands->barriers(plume::RenderBarrierStage::COPY,
            plume::RenderTextureBarrier(depth, plume::RenderTextureLayout::COPY_SOURCE));
        if (!owner.CaptureDepth(commands, depth, scene)) throw std::runtime_error("CaptureDepth failed");
        commands->barriers(plume::RenderBarrierStage::COPY,
            plume::RenderTextureBarrier(color, plume::RenderTextureLayout::COPY_SOURCE));
        gpu::temporal::MotionFrameView motion;
        motion.velocity = velocity;
        motion.reactive = reactive;
        motion.frame = frame;
        motion.epoch = epoch;
        motion.depthAllocation = 7;
        motion.width = W;
        motion.height = H;
        motion.ready = true;
        motion.state = resetInitialization ? gpu::temporal::MotionState::ResetInitialization : gpu::temporal::MotionState::Tracked;
        if (!owner.CaptureColorInputs(commands, color, scene, plan, {}, gpu::temporal::ColorEncoding::Sdr, &motion))
            throw std::runtime_error("CaptureColorInputs failed");
        if (repeatColor) scene.ObserveColor({frame, frame * 2 + 3, 0x2000, 6, W, H, true});
        Submit();

        const auto decision = gpu::temporal::EvaluateTemporalFrameEnd(owner, end, frame,
            started.experiment, started.inputProbe, started.dlssSr, scene.Ready(), true, frameTime, gapResetFrame);
        gpu::temporal::CommitTemporalFrameEnd(owner, decision, end, supportedFrame, epoch, frameTime);
        Captured captured;
        captured.longReset = longReset;
        captured.frameEndReset = decision.reset;
        captured.resetInitialization = resetInitialization;
        captured.inputsComplete = owner.InputsComplete();
        captured.completed = owner.Completed();
        captured.jitter = jitter;
        captured.epoch = epoch;
        captured.epochDelta = epoch - epochBefore;
        captured.inputs = owner.CurrentInputs();
        return captured;
    }
};

void BindPlan(OwnerRun& run) {
    run.plan.consumer = gpu::upscaling::TemporalConsumer::DlssSr;
    run.plan.dlssQuality = gpu::upscaling::DlssQuality::Quality;
    run.plan.width = W;
    run.plan.height = H;
    const auto route = gpu::temporal::RouteConsumer(run.plan, false);
    Check(route.dlssSr && !route.inputProbe && !route.legacyTaa, "owner plan is ordinary DLSS SR");
    run.policy.legacyTaa = route.legacyTaa;
    run.policy.dlssInputs = route.dlssInputs;
    run.policy.dlssSr = route.dlssSr;
    run.policy.inputProbe = route.inputProbe;
}

} // namespace

int main() {
    try {
        auto api = plume::CreateD3D12Interface();
        Check(bool(api), "D3D12 interface");
        auto device = api->createDevice();
        Check(bool(device), "D3D12 device");
        std::printf("Backend: D3D12 on %s\n", device->getDescription().name.c_str());
        auto queue = device->createCommandQueue(plume::RenderCommandListType::DIRECT);
        auto commands = queue->createCommandList();
        auto fence = device->createCommandFence();
        auto color = device->createTexture(plume::RenderTextureDesc::Texture2D(W, H, 1, plume::RenderFormat::R8G8B8A8_UNORM));
        auto depth = device->createTexture(plume::RenderTextureDesc::Texture2D(W, H, 1, plume::RenderFormat::R32_FLOAT));
        auto velocity = device->createTexture(plume::RenderTextureDesc::Texture2D(W, H, 1, plume::RenderFormat::R16G16_FLOAT));
        auto reactive = device->createTexture(plume::RenderTextureDesc::Texture2D(W, H, 1, plume::RenderFormat::R8_UNORM));
        Check(bool(color && depth && velocity && reactive), "stand-in color, depth, and motion textures");

        auto attach = [&](OwnerRun& run) {
            run.commands = commands.get();
            run.queue = queue.get();
            run.fence = fence.get();
            run.color = color.get();
            run.depth = depth.get();
            run.velocity = velocity.get();
            run.reactive = reactive.get();
            Check(run.owner.Init(device.get()), "HistoryOwner initialization");
            BindPlan(run);
        };

        OwnerRun run;
        attach(run);
        Clock::time_point now{};
        auto first = run.Frame(1, now, now, false);
        Check(first.resetInitialization && first.inputs.currentInputsComplete && first.inputs.resetHistory &&
                gpu::temporal::HasResetReason(first.inputs.resetReasons, gpu::temporal::TemporalResetReason::FirstFrame) &&
                first.inputs.temporalEpoch == 1 && !first.completed && !first.frameEndReset && first.jitter && first.inputsComplete,
            "initial complete capture requests reset and keeps epoch 1");

        now += std::chrono::milliseconds(16);
        auto second = run.Frame(2, now, now, false);
        Check(!second.longReset && !second.resetInitialization && second.inputs.currentInputsComplete &&
                !second.inputs.resetHistory && second.inputs.resetReasons == gpu::temporal::TemporalResetReason::None &&
                second.epochDelta == 0 && second.inputs.temporalEpoch == 1 && !second.completed && second.jitter && second.inputsComplete,
            "consecutive complete capture keeps history and the same epoch");

        const auto gapStart = now + std::chrono::milliseconds(300);
        const auto gapEnd = gapStart + std::chrono::milliseconds(16);
        auto gap = run.Frame(3, gapStart, gapEnd, false);
        Check(gap.longReset && gap.resetInitialization && !gap.frameEndReset && gap.epochDelta == 1 && gap.epoch == 2 &&
                gap.inputs.currentInputsComplete && gap.inputs.resetHistory && gap.inputsComplete && !gap.completed &&
                gap.inputs.temporalEpoch == 2 &&
                gpu::temporal::HasResetReason(gap.inputs.resetReasons, gpu::temporal::TemporalResetReason::EpochChanged) &&
                gpu::temporal::HasResetReason(gap.inputs.resetReasons, gpu::temporal::TemporalResetReason::FirstFrame) && gap.jitter,
            "complete 300ms gap changes epoch once; BeginFrame records EpochChanged and capture survives frame end");

        now = gapEnd + std::chrono::milliseconds(16);
        auto recovered = run.Frame(4, now, now, false);
        Check(!recovered.longReset && !recovered.resetInitialization && !recovered.frameEndReset && recovered.epochDelta == 0 &&
                recovered.epoch == 2 && recovered.inputs.currentInputsComplete && !recovered.inputs.resetHistory &&
                recovered.inputs.resetReasons == gpu::temporal::TemporalResetReason::None && recovered.inputs.temporalEpoch == 2 &&
                recovered.jitter && !recovered.completed && recovered.inputsComplete,
            "recovery frame reuses the post-gap inputs without another epoch change");

        OwnerRun rejected;
        attach(rejected);
        Clock::time_point rejectNow{};
        rejected.Frame(1, rejectNow, rejectNow, false);
        const auto rejectStart = rejectNow + std::chrono::milliseconds(300);
        auto reject = rejected.Frame(2, rejectStart, rejectStart + std::chrono::milliseconds(16), true);
        Check(reject.longReset && reject.frameEndReset && !reject.inputsComplete && reject.epochDelta == 2 &&
                reject.inputs.resetHistory &&
                gpu::temporal::HasResetReason(reject.inputs.resetReasons, gpu::temporal::TemporalResetReason::EpochChanged),
            "RepeatedColor after a gap capture is a second epoch clear, not retained history");

        std::printf("PASS: %u BR-01 HistoryOwner lifecycle checks (no NGX, no image comparison)\n", checks);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
