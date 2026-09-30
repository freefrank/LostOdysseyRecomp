#if defined(_WIN32) && defined(LO_ENABLE_STREAMLINE_FG)
#include "streamline_vulkan_dispatch.h"
#include "dlss_ngx.h"
#include "vulkan_object_trace.h"
#include <sl_helpers_vk.h>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstddef>
#include <set>

namespace gpu::dlss_fg {
VulkanDispatch* VulkanDispatch::current_{};
namespace {
VkExtensionProperties Extension(const char* name) {
    VkExtensionProperties p{};
    strncpy_s(p.extensionName, name, _TRUNCATE);
    return p;
}
void Append(std::vector<VkExtensionProperties>& extensions, const char* name) {
    if (name && std::none_of(extensions.begin(), extensions.end(), [name](const auto& p) { return strcmp(p.extensionName, name) == 0; }))
        extensions.push_back(Extension(name));
}
}
VulkanDispatch::VulkanDispatch(Runtime& sl, gpu::dlss::Controller& ngx) : sl_(sl), native_(ngx.ExtensionHooks()) {
    assert(current_ == nullptr);
    current_ = this;
}
VulkanDispatch::~VulkanDispatch() { RestoreDeviceHooks(); RestoreCreationHooks(); current_ = nullptr; }
plume::VulkanExtensionHooks VulkanDispatch::Hooks() { return {this, InstanceRequirements, DeviceRequirements}; }
bool VulkanDispatch::InstanceRequirements(void* data, std::vector<VkExtensionProperties>& ext, std::string& reason) {
    auto& self = *static_cast<VulkanDispatch*>(data);
    if (self.QueryInstance(ext, reason)) return true;
    self.reason_ = reason;
    return false;
}
bool VulkanDispatch::DeviceRequirements(void* data, VkInstance instance, VkPhysicalDevice device,
    std::vector<VkExtensionProperties>& ext, std::string& reason) {
    auto& self = *static_cast<VulkanDispatch*>(data);
    if (self.QueryDevice(instance, device, ext, reason)) return true;
    self.reason_ = reason;
    return false;
}
bool VulkanDispatch::QueryInstance(std::vector<VkExtensionProperties>& ext, std::string& reason) {
    if (native_.queryInstance && !native_.queryInstance(native_.userData, ext, reason)) return false;
    for (size_t i = 0; i < 3; ++i) {
        if (!sl_.Requirements(Runtime::kFeatures[i], requirements_[i], reason)) return false;
        if (!(uint32_t(requirements_[i].flags) & uint32_t(sl::FeatureRequirementFlags::eVulkanSupported))) {
            reason = "feature does not support Vulkan: " + std::to_string(Runtime::kFeatures[i]); return false;
        }
        for (uint32_t j = 0; j < requirements_[i].vkNumInstanceExtensions; ++j)
            Append(ext, requirements_[i].vkInstanceExtensions[j]);
    }
    nativeCreateInstance_ = vkCreateInstance;
    auto proxy = reinterpret_cast<PFN_vkCreateInstance>(sl_.InstanceProc()(nullptr, "vkCreateInstance"));
    if (!proxy) { reason = "Streamline vkCreateInstance proxy missing"; return false; }
    vkCreateInstance = proxy;
    creationInstalled_ = true;
    std::printf("INSTANCE_EXTENSIONS=%zu CREATE_INSTANCE_PROXY=1\n", ext.size());
    return true;
}
bool VulkanDispatch::QueryDevice(VkInstance instance, VkPhysicalDevice physical,
    std::vector<VkExtensionProperties>& ext, std::string& reason) {
    if (native_.queryDevice && !native_.queryDevice(native_.userData, instance, physical, ext, reason)) return false;
    // Plume's volkLoadInstance replaces the global enumeration pointer. Explicitly
    // enumerate through the interposer before device creation to populate its map.
    auto enumeration = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(sl_.InstanceProc()(instance, "vkEnumeratePhysicalDevices"));
    if (!enumeration) { reason = "Streamline physical-device enumeration proxy missing"; return false; }
    uint32_t count{};
    if (enumeration(instance, &count, nullptr) != VK_SUCCESS || !count) { reason = "SL physical enumeration failed"; return false; }
    std::vector<VkPhysicalDevice> devices(count);
    if (enumeration(instance, &count, devices.data()) != VK_SUCCESS ||
        std::find(devices.begin(), devices.begin() + count, physical) == devices.begin() + count) {
        reason = "SL enumeration does not include Plume-selected physical device"; return false;
    }
    featureSupported_ = true;
    sl::AdapterInfo adapter{};
    adapter.vkPhysicalDevice = physical;
    for (const auto feature : Runtime::kFeatures) {
        const auto result = sl_.IsFeatureSupported(feature, adapter);
        std::printf("FG_FEATURE_PROBE feature=%u result=%d\n", unsigned(feature), int(result));
        featureSupported_ &= result == sl::Result::eOk;
    }
    if (!featureSupported_) {
        std::puts("FG_UNAVAILABLE=adapter_feature_probe SR_DEVICE=native FG_DEVICE_HOOKS=off");
        return true;
    }
    for (auto& req : requirements_)
        for (uint32_t j = 0; j < req.vkNumDeviceExtensions; ++j) Append(ext, req.vkDeviceExtensions[j]);
    proxyCreateDevice_ = reinterpret_cast<PFN_vkCreateDevice>(sl_.InstanceProc()(instance, "vkCreateDevice"));
    if (!proxyCreateDevice_) { reason = "Streamline vkCreateDevice proxy missing"; return false; }
    nativeCreateDevice_ = vkCreateDevice;
    vkCreateDevice = CreateDevice;
    creationInstalled_ = true;
    std::printf("DEVICE_EXTENSIONS=%zu SL_PHYSICAL_ENUMERATION=%u CREATE_DEVICE_PROXY=1\n", ext.size(), count);
    return true;
}
bool VulkanDispatch::Preflight(VkPhysicalDevice physical, const VkDeviceCreateInfo* info) {
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(physical, &props);
    std::printf("PREFLIGHT_GPU=%s vendor=%u driver=%u api=%u\n", props.deviceName, props.vendorID, props.driverVersion, props.apiVersion);
    uint32_t familyCount{};
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &familyCount, nullptr);
    if (!familyCount) { reason_ = "no Vulkan queue families"; return false; }
    std::vector<VkQueueFamilyProperties> families(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &familyCount, families.data());
    for (uint32_t i = 0; i < familyCount; ++i)
        std::printf("PHYSICAL_QUEUE family=%u flags=0x%x count=%u\n", i, families[i].queueFlags, families[i].queueCount);
    uint32_t graphics = 0, compute = 0, optical = UINT32_MAX;
    // Match the pinned Streamline wrapper's queue-family selection.
    for (uint32_t i = 0; i < familyCount; ++i) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) graphics = i;
        else if (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) compute = i;
        // Dedicated means no client queue uses this family. Optical-flow
        // families can also expose TRANSFER.
        if ((families[i].queueFlags & VK_QUEUE_OPTICAL_FLOW_BIT_NV) &&
            !(families[i].queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) && optical == UINT32_MAX)
            optical = i;
    }
    uint32_t extrasGraphics{}, extrasCompute{}, extrasOptical{};
    for (const auto& req : requirements_) {
        extrasGraphics += req.vkNumGraphicsQueuesRequired;
        extrasCompute += req.vkNumComputeQueuesRequired;
        extrasOptical += req.vkNumOpticalFlowQueuesRequired;
    }
    if (extrasOptical && optical == UINT32_MAX) {
        reason_ = "no dedicated optical-flow queue (index 0) for requested native optical flow"; return false;
    }
    std::vector<uint32_t> requested(familyCount, 0);
    for (uint32_t i = 0; i < info->queueCreateInfoCount; ++i) {
        const auto& q = info->pQueueCreateInfos[i];
        if (q.queueFamilyIndex >= familyCount || requested[q.queueFamilyIndex]) {
            reason_ = "invalid or duplicate host queue family"; return false;
        }
        requested[q.queueFamilyIndex] = q.queueCount;
        std::printf("HOST_QUEUE family=%u count=%u capacity=%u flags=0x%x\n", q.queueFamilyIndex, q.queueCount,
            families[q.queueFamilyIndex].queueCount, families[q.queueFamilyIndex].queueFlags);
    }
    if (extrasOptical && requested[optical]) { reason_ = "host uses native optical-flow family reserved for SL index 0"; return false; }
    requested[graphics] += extrasGraphics;
    requested[compute] += extrasCompute;  // same-family counts must be cumulative
    if (optical != UINT32_MAX) requested[optical] += extrasOptical;
    for (uint32_t i = 0; i < familyCount; ++i) {
        std::printf("PREFLIGHT_QUEUE family=%u requested=%u capacity=%u graphicsFamily=%u computeFamily=%u opticalFamily=%u\n",
            i, requested[i], families[i].queueCount, graphics, compute, optical);
        if (requested[i] > families[i].queueCount) {
            reason_ = "SL extra queues exceed physical family capacity at family " + std::to_string(i);
            return false;
        }
    }
    // SL proxy merges feature pNext chains. Preflight all named 1.2/1.3
    // requirements explicitly; unsupported bits must not silently disappear.
    VkPhysicalDeviceVulkan12Features supported12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceVulkan13Features supported13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    supported12.pNext = &supported13;
    VkPhysicalDeviceFeatures2 supported{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    supported.pNext = &supported12;
    vkGetPhysicalDeviceFeatures2(physical, &supported);
    for (const auto& req : requirements_) {
        const auto check = [this](uint32_t count, const char** names, const auto& supportedBits,
                                  auto helper, size_t firstField) {
            for (uint32_t j = 0; j < count; ++j) {
                auto requested = helper(1, &names[j]);
                auto* bits = reinterpret_cast<const VkBool32*>(reinterpret_cast<const uint8_t*>(&requested) + firstField);
                auto* actual = reinterpret_cast<const VkBool32*>(reinterpret_cast<const uint8_t*>(&supportedBits) + firstField);
                const auto n = (sizeof(requested) - firstField) / sizeof(VkBool32);
                uint32_t found = 0;
                for (size_t k = 0; k < n; ++k) if (bits[k]) { ++found; if (!actual[k]) {
                    reason_ = std::string("unsupported Vulkan feature ") + names[j]; return false;
                } }
                if (found != 1) { reason_ = std::string("unknown SDK Vulkan feature name ") + names[j]; return false; }
                std::printf("SUPPORTED_FEATURE=%s\n", names[j]);
            }
            return true;
        };
        if (!check(req.vkNumFeatures12, req.vkFeatures12, supported12,
                sl::getVkPhysicalDeviceVulkan12Features, offsetof(VkPhysicalDeviceVulkan12Features, samplerMirrorClampToEdge)) ||
            !check(req.vkNumFeatures13, req.vkFeatures13, supported13,
                sl::getVkPhysicalDeviceVulkan13Features, offsetof(VkPhysicalDeviceVulkan13Features, robustImageAccess)))
            return false;
    }
    if (!supported13.privateData) {
        reason_ = "Vulkan 1.3 privateData unsupported by selected physical device";
        return false;
    }
    std::puts("SUPPORTED_FEATURE=privateData");
    return true;
}
VkResult VKAPI_PTR VulkanDispatch::CreateDevice(VkPhysicalDevice physical, const VkDeviceCreateInfo* info,
    const VkAllocationCallbacks* allocator, VkDevice* device) {
    auto& self = *current_;
    if (!self.Preflight(physical, info)) { std::printf("PREFLIGHT_FAILED=%s\n", self.reason_.c_str()); return VK_ERROR_INITIALIZATION_FAILED; }
    VkPhysicalDeviceVulkan13Features enabled13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    enabled13.privateData = VK_TRUE;
    enabled13.pNext = const_cast<void*>(info->pNext);
    VkDeviceCreateInfo enabled = *info;
    enabled.pNext = &enabled13;
    std::puts("ENABLED_FEATURE=privateData");
    return self.proxyCreateDevice_(physical, &enabled, allocator, device);
}
bool VulkanDispatch::InstallDeviceHooks(VkInstance instance, VkDevice device, std::string& reason) {
    if (!reason_.empty()) { reason = reason_; return false; }
    if (deviceInstalled_) { reason = "device hooks already installed"; return false; }
    present_ = vkQueuePresentKHR; createSwapchain_ = vkCreateSwapchainKHR; destroySwapchain_ = vkDestroySwapchainKHR;
    hookedDevice_ = device;
    getSwapchainImages_ = vkGetSwapchainImagesKHR; acquire_ = vkAcquireNextImageKHR; deviceIdle_ = vkDeviceWaitIdle;
    createSurface_ = vkCreateWin32SurfaceKHR; destroySurface_ = vkDestroySurfaceKHR;
    const auto deviceProxy = [this, device](const char* name) { return sl_.DeviceProc()(device, name); };
    const auto instanceProxy = [this, instance](const char* name) { return sl_.InstanceProc()(instance, name); };
    if (!deviceProxy("vkQueuePresentKHR") || !deviceProxy("vkCreateSwapchainKHR") ||
        !deviceProxy("vkDestroySwapchainKHR") || !deviceProxy("vkGetSwapchainImagesKHR") ||
        !deviceProxy("vkAcquireNextImageKHR") || !deviceProxy("vkDeviceWaitIdle") ||
        !instanceProxy("vkCreateWin32SurfaceKHR") || !instanceProxy("vkDestroySurfaceKHR")) {
        reason = "one or more mandatory Streamline WSI proxy functions missing"; return false;
    }
    vkQueuePresentKHR = Present; vkCreateSwapchainKHR = CreateSwapchain; vkDestroySwapchainKHR = DestroySwapchain;
    vkGetSwapchainImagesKHR = GetSwapchainImages; vkAcquireNextImageKHR = Acquire; vkDeviceWaitIdle = DeviceIdle;
    vkCreateWin32SurfaceKHR = CreateSurface; vkDestroySurfaceKHR = DestroySurface;
    // Only Plume/host calls through these Volk pointers are attributed here.
    // Streamline owns a separate dispatch table; FSR's SDK resolver is traced
    // separately. None of these wrappers alters a returned handle or wait.
    if (vk_object_trace::Enabled() && vkCreateImageView && vkDestroyImageView && vkCreateFramebuffer) {
        createImageView_ = vkCreateImageView; destroyImageView_ = vkDestroyImageView;
        createFramebuffer_ = vkCreateFramebuffer;
        vkCreateImageView = CreateImageView; vkDestroyImageView = DestroyImageView;
        vkCreateFramebuffer = CreateFramebuffer;
        objectTraceInstalled_ = true;
    }
    deviceInstalled_ = true;
    return true;
}
void VulkanDispatch::RestoreDeviceHooks() {
    if (!deviceInstalled_) return;
    if (objectTraceInstalled_) {
        vkCreateImageView = createImageView_; vkDestroyImageView = destroyImageView_;
        vkCreateFramebuffer = createFramebuffer_;
        objectTraceInstalled_ = false;
    }
    vkQueuePresentKHR = present_; vkCreateSwapchainKHR = createSwapchain_; vkDestroySwapchainKHR = destroySwapchain_;
    vkGetSwapchainImagesKHR = getSwapchainImages_; vkAcquireNextImageKHR = acquire_; vkDeviceWaitIdle = deviceIdle_;
    vkCreateWin32SurfaceKHR = createSurface_; vkDestroySurfaceKHR = destroySurface_;
    deviceInstalled_ = false;
}
void VulkanDispatch::RestoreCreationHooks() {
    if (!creationInstalled_) return;
    if (nativeCreateInstance_) vkCreateInstance = nativeCreateInstance_;
    if (nativeCreateDevice_) vkCreateDevice = nativeCreateDevice_;
    creationInstalled_ = false;
}
void VulkanDispatch::Report() const {
    const char* names[] = {"present", "createSwapchain", "destroySwapchain", "getSwapchainImages", "acquire", "deviceIdle", "createSurface", "destroySurface"};
    for (size_t i = 0; i < counts_.size(); ++i) std::printf("HOOK_%s=%llu\n", names[i], static_cast<unsigned long long>(counts_[i]));
}
// Present uses a queue, and surfaces use an instance, so those are written explicitly.
VkResult VKAPI_PTR VulkanDispatch::Present(VkQueue queue, const VkPresentInfoKHR* info) {
    ++current_->counts_[0];
    if (vk_object_trace::Enabled() && current_->tracePresentBudget_ && info) {
        --current_->tracePresentBudget_;
        if (vk_object_trace::Permit()) {
            std::fprintf(stderr, "VK_OBJECT_TRACE route=host_wsi event=present queue=0x%llx swapchains=%u waits=%u\n",
                vk_object_trace::Id(queue), info->swapchainCount, info->waitSemaphoreCount);
            for (uint32_t i = 0; i < info->swapchainCount; ++i)
                std::fprintf(stderr, "VK_OBJECT_TRACE present_chain=0x%llx image_index=%u\n",
                    vk_object_trace::Id(info->pSwapchains[i]), info->pImageIndices[i]);
            for (uint32_t i = 0; i < info->waitSemaphoreCount; ++i)
                std::fprintf(stderr, "VK_OBJECT_TRACE present_wait=0x%llx\n", vk_object_trace::Id(info->pWaitSemaphores[i]));
        }
    }
    auto p = reinterpret_cast<PFN_vkQueuePresentKHR>(current_->sl_.DeviceProc()(current_->hookedDevice_, "vkQueuePresentKHR"));
    return p ? p(queue, info) : VK_ERROR_INITIALIZATION_FAILED;
}
VkResult VKAPI_PTR VulkanDispatch::CreateSwapchain(VkDevice device, const VkSwapchainCreateInfoKHR* info, const VkAllocationCallbacks* a, VkSwapchainKHR* out) {
    ++current_->counts_[1]; auto p = reinterpret_cast<PFN_vkCreateSwapchainKHR>(current_->sl_.DeviceProc()(device, "vkCreateSwapchainKHR"));
    const auto result = p(device, info, a, out);
    if (result == VK_SUCCESS && vk_object_trace::Enabled()) current_->tracePresentBudget_ = 4;
    return result;
}
void VKAPI_PTR VulkanDispatch::DestroySwapchain(VkDevice device, VkSwapchainKHR chain, const VkAllocationCallbacks* a) {
    ++current_->counts_[2]; auto p = reinterpret_cast<PFN_vkDestroySwapchainKHR>(current_->sl_.DeviceProc()(device, "vkDestroySwapchainKHR")); p(device, chain, a);
}
VkResult VKAPI_PTR VulkanDispatch::GetSwapchainImages(VkDevice device, VkSwapchainKHR chain, uint32_t* n, VkImage* images) {
    ++current_->counts_[3]; auto p = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(current_->sl_.DeviceProc()(device, "vkGetSwapchainImagesKHR"));
    const uint32_t capacity = n ? *n : 0;
    const auto result = p(device, chain, n, images);
    if (images && n && (result == VK_SUCCESS || result == VK_INCOMPLETE) && vk_object_trace::Permit()) {
        // Log only the actual host-facing return. Do not bypass the interposer
        // with a second native query, assume fake==real, or infer GPU completion.
        const auto count = std::min(capacity, *n);
        std::fprintf(stderr, "VK_OBJECT_TRACE route=host_wsi event=images chain=0x%llx count=%u result=%d\n",
            vk_object_trace::Id(chain), count, int(result));
        for (uint32_t i = 0; i < count; ++i)
            std::fprintf(stderr, "VK_OBJECT_TRACE host_image chain=0x%llx index=%u image=0x%llx\n",
                vk_object_trace::Id(chain), i, vk_object_trace::Id(images[i]));
    }
    return result;
}
VkResult VKAPI_PTR VulkanDispatch::Acquire(VkDevice device, VkSwapchainKHR chain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t* index) {
    ++current_->counts_[4]; auto p = reinterpret_cast<PFN_vkAcquireNextImageKHR>(current_->sl_.DeviceProc()(device, "vkAcquireNextImageKHR")); return p(device, chain, timeout, semaphore, fence, index);
}
VkResult VKAPI_PTR VulkanDispatch::CreateImageView(VkDevice device, const VkImageViewCreateInfo* info,
    const VkAllocationCallbacks* allocator, VkImageView* out) {
    const auto result = current_->createImageView_(device, info, allocator, out);
    if (result == VK_SUCCESS && vk_object_trace::Permit())
        std::fprintf(stderr, "VK_OBJECT_TRACE route=host_plume event=create_view device=0x%llx image=0x%llx view=0x%llx format=%u aspect=%u mip=%u\n",
            vk_object_trace::Id(device), vk_object_trace::Id(info->image), vk_object_trace::Id(*out),
            unsigned(info->format), unsigned(info->subresourceRange.aspectMask), info->subresourceRange.baseMipLevel);
    return result;
}
void VKAPI_PTR VulkanDispatch::DestroyImageView(VkDevice device, VkImageView view, const VkAllocationCallbacks* allocator) {
    if (vk_object_trace::Permit())
        std::fprintf(stderr, "VK_OBJECT_TRACE route=host_plume event=destroy_view device=0x%llx view=0x%llx\n",
            vk_object_trace::Id(device), vk_object_trace::Id(view));
    current_->destroyImageView_(device, view, allocator);
}
VkResult VKAPI_PTR VulkanDispatch::CreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo* info,
    const VkAllocationCallbacks* allocator, VkFramebuffer* out) {
    if (info && vk_object_trace::Permit()) {
        std::fprintf(stderr, "VK_OBJECT_TRACE route=host_plume event=create_framebuffer device=0x%llx pass=0x%llx width=%u height=%u attachments=%u\n",
            vk_object_trace::Id(device), vk_object_trace::Id(info->renderPass), info->width, info->height, info->attachmentCount);
        if (!(info->flags & VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT))
            for (uint32_t i = 0; i < info->attachmentCount; ++i)
                std::fprintf(stderr, "VK_OBJECT_TRACE framebuffer_attachment=%u view=0x%llx\n", i, vk_object_trace::Id(info->pAttachments[i]));
    }
    return current_->createFramebuffer_(device, info, allocator, out);
}
VkResult VKAPI_PTR VulkanDispatch::DeviceIdle(VkDevice device) {
    ++current_->counts_[5]; auto p = reinterpret_cast<PFN_vkDeviceWaitIdle>(current_->sl_.DeviceProc()(device, "vkDeviceWaitIdle")); return p(device);
}
VkResult VKAPI_PTR VulkanDispatch::CreateSurface(VkInstance instance, const VkWin32SurfaceCreateInfoKHR* info, const VkAllocationCallbacks* a, VkSurfaceKHR* surface) {
    ++current_->counts_[6]; auto p = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(current_->sl_.InstanceProc()(instance, "vkCreateWin32SurfaceKHR")); return p(instance, info, a, surface);
}
void VKAPI_PTR VulkanDispatch::DestroySurface(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks* a) {
    ++current_->counts_[7]; auto p = reinterpret_cast<PFN_vkDestroySurfaceKHR>(current_->sl_.InstanceProc()(instance, "vkDestroySurfaceKHR")); p(instance, surface, a);
}
}

#endif // _WIN32 && LO_ENABLE_STREAMLINE_FG
