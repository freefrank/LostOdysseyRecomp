#include "fsr_upscaler.h"

#if defined(LO_GPU_PLUME)
#include <plume_vulkan.h>

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(LO_HAS_FSR) && LO_HAS_FSR
#include <FidelityFX/host/ffx_fsr3upscaler.h>
#include <FidelityFX/host/backends/vk/ffx_vk.h>
#include "fsr_prepare_spv.h"
#include "fsr_present_spv.h"
#endif

namespace gpu::fsr {

#if defined(LO_HAS_FSR) && LO_HAS_FSR
namespace {
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL FsrGetDeviceProcAddr(VkDevice device, const char* name) {
    if (auto function = vkGetDeviceProcAddr(device, name)) return function;
    // The pinned SDK asks for the KHR spelling even when the host enables the
    // Vulkan 1.1 core command instead of VK_KHR_get_memory_requirements2.
    if (std::strcmp(name, "vkGetBufferMemoryRequirements2KHR") == 0)
        return vkGetDeviceProcAddr(device, "vkGetBufferMemoryRequirements2");
    return nullptr;
}

FfxFsr3UpscalerQualityMode SdkQuality(upscaling::FsrQuality quality) {
    switch (quality) {
    case upscaling::FsrQuality::Quality: return FFX_FSR3UPSCALER_QUALITY_MODE_QUALITY;
    case upscaling::FsrQuality::Balanced: return FFX_FSR3UPSCALER_QUALITY_MODE_BALANCED;
    case upscaling::FsrQuality::Performance: return FFX_FSR3UPSCALER_QUALITY_MODE_PERFORMANCE;
    case upscaling::FsrQuality::NativeAA: return FFX_FSR3UPSCALER_QUALITY_MODE_NATIVEAA;
    }
    return FFX_FSR3UPSCALER_QUALITY_MODE_QUALITY;
}

bool FormatSupported(VkPhysicalDevice physical, VkFormat format, bool storage) {
    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(physical, format, &properties);
    VkFormatFeatureFlags required = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
        VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    if (storage) required |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
    return (properties.optimalTilingFeatures & required) == required;
}

struct CapabilityPolicy {
    FfxGetDeviceCapabilitiesFunc original = nullptr;
    bool reported = false;
};
std::mutex g_capabilityMutex;
std::unordered_map<void*, CapabilityPolicy> g_capabilityPolicies;

FfxErrorCode RestrictedCapabilities(FfxInterface* backend, FfxDeviceCapabilities* capabilities) {
    CapabilityPolicy policy;
    {
        std::lock_guard lock(g_capabilityMutex);
        auto it = g_capabilityPolicies.find(backend->scratchBuffer);
        if (it == g_capabilityPolicies.end()) return FFX_ERROR_INVALID_ARGUMENT;
        policy = it->second;
    }
    const auto result = policy.original(backend, capabilities);
    if (result != FFX_OK) return result;
    {
        std::lock_guard lock(g_capabilityMutex);
        auto it = g_capabilityPolicies.find(backend->scratchBuffer);
        if (it != g_capabilityPolicies.end() && !it->second.reported) {
            std::fprintf(stderr,
                "FSR Vulkan capability: physical-derived fp16=%u wave=%u..%u; enabled permutation policy fp32, actual-wave-range, SM5.1\n",
                unsigned(capabilities->fp16Supported), capabilities->waveLaneCountMin,
                capabilities->waveLaneCountMax);
            it->second.reported = true;
        }
    }
    // Plume's logical device does not enable shaderFloat16. The SDK Vulkan
    // backend already reports shader model 5.1, below its forced-wave64 gate;
    // preserve the real wave range while selecting generic FP32 shaders.
    capabilities->fp16Supported = false;
    return FFX_OK;
}

FfxResource MakeResource(const plume::VulkanTexture& texture, FfxResourceStates state,
    FfxResourceUsage usage = FFX_RESOURCE_USAGE_READ_ONLY) {
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = texture.imageFormat;
    info.extent = {texture.desc.width, texture.desc.height, 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (texture.desc.flags & plume::RenderTextureFlag::STORAGE) info.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    const auto description = ffxGetImageResourceDescriptionVK(texture.vk, info, usage);
    return ffxGetResourceVK(reinterpret_cast<void*>(texture.vk), description, nullptr, state);
}

std::unique_ptr<plume::VulkanTexture> CreateTexture(plume::VulkanDevice& device,
    uint32_t width, uint32_t height, plume::RenderFormat format) {
    auto base = device.createTexture(plume::RenderTextureDesc::Texture2D(width, height, 1,
        format, plume::RenderTextureFlag::STORAGE));
    if (!base) return {};
    auto* native = static_cast<plume::VulkanTexture*>(base.release());
    if (!native->vk || !native->imageView) { delete native; return {}; }
    return std::unique_ptr<plume::VulkanTexture>(native);
}

void Barrier(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = oldLayout == VK_IMAGE_LAYOUT_UNDEFINED ? 0 :
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);
}

bool ValidCamera(const FrameMetadata& m, bool resetHistory) {
    return m.cameraValid && std::isfinite(m.cameraFar) && m.cameraFar > 0 &&
        m.cameraNear == FLT_MAX && std::isfinite(m.verticalFovRadians) &&
        m.verticalFovRadians > 0 && m.verticalFovRadians < 3.14159265f &&
        std::isfinite(m.viewSpaceToMetersFactor) && m.viewSpaceToMetersFactor > 0 &&
        std::isfinite(m.frameTimeDeltaMilliseconds) &&
        (m.frameTimeDeltaMilliseconds > 0 || (resetHistory && m.frameTimeDeltaMilliseconds == 0)) &&
        std::isfinite(m.depthScale) && m.depthScale > 0 && std::isfinite(m.depthBias);
}

struct PrepareConstants {
    int32_t colorX, colorY, depthX, depthY, width, height;
    float depthScale, depthBias;
};
struct PresentConstants { int32_t width, height, renderWidth, renderHeight, colorX, colorY; };

VkDescriptorSetLayout MakeLayout(VkDevice device, uint32_t sampled, uint32_t storage, VkResult& error) {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    for (uint32_t i = 0; i < sampled + storage; ++i) {
        VkDescriptorSetLayoutBinding b{};
        b.binding = i;
        b.descriptorType = i < sampled ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        b.descriptorCount = 1;
        b.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        bindings.push_back(b);
    }
    VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    info.bindingCount = uint32_t(bindings.size());
    info.pBindings = bindings.data();
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    error = vkCreateDescriptorSetLayout(device, &info, nullptr, &layout);
    return error == VK_SUCCESS ? layout : VK_NULL_HANDLE;
}

VkPipelineLayout MakePipelineLayout(VkDevice device, VkDescriptorSetLayout setLayout, uint32_t pushSize,
    VkResult& error) {
    VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0, pushSize};
    VkPipelineLayoutCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    info.setLayoutCount = 1;
    info.pSetLayouts = &setLayout;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges = &push;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    error = vkCreatePipelineLayout(device, &info, nullptr, &layout);
    return error == VK_SUCCESS ? layout : VK_NULL_HANDLE;
}

VkPipeline MakePipeline(VkDevice device, VkPipelineLayout layout, const uint32_t* words, size_t bytes,
    VkResult& error, const char*& failedApi) {
    VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    moduleInfo.codeSize = bytes;
    moduleInfo.pCode = words;
    VkShaderModule module = VK_NULL_HANDLE;
    error = vkCreateShaderModule(device, &moduleInfo, nullptr, &module);
    if (error != VK_SUCCESS) { failedApi = "vkCreateShaderModule"; return VK_NULL_HANDLE; }
    VkComputePipelineCreateInfo info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    info.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    info.stage.module = module;
    info.stage.pName = "main";
    info.layout = layout;
    VkPipeline pipeline = VK_NULL_HANDLE;
    error = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);
    if (error != VK_SUCCESS) failedApi = "vkCreateComputePipelines";
    vkDestroyShaderModule(device, module, nullptr);
    return error == VK_SUCCESS ? pipeline : VK_NULL_HANDLE;
}
} // namespace
#endif

