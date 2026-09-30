#pragma once

#include "frame_generation_present.h"

#include <plume_render_interface.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace gpu::frame_generation {

enum class SnapshotPurpose : uint8_t { DiagnosticFiveImages, CompositedBackbuffer };

// The diagnostic captures five images; the composited-backbuffer path owns only
// the depth and motion images that Streamline reads. Pixels become stable only
// when the producer batch completes and its checked fence has signaled.
struct ProducerSnapshot {
    temporal::TemporalFrameInputs inputs{};
    TextureLease sourceColor{}, depth{}, motion{}, motionInvalidity{};
    TextureLease sceneColorCandidate{};
    std::array<plume::RenderFormat, 5> storageFormats{};
    SnapshotPurpose purpose = SnapshotPurpose::DiagnosticFiveImages;
    resolution::Size qualifiedInputExtent{};
    bool inputsQualifiedAtCapture = false;
    // Set at the successful SR composite boundary, from the actual HostTexture.
    uint64_t lineageOwner = 0, resolveSourceAllocation = 0, resolveSourceGeneration = 0;
    uint64_t producerSerial = 0;
    bool producerOnPresentQueue = false;
    bool producerWaitFailed = false;
    bool lineageCanceled = false; // cancellation does not fabricate GPU completion/discard
    bool producerCompleted = false;
    bool producerDiscarded = false;
    UiSeparation ui = UiSeparation::Unavailable;
};

inline bool FullAllocation(const temporal::TextureRegion& region) {
    return region.Complete() && region.x == 0 && region.y == 0 &&
        region.width == region.allocation.width &&
        region.height == region.allocation.height;
}

inline std::shared_ptr<ProducerSnapshot> RecordProducerSnapshot(
    plume::RenderDevice* device, plume::RenderCommandList* commands,
    const temporal::TemporalFrameInputs& inputs,
    plume::RenderFormat sourceColorFormat, plume::RenderTexture* sceneCandidate,
    plume::RenderFormat sceneFormat, resolution::Size sceneExtent,
    SnapshotPurpose purpose = SnapshotPurpose::DiagnosticFiveImages) {
    if (!device || !commands || !inputs.currentInputsComplete ||
        !inputs.CompleteForConsumer() ||
        !temporal::KnownDepthConvention(inputs.depthConvention) ||
        inputs.motionState == temporal::MotionState::Unavailable ||
        !FullAllocation(inputs.color) || !FullAllocation(inputs.depth) ||
        !FullAllocation(inputs.motion) || !FullAllocation(inputs.motionInvalidity) ||
        sourceColorFormat == plume::RenderFormat::UNKNOWN ||
        sceneFormat == plume::RenderFormat::UNKNOWN ||
        !sceneCandidate || !sceneExtent.width || !sceneExtent.height ||
        (purpose != SnapshotPurpose::DiagnosticFiveImages &&
         purpose != SnapshotPurpose::CompositedBackbuffer))
        return {};

    const auto sourceExtent = inputs.color.allocation;
    const auto sameSourceExtent = [&](const temporal::TextureRegion& region) {
        return region.allocation.width == sourceExtent.width &&
            region.allocation.height == sourceExtent.height;
    };
    if (!sameSourceExtent(inputs.depth) || !sameSourceExtent(inputs.motion) ||
        !sameSourceExtent(inputs.motionInvalidity))
        return {};

    const std::array<plume::RenderTexture*, 5> originals{
        inputs.color.texture, inputs.depth.texture, inputs.motion.texture,
        inputs.motionInvalidity.texture, sceneCandidate};
    const std::array<plume::RenderFormat, 5> formats{
        sourceColorFormat, plume::RenderFormat::R32_FLOAT,
        plume::RenderFormat::R16G16_FLOAT, plume::RenderFormat::R8_UNORM,
        sceneFormat};
    const std::array<resolution::Size, 5> sizes{
        sourceExtent, sourceExtent, sourceExtent, sourceExtent, sceneExtent};

    const bool diagnostic = purpose == SnapshotPurpose::DiagnosticFiveImages;
    const auto selected = [diagnostic](size_t i) { return diagnostic || i == 1 || i == 2; };
    // Complete all selected allocations before recording any copy. A failure
    // leaves no partially recorded capture in the producer batch.
    std::array<std::shared_ptr<plume::RenderTexture>, 5> copies;
    for (size_t i = 0; i < copies.size(); ++i) {
        if (!selected(i)) continue;
        auto image = device->createTexture(plume::RenderTextureDesc::Texture2D(
            sizes[i].width, sizes[i].height, 1, formats[i]));
        if (!image) return {};
        copies[i] = std::move(image);
    }
    auto snapshot = std::make_shared<ProducerSnapshot>();
    snapshot->inputs = inputs;
    snapshot->purpose = purpose;
    snapshot->qualifiedInputExtent = sourceExtent;
    snapshot->inputsQualifiedAtCapture = true;
    for (size_t i = 0; i < copies.size(); ++i)
        if (selected(i)) snapshot->storageFormats[i] = formats[i];
    auto lease = [&](size_t i) {
        TextureLease value;
        value.region = {copies[i].get(), sizes[i], 0, 0, sizes[i].width, sizes[i].height};
        value.lifetime = copies[i];
        return value;
    };
    if (diagnostic) snapshot->sourceColor = lease(0);
    snapshot->depth = lease(1);
    snapshot->motion = lease(2);
    if (diagnostic) {
        snapshot->motionInvalidity = lease(3);
        snapshot->sceneColorCandidate = lease(4);
    }
    // No borrowed pointer may escape the producer slot. The composited path
    // has already qualified color and invalidity but does not consume them.
    snapshot->inputs.color = diagnostic ? snapshot->sourceColor.region : temporal::TextureRegion{};
    snapshot->inputs.depth = snapshot->depth.region;
    snapshot->inputs.motion = snapshot->motion.region;
    snapshot->inputs.motionInvalidity = diagnostic ? snapshot->motionInvalidity.region : temporal::TextureRegion{};
    // These optional views have no owned copies; their borrowed pointers and
    // provenance must not escape the producer slot through this snapshot.
    snapshot->inputs.materialInstability = {};
    snapshot->inputs.fsrMask = {};

    for (size_t i = 0; i < copies.size(); ++i) {
        if (!selected(i)) continue;
        commands->barriers(plume::RenderBarrierStage::COPY,
            plume::RenderTextureBarrier(originals[i], plume::RenderTextureLayout::COPY_SOURCE));
        commands->barriers(plume::RenderBarrierStage::COPY,
            plume::RenderTextureBarrier(copies[i].get(), plume::RenderTextureLayout::COPY_DEST));
        commands->copyTextureRegion(
            plume::RenderTextureCopyLocation::Subresource(copies[i].get()),
            plume::RenderTextureCopyLocation::Subresource(originals[i]));
        commands->barriers(plume::RenderBarrierStage::ALL,
            plume::RenderTextureBarrier(originals[i], plume::RenderTextureLayout::SHADER_READ));
        commands->barriers(plume::RenderBarrierStage::ALL,
            plume::RenderTextureBarrier(copies[i].get(), plume::RenderTextureLayout::SHADER_READ));
    }
    return snapshot;
}

} // namespace gpu::frame_generation
