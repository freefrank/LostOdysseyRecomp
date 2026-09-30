#include "input_completion.h"
#include <vulkan/vulkan.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {
unsigned checks = 0;
void Check(bool ok, const char* what) {
    ++checks;
    if (!ok) throw std::runtime_error(what);
}
void Vk(VkResult result, const char* what) { Check(result == VK_SUCCESS, what); }
struct Vulkan {
    VkInstance instance{};
    VkDevice device{};
    VkQueue queue{};
    VkSemaphore timeline{};
    VkFence render{}, tail{};
    VkDebugUtilsMessengerEXT messenger{};
    std::atomic<unsigned> errors{};
    PFN_vkDestroyDebugUtilsMessengerEXT destroyMessenger{};
    static VKAPI_ATTR VkBool32 VKAPI_CALL Message(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* data, void* user) {
        if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            ++*static_cast<std::atomic<unsigned>*>(user);
            std::fprintf(stderr, "VALIDATION_ERROR: %s\n", data->pMessage);
        }
        return VK_FALSE;
    }
    void Init() {
        uint32_t count{};
        Vk(vkEnumerateInstanceLayerProperties(&count, nullptr), "layer count");
        std::vector<VkLayerProperties> layers(count);
        Vk(vkEnumerateInstanceLayerProperties(&count, layers.data()), "layers");
        bool validation = false;
        for (const auto& layer : layers)
            validation |= std::strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0;
        Check(validation, "Khronos validation is required for this test");
        const char* layer = "VK_LAYER_KHRONOS_validation";
        const char* extensions[] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME, VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME};
        const VkValidationFeatureEnableEXT sync = VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
        VkValidationFeaturesEXT features{VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
        features.enabledValidationFeatureCount = 1; features.pEnabledValidationFeatures = &sync;
        VkDebugUtilsMessengerCreateInfoEXT debug{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug.pfnUserCallback = Message; debug.pUserData = &errors; debug.pNext = &features;
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.apiVersion = VK_API_VERSION_1_2;
        VkInstanceCreateInfo create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        create.pApplicationInfo = &app; create.enabledLayerCount = 1; create.ppEnabledLayerNames = &layer;
        create.enabledExtensionCount = 2; create.ppEnabledExtensionNames = extensions; create.pNext = &debug;
        Vk(vkCreateInstance(&create, nullptr, &instance), "instance");
        auto createMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
        destroyMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
        Check(createMessenger && destroyMessenger, "debug utils functions");
        debug.pNext = nullptr;
        Vk(createMessenger(instance, &debug, nullptr, &messenger), "validation messenger");
        Vk(vkEnumeratePhysicalDevices(instance, &count, nullptr), "physical device count");
        Check(count > 0, "Vulkan device required");
        std::vector<VkPhysicalDevice> physical(count);
        Vk(vkEnumeratePhysicalDevices(instance, &count, physical.data()), "physical devices");
        VkPhysicalDeviceProperties properties{}; vkGetPhysicalDeviceProperties(physical[0], &properties);
        std::printf("DEVICE=%s LAYER=VK_LAYER_KHRONOS_validation SYNCHRONIZATION_VALIDATION=requested\n", properties.deviceName);
        VkPhysicalDeviceTimelineSemaphoreFeatures timelineFeature{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES};
        VkPhysicalDeviceFeatures2 supported{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2}; supported.pNext = &timelineFeature;
        vkGetPhysicalDeviceFeatures2(physical[0], &supported);
        Check(timelineFeature.timelineSemaphore, "timeline semaphore feature");
        vkGetPhysicalDeviceQueueFamilyProperties(physical[0], &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical[0], &count, families.data());
        uint32_t family = count;
        for (uint32_t i = 0; i < count; ++i)
            if (families[i].queueCount && (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) { family = i; break; }
        Check(family < count, "graphics queue");
        const float priority = 1;
        VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queueInfo.queueFamilyIndex = family; queueInfo.queueCount = 1; queueInfo.pQueuePriorities = &priority;
        VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        deviceInfo.pNext = &timelineFeature; deviceInfo.queueCreateInfoCount = 1; deviceInfo.pQueueCreateInfos = &queueInfo;
        Vk(vkCreateDevice(physical[0], &deviceInfo, nullptr, &device), "device");
        vkGetDeviceQueue(device, family, 0, &queue);
        VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO}; type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        VkSemaphoreCreateInfo sem{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO}; sem.pNext = &type;
        Vk(vkCreateSemaphore(device, &sem, nullptr, &timeline), "timeline");
        VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        Vk(vkCreateFence(device, &fence, nullptr, &render), "render fence");
        Vk(vkCreateFence(device, &fence, nullptr, &tail), "tail fence");
    }
    void Signal(uint64_t value) {
        VkSemaphoreSignalInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO}; info.semaphore = timeline; info.value = value;
        Vk(vkSignalSemaphore(device, &info), "host timeline signal");
    }
    VkResult Wait(uint64_t value, uint64_t timeout) {
        VkSemaphoreWaitInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
        info.semaphoreCount = 1; info.pSemaphores = &timeline; info.pValues = &value;
        return vkWaitSemaphores(device, &info, timeout);
    }
    void QueueGate(uint64_t value) {
        const VkPipelineStageFlags stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        VkTimelineSemaphoreSubmitInfo point{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
        point.waitSemaphoreValueCount = 1; point.pWaitSemaphoreValues = &value;
        VkSubmitInfo gate{VK_STRUCTURE_TYPE_SUBMIT_INFO}; gate.pNext = &point;
        gate.waitSemaphoreCount = 1; gate.pWaitSemaphores = &timeline; gate.pWaitDstStageMask = &stage;
        Vk(vkQueueSubmit(queue, 1, &gate, VK_NULL_HANDLE), "modeled presenting queue gate");
        // Same empty post-Present marker submission as the probe. This fixture
        // models queue blocking; it does not call Streamline or WSI Present.
        const VkSubmitInfo marker{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        Vk(vkQueueSubmit(queue, 1, &marker, tail), "post-gate marker");
    }
    void Close() {
        if (device) {
            if (timeline) {
                uint64_t value{};
                if (vkGetSemaphoreCounterValue(device, timeline, &value) == VK_SUCCESS && value < 100) {
                    const VkSemaphoreSignalInfo signal{VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO, nullptr, timeline, 100};
                    if (vkSignalSemaphore(device, &signal) != VK_SUCCESS) std::terminate();
                }
            }
            if (vkDeviceWaitIdle(device) != VK_SUCCESS) std::terminate(); // teardown only
            if (tail) vkDestroyFence(device, tail, nullptr);
            if (render) vkDestroyFence(device, render, nullptr);
            if (timeline) vkDestroySemaphore(device, timeline, nullptr);
            vkDestroyDevice(device, nullptr); device = VK_NULL_HANDLE;
        }
        if (messenger) { destroyMessenger(instance, messenger, nullptr); messenger = VK_NULL_HANDLE; }
        if (instance) { vkDestroyInstance(instance, nullptr); instance = VK_NULL_HANDLE; }
    }
    ~Vulkan() { Close(); }
};
void Run(Vulkan& vk) {
    using Sync = probe::InputCompletion;
    Sync sync;
    const VkSubmitInfo render{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    Vk(vkQueueSubmit(vk.queue, 1, &render, vk.render), "render submit before modeled Present");
    Vk(vkWaitForFences(vk.device, 1, &vk.render, VK_TRUE, 1'000'000'000), "early render completes");
    Check(sync.BeginPresent(1, 1) && sync.PresentReturned(true), "protected present state");
    Check(sync.Observe(1, 1, true, {}) == Sync::Observation::QueueProtected, "null signal only in protected mode");
    vk.QueueGate(1);
    Vk(vkGetFenceStatus(vk.device, vk.render), "old render fence already signaled");
    Check(vkWaitForFences(vk.device, 1, &vk.tail, VK_TRUE, 0) == VK_TIMEOUT, "post-Present marker remains pending");
    Check(!sync.QueueWaitFinished(false) && !sync.Idle(), "timeout cannot allow input reuse");
    vk.Signal(1);
    Vk(vkWaitForFences(vk.device, 1, &vk.tail, VK_TRUE, 1'000'000'000), "tail completes after modeled SDK gate");
    Check(sync.QueueWaitFinished(true) && !sync.CanEnableExplicit(), "missing SDK point does not arm explicit mode");
    uintptr_t bits{}; static_assert(sizeof(bits) == sizeof(vk.timeline));
    std::memcpy(&bits, &vk.timeline, sizeof(bits));
    Vk(vkResetFences(vk.device, 1, &vk.tail), "reset completed tail");
    Check(sync.BeginPresent(2, 1) && sync.PresentReturned(true), "second protected present");
    Check(sync.Observe(2, 1, true, {bits, 2}) == Sync::Observation::QueueProtected, "bootstrap point");
    vk.QueueGate(2); vk.Signal(2);
    Vk(vkWaitForFences(vk.device, 1, &vk.tail, VK_TRUE, 1'000'000'000), "bootstrap tail completes");
    Check(sync.QueueWaitFinished(true) && sync.EnableExplicit(), "real queue retirement enables explicit mode");
    Check(sync.BeginPresent(3, 1) && sync.PresentReturned(true), "explicit present");
    Check(sync.Observe(3, 1, true, {}) == Sync::Observation::Missing, "explicit null remains unresolved");
    Check(!sync.QueueWaitFinished(true), "old tail fence cannot retire explicit use");
    Check(sync.Observe(3, 1, true, {bits, 3}) == Sync::Observation::Timeline, "exact deferred point");
    Check(vk.Wait(3, 0) == VK_TIMEOUT, "actual timeline timeout");
    Check(!sync.TimelineWaitFinished(false) && sync.PendingPoint() == Sync::Point{bits, 3}, "failed wait retains semaphore and value");
    vk.Signal(3); Vk(vk.Wait(3, 1'000'000'000), "same timeline wait retry");
    Check(sync.TimelineWaitFinished(true) && sync.Idle(), "inputs reusable only after successful wait");
}
}
int main() {
    try {
        Vulkan vk; vk.Init(); Run(vk); vk.Close();
        Check(vk.errors.load() == 0, "no Vulkan validation errors");
        std::printf("PASS: %u Vulkan synchronization checks; modeled queue gate, no Streamline/NGX/WSI/FG execution\n", checks);
    } catch (const std::exception& e) { std::fprintf(stderr, "FAIL: %s\n", e.what()); return 1; }
}
