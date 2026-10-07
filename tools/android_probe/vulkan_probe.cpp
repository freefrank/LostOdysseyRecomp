#include "vulkan_probe.h"
#include "../../LostOdysseyRecomp/gpu/backend_selection.h"
#include "../../LostOdysseyRecomp/gpu/render_arena_policy.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace lo::android_probe {
namespace {

const char* ResultName(VkResult result) {
    switch (result) {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_INCOMPLETE: return "VK_INCOMPLETE";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
    default: return nullptr;
    }
}

std::string ResultText(VkResult result) {
    if (const char* name = ResultName(result)) return name;
    return "VkResult(" + std::to_string(static_cast<int>(result)) + ")";
}

std::string Version(uint32_t value) {
    return std::to_string(VK_VERSION_MAJOR(value)) + "." +
           std::to_string(VK_VERSION_MINOR(value)) + "." +
           std::to_string(VK_VERSION_PATCH(value));
}

std::string Hex(uint32_t value) {
    std::ostringstream out;
    out << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
    return out.str();
}

const char* YesNo(bool value) { return value ? "yes" : "no"; }

struct ExtensionStatus {
    VkResult result = VK_SUCCESS;
    bool present = false;
};

ExtensionStatus HasExtension(VkPhysicalDevice device, const char* wanted) {
    uint32_t count = 0;
    VkResult result = vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    if (result != VK_SUCCESS) return {result, false};
    std::vector<VkExtensionProperties> extensions(count);
    result = vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data());
    if (result != VK_SUCCESS) return {result, false};
    for (const auto& extension : extensions)
        if (std::string(extension.extensionName) == wanted) return {VK_SUCCESS, true};
    return {VK_SUCCESS, false};
}

struct InstanceGuard {
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    ~InstanceGuard() {
        if (surface) vkDestroySurfaceKHR(instance, surface, nullptr);
        if (instance) vkDestroyInstance(instance, nullptr);
    }
};

struct PresentSession {
    VkDevice device = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkSemaphore acquired = VK_NULL_HANDLE;
    VkSemaphore rendered = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    ~PresentSession() {
        if (!device) return;
        vkDeviceWaitIdle(device);
        if (fence) vkDestroyFence(device, fence, nullptr);
        if (rendered) vkDestroySemaphore(device, rendered, nullptr);
        if (acquired) vkDestroySemaphore(device, acquired, nullptr);
        if (pool) vkDestroyCommandPool(device, pool, nullptr);
        if (swapchain) vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroyDevice(device, nullptr);
    }
};

