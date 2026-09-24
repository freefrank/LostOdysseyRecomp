#include "probe_scene.h"
#include "probe_vulkan_dispatch.h"
#include "streamline_runtime.h"
#include "ngx_lifecycle_capture.h"
#include <algorithm>
#include <atomic>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>
#include <psapi.h>

namespace {
using namespace plume;
using namespace probe;
[[noreturn]] void Fail(const std::string& why) { throw std::runtime_error(why); }
void Check(bool condition, const char* why) { if (!condition) Fail(why); }
void SL(sl::Result result, const char* what) { if (result != sl::Result::eOk) Fail(std::string(what) + " SL result " + std::to_string(int(result))); }
void VK(VkResult result, const char* what) { if (result != VK_SUCCESS) Fail(std::string(what) + " VkResult " + std::to_string(int(result))); }
uint64_t ImageHandle(VkImage image) {
    static_assert(sizeof(VkImage) == sizeof(uint64_t));
    uint64_t value{};
    std::memcpy(&value, &image, sizeof(value));
    return value;
}
LRESULT CALLBACK WindowProc(HWND window, UINT msg, WPARAM w, LPARAM l) {
    return msg == WM_CLOSE ? 0 : DefWindowProcW(window, msg, w, l);
}
void Messages() { MSG msg{}; while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); } }
void Memory(const char* stage) {
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)))
        std::printf("MEMORY stage=%s private_bytes=%llu peak_working_set=%llu\n", stage,
            static_cast<unsigned long long>(counters.PrivateUsage),
            static_cast<unsigned long long>(counters.PeakWorkingSetSize));
}
VKAPI_ATTR VkBool32 VKAPI_CALL ValidationMessage(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* data, void* user) {
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        ++*static_cast<std::atomic<unsigned>*>(user);
        std::printf("VALIDATION_ERROR id=%s message=%s\n", data && data->pMessageIdName ? data->pMessageIdName : "unknown",
            data && data->pMessage ? data->pMessage : "(null)");
    }
    return VK_FALSE;
}
struct Window {
    HINSTANCE instance = GetModuleHandleW(nullptr);
    HWND handle{};
    bool classRegistered{};
    bool noActivate{};
    bool Activate() {
        if (GetForegroundWindow() == handle) return true;
        BringWindowToTop(handle);
        SetForegroundWindow(handle);
        SetActiveWindow(handle);
        SetFocus(handle);
        if (GetForegroundWindow() != handle) {
            const auto owner = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
            const auto self = GetCurrentThreadId();
            if (owner && owner != self && AttachThreadInput(self, owner, TRUE)) {
                BringWindowToTop(handle);
                SetForegroundWindow(handle);
                SetActiveWindow(handle);
                SetFocus(handle);
                AttachThreadInput(self, owner, FALSE);
            }
        }
        if (GetForegroundWindow() != handle) {
            auto* foreground = GetForegroundWindow();
            DWORD pid{}; GetWindowThreadProcessId(foreground, &pid);
            std::printf("FOREGROUND_OTHER hwnd=%p pid=%lu probe=%p\n", foreground, pid, handle);
        }
        return GetForegroundWindow() == handle;
    }
    explicit Window(bool disableActivation) : noActivate(disableActivation) {
        WNDCLASSW wc{}; wc.lpfnWndProc = WindowProc; wc.hInstance = instance; wc.lpszClassName = L"LOStreamlineFgP0";
        Check(RegisterClassW(&wc) != 0, "RegisterClassW");
        classRegistered = true;
        Resize(1920, 1080);
    }
    void Resize(uint32_t width, uint32_t height) {
        if (!handle) {
            handle = CreateWindowExW(WS_EX_NOACTIVATE, L"LOStreamlineFgP0", L"NGX SR + Streamline FG P0",
                WS_POPUP | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr, instance, nullptr);
            Check(handle != nullptr, "CreateWindowExW");
            ShowWindow(handle, SW_SHOWNOACTIVATE);
        } else {
            Check(SetWindowPos(handle, nullptr, 0, 0, width, height,
                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != 0, "SetWindowPos resize");
        }
        if (!noActivate) Activate();
        Messages();
        RECT client{};
        Check(GetClientRect(handle, &client) != 0, "GetClientRect");
        std::printf("WINDOW_CLIENT requested=%ux%u actual=%ldx%ld dpi=%u screen=%dx%d no_activate=%d\n",
            width, height, client.right - client.left, client.bottom - client.top, GetDpiForWindow(handle),
            GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), int(noActivate));
        std::printf("WINDOW_FOCUSED=%d foreground=%p probe=%p\n", GetForegroundWindow() == handle, GetForegroundWindow(), handle);
    }
    bool Close() {
        bool success = true;
        if (handle) { success &= DestroyWindow(handle) != 0; handle = nullptr; }
        if (classRegistered) {
            success &= UnregisterClassW(L"LOStreamlineFgP0", instance) != 0;
            classRegistered = false;
        }
        std::printf("LIFECYCLE_WINDOW_CLOSE=%d\n", int(success));
        return success;
    }
    void AbandonAfterDrainFailure() { handle = nullptr; classRegistered = false; }
    ~Window() { Close(); }
};
struct Swapchain {
    VkDevice device{};
    VkInstance instance{};
    VkSurfaceKHR surface{};
    VkSwapchainKHR swapchain{};
    VkExtent2D extent{};
    VkFormat format{};
    uint32_t epoch{};
    std::vector<VkImage> images;
    void DestroyChain() { if (swapchain) { vkDestroySwapchainKHR(device, swapchain, nullptr); swapchain = {}; images.clear(); } }
    void Destroy() { DestroyChain(); if (surface) { vkDestroySurfaceKHR(instance, surface, nullptr); surface = {}; } }
    bool Create(VulkanDevice& gpu, HWND window, uint32_t width, uint32_t height, std::string& unavailable) {
        device = gpu.vk; instance = gpu.renderInterface->instance;
        if (!surface) {
            VkWin32SurfaceCreateInfoKHR desc{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
            desc.hinstance = GetModuleHandleW(nullptr); desc.hwnd = window;
            VK(vkCreateWin32SurfaceKHR(instance, &desc, nullptr, &surface), "vkCreateWin32SurfaceKHR");
        }
        VkBool32 supported{};
        VK(vkGetPhysicalDeviceSurfaceSupportKHR(gpu.physicalDevice, gpu.queueFamilyIndices[0], surface, &supported), "surface queue support");
        if (!supported) { unavailable = "Plume DIRECT queue cannot present to Win32 surface"; return false; }
        VkSurfaceCapabilitiesKHR caps{};
        VK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu.physicalDevice, surface, &caps), "surface capabilities");
        if (!(caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)) { unavailable = "surface swapchain cannot receive SR blit"; return false; }
        uint32_t count{}; VK(vkGetPhysicalDeviceSurfaceFormatsKHR(gpu.physicalDevice, surface, &count, nullptr), "surface format count");
        std::vector<VkSurfaceFormatKHR> formats(count);
        VK(vkGetPhysicalDeviceSurfaceFormatsKHR(gpu.physicalDevice, surface, &count, formats.data()), "surface formats");
        const auto selected = std::find_if(formats.begin(), formats.end(), [](auto f) {
            return f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        });
        if (selected == formats.end()) { unavailable = "BGRA8 UNORM SDR surface format unavailable"; return false; }
        format = selected->format;
        VkFormatProperties inputProps{}, outputProps{};
        vkGetPhysicalDeviceFormatProperties(gpu.physicalDevice, VK_FORMAT_R16G16B16A16_SFLOAT, &inputProps);
        vkGetPhysicalDeviceFormatProperties(gpu.physicalDevice, format, &outputProps);
        if (!(inputProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) ||
            !(outputProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)) {
            unavailable = "FP16 SR output -> BGRA8 blit unsupported"; return false;
        }
        uint32_t modeCount{}; VK(vkGetPhysicalDeviceSurfacePresentModesKHR(gpu.physicalDevice, surface, &modeCount, nullptr), "present mode count");
        std::vector<VkPresentModeKHR> modes(modeCount);
        VK(vkGetPhysicalDeviceSurfacePresentModesKHR(gpu.physicalDevice, surface, &modeCount, modes.data()), "present modes");
        if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) == modes.end()) {
            unavailable = "VSync-off immediate present mode unsupported"; return false;
        }
        extent = caps.currentExtent.width != UINT32_MAX ? caps.currentExtent : VkExtent2D{width, height};
        std::printf("SURFACE_EXTENT requested=%ux%u current=%ux%u min=%ux%u max=%ux%u selected=%ux%u\n",
            width, height, caps.currentExtent.width, caps.currentExtent.height, caps.minImageExtent.width,
            caps.minImageExtent.height, caps.maxImageExtent.width, caps.maxImageExtent.height, extent.width, extent.height);
        if (extent.width < 1920 || extent.height < 1080) { unavailable = "drawable below 1920x1080 probe minimum"; return false; }
        VkSwapchainCreateInfoKHR desc{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        desc.surface = surface; desc.minImageCount = std::clamp(3u, caps.minImageCount, caps.maxImageCount ? caps.maxImageCount : UINT32_MAX);
        desc.imageFormat = format; desc.imageColorSpace = selected->colorSpace; desc.imageExtent = extent; desc.imageArrayLayers = 1;
        desc.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (!(caps.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)) desc.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        desc.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; desc.preTransform = caps.currentTransform;
        desc.compositeAlpha = (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) ?
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
        if (!(caps.supportedCompositeAlpha & desc.compositeAlpha)) { unavailable = "unsupported composite alpha"; return false; }
        desc.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR; desc.clipped = VK_TRUE;
        VK(vkCreateSwapchainKHR(device, &desc, nullptr, &swapchain), "hooked vkCreateSwapchainKHR");
        uint32_t n{}; VK(vkGetSwapchainImagesKHR(device, swapchain, &n, nullptr), "swapchain image count");
        images.resize(n);
        VK(vkGetSwapchainImagesKHR(device, swapchain, &n, images.data()), "swapchain images");
        ++epoch;
        for (uint32_t i = 0; i < n; ++i)
            std::printf("SWAPCHAIN_IMAGE epoch=%u index=%u handle=0x%llx extent=%ux%u\n",
                epoch, i, static_cast<unsigned long long>(ImageHandle(images[i])), extent.width, extent.height);
        std::printf("SWAPCHAIN=%ux%u images=%u present=IMMEDIATE format=%d\n", extent.width, extent.height, n, format);
        return true;
    }
};
struct App {
    StreamlineRuntime sl;
    std::unique_ptr<gpu::dlss::Controller> ngx;
    std::unique_ptr<VulkanDispatch> hooks;
    std::unique_ptr<RenderInterface> render;
    std::unique_ptr<RenderDevice> device;
    Window window;
    Swapchain swap;
    std::unique_ptr<Scene> scene;
    std::unique_ptr<RenderCommandQueue> queue;
    std::unique_ptr<RenderCommandList> prefix, isolated, continuation;
    VkSemaphore acquire{};
    std::vector<VkSemaphore> present;
    VkFence fence{};
    VkDebugUtilsMessengerEXT validationMessenger{};
    PFN_vkDestroyDebugUtilsMessengerEXT destroyValidationMessenger{};
    std::atomic<unsigned> validationErrors{};
    const sl::ViewportHandle viewport{0u};
    uint64_t serial{};
    bool fgEnabled{};
    bool noActivate{};
    bool fgResourceUsed{};
    bool fgOptionsAwaitPresent{};
    bool fgUnavailableInBackground{};
    bool nativeRetirementFailed{};
    bool inFlight{};
    uint64_t pendingUseId{};
    uint32_t lastFrameId{};
    bool sceneFirstFrame = true;
    uint64_t lastHash{};
    uint32_t generatedIntervals{};
    bool finalCleanupOk = true;
    bool cleanedUp{};
    const std::filesystem::path output = std::filesystem::current_path();
    explicit App(bool disableActivation) : window(disableActivation), noActivate(disableActivation) {}
    ~App() { Cleanup(); }
    void DiscardPendingUse() {
        if (!ngx || !pendingUseId) return;
        ngx->OnBatchDiscarded(pendingUseId);
        std::printf("LIFECYCLE_NGX_DISCARD_PENDING use=%llu\n", static_cast<unsigned long long>(pendingUseId));
        pendingUseId = 0;
    }
    void AbandonGpuObjects(const char* reason) {
        // Destructors would free Vulkan/SDK objects while GPU ownership is
        // unknown. The process exits with code 1 and the OS reclaims them.
        (void)scene.release();
        (void)prefix.release(); (void)isolated.release(); (void)continuation.release();
        (void)queue.release();
        (void)device.release(); (void)render.release(); (void)hooks.release(); (void)ngx.release();
        sl.AbandonAfterDrainFailure();
        window.AbandonAfterDrainFailure();
        finalCleanupOk = false;
        std::printf("LIFECYCLE_GPU_OBJECTS_ABANDONED reason=%s\n", reason);
    }
    void InstallValidationMonitor() {
        auto instance = static_cast<VulkanInterface*>(render.get())->instance;
        auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
        destroyValidationMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
        Check(create && destroyValidationMessenger, "Vulkan validation monitor requires VK_EXT_debug_utils");
        VkDebugUtilsMessengerCreateInfoEXT info{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        info.pfnUserCallback = ValidationMessage;
        info.pUserData = &validationErrors;
        VK(create(instance, &info, nullptr, &validationMessenger), "vkCreateDebugUtilsMessengerEXT");
        std::puts("VALIDATION_MONITOR=installed");
    }
    void CreatePresentSemaphores() {
        auto* gpu = static_cast<VulkanDevice*>(device.get());
        Check(present.empty() && !swap.images.empty(), "present semaphores require new swapchain images");
        present.reserve(swap.images.size());
        VkSemaphoreCreateInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        for (size_t i = 0; i < swap.images.size(); ++i) {
            VkSemaphore semaphore{};
            VK(vkCreateSemaphore(gpu->vk, &info, nullptr, &semaphore), "per-image present semaphore");
            present.push_back(semaphore);
        }
        std::printf("PRESENT_SEMAPHORES=%zu one_per_swapchain_image\n", present.size());
    }
    void DestroyPresentSemaphores() {
        if (!device) return;
        auto* gpu = static_cast<VulkanDevice*>(device.get());
        for (auto semaphore : present) vkDestroySemaphore(gpu->vk, semaphore, nullptr);
        present.clear();
    }
    void Drain() {
        if (!device) return;
        auto* gpu = static_cast<VulkanDevice*>(device.get());
        if (inFlight) {
            VK(vkWaitForFences(gpu->vk, 1, &fence, VK_TRUE, 10'000'000'000ull), "host submit fence");
            ngx->ReleaseCompletedThrough(serial);
            inFlight = false;
        }
        VK(vkDeviceWaitIdle(gpu->vk), "hooked device drain at lifecycle boundary");
        std::printf("BOUNDARY_DRAIN serial=%llu\n", static_cast<unsigned long long>(serial));
        Memory("boundary");
    }
    void Mode(bool enabled) {
        sl::DLSSGOptions options{};
        options.mode = enabled ? sl::DLSSGMode::eOn : sl::DLSSGMode::eOff;
        options.numFramesToGenerate = 1;
        options.queueParallelismMode = sl::DLSSGQueueParallelismMode::eBlockPresentingClientQueue;
        options.numBackBuffers = uint32_t(swap.images.size());
        options.colorWidth = swap.extent.width; options.colorHeight = swap.extent.height;
        options.colorBufferFormat = swap.format;
        if (scene) {
            options.mvecDepthWidth = scene->renderWidth; options.mvecDepthHeight = scene->renderHeight;
            options.mvecBufferFormat = VK_FORMAT_R16G16_SFLOAT;
            options.depthBufferFormat = VK_FORMAT_R32_SFLOAT;
            options.hudLessBufferFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
            options.uiBufferFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        }
        fgResourceUsed |= enabled;
        SL(sl.DLSSGSetOptions(viewport, options), "slDLSSGSetOptions");
        fgEnabled = enabled;
        fgOptionsAwaitPresent = true;
        std::printf("FG_MODE=%s fixed2x=1 block_presenting_client_queue=1\n", enabled ? "on" : "off");
    }
    void ClearTags() {
        if (!sl.SetTagForFrame || !serial) return;
        sl::FrameToken* token{}; const auto index = uint32_t(serial);
        SL(sl.NewFrameToken(token, &index), "clear tags token");
        sl::ResourceTag tags[] = {
            {nullptr, sl::kBufferTypeDepth, sl::eValidUntilPresent},
            {nullptr, sl::kBufferTypeMotionVectors, sl::eValidUntilPresent},
            {nullptr, sl::kBufferTypeHUDLessColor, sl::eValidUntilPresent},
            {nullptr, sl::kBufferTypeUIColorAndAlpha, sl::eValidUntilPresent}};
        SL(sl.SetTagForFrame(*token, viewport, tags, 4, nullptr), "clear FG tags");
    }
    void Cleanup() noexcept {
        if (cleanedUp) return;
        cleanedUp = true;
        const auto nativeErrorsBefore = NgxLifecycleErrors();
        const auto step = [this](const char* name, auto action) {
            try { action(); }
            catch (const std::exception& ex) {
                finalCleanupOk = false;
                std::fprintf(stderr, "CLEANUP_FAILURE stage=%s reason=%s\n", name, ex.what());
            }
        };
        DiscardPendingUse();
        if (device && fgEnabled && sl.DLSSGSetOptions)
            step("request_fg_off", [this] { Mode(false); });
        if (device && fgOptionsAwaitPresent) {
            if (scene && swap.swapchain && queue && prefix && isolated && continuation && fence && acquire &&
                present.size() == swap.images.size())
                step("off_present", [this] { Frame(lastFrameId + 1, false); });
            else {
                finalCleanupOk = false;
                std::puts("LIFECYCLE_OFF_PRESENT=unavailable_resources");
            }
        }
        DiscardPendingUse();
        bool drained = !device;
        if (device) step("drain", [this, &drained] { Drain(); drained = true; });
        if (!drained) { AbandonGpuObjects("drain_failed"); return; }
        if (fgOptionsAwaitPresent) { AbandonGpuObjects("off_present_failed"); return; }
        if (device && fgResourceUsed) {
            if (!sl.FreeResources(sl::kFeatureDLSS_G, viewport)) {
                AbandonGpuObjects("sl_free_resources_failed"); return;
            }
            else fgResourceUsed = false;
        }
        if (nativeRetirementFailed) { AbandonGpuObjects("native_feature_release_failed"); return; }
        if (ngx && device) {
            step("native_feature_release", [this] { ngx->ReleaseFeatureAfterGpuDrain(); });
            if (NgxLifecycleErrors() != nativeErrorsBefore || ngx->HasFeatureState()) {
                AbandonGpuObjects("native_feature_release_failed"); return;
            }
            std::puts("LIFECYCLE_NGX_FEATURE_RELEASE=complete");
        }
        if (device) step("clear_tags", [this] { ClearTags(); });
        scene.reset();
        if (device) { DestroyPresentSemaphores(); swap.Destroy(); }
        if (ngx && device) step("native_session_shutdown", [this] { ngx->ShutdownAfterGpuDrain(); });
        if (device && fence) vkDestroyFence(static_cast<VulkanDevice*>(device.get())->vk, fence, nullptr);
        if (device && acquire) vkDestroySemaphore(static_cast<VulkanDevice*>(device.get())->vk, acquire, nullptr);
        prefix.reset(); isolated.reset(); continuation.reset(); queue.reset();
        if (!sl.Shutdown()) finalCleanupOk = false;
        if (sl.ErrorCount()) {
            std::fprintf(stderr, "CLEANUP_SL_ERRORS=%u (inspect Streamline log)\n", sl.ErrorCount());
            finalCleanupOk = false;
        }
        if (NgxLifecycleErrors() != nativeErrorsBefore) {
            std::fprintf(stderr, "CLEANUP_NGX_LIFECYCLE_ERRORS=%u\n", NgxLifecycleErrors() - nativeErrorsBefore);
            finalCleanupOk = false;
        }
        if (hooks) { hooks->Report(); hooks->RestoreDeviceHooks(); hooks->RestoreCreationHooks(); }
        device.reset();
        if (validationMessenger && destroyValidationMessenger) {
            destroyValidationMessenger(static_cast<VulkanInterface*>(render.get())->instance, validationMessenger, nullptr);
            validationMessenger = {};
        }
        render.reset(); hooks.reset(); ngx.reset();
        if (validationErrors.load()) {
            std::fprintf(stderr, "VALIDATION_ERRORS=%u\n", validationErrors.load());
            finalCleanupOk = false;
        }
        if (!window.Close()) finalCleanupOk = false;
        std::printf("LIFECYCLE_CLEANUP_OK=%d\n", int(finalCleanupOk));
    }
    sl::Constants Constants() const {
        sl::Constants c{};
        // Unjittered constant forward-Z perspective, near=0.1 far=100, FOV=1 rad.
        constexpr float nearPlane = .1f, farPlane = 100.0f;
        const float sy = 1.8304877f; // 1/tan(0.5)
        const float sx = sy * float(scene->renderHeight) / scene->renderWidth;
        const float a = farPlane / (farPlane - nearPlane), b = -nearPlane * a;
        auto identity = [] { sl::float4x4 m{};
            m[0] = {1,0,0,0}; m[1] = {0,1,0,0}; m[2] = {0,0,1,0}; m[3] = {0,0,0,1}; return m; };
        c.cameraViewToClip = identity();
        c.cameraViewToClip[0] = {sx,0,0,0}; c.cameraViewToClip[1] = {0,sy,0,0};
        c.cameraViewToClip[2] = {0,0,a,b}; c.cameraViewToClip[3] = {0,0,1,0};
        c.clipToCameraView = identity();
        c.clipToCameraView[0] = {1/sx,0,0,0}; c.clipToCameraView[1] = {0,1/sy,0,0};
        c.clipToCameraView[2] = {0,0,0,1}; c.clipToCameraView[3] = {0,0,1/b,-a/b};
        c.clipToPrevClip = identity(); c.prevClipToClip = identity(); c.clipToLensClip = identity();
        // Pattern translates 2 render pixels right; camera is static, the motion
        // is explicitly written into the full MV surface by the scene producer.
        c.jitterOffset = {0,0}; c.mvecScale = {1.0f/scene->renderWidth, 1.0f/scene->renderHeight};
        c.cameraPinholeOffset = {0,0};
        c.cameraNear = nearPlane; c.cameraFar = farPlane; c.cameraFOV = 1.0f;
        c.cameraAspectRatio = float(scene->renderWidth)/scene->renderHeight;
        c.cameraPos = {0,0,0}; c.cameraUp = {0,1,0}; c.cameraRight = {1,0,0}; c.cameraFwd = {0,0,1};
        c.depthInverted = sl::Boolean::eFalse; c.cameraMotionIncluded = sl::Boolean::eTrue;
        c.motionVectors3D = sl::Boolean::eFalse;
        c.reset = sceneFirstFrame ? sl::Boolean::eTrue : sl::Boolean::eFalse;
        return c;
    }
    void Frame(uint32_t frame, bool capture) {
        const auto start = std::chrono::steady_clock::now();
        auto* gpu = static_cast<VulkanDevice*>(device.get());
        Check(scene && scene->Ready(), "scene resources unavailable");
        Check(queue && prefix && isolated && continuation, "single DIRECT queue/list unavailable");
        Check(!pendingUseId, "previous native NGX use was not settled");
        lastFrameId = frame;
        // The fence completes the prior submit's acquire wait before this
        // binary acquire semaphore can be signalled again.
        if (inFlight) {
            VK(vkWaitForFences(gpu->vk, 1, &fence, VK_TRUE, 10'000'000'000ull), "submit completion before acquire");
            ngx->ReleaseCompletedThrough(serial);
            inFlight = false;
        }
        sl::FrameToken* token{};
        SL(sl.NewFrameToken(token, &frame), "slGetNewFrameToken");
        SL(sl.ReflexSleep(*token), "slReflexSleep");
        const auto mark = [&](sl::PCLMarker m) { SL(sl.PCLSetMarker(m, *token), "slPCLSetMarker"); };
        mark(sl::PCLMarker::eSimulationStart);
        Messages();
        mark(sl::PCLMarker::eSimulationEnd);
        SL(sl.SetConstants(Constants(), *token, viewport), "slSetConstants");
        uint32_t image{};
        VK(vkAcquireNextImageKHR(gpu->vk, swap.swapchain, UINT64_MAX, acquire, VK_NULL_HANDLE, &image), "hooked acquire");
        Check(image < present.size(), "swapchain image lacks present semaphore");
        scene->Prefix(*prefix, frame);
        auto attempt = ngx->RecordIsolated(*static_cast<VulkanCommandList*>(isolated.get()), scene->config,
            scene->inputs, *static_cast<VulkanTexture*>(scene->hudless.get()));
        pendingUseId = attempt.useId;
        Check(attempt.status == gpu::dlss::SrStatus::Executable && attempt.useId, "native NGX SR create/evaluate failed");
        scene->Continuation(*continuation, swap.images.at(image), swap.extent, capture);
        std::printf("HOST_SWAP_FINAL_LAYOUT_RECORDED epoch=%u frame=%u index=%u handle=0x%llx layout=PRESENT_SRC_KHR\n",
            swap.epoch, frame, image, static_cast<unsigned long long>(ImageHandle(swap.images[image])));
        sl::Resource resources[] = {{sl::ResourceType::eTex2d, nullptr}, {sl::ResourceType::eTex2d, nullptr},
            {sl::ResourceType::eTex2d, nullptr}, {sl::ResourceType::eTex2d, nullptr}};
        sl::ResourceTag tags[] = {{nullptr, sl::kBufferTypeDepth, sl::eValidUntilPresent},
            {nullptr, sl::kBufferTypeMotionVectors, sl::eValidUntilPresent},
            {nullptr, sl::kBufferTypeHUDLessColor, sl::eValidUntilPresent},
            {nullptr, sl::kBufferTypeUIColorAndAlpha, sl::eValidUntilPresent}};
        scene->Tags(resources, tags);
        SL(sl.SetTagForFrame(*token, viewport, tags, 4, nullptr), "slSetTagForFrame real images");
        const VkCommandBuffer commands[] = {static_cast<VulkanCommandList*>(prefix.get())->vk,
            static_cast<VulkanCommandList*>(isolated.get())->vk, static_cast<VulkanCommandList*>(continuation.get())->vk};
        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.waitSemaphoreCount = 1; submit.pWaitSemaphores = &acquire; submit.pWaitDstStageMask = &waitStage;
        submit.commandBufferCount = 3; submit.pCommandBuffers = commands;
        submit.signalSemaphoreCount = 1; submit.pSignalSemaphores = &present[image];
        mark(sl::PCLMarker::eRenderSubmitStart);
        VK(vkResetFences(gpu->vk, 1, &fence), "reset host fence before submit");
        VK(vkQueueSubmit(static_cast<VulkanCommandQueue*>(queue.get())->queue->vk, 1, &submit, fence), "checked single-queue native vkQueueSubmit");
        ngx->OnBatchSubmitted(attempt.useId, ++serial);
        pendingUseId = 0;
        inFlight = true;
        mark(sl::PCLMarker::eRenderSubmitEnd);
        VkPresentInfoKHR info{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        info.waitSemaphoreCount = 1; info.pWaitSemaphores = &present[image];
        info.swapchainCount = 1; info.pSwapchains = &swap.swapchain; info.pImageIndices = &image;
        mark(sl::PCLMarker::ePresentStart);
        VK(vkQueuePresentKHR(static_cast<VulkanCommandQueue*>(queue.get())->queue->vk, &info), "one hooked present");
        fgOptionsAwaitPresent = false;
        sceneFirstFrame = false;
        mark(sl::PCLMarker::ePresentEnd);
        sl::DLSSGState state{};
        SL(sl.DLSSGGetState(viewport, state, nullptr), "slDLSSGGetState on present thread");
        std::printf("FRAME real=%u fg=%d result=%u status=%u actual_presents=%u max_fg=%u min_dim=%u fence_value=%llu elapsed_ms=%lld\n",
            frame, fgEnabled, unsigned(attempt.status), unsigned(state.status), state.numFramesActuallyPresented,
            state.numFramesToGenerateMax, state.minWidthOrHeight,
            static_cast<unsigned long long>(state.lastPresentInputsProcessingCompletionFenceValue),
            static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count()));
        if (fgEnabled && state.numFramesActuallyPresented > 1 && state.status == sl::DLSSGStatus::eOk) ++generatedIntervals;
        if (!fgEnabled && state.numFramesActuallyPresented != 1)
            Fail("FG off must present exactly one real frame");
        if (fgEnabled && (state.status != sl::DLSSGStatus::eOk || state.numFramesToGenerateMax < 1)) {
            if (noActivate) fgUnavailableInBackground = true;
            else Fail("FG enabled but SDK state is invalid");
        }
        if (capture) {
            VK(vkWaitForFences(gpu->vk, 1, &fence, VK_TRUE, 10'000'000'000ull), "capture host fence");
            ngx->ReleaseCompletedThrough(serial); inFlight = false;
            auto hash = scene->Capture(output / ("fg_host_hudless_" + std::to_string(frame) + ".rgba16f"));
            Check(hash && hash != lastHash, "native NGX output did not change across moving frames");
            lastHash = hash;
        }
    }
};
int Run(App& app) {
    const auto start = std::chrono::steady_clock::now();
    const auto root = std::filesystem::path([]() -> std::wstring {
        wchar_t path[MAX_PATH]{}; GetModuleFileNameW(nullptr, path, MAX_PATH); return path;
    }()).parent_path();
    const auto data = app.output / "ngx-data";
    std::filesystem::create_directories(data);
    std::string reason;
    if (!app.sl.Initialize(root, reason)) { std::printf("UNAVAILABLE=%s\n", reason.c_str()); return 77; }
    DWORD hags{}, bytes = sizeof(hags);
    const auto hagsRegistry = RegGetValueW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers",
        L"HwSchMode", RRF_RT_REG_DWORD, nullptr, &hags, &bytes);
    std::printf("HAGS_REGISTRY=%s value=%lu (feature support checked separately by Streamline)\n",
        hagsRegistry == ERROR_SUCCESS ? "present" : "absent_or_unreadable", hags);
    app.ngx = std::make_unique<gpu::dlss::Controller>(data, root);
    app.hooks = std::make_unique<VulkanDispatch>(app.sl, *app.ngx);
#if PLUME_SDL_VULKAN_ENABLED
    app.render = CreateVulkanInterface(nullptr, app.hooks->Hooks());
#else
    app.render = CreateVulkanInterface(app.hooks->Hooks());
#endif
    if (!app.render || !app.hooks->Ready()) {
        std::printf("UNAVAILABLE=instance creation or Streamline requirements: %s\n", app.hooks->Reason().c_str()); return 77;
    }
    uint32_t layers{};
    vkEnumerateInstanceLayerProperties(&layers, nullptr);
    std::vector<VkLayerProperties> availableLayers(layers);
    vkEnumerateInstanceLayerProperties(&layers, availableLayers.data());
    const bool validation = std::any_of(availableLayers.begin(), availableLayers.end(), [](const auto& layer) {
        return strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0;
    });
    std::printf("VK_VALIDATION_LAYER=%s (Plume enables it if present)\n", validation ? "available" : "unavailable");
    if (validation) app.InstallValidationMonitor();
    app.device = app.render->createDevice();
    if (!app.device) {
        std::printf("UNAVAILABLE=device creation or queue preflight: %s\n", app.hooks->Reason().c_str()); return 77;
    }
    auto* gpu = static_cast<VulkanDevice*>(app.device.get());
    std::printf("GPU=%s driver=%u vendor=%u Vulkan=%u\n", gpu->physicalDeviceProperties.deviceName,
        gpu->physicalDeviceProperties.driverVersion, gpu->physicalDeviceProperties.vendorID, gpu->physicalDeviceProperties.apiVersion);
    if (gpu->physicalDeviceProperties.vendorID != 0x10de) { std::puts("UNAVAILABLE=non-NVIDIA GPU"); return 77; }
    if (!app.hooks->InstallDeviceHooks(gpu->renderInterface->instance, gpu->vk, reason)) Fail(reason);
    sl::AdapterInfo adapter{}; adapter.vkPhysicalDevice = gpu->physicalDevice;
    for (const auto feature : StreamlineRuntime::kFeatures) {
        sl::FeatureVersion version{};
        const auto support = app.sl.IsFeatureSupported(feature, adapter);
        const auto versionResult = app.sl.GetFeatureVersion(feature, version);
        std::printf("FEATURE=%u supported=%d version_result=%d streamline=%s ngx=%s\n", feature,
            int(support), int(versionResult), version.versionSL.toStr().c_str(), version.versionNGX.toStr().c_str());
        if (support != sl::Result::eOk) {
            std::printf("UNAVAILABLE=SL feature %u unsupported on this adapter or OS/HAGS\n", feature); return 77;
        }
    }
    if (!app.sl.ImportFunctions(reason)) { std::printf("UNAVAILABLE=%s\n", reason.c_str()); return 77; }
    if (app.ngx->EnsureSession(*gpu) != gpu::dlss::SrStatus::Executable) {
        std::puts("UNAVAILABLE=native NGX SR session unavailable with SL initialized"); return 77;
    }
    app.queue = app.device->createCommandQueue(RenderCommandListType::DIRECT);
    Check(bool(app.queue), "one Plume DIRECT host queue creation");
    app.prefix = app.queue->createCommandList(); app.isolated = app.queue->createCommandList();
    app.continuation = app.queue->createCommandList();
    Check(app.prefix && app.isolated && app.continuation, "command list allocation");
    auto* direct = static_cast<VulkanCommandQueue*>(app.queue.get());
    std::printf("HOST_DIRECT_QUEUE family=%u index=%u vk=%p\n", direct->familyIndex, direct->queueIndex, direct->queue->vk);
    VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VK(vkCreateSemaphore(gpu->vk, &semaphore, nullptr, &app.acquire), "acquire semaphore");
    VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VK(vkCreateFence(gpu->vk, &fence, nullptr, &app.fence), "host submit fence");
    if (!app.swap.Create(*gpu, app.window.handle, 1920, 1080, reason)) { std::printf("UNAVAILABLE=%s\n", reason.c_str()); return 77; }
    app.CreatePresentSemaphores();
    auto reflex = sl::ReflexOptions{}; reflex.mode = sl::ReflexMode::eLowLatency;
    SL(app.sl.ReflexSetOptions(reflex), "slReflexSetOptions low latency");
    auto createScene = [&] {
        const auto sizing = app.ngx->QueryOutputSizing(*static_cast<VulkanInterface*>(app.render.get()), *gpu,
            {1, app.swap.extent.width, app.swap.extent.height});
        const auto& mode = sizing.modes[0];
        if (mode.state != gpu::upscaling::SizingState::Ready || !mode.optimal.width || !mode.optimal.height) {
            std::puts("UNAVAILABLE=native NGX quality sizing unavailable"); return false;
        }
        app.scene = std::make_unique<Scene>(*gpu, mode.optimal.width, mode.optimal.height,
            app.swap.extent.width, app.swap.extent.height);
        Check(app.scene->Ready(), "scene resources allocation");
        app.sceneFirstFrame = true;
        std::printf("NGX_SIZING=%ux%u -> %ux%u\n", mode.optimal.width, mode.optimal.height,
            app.swap.extent.width, app.swap.extent.height);
        const auto logInput = [&](const char* name, const std::unique_ptr<RenderTexture>& texture) {
            std::printf("SL_INPUT_IMAGE epoch=%u type=%s handle=0x%llx\n", app.swap.epoch, name,
                static_cast<unsigned long long>(ImageHandle(static_cast<VulkanTexture*>(texture.get())->vk)));
        };
        logInput("Depth", app.scene->depth);
        logInput("MotionVectors", app.scene->motion);
        logInput("HUDLessColor", app.scene->hudless);
        logInput("UIColorAndAlpha", app.scene->ui);
        std::printf("HOST_COMMAND_BUFFERS epoch=%u prefix=%p isolated=%p continuation=%p\n", app.swap.epoch,
            static_cast<void*>(static_cast<VulkanCommandList*>(app.prefix.get())->vk),
            static_cast<void*>(static_cast<VulkanCommandList*>(app.isolated.get())->vk),
            static_cast<void*>(static_cast<VulkanCommandList*>(app.continuation.get())->vk));
        return true;
    };
    if (!createScene()) return 77;
    Memory("initial");
    uint32_t frame = 1;
    const auto frames = [&](bool fg, uint32_t n) {
        app.Mode(fg);
        const auto generatedBefore = app.generatedIntervals;
        for (uint32_t i = 0; i < n; ++i) {
            if (std::chrono::steady_clock::now() - start > std::chrono::seconds(120)) Fail("probe wall timeout");
            if (fg && !app.noActivate && !app.window.Activate()) {
                // Focus can be briefly stolen by desktop overlays; allow a
                // bounded reacquisition without hiding an unavailable session.
                bool recovered = false;
                for (int retry = 0; retry < 10 && !recovered; ++retry) {
                    Sleep(100); Messages(); recovered = app.window.Activate();
                }
                if (recovered) { std::puts("WINDOW_FOCUS_RECOVERED=1"); }
                else {
                    std::puts("UNAVAILABLE=DLSS-G requires foreground interactive Win32 window");
                    return false;
                }
            }
            const auto id = frame++;
            app.Frame(id, id % 7 == 0);
        }
        if (fg && !app.noActivate && app.generatedIntervals == generatedBefore)
            Fail("no generated presents during this FG-on interval");
        std::printf("PHASE_RESULT fg=%d frames=%u generated_intervals=%u\n", fg, n,
            app.generatedIntervals - generatedBefore);
        return true;
    };
    if (!frames(false, 3) || !frames(true, 9) || !frames(false, 3) || !frames(true, 8)) return 77;
    app.Mode(false); app.Frame(frame++, false); app.Drain();
    Check(app.sl.FreeResources(sl::kFeatureDLSS_G, app.viewport), "resize slFreeResources DLSS_G");
    app.fgResourceUsed = false;
    const auto nativeErrorsBeforeResize = NgxLifecycleErrors();
    app.ngx->ReleaseFeatureAfterGpuDrain();
    app.nativeRetirementFailed = NgxLifecycleErrors() != nativeErrorsBeforeResize || app.ngx->HasFeatureState();
    Check(!app.nativeRetirementFailed, "resize native NGX feature release failed");
    app.ClearTags();
    Memory("pre_resize");
    app.scene.reset(); app.DestroyPresentSemaphores(); app.swap.DestroyChain();
    app.window.Resize(2048, 1152);
    if (!app.swap.Create(*gpu, app.window.handle, 2048, 1152, reason)) { std::printf("UNAVAILABLE=resize: %s\n", reason.c_str()); return 77; }
    app.CreatePresentSemaphores();
    if (!createScene()) return 77;
    if (!frames(false, 3) || !frames(true, 9) || !frames(false, 3) || !frames(true, 8)) return 77;
    app.Mode(false); app.Frame(frame++, false); app.Drain();
    Memory("post_resize");
    Check(app.hooks->Ready(), "hook state failed");
    if (app.noActivate && (app.generatedIntervals < 2 || app.fgUnavailableInBackground)) {
        std::puts("UNAVAILABLE=background window did not sustain generated presents");
        return 77;
    }
    Check(app.generatedIntervals >= 2, "SDK did not report actual generated presents in at least two intervals");
    return 0;
}
int Execute(bool noActivate) {
    App app(noActivate);
    int result = 1;
    try { result = Run(app); }
    catch (const std::exception& ex) { std::fprintf(stderr, "FAIL=%s\n", ex.what()); }
    app.Cleanup();
    if (!app.finalCleanupOk) return 1;
    if (result == 0) {
        std::printf("PASS=actual native NGX SR + Streamline fixed2x FG reported generated presents, generated_intervals=%u\n", app.generatedIntervals);
        std::puts("EXTERNAL_DISPLAY_EVIDENCE=not_collected; physical displayed-frame acceptance pending");
    }
    return result;
}
}
int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 2 || (argc == 2 && std::strcmp(argv[1], "--no-activate") != 0)) {
        std::fprintf(stderr, "USAGE=%s [--no-activate]\n", argv[0]);
        return 1;
    }
    const auto dpiAware = SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    std::printf("WINDOW_DPI_AWARENESS_SET=%d last_error=%lu\n", int(dpiAware), dpiAware ? 0 : GetLastError());
    try { return Execute(argc == 2); }
    catch (const std::exception& ex) { std::fprintf(stderr, "FAIL=%s\n", ex.what()); return 1; }
}
