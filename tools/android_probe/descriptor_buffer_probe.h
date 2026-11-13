#pragma once

#include <vulkan/vulkan.h>
#include <cstring>
#include <sstream>
#include <vector>

namespace lo::android_probe {
// Physical-device diagnostics only. Never enable the extension or change the
// renderer's descriptor-set path on the strength of this report.
inline std::string DescribeDescriptorBuffer(VkPhysicalDevice device,
    PFN_vkGetPhysicalDeviceFeatures2 getFeatures, PFN_vkGetPhysicalDeviceProperties2 getProperties)
{
    std::ostringstream out;
    out << "  descriptor buffer: ";
    uint32_t count = 0;
    VkResult result = vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    if (result != VK_SUCCESS)
        return out.str() + "extension query failed (" + std::to_string(int(result)) + ")\n";
    std::vector<VkExtensionProperties> extensions(count);
    result = vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data());
    if (result != VK_SUCCESS)
        return out.str() + "extension list failed (" + std::to_string(int(result)) + ")\n";
    bool advertised = false;
    for (const auto& extension : extensions)
        advertised |= std::strcmp(extension.extensionName, VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME) == 0;
    if (!advertised) return out.str() + "not advertised\n";
    if (!getFeatures || !getProperties) return out.str() + "advertised, feature query unavailable\n";

    VkPhysicalDeviceDescriptorBufferFeaturesEXT features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
    VkPhysicalDeviceFeatures2 query{};
    query.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    query.pNext = &features;
    getFeatures(device, &query);
    if (!features.descriptorBuffer) return out.str() + "advertised, descriptorBuffer=false\n";

    VkPhysicalDeviceDescriptorBufferPropertiesEXT properties{};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT;
    VkPhysicalDeviceProperties2 propertyQuery{};
    propertyQuery.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    propertyQuery.pNext = &properties;
    getProperties(device, &propertyQuery);
    out << "descriptorBuffer=true offsetAlignment=" << properties.descriptorBufferOffsetAlignment
        << " maxBindings=" << properties.maxDescriptorBufferBindings
        << " samplerBytes=" << properties.samplerDescriptorSize
        << " sampledImageBytes=" << properties.sampledImageDescriptorSize
        << " uniformBufferBytes=" << properties.uniformBufferDescriptorSize
        << " storageBufferBytes=" << properties.storageBufferDescriptorSize << '\n';
    return out.str();
}
} // namespace lo::android_probe
