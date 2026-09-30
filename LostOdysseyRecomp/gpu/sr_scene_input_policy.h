#pragma once

#include "temporal_frame_inputs.h"
#include "temporal_input_capture_failure.h"

namespace gpu::temporal {

enum class SrSceneInputFailure : uint8_t { FrameFallback, RequestFailure };

// Called only after CaptureColorInputs has rejected this frame. A missing or
// conflicting scene observation may recover on the next frame; missing GPU
// objects or incompatible planned extents are request/resource failures.
inline constexpr SrSceneInputFailure ClassifySrCaptureFailure(InputCaptureFailure reason) {
    switch (reason) {
    case InputCaptureFailure::SceneNotReady:
    case InputCaptureFailure::SceneFrameMismatch:
    case InputCaptureFailure::MissingCamera:
    case InputCaptureFailure::DepthOrdinalMismatch:
    case InputCaptureFailure::ColorAlreadyCaptured:
        return SrSceneInputFailure::FrameFallback;
    default:
        return SrSceneInputFailure::RequestFailure;
    }
}

// CaptureColorInputs succeeded, but no usable motion/visibility for this
// scene frame was produced. Keep CompleteForConsumer strict and retry only
// when the next frame supplies real motion and invalidity images.
inline constexpr SrSceneInputFailure ClassifySrIncomplete(const TemporalFrameInputs& inputs,
    bool resourceFailed = false) {
    return !resourceFailed && inputs.color.Complete() && inputs.depth.Complete() &&
        (!upscaling::RequiresMotionDepth(inputs.plan.consumer, inputs.plan.frameGeneration) ||
         KnownDepthConvention(inputs.depthConvention)) &&
        (!inputs.currentInputsComplete || inputs.motionState == MotionState::Unavailable ||
         !inputs.motion.Complete() || !inputs.motionInvalidity.Complete())
        ? SrSceneInputFailure::FrameFallback : SrSceneInputFailure::RequestFailure;
}
} // namespace gpu::temporal
