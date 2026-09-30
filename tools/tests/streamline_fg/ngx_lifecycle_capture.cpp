#include "ngx_lifecycle_capture.h"
#include <plume_vulkan.h>
#include <nvsdk_ngx_vk.h>
#include <atomic>
#include <cstdio>

namespace {
std::atomic<unsigned> lifecycleErrors{};
void Record(NVSDK_NGX_Result result) {
    if (NVSDK_NGX_FAILED(result)) ++lifecycleErrors;
}
}

unsigned probe::NgxLifecycleErrors() { return lifecycleErrors.load(); }

// The standalone probe compiles Controller's NGX calls against these wrappers.
// Their results are printed immediately, independently of its 128-call report.
extern "C" NVSDK_NGX_Result NVSDK_CONV LoProbeNgxReleaseFeature(NVSDK_NGX_Handle* handle) {
    const auto result = NVSDK_NGX_VULKAN_ReleaseFeature(handle);
    Record(result);
    std::printf("LIFECYCLE_NGX_RELEASE_FEATURE result=%d\n", int(result));
    return result;
}

extern "C" NVSDK_NGX_Result NVSDK_CONV LoProbeNgxDestroyParameters(NVSDK_NGX_Parameter* parameters) {
    const auto result = NVSDK_NGX_VULKAN_DestroyParameters(parameters);
    Record(result);
    std::printf("LIFECYCLE_NGX_DESTROY_PARAMETERS result=%d\n", int(result));
    return result;
}

extern "C" NVSDK_NGX_Result NVSDK_CONV LoProbeNgxShutdown1(VkDevice device) {
    const auto result = NVSDK_NGX_VULKAN_Shutdown1(device);
    Record(result);
    std::printf("LIFECYCLE_NGX_SHUTDOWN1 result=%d\n", int(result));
    return result;
}
