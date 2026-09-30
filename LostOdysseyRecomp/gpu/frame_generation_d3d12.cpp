#include "frame_generation_d3d12.h"
#if defined(_WIN32) && defined(LO_ENABLE_D3D12_FG)
#include <os/logger.h>
#include <cstdlib>
#include <cstdio>

namespace gpu::frame_generation {
namespace {
struct InputOwner {
    std::shared_ptr<ProducerSnapshot> snapshot;
    std::shared_ptr<dlss_fg::DepthRemapper> conversion;
};
HRESULT CreateQueue(void* context, const D3D12_COMMAND_QUEUE_DESC* desc, REFIID iid, void** out) {
    return static_cast<framegen::D3D12Session*>(context)->CreateQueue(*desc, iid, out);
}
HRESULT CreateSwapchain(void* context, ID3D12CommandQueue* queue, HWND window,
    const DXGI_SWAP_CHAIN_DESC1* desc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* full,
    IDXGISwapChain1** out) {
    return static_cast<framegen::D3D12Session*>(context)->CreateSwapchain(queue, window, *desc, full, out);
}
}
D3D12Bridge::~D3D12Bridge() { Shutdown(); }
void D3D12Bridge::Require(bool success, const std::string& reason) {
    if (success) return;
    LOG_ERROR("D3D12 FG: {}; terminating without releasing uncertain GPU ownership", reason);
    std::fflush(nullptr); std::_Exit(EXIT_FAILURE);
}
bool D3D12Bridge::Initialize(plume::D3D12Device& device, const framegen::Config& config,
    const std::filesystem::path& runtime, std::string& reason) {
    if (session_ || device_) { reason = "FG bridge already initialized"; return false; }
    // Prepare the game-only conversion before taking ownership of native hooks.
    depth_ = std::make_shared<dlss_fg::DepthRemapper>();
    if (!depth_->Initialize(&device)) { reason = "FG depth conversion initialization failed"; return false; }
    if (config.provider == framegen::Provider::Dlss) {
#ifdef FRAMEGEN_WITH_DLSS
        session_ = framegen::CreateDlssD3D12(device.d3d, device.renderInterface->dxgiFactory, config, runtime, reason);
#else
        reason = "D3D12 DLSS FG was not compiled";
#endif
    } else if (config.provider == framegen::Provider::Fsr) {
#ifdef FRAMEGEN_WITH_FSR
        session_ = framegen::CreateFsrD3D12(device.d3d, device.renderInterface->dxgiFactory, config, runtime, reason);
#else
        reason = "FSR FG was not compiled";
#endif
    }
    if (!session_) return false;
    device_ = &device;
    config_ = config;
    device.presentationHooks = {session_.get(), CreateQueue, CreateSwapchain};
    LOG_INFO("D3D12 FG: provider={} mode={} generated_frames={} target_fps={} input_capture=on sr_dependency=none",
        config.provider == framegen::Provider::Fsr ? "fsr" : "dlss",
        config.mode == framegen::Mode::Dynamic ? "dynamic" : "fixed",
        config.generatedFrames, config.targetFrameRate);
    return true;
}
void D3D12Bridge::PrepareAfterHostDrain(const CompositeHandoff& handoff,
    plume::D3D12SwapChain& swap, plume::D3D12CommandList& commands, uint64_t epoch) {
    if (!Enabled()) return;
    std::string reason;
    Require(!recording_, "prepare overlaps previous command recording");
    const bool drained = session_->Drain(reason); Require(drained, reason);
    // video already waited the ordinary present fence; Drain additionally
    // establishes completion of SDK-owned asynchronous input reads.
    depth_->ReleaseAfterInputDrain();
    const auto reject = [&] {
        const bool off = session_->Disable(reason); Require(off, reason);
        previousFrame_ = previousEpoch_ = 0;
        if (++rejected_ <= 3 || rejected_ % 120 == 0)
            LOG_INFO("D3D12 FG: input unavailable count={} reason={}", rejected_, reason);
    };
    if (!handoff.ReadyForOrderedSubmission() || !handoff.producer ||
        !handoff.outputWidth || !handoff.outputHeight || !swap.d3d) { reject(); return; }
    const auto& in = handoff.producer->inputs;
    const bool reset = previousFrame_ + 1 != in.renderFrameId || previousEpoch_ != in.temporalEpoch ||
        in.resetHistory || in.motionState == temporal::MotionState::ResetInitialization ||
        !dlss_fg::SameHistoryConfiguration(previousPlan_, in.plan);
    framegen::Camera camera;
    DepthRemap mapping;
    if (!BuildCamera(in, reset ? nullptr : &previousVP_, camera, &mapping,
            reset ? nullptr : &previousRaster_)) { reason = "invalid camera"; reject(); return; }
    // The selected source can be scaled at presentation (native SR-off path).
    // Depth and motion keep their actual render extent; the SDK sees the actual
    // swapchain output extent. No SR option, jitter or guest timer is modified.
    recording_ = &commands; attempted_ = false;
    auto* converted = depth_->Record(&commands, in.depth, mapping);
    if (!converted) { reason = "depth conversion unavailable"; reject(); return; }
    static_cast<plume::RenderCommandList&>(commands).barriers(plume::RenderBarrierStage::ALL,
        plume::RenderTextureBarrier(in.motion.texture, plume::RenderTextureLayout::SHADER_READ));
    auto* depth = static_cast<plume::D3D12Texture*>(converted);
    auto* motion = static_cast<plume::D3D12Texture*>(in.motion.texture);
    framegen::D3D12Frame frame;
    frame.frameId = in.renderFrameId; frame.temporalEpoch = in.temporalEpoch; frame.deviceEpoch = epoch;
    frame.width = swap.getWidth(); frame.height = swap.getHeight();
    frame.inputWidth = in.depth.width; frame.inputHeight = in.depth.height;
    frame.backBuffers = swap.getTextureCount(); frame.format = swap.nativeFormat;
    frame.depth = depth->d3d; frame.motion = motion->d3d;
    frame.depthState = depth->resourceStates; frame.motionState = motion->resourceStates;
    frame.camera = camera; frame.reset = reset; frame.deltaMilliseconds = in.frameTimeDeltaMilliseconds;
    frame.lifetime = std::make_shared<InputOwner>(InputOwner{handoff.producer, depth_});
    const bool prepared = session_->Prepare(frame, commands.d3d, reason);
    // FFX records native commands outside Plume; cached root signatures/tables
    // must not suppress the next real Presentation draw's bindings.
    commands.invalidateCachedNativeState();
    if (!prepared) {
        // A failed SDK request can still have recorded work. Keep the owner and
        // submit/retire normally; failure is not evidence of zero GPU use.
        reject(); return;
    }
    previousFrame_ = in.renderFrameId; previousEpoch_ = in.temporalEpoch;
    previousVP_ = in.cameraViewProjection; previousRaster_ = in.cameraRaster; previousPlan_ = in.plan;
}
void D3D12Bridge::SubmitStart() {
    if (!Enabled()) return;
    if (recording_) attempted_ = true;
    std::string reason; const bool ok = session_->SubmitStart(reason); Require(ok, reason);
}
void D3D12Bridge::HostSubmitted(bool success, uint64_t serial) {
    if (!Enabled()) return;
    if (recording_) Require(success && serial && attempted_, "host submission failed");
    std::string reason; const bool ok = session_->Submitted(success, serial, reason); Require(ok, reason);
    recording_ = nullptr; attempted_ = false;
}
void D3D12Bridge::PresentStart() {
    if (!Enabled()) return;
    std::string reason; const bool ok = session_->PresentStart(reason); Require(ok, reason);
}
void D3D12Bridge::Presented(bool accepted) {
    if (!Enabled()) return;
    std::string reason; const bool ok = session_->Presented(accepted, reason); Require(ok, reason);
    const auto& s = session_->Statistics();
    if (previousFrame_ && previousFrame_ % 120 == 0)
        LOG_INFO("D3D12 FG: source_frame={} active={} accepted={} generated_intervals={} actual_presents={} scope=sdk_not_physical_display",
            previousFrame_, session_->Active(), accepted, s.generatedIntervals, s.actualPresents);
}
void D3D12Bridge::CancelUnsubmitted(plume::RenderCommandList* list) {
    if (!session_ || !recording_ || recording_ != list) return;
    Require(!attempted_, "cancel after native submit attempt");
    if (recording_->open) {
        Require(SUCCEEDED(recording_->d3d->Close()), "cancel command close failed");
        recording_->open = false;
    }
    Require(SUCCEEDED(recording_->d3d->Reset(recording_->commandAllocator, nullptr)), "cancel command reset failed");
    Require(SUCCEEDED(recording_->d3d->Close()), "cancel empty command close failed");
    recording_->invalidateCachedNativeState();
    // An empty command list removes host reads, but the producer queue still
    // owns its earlier snapshot writes. Check it before cancelling the lease.
    auto* queue = recording_->queue;
    auto fence = device_->createCommandFence();
    auto* native = static_cast<plume::D3D12CommandFence*>(fence.get());
    Require(native && native->d3d, "cancel producer fence allocation failed");
    Require(SUCCEEDED(queue->d3d->Signal(native->d3d, 1)), "cancel producer signal failed");
    HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    Require(event != nullptr, "cancel producer event failed");
    Require(SUCCEEDED(native->d3d->SetEventOnCompletion(1, event)), "cancel producer completion event failed");
    const auto waited = WaitForSingleObject(event, 10000); CloseHandle(event);
    Require(waited == WAIT_OBJECT_0 && native->d3d->GetCompletedValue() != UINT64_MAX &&
        native->d3d->GetCompletedValue() >= 1, "cancel producer completion unknown");
    std::string reason; const bool ok = session_->CancelRecorded(true, true, reason); Require(ok, reason);
    depth_->DiscardUnsubmitted(); recording_ = nullptr; previousFrame_ = previousEpoch_ = 0;
}
void D3D12Bridge::Quiesce() {
    if (!session_) return;
    Require(!recording_, "quiesce before recording cancellation");
    std::string reason; const bool ok = session_->Quiesce(reason); Require(ok, reason);
    depth_->ReleaseAfterInputDrain(); previousFrame_ = previousEpoch_ = 0;
}
void D3D12Bridge::Suspend() {
    if (!Enabled()) return;
    Quiesce();
    suspended_ = true;
}
void D3D12Bridge::ReleaseFeatureAfterGpuDrain() {
    if (!session_) return;
    std::string reason;
    const bool released = session_->ReleaseFeatureAfterGpuDrain(reason); Require(released, reason);
}
bool D3D12Bridge::Reconfigure(const framegen::Config& config, std::string& reason) {
    if (!session_ || config.provider != config_.provider) {
        reason = "FG reconfiguration requires an existing provider"; return false;
    }
    // An SDK/GPU quiesce failure leaves ownership uncertain. Do not resume
    // ordinary presentation against those resources.
    const bool configured = session_->Reconfigure(config, reason);
    Require(configured, reason);
    config_ = config; suspended_ = false; previousFrame_ = previousEpoch_ = 0;
    return true;
}
void D3D12Bridge::Shutdown() {
    if (session_) {
        Quiesce();
        if (device_) device_->presentationHooks = {};
        std::string reason; const bool ok = session_->Shutdown(reason); Require(ok, reason);
        session_.reset();
    }
    depth_.reset(); device_ = nullptr;
}
} // namespace gpu::frame_generation
#endif
