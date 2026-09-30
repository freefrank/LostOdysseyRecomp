#pragma once
#if defined(_WIN32) && defined(LO_ENABLE_STREAMLINE_FG)
#include "streamline_runtime.h"
#include <plume_vulkan.h>
#include <array>
#include <string>
#include <vector>

namespace gpu::dlss { class Controller; }

namespace gpu::dlss_fg {
class VulkanDispatch {
public:
    VulkanDispatch(Runtime& sl, gpu::dlss::Controller& ngx);
    ~VulkanDispatch();
    plume::VulkanExtensionHooks Hooks();
    bool InstallDeviceHooks(VkInstance instance, VkDevice device, std::string& reason);
    void RestoreDeviceHooks();
    void RestoreCreationHooks();
    bool Ready() const { return reason_.empty(); }
    bool FeatureSupported() const { return featureSupported_; }
    const std::string& Reason() const { return reason_; }
    void Report() const;
private:
    static bool InstanceRequirements(void*, std::vector<VkExtensionProperties>&, std::string&);
    static bool DeviceRequirements(void*, VkInstance, VkPhysicalDevice, std::vector<VkExtensionProperties>&, std::string&);
    bool QueryInstance(std::vector<VkExtensionProperties>&, std::string&);
    bool QueryDevice(VkInstance, VkPhysicalDevice, std::vector<VkExtensionProperties>&, std::string&);
    bool Preflight(VkPhysicalDevice, const VkDeviceCreateInfo*);
    static VkResult VKAPI_PTR CreateDevice(VkPhysicalDevice, const VkDeviceCreateInfo*, const VkAllocationCallbacks*, VkDevice*);
    static VkResult VKAPI_PTR Present(VkQueue, const VkPresentInfoKHR*);
    static VkResult VKAPI_PTR CreateSwapchain(VkDevice, const VkSwapchainCreateInfoKHR*, const VkAllocationCallbacks*, VkSwapchainKHR*);
    static void VKAPI_PTR DestroySwapchain(VkDevice, VkSwapchainKHR, const VkAllocationCallbacks*);
    static VkResult VKAPI_PTR GetSwapchainImages(VkDevice, VkSwapchainKHR, uint32_t*, VkImage*);
    static VkResult VKAPI_PTR Acquire(VkDevice, VkSwapchainKHR, uint64_t, VkSemaphore, VkFence, uint32_t*);
    static VkResult VKAPI_PTR DeviceIdle(VkDevice);
    static VkResult VKAPI_PTR CreateImageView(VkDevice, const VkImageViewCreateInfo*, const VkAllocationCallbacks*, VkImageView*);
    static void VKAPI_PTR DestroyImageView(VkDevice, VkImageView, const VkAllocationCallbacks*);
    static VkResult VKAPI_PTR CreateFramebuffer(VkDevice, const VkFramebufferCreateInfo*, const VkAllocationCallbacks*, VkFramebuffer*);
    static VkResult VKAPI_PTR CreateSurface(VkInstance, const VkWin32SurfaceCreateInfoKHR*, const VkAllocationCallbacks*, VkSurfaceKHR*);
    static void VKAPI_PTR DestroySurface(VkInstance, VkSurfaceKHR, const VkAllocationCallbacks*);
    Runtime& sl_;
    plume::VulkanExtensionHooks native_;
    std::array<sl::FeatureRequirements, 3> requirements_{};
    std::string reason_;
    static VulkanDispatch* current_;
    PFN_vkCreateInstance nativeCreateInstance_{};
    PFN_vkCreateDevice nativeCreateDevice_{};
    PFN_vkCreateDevice proxyCreateDevice_{};
    VkDevice hookedDevice_{};
    PFN_vkQueuePresentKHR present_{};
    PFN_vkCreateSwapchainKHR createSwapchain_{};
    PFN_vkDestroySwapchainKHR destroySwapchain_{};
    PFN_vkGetSwapchainImagesKHR getSwapchainImages_{};
    PFN_vkAcquireNextImageKHR acquire_{};
    PFN_vkDeviceWaitIdle deviceIdle_{};
    PFN_vkCreateWin32SurfaceKHR createSurface_{};
    PFN_vkDestroySurfaceKHR destroySurface_{};
    std::array<uint64_t, 8> counts_{};
    PFN_vkCreateImageView createImageView_{};
    PFN_vkDestroyImageView destroyImageView_{};
    PFN_vkCreateFramebuffer createFramebuffer_{};
    uint32_t tracePresentBudget_ = 0;
    bool objectTraceInstalled_ = false;
    bool featureSupported_{};
    bool creationInstalled_{};
    bool deviceInstalled_{};
};
}

#endif // _WIN32 && LO_ENABLE_STREAMLINE_FG
