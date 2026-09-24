#pragma once

#include "temporal_frame_inputs.h"

namespace gpu::fsr {

inline constexpr float kReactiveMax = 0.9f;

enum class FsrMaskRejection : uint8_t {
    None, Semantic, Coverage, Region, Extent, Frame, TemporalEpoch,
    GeometryEpoch, DeviceEpoch, CapturedColor, ColorOrdinal, SourceIdentity,
    Image, Format, Allocation, Layout, ScratchUnavailable
};
struct FsrMaskDecision {
    bool useReactive = false;
    FsrMaskRejection rejection = FsrMaskRejection::Semantic;
};
inline constexpr const char* FsrMaskRejectionName(FsrMaskRejection reason) {
    switch (reason) {
    case FsrMaskRejection::None: return "none";
    case FsrMaskRejection::Semantic: return "semantic";
    case FsrMaskRejection::Coverage: return "coverage";
    case FsrMaskRejection::Region: return "region";
    case FsrMaskRejection::Extent: return "extent";
    case FsrMaskRejection::Frame: return "frame";
    case FsrMaskRejection::TemporalEpoch: return "temporal_epoch";
    case FsrMaskRejection::GeometryEpoch: return "geometry_epoch";
    case FsrMaskRejection::DeviceEpoch: return "device_epoch";
    case FsrMaskRejection::CapturedColor: return "captured_color";
    case FsrMaskRejection::ColorOrdinal: return "color_ordinal";
    case FsrMaskRejection::SourceIdentity: return "source_identity";
    case FsrMaskRejection::Image: return "image";
    case FsrMaskRejection::Format: return "format";
    case FsrMaskRejection::Allocation: return "allocation";
    case FsrMaskRejection::Layout: return "layout";
    case FsrMaskRejection::ScratchUnavailable: return "scratch_unavailable";
    }
    return "unknown";
}

// CPU identity and region contract; native Vulkan metadata is checked by the
// adapter immediately before descriptor binding.
inline FsrMaskDecision QualifyFsrMask(const temporal::TemporalFrameInputs& inputs) {
    const auto& mask = inputs.fsrMask;
    const auto& source = mask.provenance;
    const auto& region = mask.sceneContribution;
    using Reject = FsrMaskRejection;
    if (mask.semantic != temporal::FsrMaskSemantic::ConservativeTransparentAlpha) return {false, Reject::Semantic};
    if (mask.coverage != temporal::FsrMaskCoverage::Partial) return {false, Reject::Coverage};
    if (!region.Complete()) return {false, Reject::Region};
    if (region.width != inputs.color.width || region.height != inputs.color.height) return {false, Reject::Extent};
    if (!source.renderFrameId || source.renderFrameId != inputs.renderFrameId) return {false, Reject::Frame};
    if (!source.temporalEpoch || source.temporalEpoch != inputs.temporalEpoch) return {false, Reject::TemporalEpoch};
    if (!source.geometryEpoch || source.geometryEpoch != inputs.plan.geometryEpoch) return {false, Reject::GeometryEpoch};
    if (!source.deviceEpoch || source.deviceEpoch != inputs.plan.deviceEpoch) return {false, Reject::DeviceEpoch};
    if (!source.capturedColor || source.capturedColor != inputs.color.texture) return {false, Reject::CapturedColor};
    if (!source.colorOrdinal || source.colorOrdinal != inputs.colorOrdinal) return {false, Reject::ColorOrdinal};
    if (!source.sourceAllocation || !source.sourceWriteOrdinal) return {false, Reject::SourceIdentity};
    return {true, Reject::None};
}

} // namespace gpu::fsr
