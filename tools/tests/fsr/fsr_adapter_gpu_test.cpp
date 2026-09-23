// Bounded native Vulkan contract probe. No WSI, guest executable, or assets.
#include <gpu/fsr_upscaler.h>
#include <plume_vulkan.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

namespace plume { std::unique_ptr<RenderInterface> CreateVulkanInterface(); }

namespace {
using namespace plume;
constexpr uint32_t kRender = 64;
constexpr uint32_t kUploadRowBytes = 256;
constexpr uint32_t kUploadImageBytes = kRender * kUploadRowBytes;

void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

VulkanTexture& Native(RenderTexture& texture) { return static_cast<VulkanTexture&>(texture); }

void Submit(VulkanDevice& device, VulkanCommandQueue& queue, RenderCommandFence& fence,
    std::initializer_list<const RenderCommandList*> lists) {
    std::vector<const RenderCommandList*> ordered(lists);
    queue.executeCommandLists(ordered.data(), uint32_t(ordered.size()), nullptr, 0, nullptr, 0, &fence);
    const auto vkFence = static_cast<VulkanCommandFence&>(fence).vk;
    Check(vkWaitForFences(device.vk, 1, &vkFence, VK_TRUE, UINT64_MAX) == VK_SUCCESS, "submit fence");
    Check(vkResetFences(device.vk, 1, &vkFence) == VK_SUCCESS, "reset fence");
}

struct Fixture {
    std::unique_ptr<RenderInterface> api;
    std::unique_ptr<RenderDevice> device;
    std::unique_ptr<RenderCommandQueue> queue;
    std::unique_ptr<RenderCommandList> init, prefix, isolated, readbackList;
    std::unique_ptr<RenderCommandFence> fence;
    std::unique_ptr<RenderBuffer> upload, readback;
    std::unique_ptr<RenderTexture> color, depth, motion, invalidity, output;
    gpu::fsr::Controller fsr;
    gpu::fsr::Config config{kRender, kRender, kRender, kRender, gpu::upscaling::FsrQuality::NativeAA, 1};
    gpu::temporal::TemporalFrameInputs inputs{};
    uint64_t serial = 0;
    uint64_t nextRenderFrame = 1;

    VulkanDevice& Device() { return static_cast<VulkanDevice&>(*device); }
    VulkanCommandQueue& Queue() { return static_cast<VulkanCommandQueue&>(*queue); }
    VulkanCommandList& Isolated() { return static_cast<VulkanCommandList&>(*isolated); }

    Fixture() {
        api = CreateVulkanInterface(); Check(bool(api), "Vulkan interface");
        device = api->createDevice(); Check(bool(device), "Vulkan device");
        VkPhysicalDeviceProperties selected{};
        vkGetPhysicalDeviceProperties(Device().physicalDevice, &selected);
        std::fprintf(stderr,
            "FSR selected GPU: name=%s vendor_id=0x%04x device_id=0x%04x driver_version=%u api_version=%u.%u.%u\n",
            selected.deviceName, selected.vendorID, selected.deviceID, selected.driverVersion,
            VK_API_VERSION_MAJOR(selected.apiVersion), VK_API_VERSION_MINOR(selected.apiVersion),
            VK_API_VERSION_PATCH(selected.apiVersion));
        queue = device->createCommandQueue(RenderCommandListType::DIRECT); Check(bool(queue), "graphics queue");
        init = queue->createCommandList(); prefix = queue->createCommandList();
        isolated = queue->createCommandList(); readbackList = queue->createCommandList();
        fence = device->createCommandFence();
        Check(init && prefix && isolated && readbackList && fence, "commands and fence");
        color = device->createTexture(RenderTextureDesc::Texture2D(kRender, kRender, 1, RenderFormat::R8G8B8A8_UNORM));
        depth = device->createTexture(RenderTextureDesc::Texture2D(kRender, kRender, 1, RenderFormat::R32_FLOAT));
        motion = device->createTexture(RenderTextureDesc::Texture2D(kRender, kRender, 1, RenderFormat::R16G16_FLOAT));
        invalidity = device->createTexture(RenderTextureDesc::Texture2D(kRender, kRender, 1, RenderFormat::R8_UNORM));
        output = NewOutput(kRender);
        Check(color && depth && motion && invalidity && output, "input and output images");
        upload = device->createBuffer(RenderBufferDesc::UploadBuffer(4 * kUploadImageBytes));
        readback = device->createBuffer(RenderBufferDesc::ReadbackBuffer(128 * 128 * 4));
        Check(upload && readback, "staging buffers");
        UploadInputs();
        inputs.plan.consumer = gpu::upscaling::TemporalConsumer::FsrSr;
        inputs.plan.requestedUpscaler = gpu::upscaling::Upscaler::Fsr;
        inputs.color = {color.get(), {kRender, kRender}, 0, 0, kRender, kRender};
        inputs.depth = {depth.get(), {kRender, kRender}, 0, 0, kRender, kRender};
        inputs.motion = {motion.get(), {kRender, kRender}, 0, 0, kRender, kRender};
        inputs.motionInvalidity = {invalidity.get(), {kRender, kRender}, 0, 0, kRender, kRender};
        inputs.currentInputsComplete = true;
        inputs.colorEncoding = gpu::temporal::ColorEncoding::Sdr;
        inputs.depthConvention = gpu::temporal::DepthConvention::Reversed;
        inputs.motionState = gpu::temporal::MotionState::Tracked;
    }