std::string TryClearPresent(SDL_Window* window, VkPhysicalDevice physical, VkSurfaceKHR surface,
                            uint32_t queueFamily) {
    PresentSession session;
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = queueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;
    const char* extension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = 1;
    deviceInfo.ppEnabledExtensionNames = &extension;
    VkResult result = vkCreateDevice(physical, &deviceInfo, nullptr, &session.device);
    if (result != VK_SUCCESS) return "device creation failed: " + ResultText(result);

    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(session.device, queueFamily, 0, &queue);
    VkSurfaceCapabilitiesKHR caps{};
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &caps);
    if (result != VK_SUCCESS) return "surface capabilities failed: " + ResultText(result);
    if (!(caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
        return "swapchain images do not support transfer-destination clear";
    uint32_t formatCount = 0;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &formatCount, nullptr);
    if (result != VK_SUCCESS || !formatCount) return "surface formats unavailable: " + ResultText(result);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &formatCount, formats.data());
    if (result != VK_SUCCESS) return "surface formats failed: " + ResultText(result);
    VkSurfaceFormatKHR chosen = formats[0];
    if (chosen.format == VK_FORMAT_UNDEFINED) chosen.format = VK_FORMAT_R8G8B8A8_UNORM;
    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        int width = 0, height = 0;
        SDL_Vulkan_GetDrawableSize(window, &width, &height);
        extent.width = std::clamp(static_cast<uint32_t>(std::max(0, width)), caps.minImageExtent.width, caps.maxImageExtent.width);
        extent.height = std::clamp(static_cast<uint32_t>(std::max(0, height)), caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    if (!extent.width || !extent.height) return "surface has zero drawable extent; present deferred";
    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount && imageCount > caps.maxImageCount) imageCount = caps.maxImageCount;
    VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (!(caps.supportedCompositeAlpha & alpha)) {
        for (auto choice : {VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
                            VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR}) {
            if (caps.supportedCompositeAlpha & choice) { alpha = choice; break; }
        }
        if (!(caps.supportedCompositeAlpha & alpha)) return "no supported composite alpha mode";
    }
    VkSwapchainCreateInfoKHR swapInfo{};
    swapInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapInfo.surface = surface;
    swapInfo.minImageCount = imageCount;
    swapInfo.imageFormat = chosen.format;
    swapInfo.imageColorSpace = chosen.colorSpace;
    swapInfo.imageExtent = extent;
    swapInfo.imageArrayLayers = 1;
    swapInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapInfo.preTransform = caps.currentTransform;
    swapInfo.compositeAlpha = alpha;
    swapInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapInfo.clipped = VK_TRUE;
    result = vkCreateSwapchainKHR(session.device, &swapInfo, nullptr, &session.swapchain);
    if (result != VK_SUCCESS) return "swapchain creation failed: " + ResultText(result);
    uint32_t swapImageCount = 0;
    result = vkGetSwapchainImagesKHR(session.device, session.swapchain, &swapImageCount, nullptr);
    if (result != VK_SUCCESS || !swapImageCount) return "swapchain images unavailable: " + ResultText(result);
    std::vector<VkImage> images(swapImageCount);
    result = vkGetSwapchainImagesKHR(session.device, session.swapchain, &swapImageCount, images.data());
    if (result != VK_SUCCESS) return "swapchain image query failed: " + ResultText(result);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamily;
    result = vkCreateCommandPool(session.device, &poolInfo, nullptr, &session.pool);
    if (result != VK_SUCCESS) return "command pool failed: " + ResultText(result);
    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = session.pool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;
    VkCommandBuffer command = VK_NULL_HANDLE;
    result = vkAllocateCommandBuffers(session.device, &alloc, &command);
    if (result != VK_SUCCESS) return "command buffer failed: " + ResultText(result);
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    result = vkCreateSemaphore(session.device, &semaphoreInfo, nullptr, &session.acquired);
    if (result != VK_SUCCESS) return "acquire semaphore failed: " + ResultText(result);
    result = vkCreateSemaphore(session.device, &semaphoreInfo, nullptr, &session.rendered);
    if (result != VK_SUCCESS) return "render semaphore failed: " + ResultText(result);
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    result = vkCreateFence(session.device, &fenceInfo, nullptr, &session.fence);
    if (result != VK_SUCCESS) return "submit fence failed: " + ResultText(result);

    uint32_t index = 0;
    const VkResult acquireResult = vkAcquireNextImageKHR(session.device, session.swapchain,
                                                          5'000'000'000ull, session.acquired,
                                                          VK_NULL_HANDLE, &index);
    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
        return "image acquire failed: " + ResultText(acquireResult);
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = vkBeginCommandBuffer(command, &begin);
    if (result != VK_SUCCESS) return "command begin failed: " + ResultText(result);
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = images[index];
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    VkClearColorValue color{{0.04f, 0.10f, 0.22f, 1.0f}};
    vkCmdClearColorImage(command, images[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         &color, 1, &barrier.subresourceRange);
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = 0;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    result = vkEndCommandBuffer(command);
    if (result != VK_SUCCESS) return "command end failed: " + ResultText(result);
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &session.acquired;
    submit.pWaitDstStageMask = &waitStage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &command;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &session.rendered;
    result = vkQueueSubmit(queue, 1, &submit, session.fence);
    if (result != VK_SUCCESS) return "clear submit failed: " + ResultText(result);
    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &session.rendered;
    present.swapchainCount = 1;
    present.pSwapchains = &session.swapchain;
    present.pImageIndices = &index;
    const VkResult presentResult = vkQueuePresentKHR(queue, &present);
    result = vkWaitForFences(session.device, 1, &session.fence, VK_TRUE, 5'000'000'000ull);
    if (result != VK_SUCCESS) return "clear submit wait failed: " + ResultText(result);
    if (presentResult != VK_SUCCESS && presentResult != VK_SUBOPTIMAL_KHR)
        return "present failed: " + ResultText(presentResult);
    std::string status = "clear submit and present succeeded";
    if (acquireResult == VK_SUBOPTIMAL_KHR || presentResult == VK_SUBOPTIMAL_KHR)
        status += " (surface suboptimal; recreate for continued rendering)";
    return status + "; " + std::to_string(extent.width) + "x" + std::to_string(extent.height);
}

std::string FormatFeatures(VkPhysicalDevice device, const char* label, VkFormat format,
                           VkFormatFeatureFlags needed) {
    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(device, format, &properties);
    const VkFormatFeatureFlags actual = properties.optimalTilingFeatures;
    std::ostringstream out;
    out << "  " << label << ": optimal sampled=" << YesNo(actual & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
        << " color=" << YesNo(actual & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)
        << " depth/stencil=" << YesNo(actual & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        << " storage=" << YesNo(actual & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT)
        << " required=" << YesNo((actual & needed) == needed) << '\n';
    return out.str();
}

} // namespace

std::string ProbeVulkan(SDL_Window* window) {
    std::ostringstream out;
    out << "Vulkan probe (clear/present only; game renderer and assets not initialized)\n";
    if (!window) return out.str() + "SDL window unavailable\n";
    uint32_t extensionCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr) || !extensionCount)
        return out.str() + "SDL Vulkan instance extensions unavailable: " + SDL_GetError() + "\n";
    std::vector<const char*> extensions(extensionCount);
    if (!SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensions.data()))
        return out.str() + "SDL Vulkan extension query failed: " + SDL_GetError() + "\n";
    uint32_t loaderVersion = VK_API_VERSION_1_0;
    auto enumerateVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
        vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"));
    if (enumerateVersion) {
        const VkResult result = enumerateVersion(&loaderVersion);
        if (result != VK_SUCCESS) return out.str() + "Vulkan loader version failed: " + ResultText(result) + "\n";
    }
    out << "loader API=" << Version(loaderVersion) << '\n';
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "Lost Odyssey Android capability probe";
    app.apiVersion = std::min(loaderVersion, VK_API_VERSION_1_2);
    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &app;
    instanceInfo.enabledExtensionCount = extensionCount;
    instanceInfo.ppEnabledExtensionNames = extensions.data();
    InstanceGuard state;
    VkResult result = vkCreateInstance(&instanceInfo, nullptr, &state.instance);
    if (result != VK_SUCCESS) return out.str() + "Vulkan instance failed: " + ResultText(result) + "\n";
    const bool haveSurface = SDL_Vulkan_CreateSurface(window, state.instance, nullptr, &state.surface);
    if (!haveSurface) out << "SDL Vulkan surface failed: " << SDL_GetError() << '\n';
    uint32_t deviceCount = 0;
    result = vkEnumeratePhysicalDevices(state.instance, &deviceCount, nullptr);
    if (result != VK_SUCCESS) return out.str() + "physical device enumeration failed: " + ResultText(result) + "\n";
    if (!deviceCount) return out.str() + "no Vulkan physical devices\n";
    std::vector<VkPhysicalDevice> devices(deviceCount);
    result = vkEnumeratePhysicalDevices(state.instance, &deviceCount, devices.data());
    if (result != VK_SUCCESS) return out.str() + "physical device list failed: " + ResultText(result) + "\n";
    auto getFeatures2 = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
        vkGetInstanceProcAddr(state.instance, "vkGetPhysicalDeviceFeatures2"));
    auto getProperties2 = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
        vkGetInstanceProcAddr(state.instance, "vkGetPhysicalDeviceProperties2"));

    for (uint32_t i = 0; i < deviceCount; ++i) {
        const VkPhysicalDevice physical = devices[i];
        VkPhysicalDeviceProperties props{};
        VkPhysicalDeviceFeatures features{};
        VkPhysicalDeviceMemoryProperties memory{};
        vkGetPhysicalDeviceProperties(physical, &props);
        vkGetPhysicalDeviceFeatures(physical, &features);
        vkGetPhysicalDeviceMemoryProperties(physical, &memory);
        bool bda = false, scalar = false, featureQueryAvailable = false;
        std::string driverName;
        if (app.apiVersion >= VK_API_VERSION_1_2 && props.apiVersion >= VK_API_VERSION_1_2 &&
            getFeatures2 && getProperties2) {
            VkPhysicalDeviceBufferDeviceAddressFeatures address{};
            address.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
            VkPhysicalDeviceScalarBlockLayoutFeatures layout{};
            layout.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES;
            VkPhysicalDeviceFeatures2 featureQuery{};
            featureQuery.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            featureQuery.pNext = &address;
            address.pNext = &layout;
            getFeatures2(physical, &featureQuery);
            bda = address.bufferDeviceAddress != VK_FALSE;
            scalar = layout.scalarBlockLayout != VK_FALSE;
            featureQueryAvailable = true;
            VkPhysicalDeviceDriverProperties driver{};
            driver.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
            VkPhysicalDeviceProperties2 properties2{};
            properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            properties2.pNext = &driver;
            getProperties2(physical, &properties2);
            driverName = driver.driverName;
        }
        const auto& limits = props.limits;
        const uint32_t samplers = std::min(limits.maxPerStageDescriptorSamplers, limits.maxDescriptorSetSamplers);
        const uint32_t sampled = std::min(limits.maxPerStageDescriptorSampledImages, limits.maxDescriptorSetSampledImages);
        const uint32_t storage = std::min(limits.maxPerStageDescriptorStorageBuffers, limits.maxDescriptorSetStorageBuffers);
        out << "device " << i << ": " << props.deviceName << " (vendor=" << Hex(props.vendorID)
            << " device=" << Hex(props.deviceID) << ")\n"
            << "  API=" << Version(props.apiVersion) << " driverVersion=" << Hex(props.driverVersion);
        if (!driverName.empty()) out << " driver=" << driverName;
        out << "\n  shaderInt64=" << YesNo(features.shaderInt64) << " bufferDeviceAddress="
            << (featureQueryAvailable ? YesNo(bda) : "unqueried")
            << " scalarBlockLayout=" << (featureQueryAvailable ? YesNo(scalar) : "unqueried") << '\n'
            << "  limits: sets=" << limits.maxBoundDescriptorSets << " samplers=" << samplers
            << " sampledImages=" << sampled << " storageBuffers=" << storage
            << " pushConstants=" << limits.maxPushConstantsSize
            << " maxStorageBufferRange=" << limits.maxStorageBufferRange << " bytes"
            << " vertexArena=" << gpu::render_arena::kVertexArenaSize << " bytes\n";
        bool formatsOk = true;
        for (const auto& format : std::array<std::pair<const char*, VkFormat>, 6>{{
                 {"BC1 UNORM", VK_FORMAT_BC1_RGBA_UNORM_BLOCK},
                 {"BC1 SRGB", VK_FORMAT_BC1_RGBA_SRGB_BLOCK},
                 {"BC2 UNORM", VK_FORMAT_BC2_UNORM_BLOCK},
                 {"BC2 SRGB", VK_FORMAT_BC2_SRGB_BLOCK},
                 {"BC3 UNORM", VK_FORMAT_BC3_UNORM_BLOCK},
                 {"BC3 SRGB", VK_FORMAT_BC3_SRGB_BLOCK}}}) {
            out << FormatFeatures(physical, format.first, format.second, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
            VkFormatProperties properties{};
            vkGetPhysicalDeviceFormatProperties(physical, format.second, &properties);
            formatsOk &= (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0;
        }
        out << FormatFeatures(physical, "D32S8", VK_FORMAT_D32_SFLOAT_S8_UINT,
                              VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
            << FormatFeatures(physical, "RGBA16F", VK_FORMAT_R16G16B16A16_SFLOAT,
                              VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
        VkFormatProperties depth{}, color{};
        vkGetPhysicalDeviceFormatProperties(physical, VK_FORMAT_D32_SFLOAT_S8_UINT, &depth);
        vkGetPhysicalDeviceFormatProperties(physical, VK_FORMAT_R16G16B16A16_SFLOAT, &color);
        formatsOk &= (depth.optimalTilingFeatures & (VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) ==
                     (VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
        formatsOk &= (color.optimalTilingFeatures & (VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) ==
                     (VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
        out << "  memory heaps=" << memory.memoryHeapCount << " types=" << memory.memoryTypeCount << '\n';
        for (uint32_t h = 0; h < memory.memoryHeapCount; ++h)
            out << "    heap " << h << ": " << (memory.memoryHeaps[h].size >> 20) << " MiB flags="
                << Hex(memory.memoryHeaps[h].flags) << '\n';
        for (uint32_t t = 0; t < memory.memoryTypeCount; ++t)
            out << "    type " << t << ": heap=" << memory.memoryTypes[t].heapIndex
                << " flags=" << Hex(memory.memoryTypes[t].propertyFlags) << '\n';

        gpu::backend::Capabilities rendererCaps{};
        rendererCaps.device = true; // Evaluate physical-device limits before the optional present device is created.
        rendererCaps.apiVersion = props.apiVersion;
        rendererCaps.bufferDeviceAddress = bda;
        rendererCaps.shaderInt64 = features.shaderInt64 != VK_FALSE;
        rendererCaps.scalarBlockLayout = scalar;
        rendererCaps.boundSets = limits.maxBoundDescriptorSets;
        rendererCaps.samplers = samplers;
        rendererCaps.sampledImages = sampled;
        rendererCaps.storageBuffers = storage;
        rendererCaps.pushConstants = limits.maxPushConstantsSize;
        const std::string layoutReason = gpu::backend::Missing(gpu::backend::Backend::Vulkan, rendererCaps);
        const bool layoutOk = featureQueryAvailable && layoutReason.empty();
        const bool arenaOk = limits.maxStorageBufferRange >= gpu::render_arena::kVertexArenaSize;
        out << "  renderer layout gate (backend_selection.h): " << (layoutOk ? "PASS" : "FAIL");
        if (!layoutOk) out << " (" << (featureQueryAvailable || props.apiVersion < VK_API_VERSION_1_2
                                          ? layoutReason : "Vulkan feature query unavailable") << ")";
        out << '\n'
            << "  vertex arena range gate (render_arena_policy.h): " << (arenaOk ? "PASS" : "FAIL") << '\n'
            << "  sampled/render format probe: " << (formatsOk ? "PASS" : "FAIL") << '\n';

        if (!haveSurface) { out << "  clear/present: unavailable (SDL surface)\n"; continue; }
        const ExtensionStatus swapchainExtension = HasExtension(physical, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        if (swapchainExtension.result != VK_SUCCESS) {
            out << "  clear/present: unavailable (device extension enumeration failed: "
                << ResultText(swapchainExtension.result) << ")\n"; continue;
        }
        if (!swapchainExtension.present) {
            out << "  clear/present: unavailable (VK_KHR_swapchain missing)\n"; continue;
        }
        uint32_t familyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical, &familyCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(familyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physical, &familyCount, families.data());
        uint32_t queueFamily = UINT32_MAX;
        for (uint32_t f = 0; f < familyCount; ++f) {
            if (!families[f].queueCount || !(families[f].queueFlags & VK_QUEUE_GRAPHICS_BIT)) continue;
            VkBool32 present = VK_FALSE;
            result = vkGetPhysicalDeviceSurfaceSupportKHR(physical, f, state.surface, &present);
            if (result == VK_SUCCESS && present) { queueFamily = f; break; }
        }
        if (queueFamily == UINT32_MAX) {
            out << "  clear/present: unavailable (no shared graphics/present queue)\n"; continue;
        }
        out << "  clear/present queue family=" << queueFamily << ": "
            << TryClearPresent(window, physical, state.surface, queueFamily) << '\n';
    }
    return out.str();
}

} // namespace lo::android_probe