struct Controller::Impl {
    Diagnostics diagnostics{};
#if defined(LO_HAS_FSR) && LO_HAS_FSR
    plume::VulkanDevice* device = nullptr;
    Config config{};
    std::unique_ptr<FfxFsr3UpscalerContext> context;
    std::vector<uint8_t> backendScratch;
    bool contextReady = false;
    bool poisoned = false;
    bool sharedInitialized = false;
    std::unique_ptr<plume::VulkanTexture> dilatedDepth, dilatedMotion, previousDepth;
    std::unique_ptr<plume::VulkanTexture> linearColor, canonicalDepth, sdkOutput, encodedOutput;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout prepareSetLayout = VK_NULL_HANDLE, presentSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout prepareLayout = VK_NULL_HANDLE, presentLayout = VK_NULL_HANDLE;
    VkPipeline preparePipeline = VK_NULL_HANDLE, presentPipeline = VK_NULL_HANDLE;
    struct Use { uint64_t id, serial; VkDescriptorSet prepare, present; bool initializedShared, ready; };
    std::vector<Use> uses;
    uint64_t nextUse = 1, completed = 0;
    std::optional<uint64_t> lastRecordedRenderFrameId;
    std::optional<Config> lastGuardDiagnosticConfig;
    uint64_t lastGuardDiagnosticRequestSignature = 0;
    uint64_t lastGuardDiagnosticGeometryEpoch = 0;

    Status Fail(const char* api, int64_t raw, std::chrono::steady_clock::time_point started,
        Status status = Status::Failed) {
        diagnostics.failedApi = api;
        diagnostics.rawResult = raw;
        diagnostics.lastPrepareMilliseconds =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        std::fprintf(stderr, "FSR prepare failed: %s raw=%lld elapsed_ms=%.3f\n",
            api, static_cast<long long>(raw), diagnostics.lastPrepareMilliseconds);
        Destroy();
        return status;
    }

