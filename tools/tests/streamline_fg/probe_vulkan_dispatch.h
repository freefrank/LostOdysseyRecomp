#pragma once
#include "streamline_runtime.h"
#include <gpu/dlss_ngx.h>
#include <plume_vulkan.h>
#include <array>
#include <string>
#include <vector>

namespace probe {
class VulkanDispatch {
public:
    VulkanDispatch(StreamlineRuntime& sl, gpu::dlss::Controller& ngx);
    ~VulkanDispatch();
    plume::VulkanExtensionHooks Hooks();
    bool InstallDeviceHooks(VkInstance instance, VkDevice device, std::string& reason);
    void RestoreDeviceHooks();
    void RestoreCreationHooks();
    bool Ready() const { return reason_.empty(); }
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
    static VkResult VKAPI_PTR CreateSurface(VkInstance, const VkWin32SurfaceCreateInfoKHR*, const VkAllocationCallbacks*, VkSurfaceKHR*);
    static void VKAPI_PTR DestroySurface(VkInstance, VkSurfaceKHR, const VkAllocationCallbacks*);
    StreamlineRuntime& sl_;
    plume::VulkanExtensionHooks native_;
    std::array<sl::FeatureRequirements, 3> requirements_{};
    std::string reason_;
    static VulkanDispatch* current_;
    PFN_vkCreateInstance nativeCreateInstance_{};
    PFN_vkCreateDevice nativeCreateDevice_{};
    PFN_vkEnumeratePhysicalDevices nativeEnumerate_{};
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
    bool creationInstalled_{};
    bool deviceInstalled_{};
};
}
