#include "gpu/dlss_fg_completion.h"
#ifndef LO_FG_COMPLETION_ONLY
#include "gpu/temporal_frame_inputs.h"
#endif

#include <cstdio>
#include <stdexcept>

namespace {
unsigned checks = 0;
void Check(bool ok, const char* reason) {
    ++checks;
    if (!ok) throw std::runtime_error(reason);
}
void CompletionContract() {
    using gpu::dlss_fg::PresentQueueCompletion;
    PresentQueueCompletion completion;
    Check(completion.CanSubmit() && !completion.Pending(), "new session has no pending marker");
    Check(!completion.Completed(0, true), "zero fence/serial is not completion evidence");
    Check(completion.Submitted(true), "successful marker publishes serial");
    Check(completion.SubmittedSerial() == 1 && completion.CompletedSerial() == 0, "submit is not completion");
    Check(completion.Pending() && !completion.CanSubmit(), "pending marker cannot be reset/reused");
    Check(!completion.Completed(1, false) && completion.Pending(), "failed wait retains input ownership");
    Check(!completion.Completed(0, true) && completion.Pending(), "earlier producer fence cannot retire SDK inputs");
    Check(!completion.Completed(2, true) && completion.Pending(), "future serial cannot retire SDK inputs");
    Check(completion.Completed(1, true) && completion.CanSubmit(), "checked matching wait retires marker");
    Check(!completion.Completed(1, true), "duplicate completion cannot retire another use");
    Check(completion.Submitted(true) && completion.SubmittedSerial() == 2, "serial is monotonic across uses");
    Check(!completion.Completed(1, true) && completion.Pending(), "old successful wait cannot release new frame");
    Check(completion.Completed(2, true), "second checked marker completes");

    PresentQueueCompletion failedSubmit;
    Check(!failedSubmit.Submitted(false), "reset/submit failure is reported");
    Check(failedSubmit.SubmittedSerial() == 0, "failed submission publishes no serial");
    Check(!failedSubmit.CanSubmit(), "failed marker is fail-closed, not reusable");
    Check(!failedSubmit.Completed(0, true), "idle or zero cannot recover a missing marker");
    Check(!failedSubmit.Submitted(true), "failure cannot silently reenable the session");

    PresentQueueCompletion overwritten;
    Check(overwritten.Submitted(true), "first marker before erroneous reuse");
    Check(!overwritten.Submitted(true), "second pending marker is rejected");
    Check(overwritten.Pending() && overwritten.SubmittedSerial() == 1, "rejected overwrite preserves serial");
    Check(!overwritten.Completed(1, true) && !overwritten.CanSubmit(), "invalid reuse cannot be disguised as clean completion");
}
#ifndef LO_FG_COMPLETION_ONLY
void InputContract() {
    using namespace gpu;
    temporal::TemporalFrameInputs in{};
    // Opaque, never-dereferenced addresses are sufficient for metadata checks.
    in.color = {reinterpret_cast<plume::RenderTexture*>(uintptr_t(1)), {640, 360}, 0, 0, 640, 360};
    in.depth = {reinterpret_cast<plume::RenderTexture*>(uintptr_t(2)), {640, 360}, 0, 0, 640, 360};
    in.motion = {reinterpret_cast<plume::RenderTexture*>(uintptr_t(3)), {640, 360}, 0, 0, 640, 360};
    in.motionInvalidity = {reinterpret_cast<plume::RenderTexture*>(uintptr_t(4)), {640, 360}, 0, 0, 640, 360};
    in.plan.requestedUpscaler = upscaling::Upscaler::Fsr;
    in.plan.consumer = upscaling::TemporalConsumer::FsrSr;
    in.renderFrameId = 10; in.temporalEpoch = 2;
    in.currentInputsComplete = true; in.cameraValid = true;
    in.depthConvention = temporal::DepthConvention::Reversed;
    in.motionState = temporal::MotionState::Hybrid;
    for (auto quality : {upscaling::FsrQuality::Quality, upscaling::FsrQuality::Balanced,
                        upscaling::FsrQuality::Performance, upscaling::FsrQuality::NativeAA}) {
        in.plan.fsrQuality = quality;
        const auto plan = in.plan;
        Check(in.CompleteForConsumer(), "P1 retains Hybrid SR input eligibility with FG off");
        Check(in.CompleteForFrameGeneration(), "same-frame Hybrid can feed experimental composited FG");
        Check(in.plan == plan, "FG admission must not rewrite FSR ownership/quality/geometry");
    }
    in.plan.frameGeneration = upscaling::FrameGeneration::Dlss2x;
    Check(!in.CompleteForConsumer(), "existing Hybrid+FG SR restriction remains intact");
    in.plan.frameGeneration = upscaling::FrameGeneration::Off;
    in.motionState = temporal::MotionState::Tracked;
    Check(in.CompleteForConsumer() && in.CompleteForFrameGeneration(), "tracked SR input can qualify independently");
    in.color = {}; in.motionInvalidity = {};
    Check(!in.CompleteForConsumer(), "two-image snapshot cannot masquerade as complete SR input");
    Check(in.CompleteForFrameGeneration(), "FG accepts qualified depth/motion-only snapshot metadata");
    in.motionState = temporal::MotionState::ResetInitialization;
    Check(in.CompleteForFrameGeneration(), "first-frame initialized motion is eligible for FG reset");
    in.motionState = temporal::MotionState::Unavailable;
    Check(!in.CompleteForFrameGeneration(), "missing motion blocks FG");
    in.motionState = temporal::MotionState(99);
    Check(!in.CompleteForFrameGeneration(), "unknown motion state blocks FG");
    in.motionState = temporal::MotionState::Tracked;
    auto good = in;
    in.renderFrameId = 0; Check(!in.CompleteForFrameGeneration(), "unknown scene frame blocks FG");
    in = good; in.temporalEpoch = 0; Check(!in.CompleteForFrameGeneration(), "unknown temporal epoch blocks FG");
    in = good; in.currentInputsComplete = false; Check(!in.CompleteForFrameGeneration(), "incomplete capture blocks FG");
    in = good; in.cameraValid = false; Check(!in.CompleteForFrameGeneration(), "missing camera blocks FG");
    in = good; in.depthConvention = temporal::DepthConvention::Unknown;
    Check(!in.CompleteForFrameGeneration(), "unknown depth convention blocks FG");
    in = good; --in.motion.width; Check(!in.CompleteForFrameGeneration(), "mismatched depth/MV extent blocks FG");
    in = good; in.depth.texture = nullptr; Check(!in.CompleteForFrameGeneration(), "missing depth texture blocks FG");
    in = good; in.motion.x = 1; --in.motion.width;
    Check(!in.CompleteForFrameGeneration(), "offset motion region blocks this full-frame FG contract");
    in = good; in.motionState = temporal::MotionState::Hybrid;
    in.color = {}; in.motionInvalidity = {};
    Check(in.CompleteForFrameGeneration(), "Hybrid retains eligibility in the depth/motion-only snapshot");
    in.currentInputsComplete = false;
    Check(!in.CompleteForFrameGeneration(), "Hybrid cannot cover an incomplete capture");
    in = good; in.motionState = temporal::MotionState::Hybrid; --in.motion.width;
    Check(!in.CompleteForFrameGeneration(), "Hybrid cannot cover a mismatched depth/motion extent");
}
#endif
}
int main() {
    try {
        CompletionContract();
#ifndef LO_FG_COMPLETION_ONLY
        InputContract();
#endif
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s (%u checks)\n", error.what(), checks);
        return 1;
    }
#ifdef LO_FG_COMPLETION_ONLY
    std::printf("PASS: %u game completion-serial checks (CPU only; no SDK/GPU)\n", checks);
#else
    std::printf("PASS: %u game completion/input contract checks (CPU only; no SDK/GPU)\n", checks);
#endif
}
