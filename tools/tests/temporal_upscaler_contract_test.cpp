#include <gpu/temporal_upscaler.h>
#include <cstdio>
#include <cstdlib>

namespace {
void Require(bool ok, const char* reason) { if (!ok) { std::fprintf(stderr, "FAIL: %s\n", reason); std::exit(1); } }
struct Owner {
    uint64_t submittedUse = 0, serial = 0, discardedUse = 0;
    void OnBatchSubmitted(uint64_t use, uint64_t submittedSerial) { submittedUse = use; serial = submittedSerial; }
    void OnBatchDiscarded(uint64_t use) { discardedUse = use; }
};
}
int main() {
    using namespace gpu;
    using namespace gpu::upscaling;
    using namespace gpu::frame_plan;
    Require(CanQueryNgxSizing({3, 1280, 720}) && !CanQueryNgxSizing({3, 1280, 720, Upscaler::Fsr}) &&
        !CanQueryNgxSizing({3, 0, 720}), "FSR and invalid extents never query NGX");
    BackendDeviceSnapshot device{backend::Backend::Vulkan, 3, true, true};
    Require(device.Available(Upscaler::Dlss) && !device.Available(Upscaler::Fsr), "FSR capability independent of DLSS");
    device.gpuWorkStopped = true;
    Require(!device.Available(Upscaler::Dlss), "device stop invalidates provider capability");

    PlannerState state;
    PlannerInput input{};
    input.upscaler = Upscaler::Off; input.output = {{1280,720},0,0,1280,720};
    input.inputProbeRequested = true; input.device = {backend::Backend::Vulkan, 3, true, true};
    const auto off = state.Begin(input);
    Require(off.consumer == TemporalConsumer::None && !off.inputProbe, "Off+probe stays legacy");
    input.quality = DlssQuality::Performance; input.fsrQuality = FsrQuality::Performance;
    const auto offChanged = state.Begin(input);
    Require(off.requestSignature == offChanged.requestSignature && off.geometryEpoch == offChanged.geometryEpoch &&
        SameEffectiveQuality(Upscaler::Off, DlssQuality::Quality, input.quality, FsrQuality::Quality, input.fsrQuality),
        "Off ignores both saved qualities for epoch and history");

    OutputSizing sizing{}; sizing.key = {3, 1280, 720};
    for (auto& mode : sizing.modes) { mode.state = SizingState::Ready; mode.optimal = {960, 540}; }
    input.upscaler = Upscaler::Dlss; input.quality = DlssQuality::Quality; input.sizing = &sizing;
    const auto probe = state.Begin(input);
    Require(probe.consumer == TemporalConsumer::DlssInputs && probe.inputProbe, "DLSS+probe collects inputs only");
    input.inputProbeRequested = false;
    const auto dlss = state.Begin(input);
    Require(dlss.consumer == TemporalConsumer::DlssSr &&
        state.ReportExecution({dlss, 1, 9, DlssExecutionOutcome::Submitted}), "DLSS checked submission observed");
    input.fsrQuality = FsrQuality::NativeAA;
    const auto dlssSame = state.Begin(input);
    Require(dlssSame.geometryEpoch == dlss.geometryEpoch && dlssSame.requestSignature == dlss.requestSignature &&
        state.Observe().execution.has_value(), "irrelevant FSR quality retains DLSS execution and history");
    UpscalerExecutionObservation wrongProvider{dlssSame, 2, 10, DlssExecutionOutcome::Submitted};
    wrongProvider.actualProvider = Upscaler::Fsr;
    Require(!state.ReportUpscalerExecution(wrongProvider) && state.Observe().execution->submissionSerial == 9,
        "submitted observation must identify actual provider matching selected consumer");
    auto wrongConsumer = wrongProvider;
    wrongConsumer.actualProvider = Upscaler::Dlss;
    wrongConsumer.plan.consumer = TemporalConsumer::FsrSr;
    Require(!state.ReportUpscalerExecution(wrongConsumer), "submitted consumer mismatch cannot claim current plan");
    input.quality = DlssQuality::Balanced;
    const auto dlssB = state.Begin(input);
    input.quality = DlssQuality::Quality;
    const auto dlssAgain = state.Begin(input);
    Require(dlssB.geometryEpoch != dlss.geometryEpoch && dlssAgain.geometryEpoch != dlss.geometryEpoch &&
        !state.Observe().execution, "selected DLSS quality A-B-A resets history and submission");
    input.device.gpuWorkStopped = true;
    Require(state.Begin(input).consumer != TemporalConsumer::DlssSr,
        "stopped device cannot select DLSS even with cached ready sizing");
    input.device.gpuWorkStopped = false;

    input.upscaler = Upscaler::Fsr; input.inputProbeRequested = true;
    const auto fsr = state.Begin(input);
    Require(fsr.consumer != TemporalConsumer::FsrSr && !fsr.inputProbe && fsr.width == fsr.legacyWidth,
        "FSR+probe remains legacy without NGX sizing or SDK");
    input.quality = DlssQuality::Dlaa;
    const auto fsrSame = state.Begin(input);
    Require(fsrSame.geometryEpoch == fsr.geometryEpoch && fsrSame.requestSignature == fsr.requestSignature,
        "irrelevant DLSS quality does not reset FSR plan or history");
    input.fsrQuality = FsrQuality::Balanced;
    const auto fsrB = state.Begin(input);
    input.fsrQuality = FsrQuality::NativeAA;
    Require(fsrB.geometryEpoch != fsrSame.geometryEpoch && state.Begin(input).geometryEpoch != fsrSame.geometryEpoch,
        "selected FSR quality A-B-A receives new epoch");
    input.upscaler = Upscaler::Dlss; input.inputProbeRequested = true; input.readback = true;
    Require(state.Begin(input).consumer != TemporalConsumer::DlssSr && !state.Begin(input).inputProbe,
        "readback cannot use SR or probe");

    temporal::TemporalFrameInputs inputs;
    inputs.plan = dlss; inputs.currentInputsComplete = true;
    auto* texture = reinterpret_cast<plume::RenderTexture*>(uintptr_t(1));
    inputs.color = inputs.depth = inputs.motion = inputs.motionInvalidity = {texture, {960,540},0,0,960,540};
    inputs.depthConvention = temporal::DepthConvention::Reversed;
    inputs.motionState = temporal::MotionState::Tracked;
    Require(ValidSrRequest({dlss, inputs}), "complete matching request accepted");
    auto mismatch = dlss; mismatch.requestedUpscaler = Upscaler::Fsr;
    Require(!ValidSrRequest({mismatch, inputs}), "provider/consumer mismatch rejected before native record");
    Require(temporal::RouteConsumer(mismatch, false).providerMismatch &&
        !temporal::RouteConsumer(mismatch, false).sr, "provider mismatch cannot enter SR route");
    mismatch = dlss; mismatch.consumer = TemporalConsumer::DlssInputs; mismatch.requestedUpscaler = Upscaler::Fsr;
    Require(temporal::RouteConsumer(mismatch, true).providerMismatch &&
        !temporal::RouteConsumer(mismatch, true).dlssInputs,
        "provider mismatch cannot enter input-only probe route");
    inputs.motionState = temporal::MotionState::Unavailable;
    Require(!ValidSrRequest({dlss, inputs}), "missing motion prevents provider dispatch");

    Owner owner;
    SrUseToken recorded{Upscaler::Dlss, dlss.deviceEpoch, 27, dlss.requestSignature, dlss.geometryEpoch};
    input.upscaler = Upscaler::Fsr; state.Begin(input); // settings changed after recording
    RouteSubmitted(recorded, 33, owner);
    Require(owner.submittedUse == 27 && owner.serial == 33, "checked submission routes by recorded owner");
    RouteDiscarded(recorded, owner);
    Require(owner.discardedUse == 27, "unsubmitted discard routes by recorded owner");
    RouteSubmitted({Upscaler::Fsr, 3, 44}, 34, owner);
    RouteDiscarded({Upscaler::Fsr, 3, 44}, owner);
    RouteSubmitted(recorded, 0, owner);
    Require(owner.submittedUse == 27 && owner.serial == 33 && owner.discardedUse == 27,
        "unsupported provider and unchecked serial cannot notify NGX");
    std::puts("temporal upscaler contracts passed");
}
