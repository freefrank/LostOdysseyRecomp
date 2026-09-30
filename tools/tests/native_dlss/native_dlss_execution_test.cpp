#include <gpu/dlss_ngx.h>
#include <gpu/temporal_frame_inputs.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
using namespace plume;

std::unique_ptr<RenderInterface> CreateInterface(const VulkanExtensionHooks& hooks) {
#if PLUME_SDL_VULKAN_ENABLED
    return CreateVulkanInterface(nullptr, hooks);
#else
    return CreateVulkanInterface(hooks);
#endif
}

bool FiniteHalf(uint16_t value) { return (value & 0x7c00u) != 0x7c00u; }

[[noreturn]] void Fail(const char* message) { throw std::runtime_error(message); }

int Skip(const char* message) {
    std::printf("SKIP: %s\n", message);
    return 77;
}

gpu::temporal::TextureRegion Region(RenderTexture* texture, uint32_t width, uint32_t height) {
    return {texture, {width, height}, 0, 0, width, height};
}
}

int main() {
    try {
        std::setvbuf(stdout, nullptr, _IONBF, 0);
        constexpr uint64_t kDeviceEpoch = 1;
        const auto dataPath = std::filesystem::temp_directory_path() / "lost-odyssey-recomp-ngX-p2";
        const auto runtimePath = std::filesystem::current_path();
#if defined(VULKAN_VALIDATION_LAYER_ENABLED)
        std::puts("VALIDATION_BUILD=enabled");
#else
        std::puts("VALIDATION_BUILD=disabled");
#endif
        const auto volkResult = volkInitialize();
        if (volkResult != VK_SUCCESS) return Skip("Vulkan loader unavailable for validation-layer enumeration");
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> layers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, layers.data());
        const bool validationAvailable = std::any_of(layers.begin(), layers.end(), [](const VkLayerProperties& layer) {
            return std::strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0;
        });
        std::printf("VALIDATION_LAYER_KHRONOS=%s\n", validationAvailable ? "available" : "unavailable");
        gpu::dlss::Controller controller(dataPath, runtimePath);
        auto renderInterface = CreateInterface(controller.ExtensionHooks());
        if (!renderInterface) return Skip("Vulkan interface unavailable");
        auto device = renderInterface->createDevice();
        if (!device) return Skip("Vulkan device unavailable");
        auto* vkDevice = static_cast<VulkanDevice*>(device.get());
        if (controller.EnsureSession(*vkDevice) != gpu::dlss::SrStatus::Executable)
            return Skip("native DLSS session unavailable");

        const auto sizing = controller.QueryOutputSizing(*static_cast<VulkanInterface*>(renderInterface.get()), *vkDevice,
            {kDeviceEpoch, 1280, 720});
        const auto& mode = sizing.modes[0];
        if (mode.state != gpu::upscaling::SizingState::Ready || !mode.optimal.width || !mode.optimal.height)
            return Skip("native DLSS quality sizing unavailable");
        const uint32_t renderWidth = mode.optimal.width, renderHeight = mode.optimal.height;
        const uint32_t outputWidth = 1280, outputHeight = 720;
        const auto colorDesc = RenderTextureDesc::Texture2D(renderWidth, renderHeight, 1, RenderFormat::R16G16B16A16_FLOAT,
            RenderTextureFlag::RENDER_TARGET | RenderTextureFlag::STORAGE);
        const auto depthDesc = RenderTextureDesc::Texture2D(renderWidth, renderHeight, 1, RenderFormat::R32_FLOAT,
            RenderTextureFlag::RENDER_TARGET | RenderTextureFlag::STORAGE);
        const auto motionDesc = RenderTextureDesc::Texture2D(renderWidth, renderHeight, 1, RenderFormat::R16G16_FLOAT,
            RenderTextureFlag::RENDER_TARGET | RenderTextureFlag::STORAGE);
        const auto outputDesc = RenderTextureDesc::Texture2D(outputWidth, outputHeight, 1, RenderFormat::R16G16B16A16_FLOAT,
            RenderTextureFlag::RENDER_TARGET | RenderTextureFlag::STORAGE);
        auto color = device->createTexture(colorDesc);
        auto depth = device->createTexture(depthDesc);
        auto motion = device->createTexture(motionDesc);
        auto output = device->createTexture(outputDesc);
        auto readback = device->createBuffer(RenderBufferDesc::ReadbackBuffer(uint64_t(outputWidth) * outputHeight * 8));
        auto queue = device->createCommandQueue(RenderCommandListType::DIRECT);
        auto prefix = queue->createCommandList();
        auto isolated = queue->createCommandList();
        auto continuation = queue->createCommandList();
        auto fence = device->createCommandFence();
        if (!color || !depth || !motion || !output || !readback || !queue || !prefix || !isolated || !continuation || !fence)
            return Skip("native DLSS fixture resources unavailable");

        const RenderTexture* colorAttachment[] = {color.get()};
        const RenderTexture* depthAttachment[] = {depth.get()};
        const RenderTexture* motionAttachment[] = {motion.get()};
        const RenderTexture* outputAttachment[] = {output.get()};
        auto colorFb = device->createFramebuffer(RenderFramebufferDesc(colorAttachment, 1));
        auto depthFb = device->createFramebuffer(RenderFramebufferDesc(depthAttachment, 1));
        auto motionFb = device->createFramebuffer(RenderFramebufferDesc(motionAttachment, 1));
        auto outputFb = device->createFramebuffer(RenderFramebufferDesc(outputAttachment, 1));
        if (!colorFb || !depthFb || !motionFb || !outputFb) return Skip("native DLSS fixture framebuffers unavailable");

        gpu::dlss::SrConfig config;
        config.renderExtent = {renderWidth, renderHeight};
        config.outputExtent = {outputWidth, outputHeight};
        config.quality = gpu::upscaling::DlssQuality::Quality;
        config.colorSpace = gpu::dlss::SrColorSpace::DisplayEncoded;
        config.deviceEpoch = kDeviceEpoch;
        config.depthInverted = false; // Synthetic fixture has conventional R32 depth.
        config.autoExposure = false;
        gpu::temporal::TemporalFrameInputs inputs;
        inputs.plan.deviceEpoch = kDeviceEpoch;
        inputs.plan.consumer = gpu::upscaling::TemporalConsumer::DlssSr;
        inputs.renderFrameId = 1;
        inputs.color = Region(color.get(), renderWidth, renderHeight);
        inputs.depth = Region(depth.get(), renderWidth, renderHeight);
        inputs.motion = Region(motion.get(), renderWidth, renderHeight);
        inputs.motionInvalidity = Region(motion.get(), renderWidth, renderHeight);
        inputs.currentInputsComplete = true;
        inputs.resetHistory = true;
        inputs.motionState = gpu::temporal::MotionState::ResetInitialization;
        inputs.colorEncoding = gpu::temporal::ColorEncoding::Sdr;
        inputs.depthConvention = gpu::temporal::DepthConvention::Forward; // Synthetic conventional R32 fixture.

        prefix->begin();
        prefix->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(color.get(), RenderTextureLayout::COLOR_WRITE));
        prefix->setFramebuffer(colorFb.get()); prefix->clearColor(0, RenderColor(.25f, .5f, .125f, 1));
        prefix->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(depth.get(), RenderTextureLayout::COLOR_WRITE));
        prefix->setFramebuffer(depthFb.get()); prefix->clearColor(0, RenderColor(.5f, 0, 0, 0));
        prefix->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(motion.get(), RenderTextureLayout::COLOR_WRITE));
        prefix->setFramebuffer(motionFb.get()); prefix->clearColor(0, RenderColor(0, 0, 0, 0));
        prefix->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(output.get(), RenderTextureLayout::COLOR_WRITE));
        prefix->setFramebuffer(outputFb.get()); prefix->clearColor(0, RenderColor(0, 0, 0, 0));
        RenderTextureBarrier ngxBarriers[] = {
            {color.get(), RenderTextureLayout::GENERAL}, {depth.get(), RenderTextureLayout::GENERAL},
            {motion.get(), RenderTextureLayout::GENERAL}, {output.get(), RenderTextureLayout::GENERAL},
        };
        prefix->barriers(RenderBarrierStage::ALL, nullptr, 0, ngxBarriers, 4);
        prefix->end();

        auto attempt = controller.RecordIsolated(*static_cast<VulkanCommandList*>(isolated.get()), config, inputs,
            *static_cast<VulkanTexture*>(output.get()));
        if (attempt.status != gpu::dlss::SrStatus::Executable || !attempt.useId)
            Fail("first native DLSS Create/Evaluate recording failed");
        if (controller.NeedsFeatureRecreate(config)) Fail("feature was not retained after first evaluation");
        continuation->begin();
        continuation->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(output.get(), RenderTextureLayout::COPY_SOURCE));
        continuation->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(readback.get(), RenderFormat::R16G16B16A16_FLOAT,
            outputWidth, outputHeight, 1, outputWidth), RenderTextureCopyLocation::Subresource(output.get()));
        continuation->end();
        const RenderCommandList* firstLists[] = {prefix.get(), isolated.get(), continuation.get()};
        queue->executeCommandLists(firstLists, 3, nullptr, 0, nullptr, 0, fence.get());
        controller.OnBatchSubmitted(attempt.useId, 1);
        queue->waitForCommandFence(fence.get());
        controller.ReleaseCompletedThrough(1);
        const auto* pixels = static_cast<const uint16_t*>(readback->map());
        if (!FiniteHalf(pixels[0]) || !FiniteHalf(pixels[1]) || !FiniteHalf(pixels[2]) || !FiniteHalf(pixels[3]))
            Fail("native DLSS output contains a non-finite first pixel");
        if (!(pixels[0] | pixels[1] | pixels[2] | pixels[3])) Fail("native DLSS did not change the zero sentinel output");
        readback->unmap();

        inputs.renderFrameId = 2;
        inputs.resetHistory = false;
        inputs.motionState = gpu::temporal::MotionState::Tracked;
        prefix->begin();
        prefix->barriers(RenderBarrierStage::ALL, nullptr, 0, ngxBarriers, 4);
        prefix->end();
        const auto second = controller.RecordIsolated(*static_cast<VulkanCommandList*>(isolated.get()), config, inputs,
            *static_cast<VulkanTexture*>(output.get()));
        if (second.status != gpu::dlss::SrStatus::Executable || !second.useId || controller.NeedsFeatureRecreate(config))
            Fail("second native DLSS motion frame did not reuse the feature");
        continuation->begin();
        continuation->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(output.get(), RenderTextureLayout::COPY_SOURCE));
        continuation->end();
        const RenderCommandList* secondLists[] = {prefix.get(), isolated.get(), continuation.get()};
        queue->executeCommandLists(secondLists, 3, nullptr, 0, nullptr, 0, fence.get());
        controller.OnBatchSubmitted(second.useId, 2);
        queue->waitForCommandFence(fence.get());
        controller.ReleaseCompletedThrough(2);
        controller.ReleaseFeatureAfterGpuDrain();
        if (!controller.NeedsFeatureRecreate(config)) Fail("drained feature retirement was not observed");
        inputs.renderFrameId = 3;
        inputs.resetHistory = true;
        prefix->begin();
        prefix->barriers(RenderBarrierStage::ALL, nullptr, 0, ngxBarriers, 4);
        prefix->end();
        const auto recreated = controller.RecordIsolated(*static_cast<VulkanCommandList*>(isolated.get()), config, inputs,
            *static_cast<VulkanTexture*>(output.get()));
        if (recreated.status != gpu::dlss::SrStatus::Executable || !recreated.useId)
            Fail("drained feature did not recreate");
        continuation->begin();
        continuation->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(output.get(), RenderTextureLayout::COPY_SOURCE));
        continuation->end();
        const RenderCommandList* recreatedLists[] = {prefix.get(), isolated.get(), continuation.get()};
        queue->executeCommandLists(recreatedLists, 3, nullptr, 0, nullptr, 0, fence.get());
        controller.OnBatchSubmitted(recreated.useId, 3);
        queue->waitForCommandFence(fence.get());
        controller.ReleaseCompletedThrough(3);