    void Destroy() {
        if (contextReady) ffxFsr3UpscalerContextDestroy(context.get());
        contextReady = false;
        poisoned = false;
        sharedInitialized = false;
        context.reset();
        if (!backendScratch.empty()) {
            std::lock_guard lock(g_capabilityMutex);
            g_capabilityPolicies.erase(backendScratch.data());
        }
        backendScratch.clear();
        if (device && device->vk) {
            const VkDevice vk = device->vk;
            if (preparePipeline) vkDestroyPipeline(vk, preparePipeline, nullptr);
            if (presentPipeline) vkDestroyPipeline(vk, presentPipeline, nullptr);
            if (prepareLayout) vkDestroyPipelineLayout(vk, prepareLayout, nullptr);
            if (presentLayout) vkDestroyPipelineLayout(vk, presentLayout, nullptr);
            if (prepareSetLayout) vkDestroyDescriptorSetLayout(vk, prepareSetLayout, nullptr);
            if (presentSetLayout) vkDestroyDescriptorSetLayout(vk, presentSetLayout, nullptr);
            if (descriptorPool) vkDestroyDescriptorPool(vk, descriptorPool, nullptr);
            if (sampler) vkDestroySampler(vk, sampler, nullptr);
        }
        preparePipeline = presentPipeline = VK_NULL_HANDLE;
        prepareLayout = presentLayout = VK_NULL_HANDLE;
        prepareSetLayout = presentSetLayout = VK_NULL_HANDLE;
        descriptorPool = VK_NULL_HANDLE;
        sampler = VK_NULL_HANDLE;
        dilatedDepth.reset(); dilatedMotion.reset(); previousDepth.reset();
        linearColor.reset(); canonicalDepth.reset(); sdkOutput.reset(); encodedOutput.reset();
        uses.clear();
        lastRecordedRenderFrameId.reset();
        lastGuardDiagnosticConfig.reset();
        lastGuardDiagnosticRequestSignature = 0;
        lastGuardDiagnosticGeometryEpoch = 0;
        device = nullptr;
    }

    bool CreatePipelines() {
        const VkDevice vk = device->vk;
        VkResult result = VK_SUCCESS;
        VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.addressModeU = samplerInfo.addressModeV = samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        result = vkCreateSampler(vk, &samplerInfo, nullptr, &sampler);
        if (result != VK_SUCCESS) { diagnostics.failedApi = "vkCreateSampler"; diagnostics.rawResult = result; return false; }
        prepareSetLayout = MakeLayout(vk, 2, 2, result);
        if (!prepareSetLayout) { diagnostics.failedApi = "vkCreateDescriptorSetLayout(prepare)"; diagnostics.rawResult = result; return false; }
        presentSetLayout = MakeLayout(vk, 2, 1, result);
        if (!presentSetLayout) { diagnostics.failedApi = "vkCreateDescriptorSetLayout(present)"; diagnostics.rawResult = result; return false; }
        if (!prepareSetLayout || !presentSetLayout) return false;
        prepareLayout = MakePipelineLayout(vk, prepareSetLayout, sizeof(PrepareConstants), result);
        if (!prepareLayout) { diagnostics.failedApi = "vkCreatePipelineLayout(prepare)"; diagnostics.rawResult = result; return false; }
        presentLayout = MakePipelineLayout(vk, presentSetLayout, sizeof(PresentConstants), result);
        if (!presentLayout) { diagnostics.failedApi = "vkCreatePipelineLayout(present)"; diagnostics.rawResult = result; return false; }
        if (!prepareLayout || !presentLayout) return false;
        const char* shaderApi = nullptr;
        preparePipeline = MakePipeline(vk, prepareLayout, lo_fsr_prepare_spv, sizeof(lo_fsr_prepare_spv), result, shaderApi);
        if (!preparePipeline) { diagnostics.failedApi = std::string(shaderApi) + "(prepare)"; diagnostics.rawResult = result; return false; }
        presentPipeline = MakePipeline(vk, presentLayout, lo_fsr_present_spv, sizeof(lo_fsr_present_spv), result, shaderApi);
        if (!presentPipeline) { diagnostics.failedApi = std::string(shaderApi) + "(present)"; diagnostics.rawResult = result; return false; }
        if (!preparePipeline || !presentPipeline) return false;
        VkDescriptorPoolSize sizes[] = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 * 128},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 * 128}};
        VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        pool.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool.maxSets = 2 * 128;
        pool.poolSizeCount = 2;
        pool.pPoolSizes = sizes;
        result = vkCreateDescriptorPool(vk, &pool, nullptr, &descriptorPool);
        if (result != VK_SUCCESS) { diagnostics.failedApi = "vkCreateDescriptorPool"; diagnostics.rawResult = result; return false; }
        return true;
    }

    VkResult AllocateSets(VkDescriptorSet& prepare, VkDescriptorSet& present) {
        VkDescriptorSetLayout layouts[] = {prepareSetLayout, presentSetLayout};
        VkDescriptorSet sets[2]{};
        VkDescriptorSetAllocateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        info.descriptorPool = descriptorPool;
        info.descriptorSetCount = 2;
        info.pSetLayouts = layouts;
        const VkResult result = vkAllocateDescriptorSets(device->vk, &info, sets);
        if (result != VK_SUCCESS) return result;
        prepare = sets[0]; present = sets[1];
        return VK_SUCCESS;
    }

    void FreeSets(const Use& use) {
        if (device && descriptorPool) {
            VkDescriptorSet sets[] = {use.prepare, use.present};
            vkFreeDescriptorSets(device->vk, descriptorPool, 2, sets);
        }
    }
#endif
};

Controller::Controller() : impl_(std::make_unique<Impl>()) {}
Controller::~Controller() {
#if defined(LO_HAS_FSR) && LO_HAS_FSR
    // The GPU owner normally calls ShutdownAfterGpuDrain. Final teardown may
    // wait once if it missed a completion notification; never destroy live
    // Vulkan textures while an isolated batch still references them.
    if (impl_->device && !impl_->uses.empty()) vkDeviceWaitIdle(impl_->device->vk);
    if (impl_->device) impl_->Destroy();
#endif
}