    ~Fixture() {
        // The controller releases its context before Plume destroys VkDevice.
        if (device && Device().vk) vkDeviceWaitIdle(Device().vk);
        fsr.ReleaseCompletedThrough(serial);
        fsr.ShutdownAfterGpuDrain();
    }

    std::unique_ptr<RenderTexture> NewOutput(uint32_t size) {
        return device->createTexture(RenderTextureDesc::Texture2D(size, size, 1,
            RenderFormat::R8G8B8A8_UNORM));
    }

    void UploadInputs() {
        auto* bytes = static_cast<uint8_t*>(upload->map()); Check(bytes != nullptr, "map upload");
        std::memset(bytes, 0, 4 * kUploadImageBytes);
        for (uint32_t y = 0; y < kRender; ++y) for (uint32_t x = 0; x < kRender; ++x) {
            auto* rgba = bytes + y * kUploadRowBytes + x * 4;
            rgba[0] = uint8_t(32 + x * 2);
            rgba[1] = uint8_t(64 + y * 2);
            rgba[2] = 128;
            rgba[3] = 192;
            const float canonical = 1.0f / (1.0f + float(x) / kRender);
            const float rawDepth = .001f + .999f * canonical;
            std::memcpy(bytes + kUploadImageBytes + y * kUploadRowBytes + x * 4,
                &rawDepth, sizeof(rawDepth));
            // R16G16_SFLOAT zero motion and R8_UNORM zero invalidity remain zero.
        }
        upload->unmap();
        init->begin();
        RenderTexture* images[] = {color.get(), depth.get(), motion.get(), invalidity.get()};
        RenderFormat formats[] = {RenderFormat::R8G8B8A8_UNORM, RenderFormat::R32_FLOAT,
            RenderFormat::R16G16_FLOAT, RenderFormat::R8_UNORM};
        for (uint32_t i = 0; i < 4; ++i) {
            init->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(images[i], RenderTextureLayout::COPY_DEST));
            init->copyTextureRegion(RenderTextureCopyLocation::Subresource(images[i]),
                RenderTextureCopyLocation::PlacedFootprint(upload.get(), formats[i], kRender, kRender, 1,
                    i == 3 ? kUploadRowBytes : kRender, uint64_t(i) * kUploadImageBytes));
            init->barriers(RenderBarrierStage::COMPUTE, RenderTextureBarrier(images[i], RenderTextureLayout::SHADER_READ));
        }
        init->end();
        Submit(Device(), Queue(), *fence, {init.get()});
    }

    gpu::fsr::FrameMetadata Metadata(bool reset, float delta) const {
        gpu::fsr::FrameMetadata frame{};
        frame.cameraValid = true;
        frame.cameraNear = FLT_MAX;
        frame.cameraFar = 10.0f; // Synthetic projection's finite near plane.
        frame.verticalFovRadians = 0.7f;
        frame.viewSpaceToMetersFactor = 1.0f; // P1 uncalibrated SDK unit convention.
        frame.frameTimeDeltaMilliseconds = delta;
        frame.depthScale = 1.0f / .999f;
        frame.depthBias = -.001f / .999f;
        (void)reset;
        return frame;
    }

    std::vector<uint8_t> Readback() {
        const uint32_t size = config.outputWidth;
        readbackList->begin();
        readbackList->barriers(RenderBarrierStage::COPY,
            RenderTextureBarrier(output.get(), RenderTextureLayout::COPY_SOURCE));
        readbackList->copyTextureRegion(
            RenderTextureCopyLocation::PlacedFootprint(readback.get(), RenderFormat::R8G8B8A8_UNORM,
                size, size, 1, size), RenderTextureCopyLocation::Subresource(output.get()));
        readbackList->end();
        Submit(Device(), Queue(), *fence, {readbackList.get()});
        const auto* pixels = static_cast<const uint8_t*>(readback->map());
        Check(pixels != nullptr, "map readback");
        std::vector<uint8_t> copy(pixels, pixels + size * size * 4);
        readback->unmap();
        return copy;
    }

    void CheckOutput(const std::vector<uint8_t>& pixels) const {
        const uint32_t size = config.outputWidth;
        Check(pixels.size() == size_t(size) * size * 4, "readback extent");
        const size_t center = (size_t(size / 2) * size + size / 2) * 4;
        // This smooth gradient permits temporal adjustment; it still exposes
        // missing dispatch, black output, and gross gamma conversion errors.
        Check(pixels[center] > 40 && pixels[center] < 200, "red gradient/gamma");
        Check(pixels[center + 1] > 65 && pixels[center + 1] < 220, "green gradient/gamma");
        Check(pixels[center + 2] > 70 && pixels[center + 2] < 190, "blue gamma roundtrip");
        Check(pixels[center + 3] == 192, "guest alpha carried through");
        std::printf("FSR output %ux%u center RGBA=%u,%u,%u,%u\n", size, size,
            pixels[center], pixels[center + 1], pixels[center + 2], pixels[center + 3]);
    }