#if defined(_WIN32)
        _putenv_s("LO_DLSS_TEST_INJECT_EVALUATE_FAILURE", "1");
#else
        setenv("LO_DLSS_TEST_INJECT_EVALUATE_FAILURE", "1", 1);
#endif
        inputs.renderFrameId = 4;
        inputs.resetHistory = false;
        prefix->begin();
        prefix->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(output.get(), RenderTextureLayout::COLOR_WRITE));
        prefix->setFramebuffer(outputFb.get()); prefix->clearColor(0, RenderColor(.125f, 0, 0, 1));
        prefix->barriers(RenderBarrierStage::ALL, nullptr, 0, ngxBarriers, 4);
        prefix->end();
        const auto injected = controller.RecordIsolated(*static_cast<VulkanCommandList*>(isolated.get()), config, inputs,
            *static_cast<VulkanTexture*>(output.get()));
#if defined(_WIN32)
        _putenv_s("LO_DLSS_TEST_INJECT_EVALUATE_FAILURE", "");
#else
        unsetenv("LO_DLSS_TEST_INJECT_EVALUATE_FAILURE");
#endif
        if (injected.status != gpu::dlss::SrStatus::Failed || !injected.useId)
            Fail("host-injected isolated recording did not fail");
        continuation->begin();
        continuation->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(output.get(), RenderTextureLayout::COPY_SOURCE));
        continuation->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(readback.get(), RenderFormat::R16G16B16A16_FLOAT,
            outputWidth, outputHeight, 1, outputWidth), RenderTextureCopyLocation::Subresource(output.get()));
        continuation->end();
        const RenderCommandList* fallbackLists[] = {prefix.get(), continuation.get()};
        queue->executeCommandLists(fallbackLists, 2, nullptr, 0, nullptr, 0, fence.get());
        controller.OnBatchSubmitted(injected.useId, 4);
        queue->waitForCommandFence(fence.get());
        controller.ReleaseCompletedThrough(4);
        pixels = static_cast<const uint16_t*>(readback->map());
        if (!FiniteHalf(pixels[0]) || pixels[0] == 0) Fail("failed isolated list changed the submitted fallback");
        readback->unmap();
        for (const auto& call : controller.Report().calls) {
            if (std::strncmp(call.name.c_str(), "vk", 2) == 0 || call.name.find("DLSS") != std::string::npos)
                std::printf("DLSS_DIAGNOSTIC %s=%d\n", call.name.c_str(), call.result);
        }
        controller.ShutdownAfterGpuDrain();
        std::puts("PASS: native DLSS persistent create/evaluate, fence retirement, and sizing coexistence");
        return 0;
    } catch (const std::exception& exception) {
        std::fprintf(stderr, "FAIL: %s\n", exception.what());
        return 1;
    }
}
