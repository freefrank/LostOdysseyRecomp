#include <gpu/frame_generation_snapshot.h>
#include <gpu/frame_generation_composite.h>
#include <plume_vulkan.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace plume { std::unique_ptr<RenderInterface> CreateVulkanInterface(); }

namespace {
using namespace plume;
constexpr uint32_t Width = 4, Height = 4, SceneWidth = 6, Pitch = 256;
constexpr uint64_t SliceBytes = Pitch * Height;
constexpr std::array<RenderFormat, 5> Formats{
    RenderFormat::R8G8B8A8_UNORM, RenderFormat::R32_FLOAT,
    RenderFormat::R16G16_FLOAT, RenderFormat::R8_UNORM,
    RenderFormat::R8G8B8A8_UNORM};
constexpr std::array<uint32_t, 5> PixelBytes{4, 4, 4, 1, 4};

void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

void FillSlice(uint8_t* bytes, size_t slice, uint8_t seed) {
    auto* base = bytes + slice * SliceBytes;
    for (uint32_t y = 0; y < Height; ++y)
        for (uint32_t x = 0; x < (slice % 5 == 4 ? SceneWidth : Width); ++x)
            for (uint32_t c = 0; c < PixelBytes[slice % 5]; ++c)
                base[y * Pitch + x * PixelBytes[slice % 5] + c] =
                    uint8_t(seed + slice * 17 + y * 7 + x * 3 + c);
}

void Run() {
    auto api = CreateVulkanInterface(); Check(bool(api), "Vulkan interface");
    auto device = api->createDevice(); Check(bool(device), "Vulkan device");
    auto queue = device->createCommandQueue(RenderCommandListType::DIRECT);
    auto list = queue ? queue->createCommandList() : nullptr;
    auto fence = device->createCommandFence();
    Check(queue && list && fence, "queue, list, fence");
    std::array<std::unique_ptr<RenderTexture>, 5> source;
    for (size_t i = 0; i < source.size(); ++i) {
        source[i] = device->createTexture(RenderTextureDesc::Texture2D(
            i == 4 ? SceneWidth : Width, Height, 1, Formats[i]));
        Check(bool(source[i]), "source allocation");
    }
    auto upload = device->createBuffer(RenderBufferDesc::UploadBuffer(10 * SliceBytes));
    auto readback = device->createBuffer(RenderBufferDesc::ReadbackBuffer(12 * SliceBytes));
    Check(upload && readback, "staging allocation");
    auto* data = static_cast<uint8_t*>(upload->map()); Check(data != nullptr, "map upload");
    std::memset(data, 0, 10 * SliceBytes);
    for (size_t i = 0; i < 10; ++i) FillSlice(data, i, i < 5 ? 11 : 119);
    upload->unmap();

    gpu::temporal::TemporalFrameInputs inputs{};
    inputs.plan.consumer = gpu::upscaling::TemporalConsumer::FsrSr;
    inputs.plan.requestedUpscaler = gpu::upscaling::Upscaler::Fsr;
    inputs.renderFrameId = 41;
    inputs.temporalEpoch = 7;
    inputs.depthAllocation = 13;
    inputs.colorOrdinal = 99;
    inputs.colorEncoding = gpu::temporal::ColorEncoding::Sdr;
    inputs.depthConvention = gpu::temporal::DepthConvention::Reversed;
    inputs.motionState = gpu::temporal::MotionState::Tracked;
    inputs.currentInputsComplete = true;
    inputs.color = {source[0].get(), {Width, Height}, 0, 0, Width, Height};
    inputs.depth = {source[1].get(), {Width, Height}, 0, 0, Width, Height};
    inputs.motion = {source[2].get(), {Width, Height}, 0, 0, Width, Height};
    inputs.motionInvalidity = {source[3].get(), {Width, Height}, 0, 0, Width, Height};
    inputs.materialInstability = inputs.motionInvalidity;
    inputs.fsrMask.sceneContribution = inputs.motionInvalidity;
    inputs.fsrMask.provenance.capturedColor = source[0].get();
    inputs.fsrMask.provenance.renderFrameId = inputs.renderFrameId;
    inputs.fsrMask.provenance.temporalEpoch = inputs.temporalEpoch;
    inputs.fsrMask.coverage = gpu::temporal::FsrMaskCoverage::Partial;
    inputs.fsrMask.semantic = gpu::temporal::FsrMaskSemantic::ConservativeTransparentAlpha;

    auto invalid = inputs;
    invalid.motion.x = 1; // Region extends beyond its allocation.
    list->begin();
    Check(!gpu::frame_generation::RecordProducerSnapshot(device.get(), list.get(), invalid,
        Formats[0], source[4].get(), Formats[4], {SceneWidth, Height}),
        "invalid motion region rejected before recording");
    invalid = inputs;
    invalid.motion.texture = nullptr;
    Check(!gpu::frame_generation::RecordProducerSnapshot(device.get(), list.get(), invalid,
        Formats[0], source[4].get(), Formats[4], {SceneWidth, Height}),
        "missing motion rejected even with FG off");
    invalid = inputs;
    invalid.motionInvalidity.texture = nullptr;
    Check(!gpu::frame_generation::RecordProducerSnapshot(device.get(), list.get(), invalid,
        Formats[0], source[4].get(), Formats[4], {SceneWidth, Height},
        gpu::frame_generation::SnapshotPurpose::CompositedBackbuffer),
        "runtime capture still qualifies unused invalidity input");

    auto uploadSet = [&](size_t base) {
        for (size_t i = 0; i < source.size(); ++i) {
            list->barriers(RenderBarrierStage::COPY,
                RenderTextureBarrier(source[i].get(), RenderTextureLayout::COPY_DEST));
            list->copyTextureRegion(RenderTextureCopyLocation::Subresource(source[i].get()),
                RenderTextureCopyLocation::PlacedFootprint(upload.get(), Formats[i],
                    i == 4 ? SceneWidth : Width, Height, 1, Pitch / PixelBytes[i],
                    (base + i) * SliceBytes));
            list->barriers(RenderBarrierStage::ALL,
                RenderTextureBarrier(source[i].get(), RenderTextureLayout::SHADER_READ));
        }
    };
    uploadSet(0);
    auto snapshot = gpu::frame_generation::RecordProducerSnapshot(device.get(), list.get(),
        inputs, Formats[0], source[4].get(), Formats[4], {SceneWidth, Height});
    Check(bool(snapshot), "record five owned copies");
    Check(snapshot->purpose == gpu::frame_generation::SnapshotPurpose::DiagnosticFiveImages &&
        snapshot->inputsQualifiedAtCapture, "diagnostic five-image contract retained");
    Check(snapshot->ui == gpu::frame_generation::UiSeparation::Unavailable &&
        !snapshot->producerCompleted && !snapshot->producerDiscarded && !snapshot->producerSerial,
        "unavailable UI and pending producer state");
    const std::array<gpu::frame_generation::TextureLease*, 5> leases{
        &snapshot->sourceColor, &snapshot->depth, &snapshot->motion,
        &snapshot->motionInvalidity, &snapshot->sceneColorCandidate};
    for (size_t i = 0; i < leases.size(); ++i) {
        Check(leases[i]->Complete() && leases[i]->region.texture != source[i].get(),
            "copy has independent owned allocation");
        for (size_t j = 0; j < i; ++j)
            Check(leases[i]->region.texture != leases[j]->region.texture,
                "copies do not alias one another");
    }
    Check(snapshot->inputs.renderFrameId == inputs.renderFrameId &&
        snapshot->inputs.temporalEpoch == inputs.temporalEpoch &&
        snapshot->inputs.depthAllocation == inputs.depthAllocation &&
        snapshot->inputs.colorOrdinal == inputs.colorOrdinal &&
        snapshot->inputs.color.texture == leases[0]->region.texture &&
        snapshot->inputs.depth.texture == leases[1]->region.texture &&
        snapshot->inputs.motion.texture == leases[2]->region.texture &&
        snapshot->inputs.motionInvalidity.texture == leases[3]->region.texture &&
        !snapshot->inputs.materialInstability.texture &&
        !snapshot->inputs.fsrMask.sceneContribution.texture &&
        !snapshot->inputs.fsrMask.provenance.capturedColor &&
        !snapshot->inputs.fsrMask.provenance.renderFrameId &&
        !snapshot->inputs.fsrMask.provenance.temporalEpoch &&
        snapshot->inputs.fsrMask.coverage == gpu::temporal::FsrMaskCoverage::Unavailable &&
        snapshot->inputs.fsrMask.semantic == gpu::temporal::FsrMaskSemantic::Unknown &&
        inputs.color.texture == source[0].get() && inputs.fsrMask.provenance.capturedColor == source[0].get(),
        "metadata retained, borrowed views cleared, original inputs untouched");
    gpu::frame_plan::FramePlan compositePlan{};
    compositePlan.output = {{2560, 1440}, 0, 0, 2560, 1440};
    compositePlan.requestedUpscaler = gpu::upscaling::Upscaler::Dlss;
    compositePlan.consumer = gpu::upscaling::TemporalConsumer::DlssSr;
    auto runtimeInputs = inputs;
    runtimeInputs.plan = compositePlan;
    runtimeInputs.cameraValid = true;
    runtimeInputs.cameraRaster = {0, 0, double(Width), double(Height)};
    runtimeInputs.frameTimeDeltaMilliseconds = 16.0f;
    auto runtime = gpu::frame_generation::RecordProducerSnapshot(device.get(), list.get(),
        runtimeInputs, Formats[0], source[4].get(), Formats[4], {SceneWidth, Height},
        gpu::frame_generation::SnapshotPurpose::CompositedBackbuffer);
    Check(bool(runtime) && runtime->purpose == gpu::frame_generation::SnapshotPurpose::CompositedBackbuffer &&
        runtime->inputsQualifiedAtCapture, "runtime input qualification before two-image capture");
    Check(runtime->depth.Complete() && runtime->motion.Complete() &&
        runtime->depth.region.texture != source[1].get() &&
        runtime->motion.region.texture != source[2].get() &&
        !runtime->sourceColor.region.texture && !runtime->sourceColor.lifetime &&
        !runtime->motionInvalidity.region.texture && !runtime->motionInvalidity.lifetime &&
        !runtime->sceneColorCandidate.region.texture && !runtime->sceneColorCandidate.lifetime &&
        !runtime->inputs.color.texture && !runtime->inputs.motionInvalidity.texture &&
        !runtime->inputs.materialInstability.texture &&
        !runtime->inputs.fsrMask.sceneContribution.texture &&
        runtime->inputs.depth.texture == runtime->depth.region.texture &&
        runtime->inputs.motion.texture == runtime->motion.region.texture,
        "runtime owns only depth and motion and clears borrowed views");
    uploadSet(5); // Reuse the source allocations before the same batch completes.
    for (size_t i = 0; i < leases.size(); ++i) {
        RenderTexture* images[] = {leases[i]->region.texture, source[i].get()};
        for (size_t group = 0; group < 2; ++group) {
            list->barriers(RenderBarrierStage::COPY,
                RenderTextureBarrier(images[group], RenderTextureLayout::COPY_SOURCE));
            list->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(readback.get(),
                    Formats[i], i == 4 ? SceneWidth : Width, Height, 1,
                    Pitch / PixelBytes[i], (group * 5 + i) * SliceBytes),
                RenderTextureCopyLocation::Subresource(images[group]));
            list->barriers(RenderBarrierStage::ALL,
                RenderTextureBarrier(images[group], RenderTextureLayout::SHADER_READ));
        }
    }
    for (size_t i = 0; i < 2; ++i) {
        auto* image = i == 0 ? runtime->depth.region.texture : runtime->motion.region.texture;
        const size_t sourceIndex = i + 1;
        list->barriers(RenderBarrierStage::COPY,
            RenderTextureBarrier(image, RenderTextureLayout::COPY_SOURCE));
        list->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(readback.get(),
                Formats[sourceIndex], Width, Height, 1, Pitch / PixelBytes[sourceIndex],
                (10 + i) * SliceBytes), RenderTextureCopyLocation::Subresource(image));
        list->barriers(RenderBarrierStage::ALL,
            RenderTextureBarrier(image, RenderTextureLayout::SHADER_READ));
    }
    list->end();
    auto& vkDevice = static_cast<VulkanDevice&>(*device);
    auto& vkQueue = static_cast<VulkanCommandQueue&>(*queue);
    auto& vkList = static_cast<VulkanCommandList&>(*list);
    auto& vkFence = static_cast<VulkanCommandFence&>(*fence);
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &vkList.vk;
    Check(vkResetFences(vkDevice.vk, 1, &vkFence.vk) == VK_SUCCESS, "reset fence");
    Check(vkQueueSubmit(vkQueue.queue->vk, 1, &submit, vkFence.vk) == VK_SUCCESS,
        "submit snapshot batch");
    snapshot->producerSerial = 1;
    runtime->producerSerial = 1;
    Check(vkWaitForFences(vkDevice.vk, 1, &vkFence.vk, VK_TRUE, UINT64_MAX) == VK_SUCCESS,
        "snapshot producer fence");
    snapshot->producerCompleted = true;
    runtime->producerCompleted = true;
    const gpu::frame_generation::CompositeResolveGeometry validResolve{
        {2560, 1472}, {2560, 1440}, 0, 0, 2560, 1440};
    Check(validResolve.Matches(compositePlan), "padded SR source and exact output resolve");
    auto invalidResolve = validResolve;
    invalidResolve.writeHeight = 1439;
    Check(!invalidResolve.Matches(compositePlan), "incomplete output resolve rejected");
    invalidResolve = validResolve;
    invalidResolve.sourceAllocation.height = 1439;
    Check(!invalidResolve.Matches(compositePlan), "insufficient SR source extent rejected");
    invalidResolve = validResolve;
    invalidResolve.targetAllocation.height = 1472;
    Check(!invalidResolve.Matches(compositePlan), "unselected output padding rejected");

    runtime->resolveSourceAllocation = 117;
    gpu::frame_generation::CompositeHandoff handoff{};
    handoff.producer = runtime;
    handoff.plan = compositePlan;
    handoff.frame = runtime->inputs.renderFrameId;
    handoff.historyEpoch = runtime->inputs.temporalEpoch;
    handoff.sourceAllocation = runtime->resolveSourceAllocation;
    handoff.resolveOrdinal = 23;
    handoff.targetAllocation = 44;
    handoff.outputWidth = 2560;
    handoff.outputHeight = 1440;
    Check(!handoff.ReadyForOrderedSubmission() && !handoff.Ready(),
        "unsubmitted final resolve cannot enter FG");
    handoff.resolveSubmissionSerial = 1;
    handoff.resolveOnPresentQueue = true;
    runtime->producerOnPresentQueue = true;
    runtime->producerCompleted = false;
    Check(handoff.ReadyForOrderedSubmission() && !handoff.Ready(),
        "ordered submitted inputs need no CPU fence completion");
    runtime->producerCompleted = true;
    Check(!handoff.Ready(), "CPU-ready path still waits for final resolve fence");
    handoff.resolveCompleted = true;
    Check(handoff.Ready(), "completed composited backbuffer with unavailable UI is usable");
    runtime->producerSerial = 2;
    Check(!handoff.ReadyForOrderedSubmission(), "producer cannot submit after final resolve");
    runtime->producerSerial = 1;
    runtime->producerWaitFailed = true;
    Check(!handoff.ReadyForOrderedSubmission() && !handoff.Ready(), "failed producer wait rejected");
    runtime->producerWaitFailed = false;
    runtime->lineageCanceled = true;
    Check(!handoff.ReadyForOrderedSubmission() && !handoff.Ready(), "canceled producer rejected");
    runtime->lineageCanceled = false;
    handoff.resolveDiscarded = true;
    Check(!handoff.ReadyForOrderedSubmission() && !handoff.Ready(), "failed resolve submit rejected");
    handoff.resolveDiscarded = false;
    runtime->inputs.motionState = gpu::temporal::MotionState::Unavailable;
    Check(!handoff.Ready(), "missing motion state rejected");
    runtime->inputs.motionState = gpu::temporal::MotionState::Hybrid;
    Check(handoff.Ready(), "real hybrid motion retained without claiming tracked coverage");
    const auto* observed = static_cast<const uint8_t*>(readback->map());
    Check(observed != nullptr, "map readback");
    for (size_t group = 0; group < 2; ++group)
        for (size_t i = 0; i < 5; ++i)
            for (uint32_t y = 0; y < Height; ++y)
                for (uint32_t x = 0; x < (i == 4 ? SceneWidth : Width) * PixelBytes[i]; ++x)
                    Check(observed[(group * 5 + i) * SliceBytes + y * Pitch + x] ==
                        uint8_t((group == 0 ? 11 : 119) + (group * 5 + i) * 17 +
                            y * 7 + (x / PixelBytes[i]) * 3 + x % PixelBytes[i]),
                        "snapshot retains initial pixels while reused sources contain new pixels");
    for (size_t i = 0; i < 2; ++i) {
        const size_t sourceIndex = i + 1;
        for (uint32_t y = 0; y < Height; ++y)
            for (uint32_t x = 0; x < Width * PixelBytes[sourceIndex]; ++x)
                Check(observed[(10 + i) * SliceBytes + y * Pitch + x] ==
                    uint8_t(11 + sourceIndex * 17 + y * 7 +
                        (x / PixelBytes[sourceIndex]) * 3 + x % PixelBytes[sourceIndex]),
                    "runtime depth and motion retain original pixels after source reuse");
    }
    readback->unmap();
    std::printf("snapshot GPU copies and source overwrite passed on %s\n",
        device->getDescription().name.c_str());
}
} // namespace

int main() {
    try { Run(); return 0; }
    catch (const std::exception& e) {
        std::fprintf(stderr, "frame snapshot GPU fixture failed: %s\n", e.what());
        return 1;
    }
}
