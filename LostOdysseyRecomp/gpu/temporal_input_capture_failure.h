#pragma once
#include <cstdint>

namespace gpu::temporal {
enum class InputCaptureFailure : uint32_t {
    None = 0,
    MissingCommands = 1,
    MissingSource = 2,
    SceneNotReady = 3,
    SceneFrameMismatch = 4,
    MissingCamera = 5,
    DepthOrdinalMismatch = 6,
    ColorAlreadyCaptured = 7,
    SceneColorExtentMismatch = 8,
    PlanExtentInvalid = 9,
    PlanExtentMismatch = 10,
    OwnedExtentInvalid = 11,
    OwnedDepthAllocationFailed = 12,
    OwnedColorAllocationFailed = 13,
};
inline const char* InputCaptureFailureName(InputCaptureFailure reason) {
    switch (reason) {
    case InputCaptureFailure::None: return "none";
    case InputCaptureFailure::MissingCommands: return "missing_commands";
    case InputCaptureFailure::MissingSource: return "missing_source";
    case InputCaptureFailure::SceneNotReady: return "scene_not_ready";
    case InputCaptureFailure::SceneFrameMismatch: return "scene_frame_mismatch";
    case InputCaptureFailure::MissingCamera: return "missing_camera";
    case InputCaptureFailure::DepthOrdinalMismatch: return "depth_ordinal_mismatch";
    case InputCaptureFailure::ColorAlreadyCaptured: return "color_already_captured";
    case InputCaptureFailure::SceneColorExtentMismatch: return "scene_color_extent_mismatch";
    case InputCaptureFailure::PlanExtentInvalid: return "plan_extent_invalid";
    case InputCaptureFailure::PlanExtentMismatch: return "plan_extent_mismatch";
    case InputCaptureFailure::OwnedExtentInvalid: return "owned_extent_invalid";
    case InputCaptureFailure::OwnedDepthAllocationFailed: return "owned_depth_allocation_failed";
    case InputCaptureFailure::OwnedColorAllocationFailed: return "owned_color_allocation_failed";
    }
    return "unknown";
}
} // namespace gpu::temporal
