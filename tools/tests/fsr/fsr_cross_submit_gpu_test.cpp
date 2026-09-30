// Two real checked Vulkan submissions, no WSI/game. Exercises the production
// scene-copy mask owner and borrowed DTO through a producer-slot retirement.
#include <gpu/fsr_alpha_propagation_gpu.h>
#include <gpu/fsr_mask_policy.h>
#include <gpu/fsr_upscaler.h>
#include <gpu/vulkan_submission_state.h>
#include <plume_vulkan.h>

#include <array>
#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace plume { std::unique_ptr<RenderInterface> CreateVulkanInterface(); }
namespace {
using namespace plume;
constexpr uint32_t Side = 64, Pitch = 256, ImageBytes = Side * Pitch;
constexpr uint64_t Frame = 12000, Epoch = 77, ColorOrdinal = 99, SourceAllocation = 9;
constexpr uint64_t DestinationAllocation = 17, SourceWriteOrdinal = 205234;

void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

uint8_t ExpectedMask(uint32_t x) { return x < 16 ? 0 : x < 32 ? 128 : 255; }

// Same checked-queue state used by video::SubmitVulkan. A serial only exists
// after a successful actual vkQueueSubmit; the fence proves GPU completion.
uint64_t SubmitAndComplete(VulkanDevice& device, VulkanCommandQueue& queue,
    VulkanCommandFence& fence, gpu::submission::VulkanState& state,
    std::initializer_list<const RenderCommandList*> lists) {
    std::vector<VkCommandBuffer> buffers;
    for (const auto* list : lists) buffers.push_back(static_cast<const VulkanCommandList*>(list)->vk);
    VkSubmitInfo info{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    info.pCommandBuffers = buffers.data(); info.commandBufferCount = uint32_t(buffers.size());
    uint64_t serial = 0;
    int32_t result = 0;
    bool submitted;
    {
        std::scoped_lock lock(*queue.queue->mutex);
        submitted = state.SubmitBatch(
            [&] { return int32_t(vkResetFences(device.vk, 1, &fence.vk)); },
            [&] { return int32_t(vkQueueSubmit(queue.queue->vk, 1, &info, fence.vk)); },
            serial, result);
    }
    Check(submitted && result == VK_SUCCESS && serial != 0, "checked vkQueueSubmit");
    Check(state.WaitSubmitted([&] {
        return int32_t(vkWaitForFences(device.vk, 1, &fence.vk, VK_TRUE, 5'000'000'000ull));
    }), "producer/consumer GPU fence completed");
    return serial;
}

gpu::fsr::FrameMetadata Metadata() {
    gpu::fsr::FrameMetadata m{};
    m.cameraValid = true;
    m.cameraNear = FLT_MAX;
    m.cameraFar = 10.0f;
    m.verticalFovRadians = .7f;
    m.viewSpaceToMetersFactor = 1.0f;
    m.frameTimeDeltaMilliseconds = 16.6f;
    m.depthScale = 1.0f / .999f;
    m.depthBias = -.001f / .999f;
    return m;
}

void Run(const char* resultPath) {
    auto api = CreateVulkanInterface(); Check(bool(api), "Vulkan interface");
    auto device = api->createDevice(); Check(bool(device), "Vulkan device");
    auto& native = static_cast<VulkanDevice&>(*device);
    VkPhysicalDeviceProperties gpu{};
    vkGetPhysicalDeviceProperties(native.physicalDevice, &gpu);
    auto queue = device->createCommandQueue(RenderCommandListType::DIRECT);
    Check(bool(queue), "direct GPU queue");
    auto& vkQueue = static_cast<VulkanCommandQueue&>(*queue);
    auto a = queue->createCommandList(), bPrefix = queue->createCommandList();
    auto bIsolated = queue->createCommandList();
    auto fence = device->createCommandFence();
    Check(a && bPrefix && bIsolated && fence, "independent command lists and fence");
    auto& vkFence = static_cast<VulkanCommandFence&>(*fence);
    gpu::submission::VulkanState submitState;
    auto color = device->createTexture(RenderTextureDesc::Texture2D(Side, Side, 1, RenderFormat::R8G8B8A8_UNORM));
    auto depth = device->createTexture(RenderTextureDesc::Texture2D(Side, Side, 1, RenderFormat::R32_FLOAT));
    auto motion = device->createTexture(RenderTextureDesc::Texture2D(Side, Side, 1, RenderFormat::R16G16_FLOAT));
    auto invalidity = device->createTexture(RenderTextureDesc::Texture2D(Side, Side, 1, RenderFormat::R8_UNORM));
    auto output = device->createTexture(RenderTextureDesc::Texture2D(Side, Side, 1, RenderFormat::R8G8B8A8_UNORM));
    auto upload = device->createBuffer(RenderBufferDesc::UploadBuffer(5 * ImageBytes));
    auto readback = device->createBuffer(RenderBufferDesc::ReadbackBuffer(ImageBytes));
    Check(color && depth && motion && invalidity && output && upload && readback, "images and staging");
    auto* staging = static_cast<uint8_t*>(upload->map()); Check(staging != nullptr, "map upload");
    std::memset(staging, 0, 5 * ImageBytes);
    for (uint32_t y = 0; y < Side; ++y) for (uint32_t x = 0; x < Side; ++x) {
        auto* pixel = staging + y * Pitch + x * 4;
        pixel[0] = uint8_t(32 + x * 2); pixel[1] = uint8_t(64 + y * 2);
        pixel[2] = 128; pixel[3] = 192;
        const float rawDepth = .001f + .999f / (1.0f + float(x) / Side);
        std::memcpy(staging + ImageBytes + y * Pitch + x * 4, &rawDepth, 4);
        staging[4 * ImageBytes + y * Pitch + x] = ExpectedMask(x);
    }
    upload->unmap();

    gpu::temporal::TemporalFrameInputs inputs{};
    inputs.renderFrameId = Frame; inputs.temporalEpoch = Epoch; inputs.colorOrdinal = ColorOrdinal;
    inputs.plan.consumer = gpu::upscaling::TemporalConsumer::FsrSr;
    inputs.plan.requestedUpscaler = gpu::upscaling::Upscaler::Fsr;
    inputs.plan.deviceEpoch = 1; inputs.plan.geometryEpoch = Epoch;
    inputs.color = {color.get(), {Side, Side}, 0, 0, Side, Side};
    inputs.depth = {depth.get(), {Side, Side}, 0, 0, Side, Side};
    inputs.motion = {motion.get(), {Side, Side}, 0, 0, Side, Side};
    inputs.motionInvalidity = {invalidity.get(), {Side, Side}, 0, 0, Side, Side};
    inputs.currentInputsComplete = true;
    inputs.colorEncoding = gpu::temporal::ColorEncoding::Sdr;
    inputs.depthConvention = gpu::temporal::DepthConvention::Reversed;
    inputs.motionState = gpu::temporal::MotionState::Tracked;

    gpu::fsr_alpha::SceneCopyMask selection;
    std::vector<std::shared_ptr<gpu::fsr_alpha::MaskLease>> producerSlot;
    std::weak_ptr<gpu::fsr_alpha::MaskLease> weakResolved, weakRaw;
    uint64_t sourceSerial = 0;
    {
        gpu::fsr_alpha::PropagationGPU propagation(device.get());
        propagation.BeginFrame(Frame, Epoch);
        auto raw = std::make_shared<gpu::fsr_alpha::MaskLease>();
        raw->texture = device->createTexture(RenderTextureDesc::Texture2D(Side, Side, 1, RenderFormat::R8_UNORM));
        Check(bool(raw->texture), "GPU raw source mask");
        weakRaw = raw;
        a->begin();
        RenderTexture* images[] = {color.get(), depth.get(), motion.get(), invalidity.get(), raw->texture.get()};
        RenderFormat formats[] = {RenderFormat::R8G8B8A8_UNORM, RenderFormat::R32_FLOAT,
            RenderFormat::R16G16_FLOAT, RenderFormat::R8_UNORM, RenderFormat::R8_UNORM};
        for (uint32_t i = 0; i < 5; ++i) {
            a->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(images[i], RenderTextureLayout::COPY_DEST));
            a->copyTextureRegion(RenderTextureCopyLocation::Subresource(images[i]),
                RenderTextureCopyLocation::PlacedFootprint(upload.get(), formats[i], Side, Side, 1,
                    i >= 3 ? Pitch : Side, uint64_t(i) * ImageBytes));
            a->barriers(RenderBarrierStage::COMPUTE,
                RenderTextureBarrier(images[i], RenderTextureLayout::SHADER_READ));
        }
        raw->layout = RenderTextureLayout::SHADER_READ;
        propagation.PublishPostprocess({Frame, Epoch, SourceAllocation, 2104,
            Side, Side, {0, 0, Side, Side}, gpu::fsr_alpha::SourceStage::Tonemap, raw});
        auto resolve = propagation.RecordResolve(a.get(), Frame, Epoch, SourceAllocation,
            Side, Side, 0x123400, 6, DestinationAllocation, SourceWriteOrdinal,
            Side, Side, {0, 0, Side, Side}, "copy", producerSlot);
        Check(resolve.copied && resolve.version.sourceAllocation == SourceAllocation &&
            resolve.version.writeOrdinal == SourceWriteOrdinal &&
            resolve.version.sourceStage == gpu::fsr_alpha::SourceStage::Tonemap,
            "production resolve version and GPU copy");
        weakResolved = resolve.version.mask;
        auto fetched = propagation.RecordFetchView(a.get(), Frame, Epoch, 0x123400, 6,
            DestinationAllocation, SourceWriteOrdinal, color.get(), Side, Side, producerSlot);
        Check(fetched && fetched.mask == resolve.version.mask &&
            propagation.FreezeForSceneCopy(fetched, color.get(), color.get(), Frame, Epoch,
                ColorOrdinal, Side, Side), "production fetch and frozen scene-copy version");
        selection = propagation.SceneCopy();
        Check(selection && selection.sourceWriteOrdinal == SourceWriteOrdinal, "selected source version");
        a->end();
        sourceSerial = SubmitAndComplete(native, vkQueue, vkFence, submitState, {a.get()});
        // The A fence has completed, so its slot may retire and its command
        // bookkeeping (including producerSlot) can release its references.
        producerSlot.clear();
        propagation.Invalidate();
    }
    Check(!weakResolved.expired() && !weakRaw.expired(), "CPU selection holds producer chain after A slot retirement");
    Check(gpu::fsr_alpha::AttachSceneCopyMask(selection, inputs), "production borrowed handoff identity");
    std::vector<std::shared_ptr<gpu::fsr_alpha::MaskLease>> consumerSlot;
    Check(gpu::fsr_alpha::RetainSceneCopyMaskForBatch(inputs, selection.mask, consumerSlot),
        "production consuming slot holds exact image");
    selection = {}; // CPU selection lifetime ends before recording B.
    Check(!weakResolved.expired() && !weakRaw.expired() && consumerSlot.size() == 1,
        "B slot alone retains the resolved image and its producer chain");
    Check(gpu::fsr::QualifyFsrMask(inputs).useReactive, "FSR adapter mask identity qualifies");

    gpu::fsr::Controller fsr;
    const gpu::fsr::Config config{Side, Side, Side, Side, gpu::upscaling::FsrQuality::NativeAA, 1};
    Check(fsr.EnsureSession(native, config) == gpu::fsr::Status::Ready, "actual FSR adapter session");
    bPrefix->begin();
    bPrefix->barriers(RenderBarrierStage::COPY,
        RenderTextureBarrier(output.get(), RenderTextureLayout::COPY_DEST));
    auto* retained = inputs.fsrMask.sceneContribution.texture;
    bPrefix->barriers(RenderBarrierStage::COPY,
        RenderTextureBarrier(retained, RenderTextureLayout::COPY_SOURCE));
    bPrefix->copyTextureRegion(
        RenderTextureCopyLocation::PlacedFootprint(readback.get(), RenderFormat::R8_UNORM,
            Side, Side, 1, Pitch), RenderTextureCopyLocation::Subresource(retained));
    bPrefix->barriers(RenderBarrierStage::COMPUTE,
        RenderTextureBarrier(retained, RenderTextureLayout::SHADER_READ));
    bPrefix->end();
    auto attempt = fsr.RecordIsolated(static_cast<VulkanCommandList&>(*bIsolated), config,
        inputs, Metadata(), static_cast<VulkanTexture&>(*output));
    Check(attempt.status == gpu::fsr::Status::Ready && attempt.useId &&
        attempt.sdkResult == 0 && attempt.vkResult == VK_SUCCESS, "actual adapter consumes B mask");
    const uint64_t consumerSerial = SubmitAndComplete(native, vkQueue, vkFence, submitState,
        {bPrefix.get(), bIsolated.get()});
    Check(sourceSerial < consumerSerial && consumerSerial == submitState.LastSubmission(),
        "two ordered successful native GPU queue submissions");
    fsr.OnBatchSubmitted(attempt.useId, consumerSerial);
    fsr.ReleaseCompletedThrough(consumerSerial);
    const auto* observed = static_cast<const uint8_t*>(readback->map());
    Check(observed != nullptr, "map B GPU readback");
    uint32_t mismatches = 0;
    for (uint32_t y = 0; y < Side; ++y)
        for (uint32_t x = 0; x < Side; ++x)
            mismatches += observed[y * Pitch + x] != ExpectedMask(x);
    const std::array<uint32_t, 6> checkX{0, 15, 16, 31, 32, 63};
    std::array<uint32_t, checkX.size()> points{};
    for (size_t i = 0; i < checkX.size(); ++i) points[i] = observed[32 * Pitch + checkX[i]];
    readback->unmap();
    Check(mismatches == 0, "independently expected R8 bytes read by B GPU command buffer");
    Check(!weakResolved.expired() && !weakRaw.expired(), "B slot owns mask until B fence completion");
    consumerSlot.clear();
    Check(weakResolved.expired() && weakRaw.expired(), "producer chain freed after consuming fence and slot retirement");
    fsr.ShutdownAfterGpuDrain();
    std::fprintf(stdout, "source_serial=%llu consumer_serial=%llu same_version=%llu mask_pixels=%u mismatches=%u gpu=%s\n",
        static_cast<unsigned long long>(sourceSerial), static_cast<unsigned long long>(consumerSerial),
        static_cast<unsigned long long>(SourceWriteOrdinal), Side * Side, mismatches, gpu.deviceName);
    std::ofstream out(resultPath, std::ios::trunc);
    Check(bool(out), "open result JSON");
    out << "{\"status\":\"passed\",\"scope\":\"production_PropagationGPU_helpers_and_FSR_adapter_not_renderer_Flush\","
        << "\"source_serial\":" << sourceSerial << ",\"consumer_serial\":" << consumerSerial
        << ",\"frame\":" << Frame << ",\"epoch\":" << Epoch
        << ",\"source_allocation\":" << SourceAllocation
        << ",\"source_write_ordinal\":" << SourceWriteOrdinal
        << ",\"color_ordinal\":" << ColorOrdinal
        << ",\"mask_pixels\":" << Side * Side << ",\"mismatches\":" << mismatches
        << ",\"observed_middle_row\":[";
    for (size_t i = 0; i < points.size(); ++i) out << (i ? "," : "") << points[i];
    out << "],\"expected_middle_row\":[0,0,128,128,255,255],"
        << "\"producer_slot_retired\":true,\"cpu_selection_released_before_B\":true,"
        << "\"consumer_slot_released_after_B_fence\":true,\"fsr_sdk_dispatched\":true,"
        << "\"gpu_name\":\"" << gpu.deviceName << "\"}\n";
    Check(bool(out), "write result JSON");
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2) { std::fprintf(stderr, "usage: LoFsrCrossSubmitGpuTest result.json\n"); return 2; }
    try { Run(argv[1]); return 0; }
    catch (const std::exception& e) { std::fprintf(stderr, "cross-submit GPU fixture failed: %s\n", e.what()); return 1; }
}
