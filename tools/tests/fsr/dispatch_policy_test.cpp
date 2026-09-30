#include <gpu/fsr_dispatch_policy.h>
#include <gpu/temporal_upscaler.h>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {
unsigned checks = 0;
void Check(bool value, const char* reason) {
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", reason); std::exit(1); }
}
}

int main() {
    using namespace gpu;
    using fsr::Status;
    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    constexpr float inf = std::numeric_limits<float>::infinity();
    const fsr::Config config{1280, 720, 1920, 1080, upscaling::FsrQuality::Quality, 1};
    frame_plan::PlannerInput planInput{};
    planInput.output = {{1920,1080},0,0,1920,1080};
    planInput.upscaler = upscaling::Upscaler::Fsr;
    planInput.device = {backend::Backend::Vulkan, 1, true, false, false, true};
    upscaling::OutputSizing sizing{};
    sizing.key = {1,1920,1080,upscaling::Upscaler::Fsr,0,0};
    sizing.modes[0] = {upscaling::SizingState::Ready,{1280,720},{1280,720},{1280,720}};
    planInput.sizing = &sizing;
    frame_plan::PlannerState planner;
    temporal::TemporalFrameInputs inputs{};
    inputs.plan = planner.Begin(planInput);
    inputs.renderFrameId = 100;
    inputs.temporalEpoch = 1;
    inputs.currentInputsComplete = true;
    // Identity-only pointers: the production guard never dereferences images.
    auto* image = reinterpret_cast<plume::RenderTexture*>(uintptr_t(1));
    inputs.color = inputs.depth = inputs.motion = inputs.motionInvalidity = {image,{1280,720},0,0,1280,720};
    inputs.colorEncoding = temporal::ColorEncoding::Sdr;
    inputs.depthConvention = temporal::DepthConvention::Reversed;
    inputs.motionState = temporal::MotionState::Tracked;
    inputs.preExposure = 1;
    fsr::FrameMetadata frame{};
    frame.cameraValid = true;
    frame.cameraNear = FLT_MAX;
    frame.cameraFar = 10;
    frame.verticalFovRadians = 0.7f;
    frame.viewSpaceToMetersFactor = 1;
    frame.frameTimeDeltaMilliseconds = 16.6667f;
    frame.depthScale = 1.001f;
    frame.depthBias = -0.001f;
    const auto check = [&](const temporal::TemporalFrameInputs& i, const fsr::FrameMetadata& f, bool reset = false) {
        return fsr::CheckRecordGuard(true, false, true, config, i, f, {1920,1080}, reset, true).status;
    };
    Check(ValidSrRequest({inputs.plan,inputs}), "fixture uses a complete real FSR request");
    Check(check(inputs,frame) == Status::Ready, "valid reversed SDR frame accepted");
    auto changed = frame;
    changed.frameTimeDeltaMilliseconds = 0;
    Check(check(inputs,changed) == Status::InputUnavailable, "non-reset zero delta is frame-local, not capability failure");
    Check(check(inputs,changed,true) == Status::Ready, "history reset allows zero delta");
    Check(check(inputs,frame) == Status::Ready, "valid delta recovers under the unchanged request");
    for (float value : {-1.f, nan, inf}) {
        changed = frame; changed.frameTimeDeltaMilliseconds = value;
        Check(check(inputs,changed) == Status::InputUnavailable, "bad delta rejected without latching provider off");
        Check(check(inputs,changed,true) == Status::InputUnavailable, "reset does not legitimize negative/non-finite delta");
    }
    changed = frame; changed.cameraValid = false;
    Check(check(inputs,changed) == Status::InputUnavailable, "missing camera is transient");
    for (float value : {0.f, -1.f, nan, inf}) {
        changed = frame; changed.cameraFar = value;
        Check(check(inputs,changed) == Status::InputUnavailable, "invalid near distance rejected");
        changed = frame; changed.viewSpaceToMetersFactor = value;
        Check(check(inputs,changed) == Status::InputUnavailable, "invalid world scale rejected");
        changed = frame; changed.depthScale = value;
        Check(check(inputs,changed) == Status::InputUnavailable, "invalid depth scale rejected");
    }
    for (float value : {-1.f, 1.01f, nan, inf}) {
        changed = frame; changed.sharpness = value;
        Check(check(inputs,changed) == Status::InputUnavailable, "invalid RCAS metadata rejected");
    }
    for (float value : {0.f, 1.f}) {
        changed = frame; changed.sharpness = value; changed.enableSharpening = true;
        Check(check(inputs,changed) == Status::Ready, "RCAS boundary values remain valid");
    }
    auto bad = inputs; bad.jitter.pixelX = nan;
    Check(check(bad,frame) == Status::InputUnavailable, "NaN jitter rejected per-frame");
    bad = inputs; bad.jitter.pixelY = inf;
    Check(check(bad,frame) == Status::InputUnavailable, "infinite jitter rejected per-frame");
    for (float value : {0.f,-1.f,nan,inf}) {
        bad = inputs; bad.preExposure = value;
        Check(check(bad,frame) == Status::InputUnavailable, "invalid exposure is frame-local");
    }
    bad = inputs; bad.motionState = temporal::MotionState::Unavailable;
    Check(check(bad,frame) == Status::InputUnavailable, "incomplete motion follows existing scene-input recovery policy");
    bad = inputs; bad.color.texture = nullptr;
    Check(check(bad,frame) == Status::Unavailable, "missing color resource still fails closed");
    bad = inputs; bad.depth.texture = nullptr;
    Check(check(bad,frame) == Status::Unavailable, "missing depth resource still fails closed");
    bad = inputs; bad.depthConvention = temporal::DepthConvention::Forward;
    Check(bad.CompleteForConsumer(), "forward depth is structurally complete but incompatible with this FSR adapter");
    Check(check(bad,frame) == Status::Unavailable, "forward depth never reaches reversed-depth conversion");
    bad = inputs; bad.colorEncoding = temporal::ColorEncoding::HdrLinear;
    Check(check(bad,frame) == Status::Unavailable, "unsupported color encoding still fails closed");
    bad = inputs; --bad.color.width;
    Check(check(bad,frame) == Status::Unavailable, "mismatched render extent still fails closed");
    bad = inputs; ++bad.motion.allocation.width;
    Check(check(bad,frame) == Status::Unavailable, "padded motion allocation is not accepted");
    Check(fsr::CheckRecordGuard(true,false,true,config,inputs,frame,{1280,720},false,true).status == Status::Unavailable,
        "wrong output extent rejected");
    Check(fsr::CheckRecordGuard(true,false,true,config,inputs,frame,{1920,1080},false,false).status == Status::Unavailable,
        "exhausted use IDs remain a hard failure");
    Check(fsr::CheckRecordGuard(false,false,true,config,inputs,frame,{1920,1080},false,true).status == Status::NeedsReconfigure,
        "missing context uses drained reconfiguration");
    Check(fsr::CheckRecordGuard(true,true,true,config,inputs,frame,{1920,1080},false,true).status == Status::NeedsReconfigure,
        "poisoned context uses drained reconfiguration");
    Check(fsr::CheckRecordGuard(true,false,false,config,inputs,frame,{1920,1080},false,true).status == Status::NeedsReconfigure,
        "context configuration mismatch uses drained reconfiguration");

    Check(FailureAction(SrResultStatus::Ready) == SrFailureAction::None, "success records normally");
    Check(FailureAction(SrResultStatus::InputUnavailable) == SrFailureAction::FrameFallback, "record input loss does not disable request");
    Check(FailureAction(SrResultStatus::NeedsReconfigure) == SrFailureAction::Reconfigure, "reconfigure is neither success nor permanent failure");
    Check(FailureAction(SrResultStatus::DeviceLost) == SrFailureAction::StopDevice, "prepare/record device loss must stop GPU work");
    Check(FailureAction(SrResultStatus::Failed) == SrFailureAction::DisableRequest, "resource/SDK failures retain fail-closed behavior");
    Check(FailureAction(SrResultStatus::Unavailable) == SrFailureAction::DisableRequest, "unsupported capability remains unavailable");
    Check(FailureAction(static_cast<SrResultStatus>(255)) == SrFailureAction::DisableRequest, "unknown status fails closed");

    // The same planner observation used by the renderer must leave the exact
    // request active. This does not simulate SDK dispatch or image quality.
    frame_plan::UpscalerExecutionObservation skipped{};
    skipped.plan = inputs.plan;
    skipped.renderFrame = inputs.renderFrameId;
    skipped.actualProvider = upscaling::Upscaler::Fsr;
    skipped.reason = frame_plan::DlssEffectReason::NoEligibleScene;
    Check(planner.ReportUpscalerExecution(skipped), "record-input fallback observed for current request");
    const auto recovered = planner.Begin(planInput);
    Check(recovered.consumer == upscaling::TemporalConsumer::FsrSr &&
        recovered.requestSignature == inputs.plan.requestSignature && !planner.Observe().persistentFailure,
        "same FSR request survives rejected frame");
    Check(check(inputs,frame) == Status::Ready, "next valid frame remains eligible");
    std::printf("FSR dispatch/recovery policy: %u checks passed (CPU only)\n", checks);
}
