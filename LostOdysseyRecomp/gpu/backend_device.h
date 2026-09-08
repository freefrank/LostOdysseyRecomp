#pragma once
#include "backend_selection.h"
#include <algorithm>
#include <plume_d3d12.h>
#include <plume_vulkan.h>

namespace gpu::backend {
// Uses the created device's actual enabled Plume features, not adapter names.
inline Capabilities Inspect(Backend backend, plume::RenderDevice* device) {
    Capabilities c;
    if (!device) return c;
    c.device = true;
    c.geometryShader = device->getCapabilities().geometryShader;
    if (backend == Backend::D3D12) {
        auto* native = static_cast<plume::D3D12Device*>(device);
        c.shaderModel = native->shaderModel;
        D3D12_FEATURE_DATA_D3D12_OPTIONS options{};
        if (SUCCEEDED(native->d3d->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options))))
            c.bindingTier = options.ResourceBindingTier;
    } else if (backend == Backend::Vulkan) {
        const auto* native = static_cast<plume::VulkanDevice*>(device);
        const auto& limits = native->physicalDeviceProperties.limits;
        c.apiVersion = native->physicalDeviceProperties.apiVersion;
        c.bufferDeviceAddress = device->getCapabilities().bufferDeviceAddress;
        c.scalarBlockLayout = device->getCapabilities().scalarBlockLayout;
        VkPhysicalDeviceFeatures features{};
        vkGetPhysicalDeviceFeatures(native->physicalDevice, &features);
        // Plume enables the returned base features in vkCreateDevice.
        c.shaderInt64 = features.shaderInt64 != VK_FALSE;
        c.boundSets = limits.maxBoundDescriptorSets;
        c.samplers = std::min(limits.maxPerStageDescriptorSamplers, limits.maxDescriptorSetSamplers);
        c.sampledImages = std::min(limits.maxPerStageDescriptorSampledImages, limits.maxDescriptorSetSampledImages);
        c.storageBuffers = std::min(limits.maxPerStageDescriptorStorageBuffers, limits.maxDescriptorSetStorageBuffers);
        c.pushConstants = limits.maxPushConstantsSize;
    }
    return c;
}
}