std::optional<resolution::Size> RecommendedRenderSize(resolution::Size output,
    upscaling::FsrQuality quality) {
    if (!output.width || !output.height || !upscaling::KnownFsrQuality(quality)) return std::nullopt;
#if defined(LO_HAS_FSR) && LO_HAS_FSR
    uint32_t width = 0, height = 0;
    if (ffxFsr3UpscalerGetRenderResolutionFromQualityMode(&width, &height,
        output.width, output.height, SdkQuality(quality)) != FFX_OK || !width || !height) return std::nullopt;
    return resolution::Size{width, height};
#else
    return std::nullopt;
#endif
}

Status Controller::EnsureSession(plume::VulkanDevice& device, const Config& config) {
#if defined(LO_HAS_FSR) && LO_HAS_FSR
    const auto started = std::chrono::steady_clock::now();
    if (!device.vk || !device.physicalDevice || !config.renderWidth || !config.renderHeight ||
        !config.outputWidth || !config.outputHeight || !config.deviceEpoch ||
        !upscaling::KnownFsrQuality(config.quality)) return Status::Unavailable;
    if (impl_->contextReady) return !impl_->poisoned && impl_->device == &device && impl_->config == config
        ? Status::Ready : Status::NeedsReconfigure;
    if (impl_->device) return Status::NeedsReconfigure;
    impl_->diagnostics = {};
    for (const auto [format, storage] : {
        std::pair{VK_FORMAT_R8G8B8A8_UNORM, true},
        std::pair{VK_FORMAT_R16G16B16A16_SFLOAT, true},
        std::pair{VK_FORMAT_R32_SFLOAT, true},
        std::pair{VK_FORMAT_R16G16_SFLOAT, true},
        std::pair{VK_FORMAT_R32_UINT, true}}) {
        if (!FormatSupported(device.physicalDevice, format, storage))
            return impl_->Fail("vkGetPhysicalDeviceFormatProperties(optimal sampled/storage/transfer)",
                int64_t(format), started, Status::Unavailable);
    }
    if (!FsrGetDeviceProcAddr(device.vk, "vkGetBufferMemoryRequirements2KHR"))
        return impl_->Fail("vkGetDeviceProcAddr(vkGetBufferMemoryRequirements2KHR/core)",
            0, started, Status::Unavailable);
    impl_->device = &device;
    impl_->config = config;
    VkDeviceContext vkContext{device.vk, device.physicalDevice, FsrGetDeviceProcAddr};
    const size_t scratchBytes = ffxGetScratchMemorySizeVK(device.physicalDevice, FFX_FSR3UPSCALER_CONTEXT_COUNT);
    if (!scratchBytes) return impl_->Fail("ffxGetScratchMemorySizeVK", 0, started);
    impl_->backendScratch.resize(scratchBytes);
    FfxInterface backend{};
    const auto interfaceResult = ffxGetInterfaceVK(&backend, ffxGetDeviceVK(&vkContext),
        impl_->backendScratch.data(), impl_->backendScratch.size(), FFX_FSR3UPSCALER_CONTEXT_COUNT);
    if (interfaceResult != FFX_OK) return impl_->Fail("ffxGetInterfaceVK", int32_t(interfaceResult), started);
    if (!backend.fpGetDeviceCapabilities)
        return impl_->Fail("ffxGetInterfaceVK.fpGetDeviceCapabilities", 0, started, Status::Unavailable);
    {
        std::lock_guard lock(g_capabilityMutex);
        g_capabilityPolicies[backend.scratchBuffer] = {backend.fpGetDeviceCapabilities, false};
    }
    backend.fpGetDeviceCapabilities = RestrictedCapabilities;
    FfxFsr3UpscalerContextDescription desc{};
    desc.flags = FFX_FSR3UPSCALER_ENABLE_DEPTH_INVERTED | FFX_FSR3UPSCALER_ENABLE_DEPTH_INFINITE;
    desc.maxRenderSize = {config.renderWidth, config.renderHeight};
    desc.maxUpscaleSize = {config.outputWidth, config.outputHeight};
    desc.backendInterface = backend;
    impl_->context = std::make_unique<FfxFsr3UpscalerContext>();
    const auto createResult = ffxFsr3UpscalerContextCreate(impl_->context.get(), &desc);
    if (createResult != FFX_OK)
        return impl_->Fail("ffxFsr3UpscalerContextCreate", int32_t(createResult), started);
    impl_->contextReady = true;

    FfxFsr3UpscalerSharedResourceDescriptions shared{};
    if (const auto result = ffxFsr3UpscalerGetSharedResourceDescriptions(impl_->context.get(), &shared);
        result != FFX_OK)
        return impl_->Fail("ffxFsr3UpscalerGetSharedResourceDescriptions", int32_t(result), started);
    auto makeShared = [&](const FfxCreateResourceDescription& resource) {
        plume::RenderFormat format = plume::RenderFormat::UNKNOWN;
        switch (resource.resourceDescription.format) {
        case FFX_SURFACE_FORMAT_R32_FLOAT: format = plume::RenderFormat::R32_FLOAT; break;
        case FFX_SURFACE_FORMAT_R16G16_FLOAT: format = plume::RenderFormat::R16G16_FLOAT; break;
        case FFX_SURFACE_FORMAT_R32_UINT: format = plume::RenderFormat::R32_UINT; break;
        default: return std::unique_ptr<plume::VulkanTexture>{};
        }
        return CreateTexture(device, resource.resourceDescription.width, resource.resourceDescription.height, format);
    };
    impl_->dilatedDepth = makeShared(shared.dilatedDepth);
    impl_->dilatedMotion = makeShared(shared.dilatedMotionVectors);
    impl_->previousDepth = makeShared(shared.reconstructedPrevNearestDepth);
    impl_->linearColor = CreateTexture(device, config.renderWidth, config.renderHeight, plume::RenderFormat::R16G16B16A16_FLOAT);
    impl_->canonicalDepth = CreateTexture(device, config.renderWidth, config.renderHeight, plume::RenderFormat::R32_FLOAT);
    impl_->sdkOutput = CreateTexture(device, config.outputWidth, config.outputHeight, plume::RenderFormat::R16G16B16A16_FLOAT);
    impl_->encodedOutput = CreateTexture(device, config.outputWidth, config.outputHeight, plume::RenderFormat::R8G8B8A8_UNORM);
    if (!impl_->dilatedDepth || !impl_->dilatedMotion || !impl_->previousDepth ||
        !impl_->linearColor || !impl_->canonicalDepth || !impl_->sdkOutput || !impl_->encodedOutput)
        return impl_->Fail("VulkanDevice::createTexture(FSR shared or conversion)", 0, started);
    if (!impl_->CreatePipelines()) {
        const std::string api = impl_->diagnostics.failedApi;
        const int64_t raw = impl_->diagnostics.rawResult;
        return impl_->Fail(api.c_str(), raw, started,
            raw == VK_ERROR_DEVICE_LOST ? Status::DeviceLost : Status::Failed);
    }
    impl_->diagnostics.lastPrepareMilliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    std::fprintf(stderr, "FSR prepare ready: %ux%u -> %ux%u quality=%u elapsed_ms=%.3f\n",
        config.renderWidth, config.renderHeight, config.outputWidth, config.outputHeight,
        unsigned(config.quality), impl_->diagnostics.lastPrepareMilliseconds);
    return Status::Ready;
#else
    (void)device; (void)config;
    return Status::Unavailable;
#endif
}

