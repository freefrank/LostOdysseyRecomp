#include "../android_probe/descriptor_buffer_probe.h"
#include <cassert>
#include <cstdio>

namespace {
bool advertised = false, supported = false;
VkResult firstResult = VK_SUCCESS, secondResult = VK_SUCCESS;
int featureCalls = 0, propertyCalls = 0;
VKAPI_ATTR void VKAPI_CALL Features(VkPhysicalDevice, VkPhysicalDeviceFeatures2* query)
{
    ++featureCalls;
    assert(query->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
    auto* value = static_cast<VkPhysicalDeviceDescriptorBufferFeaturesEXT*>(query->pNext);
    assert(value->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT);
    value->descriptorBuffer = supported;
}
VKAPI_ATTR void VKAPI_CALL Properties(VkPhysicalDevice, VkPhysicalDeviceProperties2* query)
{
    ++propertyCalls;
    assert(query->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);
    auto* value = static_cast<VkPhysicalDeviceDescriptorBufferPropertiesEXT*>(query->pNext);
    assert(value->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT);
    value->descriptorBufferOffsetAlignment = 64;
    value->maxDescriptorBufferBindings = 8;
    value->samplerDescriptorSize = 16;
    value->sampledImageDescriptorSize = 32;
    value->uniformBufferDescriptorSize = 48;
    value->storageBufferDescriptorSize = 24;
}
}
VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceExtensionProperties(
    VkPhysicalDevice, const char*, uint32_t* count, VkExtensionProperties* extensions)
{
    if (!extensions) { *count = 1; return firstResult; }
    std::strcpy(extensions[0].extensionName, advertised ? VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME : "unrelated");
    return secondResult;
}
int main()
{
    using lo::android_probe::DescribeDescriptorBuffer;
    const auto report = [] { return DescribeDescriptorBuffer(VK_NULL_HANDLE, &Features, &Properties); };
    assert(report().find("not advertised") != std::string::npos);
    assert(featureCalls == 0 && propertyCalls == 0);
    advertised = true;
    assert(DescribeDescriptorBuffer(VK_NULL_HANDLE, nullptr, &Properties).find("query unavailable") != std::string::npos);
    assert(DescribeDescriptorBuffer(VK_NULL_HANDLE, &Features, nullptr).find("query unavailable") != std::string::npos);
    assert(featureCalls == 0 && propertyCalls == 0);
    assert(report().find("descriptorBuffer=false") != std::string::npos);
    assert(featureCalls == 1 && propertyCalls == 0);
    supported = true;
    const auto output = report();
    for (const char* expected : {"descriptorBuffer=true", "offsetAlignment=64", "maxBindings=8", "samplerBytes=16",
                                 "sampledImageBytes=32", "uniformBufferBytes=48", "storageBufferBytes=24"})
        assert(output.find(expected) != std::string::npos);
    assert(featureCalls == 2 && propertyCalls == 1);
    firstResult = VK_ERROR_INITIALIZATION_FAILED;
    assert(report().find("extension query failed") != std::string::npos);
    firstResult = VK_SUCCESS;
    secondResult = VK_INCOMPLETE;
    assert(report().find("extension list failed") != std::string::npos);
    assert(featureCalls == 2 && propertyCalls == 1);
    std::puts("PASS descriptor-buffer advertisement, feature/property guards and query errors");
}
