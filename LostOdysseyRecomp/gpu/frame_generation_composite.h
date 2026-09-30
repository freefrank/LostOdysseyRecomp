#pragma once

#include "frame_generation_snapshot.h"
#include "dlss_fg_policy.h"

#include <cmath>
#include <cstdint>
#include <memory>

namespace gpu::frame_generation {

inline bool NativeCompositePlan(const frame_plan::FramePlan& plan) {
    return plan.requestedUpscaler == upscaling::Upscaler::Off &&
        plan.consumer == upscaling::TemporalConsumer::None && !plan.requiresReadback &&
        !plan.inputProbe && !plan.failed && plan.frameGeneration == upscaling::FrameGeneration::Off &&
        plan.width && plan.height;
}
inline bool CompositePlanSupported(const frame_plan::FramePlan& plan) {
    return dlss_fg::CompositePlanSupported(plan) || NativeCompositePlan(plan);
}
inline resolution::Size CompositeSourceExtent(const frame_plan::FramePlan& plan) {
    return NativeCompositePlan(plan) ? resolution::Size{plan.width, plan.height} :
        resolution::Size{plan.output.width, plan.output.height};
}

// Source storage may include alignment rows (2560x1472 for a 2560x1440
// scene). Only the full, selected output resolve defines the backbuffer.
struct CompositeResolveGeometry {
    resolution::Size sourceAllocation{}, targetAllocation{};
    uint32_t writeX = 0, writeY = 0, writeWidth = 0, writeHeight = 0;

    bool Matches(const frame_plan::FramePlan& plan) const {
        const auto output = CompositeSourceExtent(plan);
        return output.width && output.height &&
            sourceAllocation.width >= output.width &&
            sourceAllocation.height >= output.height &&
            targetAllocation.width == output.width &&
            targetAllocation.height == output.height &&
            !writeX && !writeY &&
            writeWidth == output.width && writeHeight == output.height;
    }
};

// The selected, fully composited game backbuffer is intercepted by Streamline.
// Depth and motion remain producer-owned snapshots. This path does not claim
// that a HUD-less image or a separate UI image exists.
struct CompositeHandoff {
    std::shared_ptr<ProducerSnapshot> producer;
    frame_plan::FramePlan plan{};
    uint64_t frame = 0, historyEpoch = 0;
    uint64_t sourceAllocation = 0, resolveOrdinal = 0, targetAllocation = 0;
    uint64_t resolveSubmissionSerial = 0;
    bool resolveOnPresentQueue = false;
    bool resolveCompleted = false, resolveDiscarded = false, resolveWaitFailed = false;
    uint32_t outputWidth = 0, outputHeight = 0;

    bool MetadataReady() const {
        if (!producer) return false;
        const auto& p = *producer;
        const auto& input = p.inputs;
        const bool diagnosticInputs = p.purpose == SnapshotPurpose::DiagnosticFiveImages &&
            input.CompleteForConsumer() && p.sourceColor.Matches(input.color) &&
            p.motionInvalidity.Matches(input.motionInvalidity) && p.sceneColorCandidate.Complete();
        const bool compositedInputs = p.purpose == SnapshotPurpose::CompositedBackbuffer &&
            !input.color.texture && !input.motionInvalidity.texture &&
            !input.materialInstability.texture && !input.fsrMask.sceneContribution.texture &&
            !p.sourceColor.region.texture && !p.sourceColor.lifetime &&
            !p.motionInvalidity.region.texture && !p.motionInvalidity.lifetime &&
            !p.sceneColorCandidate.region.texture && !p.sceneColorCandidate.lifetime;
        return CompositePlanSupported(plan) &&
            p.ui == UiSeparation::Unavailable &&
            !p.lineageCanceled && !p.producerDiscarded && !p.producerWaitFailed &&
            !resolveDiscarded && !resolveWaitFailed &&
            p.inputsQualifiedAtCapture &&
            (diagnosticInputs || compositedInputs) &&
            input.CompleteForFrameGeneration() &&
            std::isfinite(input.cameraRaster.width) && input.cameraRaster.width > 0.0 &&
            std::isfinite(input.cameraRaster.height) && input.cameraRaster.height > 0.0 &&
            std::isfinite(input.frameTimeDeltaMilliseconds) &&
            input.frameTimeDeltaMilliseconds > 0.0f &&
            p.depth.Matches(input.depth) && p.motion.Matches(input.motion) &&
            input.depth.width == p.qualifiedInputExtent.width &&
            input.depth.height == p.qualifiedInputExtent.height &&
            input.motion.width == p.qualifiedInputExtent.width &&
            input.motion.height == p.qualifiedInputExtent.height &&
            input.plan == plan && input.renderFrameId == frame &&
            input.temporalEpoch == historyEpoch &&
            p.resolveSourceAllocation == sourceAllocation &&
            sourceAllocation && resolveOrdinal && targetAllocation &&
            outputWidth == CompositeSourceExtent(plan).width && outputHeight == CompositeSourceExtent(plan).height &&
            outputWidth && outputHeight;
    }
    bool ReadyForOrderedSubmission() const {
        return MetadataReady() && producer->producerOnPresentQueue && resolveOnPresentQueue &&
            producer->producerSerial && resolveSubmissionSerial &&
            producer->producerSerial <= resolveSubmissionSerial;
    }
    bool Ready() const {
        return MetadataReady() && producer->producerSerial && resolveSubmissionSerial &&
            producer->producerCompleted && resolveCompleted;
    }
};

} // namespace gpu::frame_generation
