#pragma once

#include "fsr_upscaler.h"
#include "sr_scene_input_policy.h"

#if defined(LO_GPU_PLUME)
#include <cfloat>
#include <cmath>

namespace gpu::fsr {

// Value-only validation shared by the production adapter and CPU regression
// tests. No descriptors, command buffers, SDK state, or images are touched here.
inline bool ValidFrameMetadata(const FrameMetadata& m, bool resetHistory) {
    return m.cameraValid && std::isfinite(m.cameraFar) && m.cameraFar > 0 &&
        m.cameraNear == FLT_MAX && std::isfinite(m.verticalFovRadians) &&
        m.verticalFovRadians > 0 && m.verticalFovRadians < 3.14159265f &&
        std::isfinite(m.viewSpaceToMetersFactor) && m.viewSpaceToMetersFactor > 0 &&
        std::isfinite(m.frameTimeDeltaMilliseconds) &&
        (m.frameTimeDeltaMilliseconds > 0 || (resetHistory && m.frameTimeDeltaMilliseconds == 0)) &&
        std::isfinite(m.depthScale) && m.depthScale > 0 && std::isfinite(m.depthBias) &&
        std::isfinite(m.sharpness) && m.sharpness >= 0.0f && m.sharpness <= 1.0f;
}

struct RecordGuard {
    Status status = Status::Ready;
    const char* reason = "none";
};

inline RecordGuard CheckRecordGuard(bool contextReady, bool poisoned, bool configMatches,
    const Config& config, const temporal::TemporalFrameInputs& inputs,
    const FrameMetadata& frame, resolution::Size outputExtent, bool resetHistory, bool useIdAvailable) {
    // These states require the existing drained, next-frame reconfiguration
    // path. They are not evidence that the selected provider is unsupported.
    if (!contextReady) return {Status::NeedsReconfigure, "context_not_ready"};
    if (poisoned) return {Status::NeedsReconfigure, "context_poisoned"};
    if (!configMatches) return {Status::NeedsReconfigure, "config_mismatch"};
    if (!ValidFrameMetadata(frame, resetHistory)) return {Status::InputUnavailable, "camera_invalid"};
    if (!inputs.CompleteForConsumer()) {
        const bool transient = temporal::ClassifySrIncomplete(inputs) == temporal::SrSceneInputFailure::FrameFallback;
        return {transient ? Status::InputUnavailable : Status::Unavailable, "inputs_incomplete"};
    }
    if (inputs.colorEncoding != temporal::ColorEncoding::Sdr) return {Status::Unavailable, "color_not_sdr"};
    // The preparation shader canonicalizes reversed guest depth. A valid
    // forward-depth image is not interchangeable with that contract.
    if (inputs.depthConvention != temporal::DepthConvention::Reversed)
        return {Status::Unavailable, "depth_not_reversed"};
    if (inputs.motionState == temporal::MotionState::Unavailable) return {Status::InputUnavailable, "motion_unavailable"};
    if (inputs.color.width != config.renderWidth || inputs.color.height != config.renderHeight)
        return {Status::Unavailable, "color_extent"};
    if (inputs.depth.width != config.renderWidth || inputs.depth.height != config.renderHeight)
        return {Status::Unavailable, "depth_extent"};
    if (inputs.motion.width != config.renderWidth || inputs.motion.height != config.renderHeight)
        return {Status::Unavailable, "motion_extent"};
    if (inputs.motion.x || inputs.motion.y || inputs.motion.allocation.width != config.renderWidth ||
        inputs.motion.allocation.height != config.renderHeight)
        return {Status::Unavailable, "motion_allocation_or_origin"};
    if (outputExtent.width != config.outputWidth || outputExtent.height != config.outputHeight)
        return {Status::Unavailable, "output_extent"};
    if (!std::isfinite(inputs.jitter.pixelX) || !std::isfinite(inputs.jitter.pixelY))
        return {Status::InputUnavailable, "jitter_invalid"};
    if (!std::isfinite(inputs.preExposure) || inputs.preExposure <= 0)
        return {Status::InputUnavailable, "pre_exposure_invalid"};
    if (!useIdAvailable) return {Status::Unavailable, "use_id_exhausted"};
    return {};
}

} // namespace gpu::fsr
#endif
