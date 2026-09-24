include_guard(GLOBAL)

# Transform only the pinned SDK's generated build copy, never its checkout or
# offline shader manifest. Used by LoFsr.cmake and the SDK-source regression.
function(lo_fsr_patch_memory_selection source output_variable)
    set(_begin "uint32_t findMemoryTypeIndex(VkPhysicalDevice physicalDevice, VkMemoryRequirements memRequirements, VkMemoryPropertyFlags requestedProperties, VkMemoryPropertyFlags& outProperties)")
    set(_end "VkBufferUsageFlags ffxGetVKBufferUsageFlagsFromResourceUsage")
    string(FIND "${source}" "${_begin}" _start)
    string(FIND "${source}" "${_end}" _stop)
    if(_start LESS 0 OR _stop LESS_EQUAL _start)
        message(FATAL_ERROR "Pinned FSR Vulkan memory selection anchors missing")
    endif()
    math(EXPR _length "${_stop} - ${_start}")
    string(SUBSTRING "${source}" ${_start} ${_length} _original)
    string(FIND "${_original}" "memProperties.memoryTypes[i].propertyFlags & requestedProperties" _match)
    string(FIND "${_original}" "requestedProperties == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT" _preference)
    if(_match LESS 0 OR _preference LESS 0)
        message(FATAL_ERROR "Pinned FSR Vulkan memory selection body changed")
    endif()
    set(_replacement [=[uint32_t findMemoryTypeIndex(VkPhysicalDevice physicalDevice, VkMemoryRequirements memRequirements, VkMemoryPropertyFlags requestedProperties, VkMemoryPropertyFlags& outProperties)
{
    FFX_ASSERT(NULL != physicalDevice);
    VkPhysicalDeviceMemoryProperties memProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    const uint32_t selected = lo::fsr::SelectMemoryType(
        memRequirements.memoryTypeBits, memProperties.memoryTypeCount, requestedProperties,
        {VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD},
        [&](uint32_t i) { return memProperties.memoryTypes[i].propertyFlags; });
    if (selected != UINT32_MAX) outProperties = memProperties.memoryTypes[selected].propertyFlags;
    return selected;
}

]=])
    string(REPLACE "${_original}" "${_replacement}" _patched "${source}")
    set(${output_variable} "${_patched}" PARENT_SCOPE)
endfunction()