Attempt Controller::RecordIsolated(plume::VulkanCommandList& commands, const Config& config,
    const temporal::TemporalFrameInputs& inputs, const FrameMetadata& frame,
    plume::VulkanTexture& output) {
    Attempt attempt{};
#if defined(LO_HAS_FSR) && LO_HAS_FSR
    const bool frameGap = impl_->lastRecordedRenderFrameId &&
        !(inputs.renderFrameId > *impl_->lastRecordedRenderFrameId &&
          inputs.renderFrameId - *impl_->lastRecordedRenderFrameId == 1);
    const bool effectiveReset = inputs.resetHistory ||
        !impl_->lastRecordedRenderFrameId || frameGap;
    auto logGuardRejection = [&](const char* reason) {
        if (impl_->lastGuardDiagnosticConfig == config &&
            impl_->lastGuardDiagnosticRequestSignature == inputs.plan.requestSignature &&
            impl_->lastGuardDiagnosticGeometryEpoch == inputs.plan.geometryEpoch) return;
        impl_->lastGuardDiagnosticConfig = config;
        impl_->lastGuardDiagnosticRequestSignature = inputs.plan.requestSignature;
        impl_->lastGuardDiagnosticGeometryEpoch = inputs.plan.geometryEpoch;
        auto describeRegion = [](const char* name, const temporal::TextureRegion& region) {
            const auto* native = static_cast<const plume::VulkanTexture*>(region.texture);
            std::fprintf(stderr,
                " %s={format:%d,layout:%d,image:%d,region:%u,%u,%u,%u,allocation:%u,%u}",
                name, native ? int(native->imageFormat) : -1,
                native ? int(native->textureLayout) : -1, native && native->vk ? 1 : 0,
                region.x, region.y, region.width, region.height,
                region.allocation.width, region.allocation.height);
        };
        std::fprintf(stderr,
            "FSR record guard rejected: frame=%llu reason=%s request=0x%llx geometry_epoch=%llu config=%ux%u->%ux%u quality=%u device_epoch=%llu"
            " expected_formats={color:R8G8B8A8_UNORM,depth:R32_SFLOAT,motion:R16G16_SFLOAT,output:R8G8B8A8_UNORM}",
            static_cast<unsigned long long>(inputs.renderFrameId), reason,
            static_cast<unsigned long long>(inputs.plan.requestSignature),
            static_cast<unsigned long long>(inputs.plan.geometryEpoch),
            config.renderWidth, config.renderHeight, config.outputWidth, config.outputHeight,
            unsigned(config.quality), static_cast<unsigned long long>(config.deviceEpoch));
        describeRegion("color", inputs.color);
        describeRegion("depth", inputs.depth);
        describeRegion("motion", inputs.motion);
        std::fprintf(stderr, " output={format:%d,layout:%d,image:%d,extent:%u,%u}\n",
            int(output.imageFormat), int(output.textureLayout), output.vk ? 1 : 0,
            output.desc.width, output.desc.height);
    };
    if (!impl_->contextReady || impl_->poisoned || impl_->config != config ||
        !ValidCamera(frame, effectiveReset) ||
        !inputs.CompleteForConsumer() || inputs.colorEncoding != temporal::ColorEncoding::Sdr ||
        inputs.motionState == temporal::MotionState::Unavailable ||
        inputs.color.width != config.renderWidth || inputs.color.height != config.renderHeight ||
        inputs.depth.width != config.renderWidth || inputs.depth.height != config.renderHeight ||
        inputs.motion.width != config.renderWidth || inputs.motion.height != config.renderHeight ||
        inputs.motion.x || inputs.motion.y ||
        inputs.motion.allocation.width != config.renderWidth ||
        inputs.motion.allocation.height != config.renderHeight ||
        output.desc.width != config.outputWidth || output.desc.height != config.outputHeight ||
        !std::isfinite(inputs.jitter.pixelX) || !std::isfinite(inputs.jitter.pixelY) ||
        !std::isfinite(inputs.preExposure) || inputs.preExposure <= 0 ||
        !impl_->nextUse) {
        const char* reason = !impl_->contextReady ? "context_not_ready" :
            impl_->poisoned ? "context_poisoned" : impl_->config != config ? "config_mismatch" :
            !ValidCamera(frame, effectiveReset) ? "camera_invalid" :
            !inputs.CompleteForConsumer() ? "inputs_incomplete" :
            inputs.colorEncoding != temporal::ColorEncoding::Sdr ? "color_not_sdr" :
            inputs.motionState == temporal::MotionState::Unavailable ? "motion_unavailable" :
            inputs.color.width != config.renderWidth || inputs.color.height != config.renderHeight ? "color_extent" :
            inputs.depth.width != config.renderWidth || inputs.depth.height != config.renderHeight ? "depth_extent" :
            inputs.motion.width != config.renderWidth || inputs.motion.height != config.renderHeight ? "motion_extent" :
            inputs.motion.x || inputs.motion.y || inputs.motion.allocation.width != config.renderWidth ||
                inputs.motion.allocation.height != config.renderHeight ? "motion_allocation_or_origin" :
            output.desc.width != config.outputWidth || output.desc.height != config.outputHeight ? "output_extent" :
            !std::isfinite(inputs.jitter.pixelX) || !std::isfinite(inputs.jitter.pixelY) ? "jitter_invalid" :
            !std::isfinite(inputs.preExposure) || inputs.preExposure <= 0 ? "pre_exposure_invalid" :
            "use_id_exhausted";
        logGuardRejection(reason);
        return attempt;
    }
    const auto& color = *static_cast<const plume::VulkanTexture*>(inputs.color.texture);
    const auto& depth = *static_cast<const plume::VulkanTexture*>(inputs.depth.texture);
    const auto& motion = *static_cast<const plume::VulkanTexture*>(inputs.motion.texture);
    if (!color.vk || !depth.vk || !motion.vk || !output.vk ||
        color.imageFormat != VK_FORMAT_R8G8B8A8_UNORM ||
        depth.imageFormat != VK_FORMAT_R32_SFLOAT ||
        motion.imageFormat != VK_FORMAT_R16G16_SFLOAT ||
        output.imageFormat != VK_FORMAT_R8G8B8A8_UNORM ||
        color.textureLayout != plume::RenderTextureLayout::SHADER_READ ||
        depth.textureLayout != plume::RenderTextureLayout::SHADER_READ ||
        motion.textureLayout != plume::RenderTextureLayout::SHADER_READ ||
        output.textureLayout != plume::RenderTextureLayout::COPY_DEST) {
        const char* reason = !color.vk || !depth.vk || !motion.vk || !output.vk ? "missing_image" :
            color.imageFormat != VK_FORMAT_R8G8B8A8_UNORM ? "color_format" :
            depth.imageFormat != VK_FORMAT_R32_SFLOAT ? "depth_format" :
            motion.imageFormat != VK_FORMAT_R16G16_SFLOAT ? "motion_format" :
            output.imageFormat != VK_FORMAT_R8G8B8A8_UNORM ? "output_format" :
            color.textureLayout != plume::RenderTextureLayout::SHADER_READ ? "color_layout" :
            depth.textureLayout != plume::RenderTextureLayout::SHADER_READ ? "depth_layout" :
            motion.textureLayout != plume::RenderTextureLayout::SHADER_READ ? "motion_layout" :
            "output_layout";
        logGuardRejection(reason);
        return attempt;
    }

    VkDescriptorSet prepare = VK_NULL_HANDLE, present = VK_NULL_HANDLE;
    if (const VkResult allocate = impl_->AllocateSets(prepare, present); allocate != VK_SUCCESS) {
        attempt.vkResult = int32_t(allocate);
        attempt.status = allocate == VK_ERROR_DEVICE_LOST ? Status::DeviceLost : Status::Failed;
        return attempt;
    }
    const uint64_t useId = impl_->nextUse++;
    impl_->uses.push_back({useId, 0, prepare, present, !impl_->sharedInitialized, false});
    attempt.useId = useId;

    VkDescriptorImageInfo prepareImages[] = {
        {impl_->sampler, color.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {impl_->sampler, depth.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {VK_NULL_HANDLE, impl_->linearColor->imageView, VK_IMAGE_LAYOUT_GENERAL},
        {VK_NULL_HANDLE, impl_->canonicalDepth->imageView, VK_IMAGE_LAYOUT_GENERAL}
    };
    VkDescriptorImageInfo presentImages[] = {
        {impl_->sampler, impl_->sdkOutput->imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {impl_->sampler, color.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {VK_NULL_HANDLE, impl_->encodedOutput->imageView, VK_IMAGE_LAYOUT_GENERAL}
    };
    VkWriteDescriptorSet writes[7]{};
    for (uint32_t i = 0; i < 7; ++i) {
        auto& write = writes[i];
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = i < 4 ? prepare : present;
        write.dstBinding = i < 4 ? i : i - 4;
        write.descriptorType = (i < 2 || i == 4 || i == 5) ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        write.descriptorCount = 1;
        write.pImageInfo = i < 4 ? &prepareImages[i] : &presentImages[i - 4];
    }
    vkUpdateDescriptorSets(impl_->device->vk, 7, writes, 0, nullptr);

    const VkResult resetResult = commands.vk ? vkResetCommandBuffer(commands.vk, 0) : VK_ERROR_INITIALIZATION_FAILED;
    if (resetResult != VK_SUCCESS) {
        attempt.vkResult = int32_t(resetResult);
        attempt.status = resetResult == VK_ERROR_DEVICE_LOST ? Status::DeviceLost : Status::Failed;
        return attempt;
    }
    commands.activeGraphicsDescriptorSets.clear();
    commands.recording = false;
    commands.externalCommandsOpen = false;
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    const VkResult beginResult = vkBeginCommandBuffer(commands.vk, &beginInfo);
    if (beginResult != VK_SUCCESS) {
        attempt.vkResult = int32_t(beginResult);
        attempt.status = beginResult == VK_ERROR_DEVICE_LOST ? Status::DeviceLost : Status::Failed;
        return attempt;
    }
    commands.recording = true;
    VkCommandBuffer cmd = commands.beginExternalCommands();
    if (!cmd) { attempt.status = Status::Failed; impl_->poisoned = true; return attempt; }
    Barrier(cmd, impl_->linearColor->vk, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    Barrier(cmd, impl_->canonicalDepth->vk, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    Barrier(cmd, impl_->sdkOutput->vk, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    Barrier(cmd, impl_->encodedOutput->vk, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    if (!impl_->sharedInitialized) {
        Barrier(cmd, impl_->dilatedDepth->vk, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        Barrier(cmd, impl_->dilatedMotion->vk, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        Barrier(cmd, impl_->previousDepth->vk, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    }
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, impl_->preparePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, impl_->prepareLayout,
        0, 1, &prepare, 0, nullptr);
    const PrepareConstants prepareParams{int32_t(inputs.color.x), int32_t(inputs.color.y),
        int32_t(inputs.depth.x), int32_t(inputs.depth.y), int32_t(config.renderWidth),
        int32_t(config.renderHeight), frame.depthScale, frame.depthBias};
    vkCmdPushConstants(cmd, impl_->prepareLayout, VK_SHADER_STAGE_COMPUTE_BIT,
        0, sizeof(prepareParams), &prepareParams);
    vkCmdDispatch(cmd, (config.renderWidth + 7) / 8, (config.renderHeight + 7) / 8, 1);
    Barrier(cmd, impl_->linearColor->vk, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    Barrier(cmd, impl_->canonicalDepth->vk, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    FfxFsr3UpscalerDispatchDescription dispatch{};
    dispatch.commandList = ffxGetCommandListVK(cmd);
    dispatch.color = MakeResource(*impl_->linearColor, FFX_RESOURCE_STATE_COMPUTE_READ);
    dispatch.depth = MakeResource(*impl_->canonicalDepth, FFX_RESOURCE_STATE_COMPUTE_READ);
    dispatch.motionVectors = MakeResource(motion, FFX_RESOURCE_STATE_COMPUTE_READ);
    dispatch.dilatedDepth = MakeResource(*impl_->dilatedDepth, FFX_RESOURCE_STATE_UNORDERED_ACCESS, FFX_RESOURCE_USAGE_UAV);
    dispatch.dilatedMotionVectors = MakeResource(*impl_->dilatedMotion, FFX_RESOURCE_STATE_UNORDERED_ACCESS, FFX_RESOURCE_USAGE_UAV);
    dispatch.reconstructedPrevNearestDepth = MakeResource(*impl_->previousDepth, FFX_RESOURCE_STATE_UNORDERED_ACCESS, FFX_RESOURCE_USAGE_UAV);
    dispatch.output = MakeResource(*impl_->sdkOutput, FFX_RESOURCE_STATE_UNORDERED_ACCESS, FFX_RESOURCE_USAGE_UAV);
    dispatch.jitterOffset = {float(inputs.jitter.pixelX), float(inputs.jitter.pixelY)};
    dispatch.motionVectorScale = {1.0f, 1.0f};
    dispatch.renderSize = {config.renderWidth, config.renderHeight};
    dispatch.upscaleSize = {config.outputWidth, config.outputHeight};
    dispatch.enableSharpening = false;
    dispatch.frameTimeDelta = frame.frameTimeDeltaMilliseconds;
    dispatch.preExposure = 1.0f;
    dispatch.reset = effectiveReset;
    dispatch.cameraNear = frame.cameraNear;
    dispatch.cameraFar = frame.cameraFar;
    dispatch.cameraFovAngleVertical = frame.verticalFovRadians;
    dispatch.viewSpaceToMetersFactor = frame.viewSpaceToMetersFactor;
    const auto result = ffxFsr3UpscalerContextDispatch(impl_->context.get(), &dispatch);
    attempt.sdkResult = int32_t(result);
    if (result == FFX_OK) {
        Barrier(cmd, impl_->sdkOutput->vk, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, impl_->presentPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, impl_->presentLayout,
            0, 1, &present, 0, nullptr);
        const PresentConstants presentParams{int32_t(config.outputWidth), int32_t(config.outputHeight),
            int32_t(config.renderWidth), int32_t(config.renderHeight),
            int32_t(inputs.color.x), int32_t(inputs.color.y)};
        vkCmdPushConstants(cmd, impl_->presentLayout, VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(presentParams), &presentParams);
        vkCmdDispatch(cmd, (config.outputWidth + 7) / 8, (config.outputHeight + 7) / 8, 1);
        Barrier(cmd, impl_->sdkOutput->vk, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
        Barrier(cmd, impl_->encodedOutput->vk, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        VkImageCopy copy{};
        copy.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.extent = {config.outputWidth, config.outputHeight, 1};
        vkCmdCopyImage(cmd, impl_->encodedOutput->vk, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            output.vk, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    }
    commands.endExternalCommands();
    const VkResult endResult = vkEndCommandBuffer(cmd);
    attempt.vkResult = int32_t(endResult);
    if (endResult == VK_SUCCESS) {
        commands.targetFramebuffer = nullptr;
        commands.activeComputePipelineLayout = nullptr;
        commands.activeGraphicsPipelineLayout = nullptr;
        commands.activeRaytracingPipelineLayout = nullptr;
        commands.activeGraphicsDescriptorSets.clear();
        commands.recording = false;
    }
    attempt.status = endResult == VK_ERROR_DEVICE_LOST ? Status::DeviceLost :
        result == FFX_OK && endResult == VK_SUCCESS ? Status::Ready : Status::Failed;
    impl_->uses.back().ready = attempt.status == Status::Ready;
    if (attempt.status == Status::Ready) {
        if (effectiveReset && !inputs.resetHistory)
            std::fprintf(stderr, "FSR dispatch frame=%llu input_reset=0 sdk_reset=1 reason=%s previous_fsr_frame=%llu\n",
                static_cast<unsigned long long>(inputs.renderFrameId),
                frameGap ? "frame_gap" : "new_context",
                static_cast<unsigned long long>(impl_->lastRecordedRenderFrameId.value_or(0)));
        impl_->lastRecordedRenderFrameId = inputs.renderFrameId;
        impl_->diagnostics.lastDispatchRenderFrameId = inputs.renderFrameId;
        impl_->diagnostics.lastDispatchReset = effectiveReset;
        impl_->diagnostics.lastResetForFrameGap = frameGap;
    } else impl_->poisoned = true;
#else
    (void)commands; (void)config; (void)inputs; (void)frame; (void)output;
#endif
    return attempt;
}

void Controller::OnBatchSubmitted(uint64_t useId, uint64_t serial) {
#if defined(LO_HAS_FSR) && LO_HAS_FSR
    if (!useId || !serial || serial <= impl_->completed) return;
    for (auto& use : impl_->uses) if (use.id == useId && !use.serial) {
        use.serial = serial;
        if (use.ready && use.initializedShared) impl_->sharedInitialized = true;
        break;
    }
#else
    (void)useId; (void)serial;
#endif
}
void Controller::OnBatchDiscarded(uint64_t useId) {
#if defined(LO_HAS_FSR) && LO_HAS_FSR
    auto it = std::find_if(impl_->uses.begin(), impl_->uses.end(),
        [useId](const Impl::Use& use) { return use.id == useId && !use.serial; });
    if (it != impl_->uses.end()) {
        // FSR advances CPU-side history when dispatch records. A successfully
        // recorded list omitted from submission invalidates that history.
        if (it->ready) impl_->poisoned = true;
        impl_->FreeSets(*it);
        impl_->uses.erase(it);
    }
#else
    (void)useId;
#endif
}
void Controller::ReleaseCompletedThrough(uint64_t serial) {
#if defined(LO_HAS_FSR) && LO_HAS_FSR
    impl_->completed = std::max(impl_->completed, serial);
    auto it = impl_->uses.begin();
    while (it != impl_->uses.end()) {
        if (it->serial && it->serial <= impl_->completed) {
            impl_->FreeSets(*it);
            it = impl_->uses.erase(it);
        } else ++it;
    }
#else
    (void)serial;
#endif
}
bool Controller::HasFeatureState() const {
#if defined(LO_HAS_FSR) && LO_HAS_FSR
    return impl_->device != nullptr || !impl_->uses.empty();
#else
    return false;
#endif
}
const Diagnostics& Controller::LastDiagnostics() const { return impl_->diagnostics; }
void Controller::ReleaseFeatureAfterGpuDrain() {
#if defined(LO_HAS_FSR) && LO_HAS_FSR
    if (impl_->uses.empty()) impl_->Destroy();
#endif
}
void Controller::ShutdownAfterGpuDrain() { ReleaseFeatureAfterGpuDrain(); }
void Controller::AbandonUsesAfterDeviceLoss() {
#if defined(LO_HAS_FSR) && LO_HAS_FSR
    // The caller still owns a live VkDevice; after loss, no submitted work can
    // complete successfully. Tear down before that device is destroyed.
    impl_->uses.clear();
    impl_->Destroy();
#endif
}

} // namespace gpu::fsr
#endif