    void RecordAndSubmit(bool reset, float delta, bool expectedDispatchReset, bool expectedGapReset) {
        Check(fsr.EnsureSession(Device(), config) == gpu::fsr::Status::Ready, "FSR context ready");
        inputs.renderFrameId = nextRenderFrame++;
        inputs.resetHistory = reset;
        inputs.jitter = gpu::temporal::FrameJitter(serial + 1, kRender, kRender);
        prefix->begin();
        prefix->barriers(RenderBarrierStage::COPY,
            RenderTextureBarrier(output.get(), RenderTextureLayout::COPY_DEST));
        prefix->end();
        auto attempt = fsr.RecordIsolated(Isolated(), config, inputs, Metadata(reset, delta), Native(*output));
        Check(attempt.status == gpu::fsr::Status::Ready && attempt.useId, "FSR dispatch recorded");
        const auto& diagnostics = fsr.LastDiagnostics();
        Check(diagnostics.lastDispatchRenderFrameId == inputs.renderFrameId &&
            diagnostics.lastDispatchReset == expectedDispatchReset &&
            diagnostics.lastResetForFrameGap == expectedGapReset, "FSR effective SDK history reset");
        Submit(Device(), Queue(), *fence, {prefix.get(), isolated.get()});
        fsr.OnBatchSubmitted(attempt.useId, ++serial);
        fsr.ReleaseCompletedThrough(serial);
        CheckOutput(Readback());
    }

    void DiscardAndRecreate() {
        auto disposableOutput = NewOutput(config.outputWidth); Check(bool(disposableOutput), "discard output");
        prefix->begin();
        prefix->barriers(RenderBarrierStage::COPY,
            RenderTextureBarrier(disposableOutput.get(), RenderTextureLayout::COPY_DEST));
        prefix->end();
        inputs.renderFrameId = nextRenderFrame++;
        inputs.resetHistory = false;
        auto attempt = fsr.RecordIsolated(Isolated(), config, inputs, Metadata(false, 16.6f), Native(*disposableOutput));
        Check(attempt.status == gpu::fsr::Status::Ready && attempt.useId, "record for discard");
        Check(vkResetCommandBuffer(Isolated().vk, 0) == VK_SUCCESS, "reset discarded isolated command buffer");
        Check(vkResetCommandBuffer(static_cast<VulkanCommandList&>(*prefix).vk, 0) == VK_SUCCESS,
            "reset discarded prefix command buffer");
        fsr.OnBatchDiscarded(attempt.useId);
        Check(fsr.EnsureSession(Device(), config) == gpu::fsr::Status::NeedsReconfigure,
            "successful record discard poisons CPU history");
        fsr.ReleaseFeatureAfterGpuDrain();
        Check(fsr.EnsureSession(Device(), config) == gpu::fsr::Status::Ready, "context recreated after discard");
    }

    void ResizeAndRecord() {
        const auto recommended = gpu::fsr::RecommendedRenderSize({96, 96}, gpu::upscaling::FsrQuality::Quality);
        Check(recommended && recommended->width == kRender && recommended->height == kRender,
            "explicit quality mapping");
        auto resizedOutput = NewOutput(96); Check(bool(resizedOutput), "resized output");
        config.outputWidth = config.outputHeight = 96;
        config.quality = gpu::upscaling::FsrQuality::Quality;
        Check(fsr.EnsureSession(Device(), config) == gpu::fsr::Status::NeedsReconfigure,
            "resize requires drained reconfiguration");
        fsr.ReleaseFeatureAfterGpuDrain();
        output = std::move(resizedOutput);
        RecordAndSubmit(true, 16.6f, true, false);
    }
};
} // namespace

int main(int argc, char** argv) {
    try {
        const bool gapOnly = argc == 2 && std::strcmp(argv[1], "--gap-only") == 0;
        Check(argc == 1 || gapOnly, "usage: LoFsrAdapterGpuTest [--gap-only]");
        Fixture fixture;
        fixture.RecordAndSubmit(true, 0.0f, true, false);
        fixture.RecordAndSubmit(false, 16.6f, false, false);
        ++fixture.nextRenderFrame; // A captured game frame where FSR was ineligible.
        fixture.RecordAndSubmit(false, 16.6f, true, true);
        if (!gapOnly) {
            fixture.DiscardAndRecreate();
            fixture.RecordAndSubmit(true, 16.6f, true, false);
            fixture.ResizeAndRecord();
        }
        std::puts(gapOnly ? "FSR adapter Vulkan gap: PASS" : "FSR adapter Vulkan contract: PASS");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FSR adapter Vulkan contract: FAIL: %s\n", error.what());
        return 1;
    }
}
