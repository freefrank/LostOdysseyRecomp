#if !defined(LO_VIDEO_SUBMISSION_UNIT)
#include <version.h>
#include <stdafx.h>
#endif
#include "video.h"
#include "frame_plan.h"
#if defined(LO_GPU_PLUME) || defined(LO_VIDEO_SUBMISSION_UNIT)
#include "backend_device.h"
#include "vulkan_submission_state.h"
#include "vulkan_command_recording.h"
#if !defined(LO_VIDEO_SUBMISSION_UNIT)
#include "dlss_ngx.h"
#include "temporal_upscaler.h"
#endif
#endif
#if !defined(LO_VIDEO_SUBMISSION_UNIT)
#include "renderer.h"
#include "presentation.h"
#if defined(LO_GPU_PLUME)
#include "frame_generation_present_bridge.h"
#include "frame_generation_composite.h"
#include "fsr_frame_generation_vulkan.h"
#include "metalfx_frame_generation.h"
#include "optiscaler_loader.h"
#include "../../shared/frame_generation/environment.h"
#if defined(LO_ENABLE_D3D12_FG) && defined(_WIN32)
#include "frame_generation_d3d12.h"
#endif
#if defined(LO_ENABLE_STREAMLINE_FG) && defined(_WIN32)
#include "streamline_runtime.h"
#include "streamline_vulkan_dispatch.h"
#include "dlss_frame_generation.h"
#include "frame_generation_composite.h"
#endif
#endif
#include "command_processor.h"
#include "frame_rate.h"
#include "vrr_policy.h"
#include "frame_pacer.h"
#include "deadline_wait.h"
#include "frame_plan.h"
#include <settings/config.h>
#include "frame_generation_settings.h"
#include <settings/menu.h>
#include <settings/restart.h>
#include <kernel/memory.h>
#include <os/main_thread.h>
#include <os/platform.h>
#if LO_PLATFORM_MACOS
#include <objc/message.h>
#include <objc/runtime.h>
#endif
#include <os/runtime_libraries.h>
#include <os/shader_log.h>
#include <os/user_paths.h>
#include <hid/hid.h>
#if defined(__ANDROID__)
#include <hid/android_touch.h>
#endif
#include <debug/battle_menu.h>
#include <debug/menu_overlay.h>
#include <host_ui/host_ui.h>
#include <host_ui/rasterizer.h>

#include <SDL.h>
#include <SDL_syswm.h>
#include "window_pixels.h"
#include "window_mode.h"
#include "display_choice.h"
#endif
#include <os/logger.h>

#if defined(LO_GPU_PLUME) || defined(LO_VIDEO_SUBMISSION_UNIT)
#include <plume_render_interface.h>
#include <plume_vulkan.h>
#ifdef _WIN32
#include <plume_d3d12.h>
#endif
#if LO_PLATFORM_MACOS
// Declared the way plume's examples do; plume_metal.h pulls in metal-cpp.

namespace plume {
    std::unique_ptr<RenderInterface> CreateMetalInterface();
    void SetMetalMinimumPresentDuration(RenderSwapChain* swapChain, double seconds);
    bool EncodeMetalFxSpatialScale(RenderCommandList* commandList, const RenderTexture* input,
        const RenderTexture* output, uint32_t inputWidth, uint32_t inputHeight);
    bool SupportsMetalFxTemporal(RenderDevice* device);
}
#endif
#if !defined(LO_VIDEO_SUBMISSION_UNIT)
#include "diagnostic_log.h"
#endif
#endif

#include <vector>
#include <future>
#include <thread>
#include <atomic>
#if defined(LO_VIDEO_SUBMISSION_UNIT)
#include <mutex>
#ifdef _WIN32
#include <windows.h>
#endif
#endif

#if defined(__ANDROID__) && defined(LO_HAS_ADRENOTOOLS)
#include <adrenotools/driver.h>
#include <dlfcn.h>
#endif
#ifdef __ANDROID__
#include <jni.h>
#endif

// LO_VK_CUSTOM_DRIVER=<soname>: load a custom Vulkan driver (Mesa Turnip) from
// LO_CUSTOM_DRIVER_DIR through libadrenotools before plume initialises volk.
// The Android app sets these from the player's choice on the GPU driver page;
// the Qualcomm proprietary driver drops the highlighted menu row's text.
#if defined(__ANDROID__) && defined(LO_HAS_ADRENOTOOLS)
static bool g_customVulkanDriver = false;
#endif

static void LoadCustomVulkanDriver()
{
#if defined(__ANDROID__) && defined(LO_HAS_ADRENOTOOLS)
    const char* driver = getenv("LO_VK_CUSTOM_DRIVER");
    if (!driver || !*driver) return;
    // Turnip keeps one of the five Adreno 6xx bindless sets for dynamic buffer
    // offsets and reports four; the renderer binds five sets and has no dynamic
    // offsets (#185). Mesa reads driconf options from the environment.
    setenv("tu_dont_reserve_descriptor_set", "true", 0);
    const char* hookDir = getenv("LO_NATIVE_LIB_DIR");
    const char* driverDir = getenv("LO_CUSTOM_DRIVER_DIR");
    void* handle = adrenotools_open_libvulkan(RTLD_NOW, ADRENOTOOLS_DRIVER_CUSTOM, nullptr,
        hookDir ? hookDir : "", driverDir ? driverDir : "", driver, nullptr, nullptr);
    if (!handle) {
        LOG_ERROR("vulkan: custom driver '{}' failed to load (hooks '{}', dir '{}'), using the system driver", driver,
            hookDir ? hookDir : "", driverDir ? driverDir : "");
        return;
    }
    auto gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(handle, "vkGetInstanceProcAddr"));
    if (!gipa) {
        LOG_ERROR("vulkan: custom driver '{}' has no vkGetInstanceProcAddr, using the system driver", driver);
        return;
    }
    volkInitializeCustom(gipa);
    g_customVulkanDriver = true;
    // libadrenotools resolves the package lazily: an unloadable .so still ends
    // up on the system driver, which the "video device" line below reveals.
    LOG_INFO("vulkan: custom driver '{}' opened through libadrenotools, instance version {:#x}",
        driver, volkGetInstanceVersion());
#endif
}

// A custom driver that loads but cannot create an instance or device: go back
// to the system loader once (volk re-opens libvulkan.so and replaces the
// custom entry points) so a bad package never locks the player out.
static bool RetryWithSystemVulkanDriver(const char* stage)
{
#if defined(__ANDROID__) && defined(LO_HAS_ADRENOTOOLS)
    if (!g_customVulkanDriver) return false;
    g_customVulkanDriver = false;
    if (volkInitialize() != VK_SUCCESS) {
        LOG_ERROR("vulkan: custom driver failed at {} and the system loader could not be reopened", stage);
        return false;
    }
    LOG_WARNING("vulkan: custom driver failed at {}, retrying with the system driver", stage);
    return true;
#else
    (void)stage;
    return false;
#endif
}

#ifdef __ANDROID__
// Every GPU the instance reports, before device creation can fail or crash:
// player reports name a phone, not the driver that actually loaded.
static void LogVulkanPhysicalDevices(VkInstance instance)
{
    uint32_t count = 0;
    if (!instance || vkEnumeratePhysicalDevices(instance, &count, nullptr) != VK_SUCCESS || !count) {
        LOG_WARNING("vulkan: no physical devices reported");
        return;
    }
    std::vector<VkPhysicalDevice> devices(count);
    if (vkEnumeratePhysicalDevices(instance, &count, devices.data()) < VK_SUCCESS) return;
    devices.resize(count);
    for (const auto device : devices) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device, &properties);
        const uint32_t driver = properties.driverVersion;
        LOG_INFO("vulkan gpu: name='{}' vendor={:#06x} device={:#010x} api={}.{}.{} driver_raw={:#x} driver_decoded={}.{}.{}",
            properties.deviceName, properties.vendorID, properties.deviceID,
            VK_API_VERSION_MAJOR(properties.apiVersion), VK_API_VERSION_MINOR(properties.apiVersion),
            VK_API_VERSION_PATCH(properties.apiVersion), driver, driver >> 22, (driver >> 12) & 0x3ff, driver & 0xfff);
        // driverName/driverInfo tell the Qualcomm proprietary driver from
        // Turnip and carry the Mesa or vendor build string.
        if (properties.apiVersion < VK_API_VERSION_1_2 || !vkGetPhysicalDeviceProperties2) continue;
        VkPhysicalDeviceDriverProperties driverProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
        VkPhysicalDeviceProperties2 properties2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &driverProperties};
        vkGetPhysicalDeviceProperties2(device, &properties2);
        LOG_INFO("vulkan gpu driver: id={} name='{}' info='{}' conformance={}.{}.{}.{}",
            uint32_t(driverProperties.driverID), driverProperties.driverName, driverProperties.driverInfo,
            driverProperties.conformanceVersion.major, driverProperties.conformanceVersion.minor,
            driverProperties.conformanceVersion.subminor, driverProperties.conformanceVersion.patch);
    }
}
#endif

#if defined(__ANDROID__) && !defined(LO_VIDEO_SUBMISSION_UNIT)
// A driver the renderer cannot use fails the same way on every start. When the
// native main returns, the activity shows this reason (on the GPU driver page
// on Qualcomm devices, in a dialog elsewhere) instead of closing (#185).
static void ReportGraphicsFailureToActivity(const std::string& reason)
{
    auto* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    auto activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if (!env || !activity) return;
    jclass type = env->GetObjectClass(activity);
    jmethodID method = type ? env->GetMethodID(type, "reportGraphicsFailure", "(Ljava/lang/String;)V") : nullptr;
    if (method) {
        jstring text = env->NewStringUTF(reason.c_str());
        if (text) {
            env->CallVoidMethod(activity, method, text);
            env->DeleteLocalRef(text);
        }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (type) env->DeleteLocalRef(type);
    env->DeleteLocalRef(activity);
}
#endif


#ifdef LO_GPU_PLUME
namespace plume
{
    // Defined in plume_d3d12.cpp / plume_vulkan.cpp but not exported by a header.
#ifdef _WIN32
    std::unique_ptr<RenderInterface> CreateD3D12Interface();
    std::unique_ptr<RenderInterface> CreateVulkanInterface();
#else
    std::unique_ptr<RenderInterface> CreateVulkanInterface(RenderWindow sdlWindow);
#endif
}
#endif

namespace gpu::video
{
    namespace
    {
        constexpr uint32_t kMaxWidth = 1920;
        constexpr uint32_t kMaxHeight = 1080;

#if !defined(LO_VIDEO_SUBMISSION_UNIT)
        SDL_Window* g_window = nullptr;
#if defined(__ANDROID__)
        std::atomic<uintptr_t> g_androidNativeWindowIdentity{0};
        std::atomic<uint64_t> g_androidSurfaceChangeSerial{0};
        std::atomic<bool> g_androidSurfaceReady{false};
        bool g_androidWasBackgrounded = false; // SDL event thread only.

        uintptr_t CurrentAndroidNativeWindow()
        {
            if (!g_window) return 0;
            SDL_SysWMinfo info{};
            SDL_VERSION(&info.version);
            if (!SDL_GetWindowWMInfo(g_window, &info) || info.subsystem != SDL_SYSWM_ANDROID) return 0;
            return reinterpret_cast<uintptr_t>(info.info.android.window);
        }
#endif
#if LO_PLATFORM_MACOS
        // Main-thread owned; the swap chain presents to the view's CAMetalLayer.
        SDL_MetalView g_metalView = nullptr;
        void* g_cocoaWindow = nullptr;
        void* g_metalLayer = nullptr;
#endif
        std::atomic<uint32_t> g_displayRefreshHz{0}; // Window thread -> presentation thread.
        std::chrono::steady_clock::time_point g_nextRefreshPoll{};
        std::atomic<bool> g_exitRequested{false};
        bool g_videoSubsystemOwned = false;
        constexpr auto kCursorIdleTimeout = std::chrono::milliseconds(2000);
        std::chrono::steady_clock::time_point g_lastPointerActivity{};
        bool g_cursorHidden = false;
        bool g_cursorManaged = false;

        void SetGameCursorHidden(bool hidden)
        {
            if (!g_cursorManaged || g_cursorHidden == hidden) return;
            SDL_ShowCursor(hidden ? SDL_DISABLE : SDL_ENABLE);
            g_cursorHidden = hidden;
        }

        // Pair only this lifecycle's reference, on its window-owning thread.
        // HID or other SDL clients retain their independent subsystem references.
        void DestroyWindowResources()
        {
            if (g_cursorManaged) {
                SDL_ShowCursor(SDL_ENABLE);
                g_cursorManaged = false;
                g_cursorHidden = false;
            }
#if LO_PLATFORM_MACOS
            if (g_metalView) { SDL_Metal_DestroyView(g_metalView); g_metalView = nullptr; }
            g_cocoaWindow = g_metalLayer = nullptr;
#endif
            if (g_window) { SDL_DestroyWindow(g_window); g_window = nullptr; }
#if defined(__ANDROID__)
            g_androidNativeWindowIdentity = 0;
            g_androidSurfaceChangeSerial = 0;
            g_androidSurfaceReady = false;
            g_androidWasBackgrounded = false;
#endif
            g_displayRefreshHz = 0;
            g_nextRefreshPoll = {};
            if (g_videoSubsystemOwned) {
                SDL_QuitSubSystem(SDL_INIT_VIDEO);
                g_videoSubsystemOwned = false;
            }
        }
#endif
        std::atomic<uint64_t> g_shaderProgress{0};
        std::atomic<bool> g_vulkan{false};
        std::atomic<bool> g_metal{false};
        std::atomic<uint64_t> g_deviceEpoch{0};
        constexpr uint64_t kProgressMask = (1ull << 28) - 1;
#if !defined(LO_VIDEO_SUBMISSION_UNIT)
        const wchar_t* PreparationTitle(PreparationStage stage) {
            switch (stage) {
            case PreparationStage::CacheValidation: return L"Validating shader cache";
            case PreparationStage::CachedShaders: return L"Loading cached shaders";
            case PreparationStage::IndexedExtraction: return L"Extracting indexed shaders";
            case PreparationStage::FallbackScan: return L"Scanning game resources";
            case PreparationStage::Pipelines: return L"Preparing pipelines";
            default: return L"Preparing shaders";
            }
        }
        const char* PreparationTitleNarrow(PreparationStage stage) {
            switch (stage) {
            case PreparationStage::CacheValidation: return "Validating shader cache";
            case PreparationStage::CachedShaders: return "Loading cached shaders";
            case PreparationStage::IndexedExtraction: return "Extracting indexed shaders";
            case PreparationStage::FallbackScan: return "Scanning game resources";
            case PreparationStage::Pipelines: return "Preparing pipelines";
            default: return "Preparing shaders";
            }
        }
        const wchar_t* PreparationSuffix(PreparationUnit unit) {
            switch (unit) {
            case PreparationUnit::Files: return L" files";
            case PreparationUnit::MiB: return L" MiB";
            case PreparationUnit::Entries: return L" entries";
            case PreparationUnit::Pipelines: return L" pipelines";
            default: return L" shaders";
            }
        }
        const char* PreparationSuffixNarrow(PreparationUnit unit) {
            switch (unit) {
            case PreparationUnit::Files: return " files";
            case PreparationUnit::MiB: return " MiB";
            case PreparationUnit::Entries: return " entries";
            case PreparationUnit::Pipelines: return " pipelines";
            default: return " shaders";
            }
        }
        std::atomic<int> g_displayMode{-1};
        std::atomic<bool> g_displayFailed{false},g_reapplyWindow{false};
        std::atomic<bool> g_windowResizeRequested{false};
        std::atomic<uint64_t> g_settingsDisplayEpoch{0};
        std::atomic<bool> g_windowModeOverridden{false};
        uint64_t g_completedPresentCount = 0;
        DisplayChangeTracker g_displayChanges;
        struct WindowDisplayState {
            settings::Config applied;
            bool initialized = false;
            uint64_t settingsEpoch = 0, shortcutTicket = 0;
            std::optional<settings::WindowMode> shortcutMode, shortcutPrevious;
            window_mode::Placement placement;
            SDL_Scancode consumedKey = SDL_SCANCODE_UNKNOWN;
        } g_windowDisplay;
        std::vector<uint32_t> g_menuPixels;
        uint64_t g_menuRevision=0;
#ifdef _WIN32
        std::jthread g_windowThread;
        HWND g_nativeWindow = nullptr;
        HWND g_preparationWindow = nullptr;
        LRESULT CALLBACK PreparationWindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
        {
            if (message == WM_ERASEBKGND) return 1;
            if (message == WM_KEYDOWN) {
                if (wparam == VK_ESCAPE || wparam == VK_SPACE || wparam == 'B' || wparam == 'b') {
                    RequestSkipShaderPreparation();
                    return 0;
                }
            }
            if (message == WM_PAINT || message == WM_PRINTCLIENT) {
                PAINTSTRUCT paint{};
                HDC target=message == WM_PRINTCLIENT ? reinterpret_cast<HDC>(wparam) : BeginPaint(window,&paint);
                RECT bounds{}; GetClientRect(window,&bounds);
                // Compose the complete frame offscreen. Clearing the visible DC
                // first exposes a blank text area between GDI drawing batches.
                HDC dc=CreateCompatibleDC(target);
                HBITMAP bitmap=CreateCompatibleBitmap(target,std::max(1L,bounds.right),std::max(1L,bounds.bottom));
                if (!dc || !bitmap) {
                    if (bitmap) DeleteObject(bitmap);
                    if (dc) DeleteDC(dc);
                    if (message == WM_PAINT) EndPaint(window,&paint);
                    return 0;
                }
                auto oldBitmap=SelectObject(dc,bitmap);
                HBRUSH background=CreateSolidBrush(RGB(20,24,31));
                FillRect(dc,&bounds,background); DeleteObject(background);
                const uint64_t state=g_shaderProgress.load();
                const uint32_t total=uint32_t((state>>28)&kProgressMask), done=uint32_t(state&kProgressMask);
                const auto stage=PreparationStage((state>>56)&15);
                const auto unit=PreparationUnit(state>>60);
                SetBkMode(dc,TRANSPARENT); SetTextColor(dc,RGB(235,238,242));
                HFONT font=CreateFontW(-28,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
                auto old=SelectObject(dc,font);
                RECT title{20,bounds.bottom/2-85,bounds.right-20,bounds.bottom/2-35};
                DrawTextW(dc,PreparationTitle(stage),-1,&title,DT_CENTER|DT_SINGLELINE);
                SelectObject(dc,GetStockObject(DEFAULT_GUI_FONT));
                const auto detail=std::to_wstring(done)+L" / "+std::to_wstring(total)+PreparationSuffix(unit);
                RECT count{20,bounds.bottom/2-35,bounds.right-20,bounds.bottom/2};
                DrawTextW(dc,detail.c_str(),-1,&count,DT_CENTER|DT_SINGLELINE);
                const int width=std::min(480,std::max(0,int(bounds.right)-80));
                RECT bar{(bounds.right-width)/2,bounds.bottom/2+8,(bounds.right+width)/2,bounds.bottom/2+14};
                HBRUSH track=CreateSolidBrush(RGB(51,58,70)); FillRect(dc,&bar,track); DeleteObject(track);
                bar.right=bar.left+int(total ? uint64_t(width)*std::min(done,total)/total : 0);
                HBRUSH fill=CreateSolidBrush(RGB(111,177,218)); FillRect(dc,&bar,fill); DeleteObject(fill);
                SetTextColor(dc,RGB(157,168,184));
                RECT hint{20,bounds.bottom/2+45,bounds.right-20,bounds.bottom/2+120};
                DrawTextW(dc,L"The game will continue automatically.\nFuture launches reuse the shader cache.\nPress ESC, Space, or Controller (B) to skip.",-1,&hint,DT_CENTER);
                SelectObject(dc,old); DeleteObject(font);
                BitBlt(target,0,0,bounds.right,bounds.bottom,dc,0,0,SRCCOPY);
                SelectObject(dc,oldBitmap); DeleteObject(bitmap); DeleteDC(dc);
                if (message == WM_PAINT) EndPaint(window,&paint);
                return 0;
            }
            return DefWindowProcW(window,message,wparam,lparam);
        }
#endif
        void PumpWindowEvents();
#endif
        bool g_initAttempted = false;
        bool g_available = false;
        bool g_initializing = false;
        std::atomic<int> g_selectedBackend{-1};

#if !defined(LO_VIDEO_SUBMISSION_UNIT)
        std::vector<uint32_t> g_pixels;   // last untiled frame, R8G8B8A8
        uint32_t g_frameWidth = 0, g_frameHeight = 0;
        bool g_frameOnGpu = false;        // last frame came straight from a resolved surface
        uint32_t g_frontbufferPhysical = 0;
#endif

#ifdef LO_GPU_PLUME
        // This outlives g_interface because Plume retains the copied hook
        // userdata until VulkanInterface destruction.
#if !defined(LO_VIDEO_SUBMISSION_UNIT)
        std::unique_ptr<dlss::Controller> g_dlssController;
#if defined(LO_ENABLE_D3D12_FG) && defined(_WIN32)
        std::unique_ptr<frame_generation::D3D12Bridge> g_d3dFg;
#endif
#if (defined(_WIN32) && (defined(LO_ENABLE_D3D12_FG) || defined(LO_ENABLE_STREAMLINE_FG) || defined(LO_ENABLE_VULKAN_FSR_FG))) || (defined(__APPLE__) && defined(LO_ENABLE_METALFX_FG))
        std::mutex g_fgSettingsMutex;
        framegen::Config g_fgAppliedConfig{};
        framegen::Provider g_fgSessionProvider = framegen::Provider::Off;
        std::optional<framegen::Config> g_fgFailedRequest;
        // Vulkan provider whose startup failed on this device; kept across
        // settings changes so returning to it is not offered as a restart.
        framegen::Provider g_fgStartupFailure = framegen::Provider::Off;
#endif
#if defined(LO_ENABLE_STREAMLINE_FG) && defined(_WIN32)
        std::unique_ptr<dlss_fg::Runtime> g_fgRuntime;
        std::unique_ptr<dlss_fg::VulkanDispatch> g_fgDispatch;
        std::unique_ptr<dlss_fg::Session> g_fgSession;
#endif
#if defined(LO_ENABLE_VULKAN_FSR_FG) && defined(_WIN32)
        std::unique_ptr<fsr_fg::Session> g_fsrVulkanFg;
#endif
#if (defined(_WIN32) && (defined(LO_ENABLE_STREAMLINE_FG) || defined(LO_ENABLE_D3D12_FG) || defined(LO_ENABLE_VULKAN_FSR_FG))) || (defined(__APPLE__) && defined(LO_ENABLE_METALFX_FG))
        std::atomic<bool> g_fgWindowSynchronization{false};
        std::atomic<int> g_fgWindowChange{0}; // 0 idle, 1 requested, 2 GPU quiescent
#endif
#if defined(__APPLE__) && defined(LO_ENABLE_METALFX_FG)
        std::unique_ptr<metalfx_fg::Session> g_metalFg;
#endif
        std::unique_ptr<TemporalUpscaler> g_temporalUpscaler;
#endif
        std::unique_ptr<plume::RenderInterface> g_interface;
        std::unique_ptr<plume::RenderDevice> g_device;
        std::unique_ptr<plume::RenderCommandQueue> g_queue;
        std::unique_ptr<plume::RenderCommandList> g_commandList;
        std::unique_ptr<plume::RenderCommandFence> g_fence;
        std::unique_ptr<plume::RenderCommandSemaphore> g_acquireSemaphore;
        std::unique_ptr<plume::RenderCommandSemaphore> g_releaseSemaphore;
        std::vector<std::unique_ptr<plume::RenderCommandSemaphore>> g_presentSemaphores;
        std::unique_ptr<plume::RenderSwapChain> g_swapChain;
        std::unique_ptr<plume::RenderBuffer> g_uploadBuffer;
        uint64_t g_uploadCapacity = uint64_t(kMaxWidth) * kMaxHeight * 4;
#if !defined(LO_VIDEO_SUBMISSION_UNIT)
        std::unique_ptr<Presentation> g_presentation;
        bool g_hdrSwapchain = false;
        // An HDR choice whose swap chain could not be created; cleared when the
        // setting changes.
        std::optional<bool> g_hdrSwapchainRefused;
        bool g_hdrSceneEnabled = false;
        float g_hdrPaperWhiteNits = 203.0f, g_hdrPeakNits = 1000.0f;
        hdr::OutputTransform g_hdrOutput;
        plume::RenderFormat g_presentationFormat = plume::RenderFormat::UNKNOWN;
        std::optional<bool> g_hdrReportedActive;
        FgPresentBridge g_fgPresent;
        uint64_t g_fgPresentSerial = 0;
#endif
        std::unique_ptr<plume::RenderTexture> g_cpuFrame;
        std::unique_ptr<plume::RenderTexture> g_presentedSnapshot;
        uint32_t g_snapshotWidth=0,g_snapshotHeight=0;
        plume::RenderFormat g_snapshotFormat=plume::RenderFormat::UNKNOWN;
        float g_snapshotOutputScale = 1.0f;
        // Last 3D game scene for the HDR and brightness previews. The game
        // resolves every frame to one frontbuffer, so the scene is gone once
        // its own menu draws. Gameplay frames refresh two copies in turn every
        // 250 ms (FP16 extended gamma when available, otherwise SDR); the first
        // frame without the scene, or a host menu, freezes the older one: the
        // newer can hold the frame where the menu blurs the scene.
        struct HdrCalibrationCache {
            std::unique_ptr<plume::RenderTexture> copies[2];
            bool filled[2] = {};
            uint32_t next = 0; // the copy refreshed next, the older one
            uint32_t width = 0, height = 0;
            std::chrono::steady_clock::time_point copied{};
            plume::RenderTexture* scene = nullptr; // frozen copy
            bool ready = false, extended = false;
        } g_hdrCalibrationCache;
        uint32_t g_cpuWidth=0,g_cpuHeight=0;
        uint32_t g_lastPresentedImage=0;
        bool g_hasPresentedImage=false;
        bool g_presentPending=false;
        bool g_forceSwapResize=false;
#if defined(__ANDROID__)
        uint64_t g_androidAppliedSurfaceChangeSerial = 0; // Presentation thread only.
        bool g_androidSurfaceRebuildFailureLogged = false;
#endif
        struct PresentCaptureCopy {
            bool queued = false;
            std::string failure;
            gpu::present_capture::Ticket ticket{};
            std::unique_ptr<plume::RenderBuffer> buffer;
            uint32_t width = 0, height = 0, pitch = 0;
            plume::RenderFormat format = plume::RenderFormat::R8G8B8A8_UNORM;
            float outputScale = 1.0f;
            bool srgbPreview = false;
            uint64_t d3dFenceValue = 0;
        };
        PresentCaptureCopy g_captureCopy;
        std::vector<std::unique_ptr<plume::RenderBuffer>> g_captureRetained;
        uint64_t g_captureAllocs = 0, g_captureCopies = 0, g_captureMaps = 0;
        bool g_captureCompletionFault = false;
        submission::VulkanState g_submissionState;
        // Null in production. A submission test can attach a real queue and make
        // the next reset/wait return before vkQueueSubmit or vkWaitForFences.
        plume::RenderCommandQueue* g_probeQueue = nullptr;
        int32_t g_probeSubmitFault = 0;
        int32_t g_probeWaitFault = 0;
        plume::RenderCommandQueue* ActiveSubmissionQueue()
        {
            return g_probeQueue ? g_probeQueue : g_queue.get();
        }
        struct PresentationDisplayState {
            uint64_t resizedTicket = 0;
            // Reset with swapchain ownership, not on ordinary resize. Preserve
            // the backend/SDK's original low-rate policy when leaving high FPS.
            bool nativeVsyncInitialized = false;
            bool nativeVsyncBaseline = true, nativeVsyncRequested = true;
            bool nativeVsyncReportPending = false;
            double metalMinimumPresentDuration = 0.0;
        } g_presentationDisplay;
        constexpr plume::RenderFormat kSwapChainFormat = plume::RenderFormat::R8G8B8A8_UNORM;
        constexpr uint32_t kSwapChainBuffers = 3;

        bool SubmitVulkan(const plume::RenderCommandList* const* lists, uint32_t count,
            plume::RenderCommandSemaphore* const* waits, uint32_t waitCount,
            plume::RenderCommandSemaphore* const* signals, uint32_t signalCount,
            plume::RenderCommandFence* fence, uint64_t* serial, int32_t* rawResult)
        {
            if (serial) *serial = 0;
            if (rawResult) *rawResult = submission::VulkanState::InvalidState;
            auto* queueHolder = ActiveSubmissionQueue();
            if (GpuWorkStopped()) {
                if (rawResult) *rawResult = g_submissionState.Failure();
                return false;
            }
            if (!g_vulkan || !queueHolder || !lists || !count || !fence) {
                StopGpuWork(submission::VulkanState::InvalidState); return false;
            }
            auto* queue = static_cast<plume::VulkanCommandQueue*>(queueHolder);
            auto* nativeFence = static_cast<plume::VulkanCommandFence*>(fence);
            std::vector<VkCommandBuffer> commandBuffers;
            std::vector<VkSemaphore> waitSemaphores, signalSemaphores;
            commandBuffers.reserve(count); waitSemaphores.reserve(waitCount); signalSemaphores.reserve(signalCount);
            for (uint32_t i = 0; i < count; ++i) {
                if (!lists[i]) { StopGpuWork(submission::VulkanState::InvalidState); return false; }
                commandBuffers.push_back(static_cast<const plume::VulkanCommandList*>(lists[i])->vk);
            }
            for (uint32_t i = 0; i < waitCount; ++i)
                waitSemaphores.push_back(static_cast<plume::VulkanCommandSemaphore*>(waits[i])->vk);
            for (uint32_t i = 0; i < signalCount; ++i)
                signalSemaphores.push_back(static_cast<plume::VulkanCommandSemaphore*>(signals[i])->vk);
            // Presentation may copy to the acquired image before rendering.
            // There must also be one stage mask for EACH waited semaphore.
            const std::vector<VkPipelineStageFlags> waitStages(waitCount, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submit.commandBufferCount = uint32_t(commandBuffers.size()); submit.pCommandBuffers = commandBuffers.data();
            submit.waitSemaphoreCount = uint32_t(waitSemaphores.size()); submit.pWaitSemaphores = waitSemaphores.data();
            submit.pWaitDstStageMask = waitSemaphores.empty() ? nullptr : waitStages.data();
            submit.signalSemaphoreCount = uint32_t(signalSemaphores.size()); submit.pSignalSemaphores = signalSemaphores.data();
            uint64_t acceptedSerial = 0;
            int32_t result = 0;
            bool submitted;
            {
                const std::scoped_lock lock(*queue->queue->mutex);
                submitted = g_submissionState.SubmitBatch(
                    [&] {
                        if (g_probeSubmitFault) {
                            const int32_t fault = g_probeSubmitFault;
                            g_probeSubmitFault = 0;
                            return fault;
                        }
                        return int32_t(vkResetFences(queue->device->vk, 1, &nativeFence->vk));
                    },
                    [&] { return int32_t(vkQueueSubmit(queue->queue->vk, 1, &submit, nativeFence->vk)); },
                    acceptedSerial, result);
            }
            if (rawResult) *rawResult = result;
            if (serial) *serial = acceptedSerial;
            // SubmitBatch already latched the device. Publish only after the
            // queue mutex is released so a present failure cannot leave the
            // previous DLSS submission looking active.
            if (!submitted) {
                LOG_ERROR("video: Vulkan submission stopped raw_vk={}", result);
                StopGpuWork(result);
            }
            return submitted;
        }

#if LO_PLATFORM_MACOS
        // plume's Metal fence is a counting semaphore: each submission signals it
        // once and each wait consumes one signal. The runtime's waits assume an
        // idempotent fence (a completed Vulkan fence or D3D12 value stays
        // complete), so outstanding signals are counted per fence and a wait
        // consumes exactly those.
        std::mutex g_metalFenceMutex;
        std::unordered_map<const plume::RenderCommandFence*, uint32_t> g_metalFenceSignals;

        void ConsumeMetalFenceSignals(plume::RenderCommandQueue* queue, plume::RenderCommandFence* fence)
        {
            uint32_t pending = 0;
            {
                const std::scoped_lock lock(g_metalFenceMutex);
                if (const auto it = g_metalFenceSignals.find(fence); it != g_metalFenceSignals.end())
                    pending = std::exchange(it->second, 0);
            }
            for (; pending; --pending)
                queue->waitForCommandFence(fence);
        }

        // Metal reports command buffer errors asynchronously, never at submit, so
        // an accepted submission always succeeds here.
        bool SubmitMetal(const plume::RenderCommandList* const* lists, uint32_t count,
            plume::RenderCommandSemaphore* const* waits, uint32_t waitCount,
            plume::RenderCommandSemaphore* const* signals, uint32_t signalCount,
            plume::RenderCommandFence* fence, uint64_t* serial, int32_t* rawResult)
        {
            if (serial) *serial = 0;
            if (rawResult) *rawResult = submission::VulkanState::InvalidState;
            auto* queue = ActiveSubmissionQueue();
            if (GpuWorkStopped()) {
                if (rawResult) *rawResult = g_submissionState.Failure();
                return false;
            }
            if (!g_metal || !queue || !lists || !count) {
                StopGpuWork(submission::VulkanState::InvalidState); return false;
            }
            for (uint32_t i = 0; i < count; ++i)
                if (!lists[i]) { StopGpuWork(submission::VulkanState::InvalidState); return false; }
            uint64_t acceptedSerial = 0;
            int32_t result = 0;
            const bool submitted = g_submissionState.SubmitBatch(
                [] { return submission::VulkanState::Success; },
                [&] {
                    queue->executeCommandLists(const_cast<const plume::RenderCommandList**>(lists), count,
                        const_cast<plume::RenderCommandSemaphore**>(waits), waitCount,
                        const_cast<plume::RenderCommandSemaphore**>(signals), signalCount, fence);
                    if (fence) {
                        const std::scoped_lock lock(g_metalFenceMutex);
                        ++g_metalFenceSignals[fence];
                    }
                    return submission::VulkanState::Success;
                },
                acceptedSerial, result);
            if (rawResult) *rawResult = result;
            if (serial) *serial = acceptedSerial;
            if (!submitted) StopGpuWork(result);
            return submitted;
        }
#endif

#ifdef _WIN32
        int32_t D3DResult(HRESULT result) { return FAILED(result) ? int32_t(result) : 0; }

        // ExecuteCommandLists has no result. A later Signal failure leaves its
        // commands potentially in flight, so the caller must retain their uses.
        bool SubmitD3D12(const plume::RenderCommandList* const* lists, uint32_t count,
            plume::RenderCommandSemaphore* const* waits, uint32_t waitCount,
            plume::RenderCommandSemaphore* const* signals, uint32_t signalCount,
            plume::RenderCommandFence* fence, uint64_t* serial, int32_t* rawResult,
            bool* executionMayBeInFlight)
        {
            if (serial) *serial = 0;
            if (rawResult) *rawResult = submission::VulkanState::InvalidState;
            if (executionMayBeInFlight) *executionMayBeInFlight = false;
            auto* holder = ActiveSubmissionQueue();
            if (GpuWorkStopped()) {
                if (rawResult) *rawResult = g_submissionState.Failure();
                return false;
            }
            if (g_vulkan || !holder || !lists || !count || !fence) {
                StopGpuWork(submission::VulkanState::InvalidState);
                return false;
            }
            auto* queue = static_cast<plume::D3D12CommandQueue*>(holder);
            auto* nativeFence = static_cast<plume::D3D12CommandFence*>(fence);
            if (!queue->d3d || !queue->device || !queue->device->d3d ||
                !nativeFence->d3d || !nativeFence->fenceEvent ||
                nativeFence->fenceValue == UINT64_MAX ||
                (waitCount && !waits) || (signalCount && !signals)) {
                StopGpuWork(submission::VulkanState::InvalidState);
                return false;
            }
            std::vector<ID3D12CommandList*> nativeLists;
            nativeLists.reserve(count);
            for (uint32_t i = 0; i < count; ++i) {
                if (!lists[i]) { StopGpuWork(submission::VulkanState::InvalidState); return false; }
                auto* list = static_cast<const plume::D3D12CommandList*>(lists[i]);
                if (!list->d3d || list->open) { StopGpuWork(submission::VulkanState::InvalidState); return false; }
                nativeLists.push_back(list->d3d);
            }
            for (uint32_t i = 0; i < waitCount; ++i) {
                if (!waits[i] || !static_cast<plume::D3D12CommandSemaphore*>(waits[i])->d3d) {
                    StopGpuWork(submission::VulkanState::InvalidState); return false;
                }
            }
            for (uint32_t i = 0; i < signalCount; ++i) {
                if (!signals[i] || !static_cast<plume::D3D12CommandSemaphore*>(signals[i])->d3d ||
                    static_cast<plume::D3D12CommandSemaphore*>(signals[i])->semaphoreValue == UINT64_MAX) {
                    StopGpuWork(submission::VulkanState::InvalidState); return false;
                }
            }
            uint64_t acceptedSerial = 0;
            int32_t result = 0;
            bool executed = false;
            const bool submitted = g_submissionState.SubmitBatch(
                [] { return 0; },
                [&] {
                    for (uint32_t i = 0; i < waitCount; ++i) {
                        auto* semaphore = static_cast<plume::D3D12CommandSemaphore*>(waits[i]);
                        const HRESULT hr = queue->d3d->Wait(semaphore->d3d, semaphore->semaphoreValue);
                        if (FAILED(hr)) return D3DResult(hr);
                    }
                    queue->d3d->ExecuteCommandLists(UINT(nativeLists.size()), nativeLists.data());
                    executed = true;
                    for (uint32_t i = 0; i < signalCount; ++i) {
                        auto* semaphore = static_cast<plume::D3D12CommandSemaphore*>(signals[i]);
                        const HRESULT hr = queue->d3d->Signal(semaphore->d3d, semaphore->semaphoreValue + 1);
                        if (FAILED(hr)) return D3DResult(hr);
                        ++semaphore->semaphoreValue;
                    }
                    const HRESULT signal = queue->d3d->Signal(nativeFence->d3d, nativeFence->fenceValue);
                    if (FAILED(signal)) return D3DResult(signal);
                    const HRESULT event = nativeFence->d3d->SetEventOnCompletion(nativeFence->fenceValue, nativeFence->fenceEvent);
                    ++nativeFence->fenceValue;
                    if (FAILED(event)) return D3DResult(event);
                    return D3DResult(queue->device->d3d->GetDeviceRemovedReason());
                }, acceptedSerial, result);
            if (rawResult) *rawResult = result;
            if (serial) *serial = acceptedSerial;
            if (!submitted) {
                if (executionMayBeInFlight) *executionMayBeInFlight = executed;
                LOG_ERROR("video: D3D12 submission stopped raw_hr={} commands_may_be_in_flight={}", result, executed);
                StopGpuWork(result);
            }
            return submitted;
        }
#endif

        bool SubmitPresentationBatch(const plume::RenderCommandList* const* lists, uint32_t count,
            plume::RenderCommandSemaphore* const* waits, uint32_t waitCount,
            plume::RenderCommandSemaphore* const* signals, uint32_t signalCount,
            plume::RenderCommandFence* fence, uint64_t* serial, int32_t* rawResult)
        {
#if LO_PLATFORM_MACOS
            if (g_metal)
                return SubmitMetal(lists, count, waits, waitCount, signals, signalCount,
                    fence, serial, rawResult);
#endif
            if (g_vulkan)
                return SubmitVulkan(lists, count, waits, waitCount, signals, signalCount,
                    fence, serial, rawResult);
#ifdef _WIN32
            return SubmitD3D12(lists, count, waits, waitCount, signals, signalCount,
                fence, serial, rawResult, nullptr);
#else
            StopGpuWork(submission::VulkanState::InvalidState);
            return false;
#endif
        }

#if !defined(LO_VIDEO_SUBMISSION_UNIT)
        std::filesystem::path DlssApplicationDataPath()
        {
            return os::user_paths::UsePortableLayout() ? std::filesystem::path("cache") / "ngx"
                : os::user_paths::DataDir() / "cache" / "ngx";
        }

        // NGX, Streamline and FidelityFX libraries are searched from the executable
        // (see os/runtime_libraries.h); LO_DLSS_RUNTIME_PATH overrides the search.
        std::filesystem::path RuntimeDirectory(std::string_view library)
        {
            const char* override = std::getenv("LO_DLSS_RUNTIME_PATH");
            if (override && *override) return std::filesystem::path(override);
            std::error_code error;
            return os::runtime_libraries::Find(library, os::user_paths::ExecutableDir(),
                std::filesystem::current_path(error));
        }

        std::filesystem::path DlssRuntimePath()
        {
#ifdef _WIN32
            return RuntimeDirectory("nvngx_dlss.dll");
#else
            return RuntimeDirectory("libnvidia-ngx-dlss.so");
#endif
        }
        [[maybe_unused]] std::filesystem::path StreamlineRuntimePath() { return RuntimeDirectory("sl.interposer.dll"); }
        [[maybe_unused]] std::filesystem::path FidelityFxRuntime(const char* overrideVariable, std::string_view library)
        {
            const char* override = std::getenv(overrideVariable);
            return override && *override ? std::filesystem::path(override) : RuntimeDirectory(library) / library;
        }
        // Directory with libxess_fg.dll and libxell.dll; LO_XESS_FG_RUNTIME_PATH overrides.
        [[maybe_unused]] std::filesystem::path XessFgRuntimeDirectory()
        {
            // Like XeSS SR, independent of LO_DLSS_RUNTIME_PATH.
            const char* override = std::getenv("LO_XESS_FG_RUNTIME_PATH");
            if (override && *override) return std::filesystem::path(override);
            std::error_code error;
            return os::runtime_libraries::Find("libxess_fg.dll", os::user_paths::ExecutableDir(),
                std::filesystem::current_path(error));
        }
        // Each D3D12 FG adapter's runtime: a file for FidelityFX, a directory otherwise.
        [[maybe_unused]] std::filesystem::path D3D12FgRuntime(framegen::Provider provider)
        {
            if (provider == framegen::Provider::Fsr) return FidelityFxRuntime("LO_FSR_FG_RUNTIME", "amd_fidelityfx_dx12.dll");
            if (provider == framegen::Provider::Xess) return XessFgRuntimeDirectory();
            return StreamlineRuntimePath();
        }

        // One parser for every Vulkan FG decision, so device features, SDK
        // sessions, reconciliation and status always see the same request.
        [[maybe_unused]] framegen::EnvironmentSelection VulkanFgRequest()
        {
            return frame_generation::ResolveVulkanSelection(settings::GetConfig(), std::getenv("LO_FG_PROVIDER"),
                std::getenv("LO_FG_MODE"), std::getenv("LO_FG_MULTIPLIER"),
                std::getenv("LO_FG_TARGET_FPS"), std::getenv("LO_DLSS_FG"));
        }
        [[maybe_unused]] framegen::EnvironmentSelection MetalFgRequest()
        {
            return frame_generation::ResolveSelection(backend::Backend::Metal, settings::GetConfig(),
                std::getenv("LO_FG_PROVIDER"), std::getenv("LO_FG_MODE"), std::getenv("LO_FG_MULTIPLIER"),
                std::getenv("LO_FG_TARGET_FPS"), std::getenv("LO_DLSS_FG"));
        }

        // Frame generation presents through each SDK's own swap chain. D3D12
        // and Metal recreate that chain in SDR, and the Vulkan FSR presenter
        // only takes RGBA8/BGRA8. Vulkan DLSS-G accepts the HDR10 chain plume
        // prefers (Streamline DLSS-G guide 11.0); LO_HDR_FG=0 opts out.
        bool HdrFrameGenerationCompatible()
        {
            const auto fg = frame_generation::ResolveSelection(g_metal ? backend::Backend::Metal :
                g_vulkan ? backend::Backend::Vulkan : backend::Backend::D3D12,
                settings::GetConfig(), std::getenv("LO_FG_PROVIDER"), std::getenv("LO_FG_MODE"),
                std::getenv("LO_FG_MULTIPLIER"), std::getenv("LO_FG_TARGET_FPS"), std::getenv("LO_DLSS_FG"));
            if (!fg.Enabled()) return true;
            if (!g_vulkan || fg.config.provider != framegen::Provider::Dlss) return false;
            const char* optOut = std::getenv("LO_HDR_FG");
            return !optOut || std::strcmp(optOut, "0") != 0;
        }

        // Per-frame scene conditions. AA runs on the FP16 scene and upscaling
        // (DLSS/FSR/MetalFX) keeps the pre-upscale scene for presentation's
        // highlight gain, so only frame generation can still hold HDR back.
        bool HdrConfigurationCompatible()
        {
            return HdrFrameGenerationCompatible();
        }

        // The swap chain for the current HDR choice: FP16 linear output when
        // g_hdrSwapchain, which plume presents as scRGB, HDR10 or EDR.
        plume::RenderSwapChainDesc SwapChainDescription()
        {
#ifdef _WIN32
            plume::RenderSwapChainDesc desc(g_nativeWindow, kSwapChainFormat, kSwapChainBuffers);
#elif LO_PLATFORM_MACOS
            plume::RenderSwapChainDesc desc(plume::RenderWindow{ g_cocoaWindow, g_metalLayer }, kSwapChainFormat, kSwapChainBuffers);
#else
            plume::RenderSwapChainDesc desc(g_window, kSwapChainFormat, kSwapChainBuffers);
#endif
            if (g_hdrSwapchain) {
                desc.format = plume::RenderFormat::R16G16B16A16_FLOAT;
                desc.outputMode = plume::RenderOutputMode::HDR_LINEAR;
            }
            return desc;
        }

        void UpdateHdrOutput(bool refreshDisplay = false)
        {
            if (!g_swapChain || !g_presentation) return;
            // D3D12 walks every adapter output to answer this (about half a
            // millisecond), so poll once a second and on swap chain changes.
            static plume::RenderDisplayState display;
            static std::chrono::steady_clock::time_point polled;
            const auto now = std::chrono::steady_clock::now();
            if (refreshDisplay || polled == std::chrono::steady_clock::time_point{} || now - polled >= std::chrono::seconds(1)) {
                display = g_swapChain->getDisplayState();
                polled = now;
            }
            const bool linear = display.encoding != plume::RenderOutputEncoding::SDR;
            const bool active = display.hdrActive || display.hdrTransport;
            auto calibration = settings::GetHdrCalibration();
            const bool relative = display.encoding == plume::RenderOutputEncoding::EDR;
            const float reportedPeak = relative && active ?
                float(calibration.paperWhiteNits) * display.headroom : display.peakNits;
            const uint32_t detected = std::isfinite(reportedPeak) && reportedPeak >= 80.0f && reportedPeak <= 10000.0f ?
                uint32_t(std::lround(reportedPeak)) : 0u;
            settings::SetHdrDisplayInfo({active, detected, relative});
            calibration = settings::GetHdrCalibration();
            g_hdrPaperWhiteNits = float(calibration.paperWhiteNits);
            g_hdrPeakNits = float(calibration.effectiveNits);
            g_hdrOutput = hdr::MakeOutput(linear, display.encoding == plume::RenderOutputEncoding::EDR,
                active, g_hdrPaperWhiteNits, g_hdrPeakNits, display.encoding == plume::RenderOutputEncoding::HDR10_PQ);
            g_presentation->SetOutputTransform(g_hdrOutput);
            g_hdrSceneEnabled = g_hdrSwapchain && g_hdrOutput.active && HdrConfigurationCompatible();
            renderer::SetHdrSceneEnabled(g_hdrSceneEnabled);
            if (g_hdrSwapchain && g_hdrReportedActive != g_hdrSceneEnabled) {
                LOG_INFO("HDR: scene_enabled={} display_active={} display_state_known={} transport={} encoding={} paper_white={} peak={} scale={} ratio={}",
                    g_hdrSceneEnabled, display.hdrActive, display.hdrStateKnown, display.hdrTransport, uint32_t(display.encoding),
                    g_hdrPaperWhiteNits, g_hdrPeakNits, g_hdrOutput.scale, g_hdrOutput.peakRatio);
                g_hdrReportedActive = g_hdrSceneEnabled;
            }
        }

        void LogDlssProbe(const dlss::ProbeReport& report)
        {
            LOG_INFO("DLSS P0: state={} reason='{}' sdk={} runtime='{}' device='{}' vendor={:#x} device_id={:#x} driver={} driver_raw={:#x} sr_flags={} sr_implemented={} sr_evaluated={} fg=not_probed/not_implemented",
                dlss::ProbeStateName(report.state), report.reason, report.sdkVersion, report.runtimePath,
                report.deviceName, report.vendorId, report.deviceId, report.driverVersionText, report.driverVersion,
                report.featureSupport.value_or(UINT32_MAX), report.srImplemented, report.srEvaluated);
            for (const auto& call : report.calls)
                LOG_INFO("DLSS P0: {} raw={}", call.name, call.result);
            for (const auto& optimal : report.optimalSettings)
                LOG_INFO("DLSS P0: {} optimal={}x{} min={}x{} max={}x{} sharpness={} raw={}", optimal.quality,
                    optimal.optimalWidth, optimal.optimalHeight, optimal.minWidth, optimal.minHeight,
                    optimal.maxWidth, optimal.maxHeight, optimal.sharpness, optimal.result.value_or(0));
        }

        // The presentation fence completed. FG inputs recorded into that batch
        // retire here; every wait that clears g_presentPending must call this,
        // or the next FG recording would find its previous lease still open.
        void PresentationFenceCompleted()
        {
            g_presentPending = false;
#if defined(LO_ENABLE_VULKAN_FSR_FG) && defined(_WIN32)
            if (g_fsrVulkanFg) g_fsrVulkanFg->AfterHostDrain();
#endif
#if defined(__APPLE__) && defined(LO_ENABLE_METALFX_FG)
            if (g_metalFg) g_metalFg->AfterHostDrain();
#endif
        }

        bool WaitForPresentGpuImpl()
        {
            if (g_presentPending) {
                if (!g_queue || !g_fence || !WaitForGpuFence(g_fence.get())) {
                    g_fgPresent.WaitFailed();
                    return false;
                }
                const auto completed = g_fgPresent.Completed(g_deviceEpoch.load(), g_fgPresentSerial);
                if (completed) LOG_INFO("video: FG diagnostic completed={} present_serial={} provider_ready=0 ui=unavailable",
                    completed, g_fgPresentSerial);
                PresentationFenceCompleted();
            }
            return !GpuWorkStopped();
        }

        void LogOutputPixels(const char* reason)
        {
#ifdef _WIN32
            RECT raw{};
            GetClientRect(g_nativeWindow, &raw);
            uint32_t physicalWidth = 0, physicalHeight = 0;
            plume::GetWindowClientPixels(g_nativeWindow, physicalWidth, physicalHeight);
            LOG_INFO("video output: {} thread={} awareness={} dpi={} raw={}x{} physical={}x{} swapchain={}x{} mode={}",
                reason, GetCurrentThreadId(), int(GetAwarenessFromDpiAwarenessContext(GetThreadDpiAwarenessContext())),
                GetDpiForWindow(g_nativeWindow), raw.right - raw.left, raw.bottom - raw.top,
                physicalWidth, physicalHeight, g_swapChain->getWidth(), g_swapChain->getHeight(), g_displayMode.load());
#else
            LOG_INFO("video output: {} swapchain={}x{} mode={}", reason, g_swapChain->getWidth(), g_swapChain->getHeight(), g_displayMode.load());
#endif
        }

        plume::RenderCommandSemaphore* PresentSemaphore(uint32_t imageIndex)
        {
            if(!g_vulkan) return g_releaseSemaphore.get();
            // A submit fence does not prove vkQueuePresent consumed its wait.
            // Reacquiring this image does; index present semaphores by image.
            while(g_presentSemaphores.size()<=imageIndex)
                g_presentSemaphores.push_back(g_device->createCommandSemaphore());
            return g_presentSemaphores[imageIndex].get();
        }

        void RecordPresentedSnapshot(plume::RenderTexture* frame)
        {
            // Vulkan gives the presentation engine ownership after vkQueuePresent.
            // The explicit presented-screenshot diagnostic retains an owned image
            // before that handoff; no extra copy is made during normal gameplay.
            if(!g_vulkan || !getenv("LO_SCREENSHOT_PRESENTED")) return;
            const auto w=g_swapChain->getWidth(),h=g_swapChain->getHeight();
            const auto format=g_swapChain->getFormat();
            if(!g_presentedSnapshot || w!=g_snapshotWidth || h!=g_snapshotHeight || format!=g_snapshotFormat) {
                g_presentedSnapshot=g_device->createTexture(plume::RenderTextureDesc::Texture2D(w,h,1,format));
                g_snapshotWidth=w;g_snapshotHeight=h;g_snapshotFormat=format;
            }
            if(!g_presentedSnapshot) return;
            g_commandList->barriers(plume::RenderBarrierStage::COPY,plume::RenderTextureBarrier(frame,plume::RenderTextureLayout::COPY_SOURCE));
            g_commandList->barriers(plume::RenderBarrierStage::COPY,plume::RenderTextureBarrier(g_presentedSnapshot.get(),plume::RenderTextureLayout::COPY_DEST));
            g_commandList->copyTexture(g_presentedSnapshot.get(),frame);
            g_snapshotOutputScale = g_hdrOutput.scale;
        }

        void PreparePresentImage(plume::RenderTexture* image) {
#if defined(LO_ENABLE_VULKAN_FSR_FG) && defined(_WIN32)
            if (g_fsrVulkanFg && g_fsrVulkanFg->UsesProxySwapchain()) {
                // SDK 1.1.4 ReplacementBufferTransferState is SHADER_READ_ONLY,
                // including passthrough while FG is Off. These are SDK images.
                g_commandList->barriers(plume::RenderBarrierStage::ALL,
                    plume::RenderTextureBarrier(image, plume::RenderTextureLayout::SHADER_READ));
                return;
            }
#endif
#if defined(LO_ENABLE_STREAMLINE_FG) && defined(_WIN32)
            if (g_fgSession) {
                // The SL Vulkan proxy is copied by its present worker. This
                // experimental path uses the layout validated by the P0 probe.
                g_commandList->barriers(plume::RenderBarrierStage::COPY,
                    plume::RenderTextureBarrier(image, plume::RenderTextureLayout::COPY_SOURCE));
                return;
            }
#endif
            g_commandList->barriers(plume::RenderBarrierStage::NONE,
                plume::RenderTextureBarrier(image, plume::RenderTextureLayout::PRESENT));
        }

        void FgSubmitStart() {
#if defined(LO_ENABLE_D3D12_FG) && defined(_WIN32)
            if (g_d3dFg) g_d3dFg->SubmitStart();
#endif
#if defined(LO_ENABLE_STREAMLINE_FG) && defined(_WIN32)
            if (g_fgSession) g_fgSession->SubmitStart();
#endif
#if defined(LO_ENABLE_VULKAN_FSR_FG) && defined(_WIN32)
            if (g_fsrVulkanFg) g_fsrVulkanFg->SubmitStart();
#endif
#if defined(__APPLE__) && defined(LO_ENABLE_METALFX_FG)
            if (g_metalFg) g_metalFg->SubmitStart();
#endif
        }
        void FgHostSubmitted(bool success, uint64_t serial, int32_t nativeResult) {
#if defined(LO_ENABLE_D3D12_FG) && defined(_WIN32)
            if (g_d3dFg) g_d3dFg->HostSubmitted(success, serial);
#endif
#if defined(LO_ENABLE_STREAMLINE_FG) && defined(_WIN32)
            if (g_fgSession) g_fgSession->HostSubmitted(success, serial, nativeResult);
#endif
#if defined(LO_ENABLE_VULKAN_FSR_FG) && defined(_WIN32)
            if (g_fsrVulkanFg) g_fsrVulkanFg->HostSubmitted(success, serial);
#endif
#if defined(__APPLE__) && defined(LO_ENABLE_METALFX_FG)
            if (g_metalFg) g_metalFg->HostSubmitted(success, serial);
#endif
        }
        void FgPresentStart() {
#if defined(LO_ENABLE_D3D12_FG) && defined(_WIN32)
            if (g_d3dFg) g_d3dFg->PresentStart();
#endif
#if defined(LO_ENABLE_STREAMLINE_FG) && defined(_WIN32)
            if (g_fgSession) { g_fgSession->SubmitEnd(); g_fgSession->PresentStart(); }
#endif
        }
        void FgPresented(bool accepted) {
#if defined(LO_ENABLE_D3D12_FG) && defined(_WIN32)
            if (g_d3dFg) g_d3dFg->Presented(accepted);
#endif
#if defined(LO_ENABLE_STREAMLINE_FG) && defined(_WIN32)
            if (g_fgSession) g_fgSession->Presented(accepted);
#endif
#if defined(LO_ENABLE_VULKAN_FSR_FG) && defined(_WIN32)
            if (g_fsrVulkanFg) {
                g_fsrVulkanFg->Presented(accepted);
                if (g_fsrVulkanFg->Failed()) {
                    renderer::SetFrameGenerationInputCaptureEnabled(false);
                    // A failed session never recovers in this process. Mark the
                    // current FSR request, not the applied config: that is Off when
                    // the failure came first, and reconciliation would then retry
                    // every frame. Other providers keep their own status.
                    const auto request = VulkanFgRequest().config;
                    if (request.provider == framegen::Provider::Fsr) {
                        std::lock_guard lock(g_fgSettingsMutex);
                        g_fgFailedRequest = request;
                    }
                }
            }
#endif
        }

        void RetainPresentCapture()
        {
            if (g_captureCopy.buffer) g_captureRetained.push_back(std::move(g_captureCopy.buffer));
            g_captureCopy.queued = false;
        }

        void QueuePresentCapture(plume::RenderTexture *frame, const gpu::present_capture::Ticket *ticket)
        {
            if (g_captureCopy.buffer) RetainPresentCapture();
            g_captureCopy = {};
            if (!ticket || !ticket->active || !frame || !g_swapChain || !g_device || !g_commandList) return;
            g_captureCopy.ticket = *ticket;
            g_captureCopy.format = g_swapChain->getFormat();
            g_captureCopy.outputScale = g_hdrOutput.scale;
            g_captureCopy.srgbPreview = g_hdrOutput.linear && !g_hdrOutput.active;
            const uint32_t bytesPerPixel = g_captureCopy.format == plume::RenderFormat::R16G16B16A16_FLOAT ? 8u : 4u;
            const uint32_t width = g_swapChain->getWidth(), height = g_swapChain->getHeight();
            if (!width || !height || width > (UINT32_MAX - 255u) / bytesPerPixel)
            {
                g_captureCopy.failure = "swapchain_extent";
                return;
            }
            const uint32_t pitch = (width * bytesPerPixel + 255u) & ~255u;
            auto buffer = g_device->createBuffer(plume::RenderBufferDesc::ReadbackBuffer(uint64_t(pitch) * height));
            ++g_captureAllocs;
            if (!buffer)
            {
                g_captureCopy.failure = "allocation_failed";
                return;
            }
            g_commandList->barriers(plume::RenderBarrierStage::COPY, plume::RenderTextureBarrier(frame, plume::RenderTextureLayout::COPY_SOURCE));
            g_commandList->copyTextureRegion(
                plume::RenderTextureCopyLocation::PlacedFootprint(buffer.get(), g_captureCopy.format, width, height, 1, pitch / bytesPerPixel),
                plume::RenderTextureCopyLocation::Subresource(frame, 0));
            ++g_captureCopies;
            g_captureCopy.queued = true;
            g_captureCopy.buffer = std::move(buffer);
            g_captureCopy.width = width;
            g_captureCopy.height = height;
            g_captureCopy.pitch = pitch;
        }

        bool ConfirmD3D12PresentFence(uint64_t signaled)
        {
#ifdef _WIN32
            if (!signaled || g_captureCompletionFault) return false;
            auto *fence = static_cast<plume::D3D12CommandFence *>(g_fence.get());
            auto *device = static_cast<plume::D3D12Device *>(g_device.get());
            if (!fence || !fence->d3d || !device || !device->d3d) return false;
            const auto completed = fence->d3d->GetCompletedValue();
            if (completed == UINT64_MAX || completed < signaled) return false;
            return SUCCEEDED(device->d3d->GetDeviceRemovedReason());
#else
            (void)signaled;
            return false;
#endif
        }

        void ReadPresentCapture(gpu::present_capture::Result *result, bool submitted, bool presented, uint64_t serial)
        {
            if (!result)
            {
                if (g_captureCopy.queued) RetainPresentCapture();
                return;
            }
            if (!g_captureCopy.ticket.active && !g_captureCopy.queued) return;
            result->attempted = true;
            result->rendererFrame = g_captureCopy.ticket.rendererFrame;
            result->swap = g_captureCopy.ticket.swap;
            result->deviceEpoch = g_captureCopy.ticket.deviceEpoch;
            result->presentAccepted = presented;
            result->backend = g_vulkan ? "Vulkan" : g_metal ? "Metal" : "D3D12";
            result->format = "R8G8B8A8_UNORM";
            result->available = false;
            if (!g_captureCopy.ticket.active)
            {
                result->reason = "no_request";
                return;
            }
            if (!g_captureCopy.queued)
            {
                result->reason = g_captureCopy.failure.empty() ? "not_recorded" : g_captureCopy.failure;
                return;
            }
            result->width = g_captureCopy.width;
            result->height = g_captureCopy.height;
            if (g_captureCompletionFault || !submitted)
            {
                result->reason = submitted ? "completion_unconfirmed" : "submit_failed";
                RetainPresentCapture();
                return;
            }
            bool complete = false;
            // Metal's counted fence wait proves completion as Vulkan's fence does.
            if (g_vulkan || g_metal)
            {
                complete = WaitForGpuFence(g_fence.get());
                if (complete)
                {
                    result->hasSubmissionSerial = true;
                    result->submissionSerial = serial;
                    g_fgPresent.Completed(g_deviceEpoch.load(), serial);
                    PresentationFenceCompleted();
                }
            }
            else
            {
                complete = WaitForGpuFence(g_fence.get()) &&
                    ConfirmD3D12PresentFence(g_captureCopy.d3dFenceValue);
                if (complete)
                {
                    result->hasFenceValue = true;
                    result->fenceValue = g_captureCopy.d3dFenceValue;
                    PresentationFenceCompleted();
                }
            }
            if (!complete)
            {
                result->reason = "completion_unconfirmed";
                RetainPresentCapture();
                return;
            }
            auto *mapped = static_cast<const uint8_t *>(g_captureCopy.buffer->map());
            if (!mapped)
            {
                result->reason = "map_failed";
                g_captureCopy.buffer.reset();
                g_captureCopy.queued = false;
                return;
            }
            ++g_captureMaps;
            result->pixels.resize(size_t(result->width) * result->height);
            const bool fp16Capture = g_captureCopy.format == plume::RenderFormat::R16G16B16A16_FLOAT;
            const bool pqCapture = g_captureCopy.format == plume::RenderFormat::R10G10B10A2_UNORM ||
                g_captureCopy.format == plume::RenderFormat::B10G10R10A2_UNORM;
            const bool hdrCapture = fp16Capture || pqCapture;
            for (uint32_t y = 0; y < result->height; ++y) {
                if (fp16Capture) {
                    const auto* row = reinterpret_cast<const uint16_t*>(mapped + size_t(y) * g_captureCopy.pitch);
                    for (uint32_t x = 0; x < result->width; ++x)
                        result->pixels[size_t(y) * result->width + x] = hdr::PreviewRgba(
                            hdr::DecodeHalf(row[x * 4]), hdr::DecodeHalf(row[x * 4 + 1]),
                            hdr::DecodeHalf(row[x * 4 + 2]), g_captureCopy.outputScale, g_captureCopy.srgbPreview);
                } else if (pqCapture) {
                    const auto* row = reinterpret_cast<const uint32_t*>(mapped + size_t(y) * g_captureCopy.pitch);
                    for (uint32_t x = 0; x < result->width; ++x) {
                        const auto rgb = hdr::DecodePq10(row[x], g_captureCopy.format == plume::RenderFormat::B10G10R10A2_UNORM);
                        result->pixels[size_t(y) * result->width + x] = hdr::PreviewRgba(rgb[0], rgb[1], rgb[2], g_captureCopy.outputScale);
                    }
                } else memcpy(result->pixels.data() + size_t(y) * result->width,
                    mapped + size_t(y) * g_captureCopy.pitch, size_t(result->width) * 4);
            }
            g_captureCopy.buffer->unmap();
            g_captureCopy.buffer.reset();
            g_captureCopy.queued = false;
            result->available = true;
            result->reason = hdrCapture ? "hdr_swapchain_sdr_preview" : "swapchain_readback";
        }
#endif
#endif

#if !defined(LO_VIDEO_SUBMISSION_UNIT)
        uint32_t GpuSwap(uint32_t value, uint32_t endian)
        {
            switch (endian & 3)
            {
            case 1: return ((value & 0xFF00FF00u) >> 8) | ((value & 0x00FF00FFu) << 8);
            case 2: return ByteSwap(value);
            case 3: return (value >> 16) | (value << 16);
            default: return value;
            }
        }
#endif

        // Owner thread only. Report() is read here, after NGX has returned,
        // and the capability fields are published as one value.
        void PublishOwnedDeviceCapability()
        {
            upscaling::BackendDeviceSnapshot snapshot;
            const bool vulkan = g_vulkan.load(std::memory_order_acquire);
            snapshot.backend = vulkan ? backend::Backend::Vulkan
                : g_metal.load(std::memory_order_acquire) ? backend::Backend::Metal : backend::Backend::D3D12;
            snapshot.deviceEpoch = g_deviceEpoch.load(std::memory_order_acquire);
#if defined(LO_GPU_PLUME) && !defined(LO_VIDEO_SUBMISSION_UNIT)
            snapshot.deviceReady = g_available && g_device != nullptr;
            snapshot.dlssAvailable = g_dlssController &&
                g_dlssController->Report().state == dlss::ProbeState::Available;
            snapshot.fsrAvailable = snapshot.deviceReady &&
                (vulkan ? LO_HAS_FSR : LO_HAS_FSR_D3D12);
#if defined(_WIN32) && defined(LO_HAS_XESS) && LO_HAS_XESS
            // libxess.dll and adapter support are checked by the sizing query.
            snapshot.xessAvailable = snapshot.deviceReady && !vulkan;
#endif
#if LO_PLATFORM_MACOS
            snapshot.fsrAvailable = false;
            snapshot.metalFxAvailable = snapshot.deviceReady && g_temporalUpscaler &&
                snapshot.backend == backend::Backend::Metal && plume::SupportsMetalFxTemporal(g_device.get());
#endif
            snapshot.gpuWorkStopped = g_submissionState.Stopped();
#elif defined(LO_GPU_PLUME)
            snapshot.deviceReady = false;
            snapshot.dlssAvailable = false;
            snapshot.gpuWorkStopped = g_submissionState.Stopped();
#else
            snapshot.deviceReady = false;
            snapshot.dlssAvailable = false;
            snapshot.gpuWorkStopped = false;
#endif
            const auto previous = upscaling::PublishedDeviceCapability();
            upscaling::PublishDeviceCapability(snapshot);
            if (previous != snapshot)
                frame_plan::NoteCurrentDlssStatus();
        }

        void PublishClearedDeviceCapability()
        {
            upscaling::BackendDeviceSnapshot snapshot;
            snapshot.backend = g_vulkan.load(std::memory_order_acquire) ? backend::Backend::Vulkan
                : g_metal.load(std::memory_order_acquire) ? backend::Backend::Metal : backend::Backend::D3D12;
            snapshot.deviceEpoch = g_deviceEpoch.load(std::memory_order_acquire);
            snapshot.deviceReady = false;
            snapshot.dlssAvailable = false;
            snapshot.gpuWorkStopped = false;
            const auto previous = upscaling::PublishedDeviceCapability();
            upscaling::PublishDeviceCapability(snapshot);
            if (previous != snapshot)
                frame_plan::NoteCurrentDlssStatus();
        }
    }

    plume::RenderDevice* GetDevice()
    {
#ifdef LO_GPU_PLUME
        return (g_available || g_initializing) ? g_device.get() : nullptr;
#else
        return nullptr;
#endif
    }

    plume::RenderCommandQueue* GetQueue()
    {
#ifdef LO_GPU_PLUME
        return (g_available || g_initializing) ? g_queue.get() : nullptr;
#else
        return nullptr;
#endif
    }
#if defined(LO_GPU_PLUME) && !defined(LO_VIDEO_SUBMISSION_UNIT)
    dlss::Controller* GetDlssController() { return g_dlssController.get(); }
    TemporalUpscaler* GetTemporalUpscaler() { return g_temporalUpscaler.get(); }
#endif
#if defined(LO_GPU_PLUME)
    bool GpuWorkStopped() { return g_submissionState.Stopped(); }
    void StopGpuWork(int32_t nativeResult) {
        if (!g_submissionState.Stopped())
            LOG_ERROR("video: native GPU work stopped backend={} raw_result={}; device restart required",
                g_vulkan ? "Vulkan" : g_metal ? "Metal" : "D3D12", nativeResult);
        g_submissionState.Stop(nativeResult);
#if !defined(LO_VIDEO_SUBMISSION_UNIT)
        g_fgPresent.CancelAll(frame_generation::HandoffCancel::DeviceLost);
        renderer::CancelFgHandoffs();
#endif
        // Publish even when the first-stop log is skipped. Resource teardown
        // is unchanged; only the capability snapshot gains the stopped bit.
        PublishOwnedDeviceCapability();
    }
    bool BeginGpuCommands(plume::RenderCommandList* list) {
        if (GpuWorkStopped()) return false;
        if (!list) { StopGpuWork(submission::VulkanState::InvalidState); return false; }
#if LO_PLATFORM_MACOS
        // Metal command encoding reports no errors at record time.
        if (g_metal) { list->begin(); return true; }
#endif
        if (!g_vulkan) {
#ifdef _WIN32
            auto* native = static_cast<plume::D3D12CommandList*>(list);
            if (!native->d3d || !native->commandAllocator || native->open) {
                StopGpuWork(submission::VulkanState::InvalidState); return false;
            }
            native->invalidateCachedNativeState();
            native->resetRootBindingStats();
            HRESULT result = native->commandAllocator->Reset();
            if (SUCCEEDED(result)) result = native->d3d->Reset(native->commandAllocator, nullptr);
            if (FAILED(result)) { StopGpuWork(int32_t(result)); return false; }
            native->open = true;
            return true;
#else
            StopGpuWork(submission::VulkanState::InvalidState); return false;
#endif
        }
        const auto result = submission::BeginCommands(*static_cast<plume::VulkanCommandList*>(list));
        if (result != VK_SUCCESS) StopGpuWork(int32_t(result));
        return result == VK_SUCCESS;
    }
    bool EndGpuCommands(plume::RenderCommandList* list) {
        if (GpuWorkStopped()) return false;
        if (!list) { StopGpuWork(submission::VulkanState::InvalidState); return false; }
#if LO_PLATFORM_MACOS
        if (g_metal) { list->end(); return true; }
#endif
        if (!g_vulkan) {
#ifdef _WIN32
            auto* native = static_cast<plume::D3D12CommandList*>(list);
            if (!native->d3d || !native->open) {
                StopGpuWork(submission::VulkanState::InvalidState); return false;
            }
            native->resetSamplePositions();
            const HRESULT result = native->d3d->Close();
            native->open = false;
            native->invalidateCachedNativeState();
            if (FAILED(result)) { StopGpuWork(int32_t(result)); return false; }
            return true;
#else
            StopGpuWork(submission::VulkanState::InvalidState); return false;
#endif
        }
        const auto result = submission::EndCommands(*static_cast<plume::VulkanCommandList*>(list));
        if (result != VK_SUCCESS) StopGpuWork(int32_t(result));
        return result == VK_SUCCESS;
    }
    bool WaitForGpuFence(plume::RenderCommandFence* fence) {
        auto* queueHolder = ActiveSubmissionQueue();
        if (!queueHolder || !fence) { StopGpuWork(submission::VulkanState::InvalidState); return false; }
#if LO_PLATFORM_MACOS
        if (g_metal) {
            ConsumeMetalFenceSignals(queueHolder, fence);
            return g_submissionState.WaitSubmitted([] { return submission::VulkanState::Success; });
        }
#endif
        if (!g_vulkan) {
#ifdef _WIN32
            auto* queue = static_cast<plume::D3D12CommandQueue*>(queueHolder);
            auto* native = static_cast<plume::D3D12CommandFence*>(fence);
            const bool complete = g_submissionState.WaitSubmitted([&] {
                if (!queue->device || !queue->device->d3d || !native->d3d ||
                    !native->fenceEvent || native->fenceValue == 0)
                    return submission::VulkanState::InvalidState;
                const UINT64 target = native->fenceValue - 1;
                const UINT64 current = native->d3d->GetCompletedValue();
                if (current == UINT64_MAX) {
                    const HRESULT removed = queue->device->d3d->GetDeviceRemovedReason();
                    return FAILED(removed) ? D3DResult(removed) : submission::VulkanState::InvalidState;
                }
                if (current < target) {
                    const HRESULT event = native->d3d->SetEventOnCompletion(target, native->fenceEvent);
                    if (FAILED(event)) return D3DResult(event);
                    for (;;) {
                        const UINT64 observed = native->d3d->GetCompletedValue();
                        if (observed == UINT64_MAX) {
                            const HRESULT removed = queue->device->d3d->GetDeviceRemovedReason();
                            return FAILED(removed) ? D3DResult(removed) : submission::VulkanState::InvalidState;
                        }
                        if (observed >= target) break;
                        const DWORD waited = WaitForSingleObjectEx(native->fenceEvent, 100, FALSE);
                        if (waited != WAIT_OBJECT_0 && waited != WAIT_TIMEOUT)
                            return int32_t(HRESULT_FROM_WIN32(waited == WAIT_FAILED ? GetLastError() : ERROR_GEN_FAILURE));
                        const HRESULT removed = queue->device->d3d->GetDeviceRemovedReason();
                        if (FAILED(removed)) return D3DResult(removed);
                    }
                }
                const UINT64 completed = native->d3d->GetCompletedValue();
                if (completed == UINT64_MAX || completed < target)
                    return submission::VulkanState::InvalidState;
                return D3DResult(queue->device->d3d->GetDeviceRemovedReason());
            });
            if (!complete) {
                LOG_ERROR("video: D3D12 fence wait failed raw_hr={}; resources retained", g_submissionState.Failure());
                StopGpuWork(g_submissionState.Failure());
            }
            return complete;
#else
            StopGpuWork(submission::VulkanState::InvalidState); return false;
#endif
        }
        auto* queue = static_cast<plume::VulkanCommandQueue*>(queueHolder);
        auto* nativeFence = static_cast<plume::VulkanCommandFence*>(fence);
        const bool complete = g_submissionState.WaitSubmitted([&] {
            if (g_probeWaitFault) {
                const int32_t fault = g_probeWaitFault;
                g_probeWaitFault = 0;
                return fault;
            }
            return int32_t(vkWaitForFences(queue->device->vk, 1, &nativeFence->vk, VK_TRUE, UINT64_MAX));
        });
        if (!complete) {
            LOG_ERROR("video: GPU fence wait failed raw_vk={}; resources retained", g_submissionState.Failure());
            StopGpuWork(g_submissionState.Failure());
        }
        return complete;
    }
#if !defined(LO_VIDEO_SUBMISSION_UNIT)
    void DrainGpuForShutdown() {
        if (!g_device) return;
#if LO_PLATFORM_MACOS
        if (g_metal) {
            // Every submission signalled a counted fence; consuming them all
            // proves completion.
            std::vector<plume::RenderCommandFence*> fences;
            {
                const std::scoped_lock lock(g_metalFenceMutex);
                for (const auto& [fence, pending] : g_metalFenceSignals)
                    if (pending) fences.push_back(const_cast<plume::RenderCommandFence*>(fence));
            }
            if (g_queue)
                for (auto* fence : fences) ConsumeMetalFenceSignals(g_queue.get(), fence);
            return;
        }
#endif
        if (g_vulkan) {
            const auto result = vkDeviceWaitIdle(static_cast<plume::VulkanDevice*>(g_device.get())->vk);
            if (result == VK_SUCCESS) {
                if (g_temporalUpscaler) g_temporalUpscaler->ReleaseCompleted(g_submissionState.LastSubmission());
                return;
            }
            if (result == VK_ERROR_DEVICE_LOST) {
                StopGpuWork(int32_t(result));
                if (g_temporalUpscaler) g_temporalUpscaler->AbandonAfterDeviceLoss();
                return;
            }
            LOG_ERROR("video: shutdown drain failed raw_vk={}; terminating without unsafe GPU destruction", int32_t(result));
        } else {
#ifdef _WIN32
            if (!g_queue) return; // No queue means this device never submitted work.
            auto* device = static_cast<plume::D3D12Device*>(g_device.get());
            auto* queue = static_cast<plume::D3D12CommandQueue*>(g_queue.get());
            const HRESULT removed = device->d3d->GetDeviceRemovedReason();
            if (FAILED(removed)) {
                StopGpuWork(int32_t(removed));
                if (g_temporalUpscaler) g_temporalUpscaler->AbandonAfterDeviceLoss();
                return;
            }
            ID3D12Fence* drainFence = nullptr;
            HRESULT result = device->d3d->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&drainFence));
            HANDLE event = SUCCEEDED(result) ? CreateEvent(nullptr, FALSE, FALSE, nullptr) : nullptr;
            if (SUCCEEDED(result) && !event) {
                const DWORD error = GetLastError();
                result = HRESULT_FROM_WIN32(error ? error : ERROR_GEN_FAILURE);
            }
            if (SUCCEEDED(result) && queue && queue->d3d) result = queue->d3d->Signal(drainFence, 1);
            else if (SUCCEEDED(result)) result = E_FAIL;
            if (SUCCEEDED(result)) result = drainFence->SetEventOnCompletion(1, event);
            while (SUCCEEDED(result) && drainFence->GetCompletedValue() < 1) {
                const DWORD waited = WaitForSingleObjectEx(event, 100, FALSE);
                if (waited == WAIT_OBJECT_0) break;
                if (waited != WAIT_TIMEOUT) {
                    const DWORD error = GetLastError();
                    result = HRESULT_FROM_WIN32(error ? error : ERROR_GEN_FAILURE);
                    break;
                }
                result = device->d3d->GetDeviceRemovedReason();
            }
            if (SUCCEEDED(result) && drainFence->GetCompletedValue() != 1) result = E_FAIL;
            if (event) CloseHandle(event);
            if (drainFence) drainFence->Release();
            if (SUCCEEDED(result)) result = device->d3d->GetDeviceRemovedReason();
            if (SUCCEEDED(result)) {
                if (g_temporalUpscaler) g_temporalUpscaler->ReleaseCompleted(g_submissionState.LastSubmission());
                return;
            }
            const HRESULT finalRemoved = device->d3d->GetDeviceRemovedReason();
            if (FAILED(finalRemoved)) {
                StopGpuWork(int32_t(finalRemoved));
                if (g_temporalUpscaler) g_temporalUpscaler->AbandonAfterDeviceLoss();
                return;
            }
            LOG_ERROR("video: shutdown drain failed raw_hr={}; terminating without unsafe GPU destruction", int32_t(result));
#endif
        }
        // No proven completion or lost-device disposal boundary. Never free
        // resources still referenced by native work. OS process teardown is
        // safer than running their destructors against an undrained device.
        // Pipeline recipes and the driver cache are still written (no GPU wait).
        if (const auto flush = os::shaderlog::ExitFlush().exchange(nullptr)) flush();
        std::fflush(nullptr);
        std::_Exit(EXIT_FAILURE);
    }
#endif

    bool SubmitRendererBatch(const plume::RenderCommandList* const* lists, uint32_t count,
        plume::RenderCommandFence* fence, uint64_t& submissionSerial, int32_t& rawResult,
        bool* executionMayBeInFlight)
    {
#if LO_PLATFORM_MACOS
        if (g_metal) {
            if (executionMayBeInFlight) *executionMayBeInFlight = false;
            return SubmitMetal(lists, count, nullptr, 0, nullptr, 0, fence, &submissionSerial, &rawResult);
        }
#endif
        if (g_vulkan) {
            if (executionMayBeInFlight) *executionMayBeInFlight = false;
            return SubmitVulkan(lists, count, nullptr, 0, nullptr, 0, fence, &submissionSerial, &rawResult);
        }
#ifdef _WIN32
        return SubmitD3D12(lists, count, nullptr, 0, nullptr, 0, fence, &submissionSerial,
            &rawResult, executionMayBeInFlight);
#else
        StopGpuWork(submission::VulkanState::InvalidState);
        return false;
#endif
    }
    // Test seam for the production submit and fence-wait stop paths. A fault
    // is consumed once and returns before the native queue call. resetStop
    // clears the device latch and publishes not-stopped so the next failure
    // is the one that becomes visible.
    void ConfigureSubmissionProbe(plume::RenderCommandQueue* queue, int32_t submitFault, int32_t waitFault, bool resetStop)
    {
        if (resetStop) {
            g_submissionState = {};
            PublishClearedDeviceCapability();
        }
        g_probeSubmitFault = submitFault;
        g_probeWaitFault = waitFault;
        g_probeQueue = queue;
        if (queue) g_vulkan.store(true, std::memory_order_release);
    }
#endif

#if !defined(LO_VIDEO_SUBMISSION_UNIT)
    uint32_t TiledOffset2D(uint32_t x, uint32_t y, uint32_t pitchBlocks, uint32_t bytesPerBlockLog2)
    {
        // Macro tiles are 32x32 blocks; the pitch is given in blocks.
        const uint32_t pitchMacroTiles = pitchBlocks >> 5;
        const uint32_t outerBlocks = (((y >> 5) * pitchMacroTiles) + (x >> 5)) << 6;
        const uint32_t innerBlocks = (((y >> 1) & 0x7) << 3) | (x & 0x7);
        const uint32_t outerInnerBytes = (outerBlocks | innerBlocks) << bytesPerBlockLog2;
        const uint32_t bank = (y >> 4) & 0x1;
        const uint32_t pipe = ((x >> 3) & 0x3) ^ (((y >> 3) & 0x1) << 1);
        const uint32_t yLsb = y & 1;
        return (yLsb << 4) | (pipe << 6) | (bank << 11) | (outerInnerBytes & 0xF) |
               (((outerInnerBytes >> 4) & 0x1) << 5) |
               (((outerInnerBytes >> 5) & 0x7) << 8) |
               ((outerInnerBytes >> 8) << 12);
    }

    bool IsVulkan() { return g_vulkan; }
    bool IsMetal() { return g_metal; }
    bool UsesSpirv() { return g_vulkan || g_metal; }
    uint32_t LogicalOutputHeight()
    {
#if !defined(_WIN32)
        int width = 0, height = 0;
        if (g_window) SDL_GetWindowSize(g_window, &width, &height);
        return height > 0 ? uint32_t(height) : 0;
#else
        return 0;
#endif
    }
    upscaling::BackendDeviceSnapshot BackendDeviceState()
    {
        return upscaling::PublishedDeviceCapability();
    }
#if defined(LO_GPU_PLUME)
    // Called by video's GPU-owning presentation path. TakeSizingRequest releases
    // the CPU cache lock before NGX initialization and capability work begins.
    static void ServicePendingDlssSizing()
    {
        if (!g_temporalUpscaler || !g_interface || !g_device) return;
        const auto key = frame_plan::TakeSizingRequest();
        if (!key || key->deviceEpoch != g_deviceEpoch.load(std::memory_order_acquire)) return;
        if (g_vulkan)
            frame_plan::PublishSizing(upscaling::SizingService::QueryOutputSizing(*g_temporalUpscaler,
                *static_cast<plume::VulkanInterface*>(g_interface.get()), *static_cast<plume::VulkanDevice*>(g_device.get()), *key));
#ifdef _WIN32
        else
            frame_plan::PublishSizing(g_temporalUpscaler->QuerySizing(*static_cast<plume::D3D12Device*>(g_device.get()), *key));
#elif LO_PLATFORM_MACOS
        else
            frame_plan::PublishSizing(g_temporalUpscaler->QuerySizing(*g_device, *key));
#endif
        PublishOwnedDeviceCapability();
    }
#endif
    bool WaitForPresentGpu()
    {
#ifdef LO_GPU_PLUME
        return WaitForPresentGpuImpl();
#else
        return true;
#endif
    }
    static std::atomic<bool> g_textureCompressionBC{true};
    bool TextureCompressionBC() { return g_textureCompressionBC.load(std::memory_order_relaxed); }
    std::optional<backend::Backend> SelectedBackend() {
        const auto selected = g_selectedBackend.load();
        return selected < 0 ? std::nullopt : std::optional(static_cast<backend::Backend>(selected));
    }
    static std::mutex g_gpuNamesMutex;
    static std::vector<std::string> g_gpuDeviceNames;
    static std::string g_activeGpuDeviceName;
    std::vector<std::string> GpuDeviceNames() {
        std::lock_guard lock(g_gpuNamesMutex);
        return g_gpuDeviceNames;
    }
    std::string ActiveGpuDeviceName() {
        std::lock_guard lock(g_gpuNamesMutex);
        return g_activeGpuDeviceName;
    }
    static std::vector<display_choice::Display> g_displays;
    std::vector<display_choice::Display> Displays() {
        std::lock_guard lock(g_gpuNamesMutex);
        return g_displays;
    }
    static std::string DescribeDisplays(const std::vector<display_choice::Display>& displays) {
        std::string text;
        for (size_t i = 0; i < displays.size(); ++i) {
            const auto& d = displays[i];
            text += fmt::format("{}#{} \"{}\" {}x{} at {},{}", i ? "; " : "", i, d.name, d.width, d.height, d.x, d.y);
        }
        return text.empty() ? std::string("none") : text;
    }
    // Window owner thread only: SDL's display list is not thread-safe.
    static std::vector<display_choice::Display> QueryDisplays() {
        std::vector<display_choice::Display> displays;
        const int count = SDL_GetNumVideoDisplays();
        for (int i = 0; i < count; ++i) {
            const char* name = SDL_GetDisplayName(i);
            display_choice::Display display{name && *name ? std::string(name) : "Display " + std::to_string(i + 1)};
            SDL_Rect bounds{};
            if (SDL_GetDisplayBounds(i, &bounds) == 0) {
                display.x = bounds.x; display.y = bounds.y; display.width = bounds.w; display.height = bounds.h;
            }
            displays.push_back(std::move(display));
        }
        std::lock_guard lock(g_gpuNamesMutex);
        g_displays = displays;
        return displays;
    }
    // SDL display index for the saved display, or -1 for system placement.
    static int ChosenDisplay(const settings::Config& config, const std::vector<display_choice::Display>& displays) {
        const int display = display_choice::Resolve(displays, config.displayName, config.displayIndex);
        if (display < 0 && !config.displayName.empty())
            LOG_WARNING("video: display \"{}\"#{} is not connected; using automatic placement",
                config.displayName, config.displayIndex);
        return display;
    }

    // The command thread calls this only before guest startup, or after all
    // rendering has stopped. The window/event thread is deliberately retained
    // between candidates; device children are destroyed before their parents.
    static void ResetGpu() {
#if defined(__APPLE__) && defined(LO_ENABLE_METALFX_FG)
        if (g_metalFg) {
            // After a GPU error the host wait can report failure; the session
            // then waits for its own command buffer instead of ending the process.
            WaitForPresentGpu();
            g_metalFg->Retire();
            g_metalFg->SuspendAfterHostDrain();
            g_metalFg.reset();
        }
#endif
#if defined(LO_ENABLE_VULKAN_FSR_FG) && defined(_WIN32)
        if (g_fsrVulkanFg) g_fsrVulkanFg->Quiesce();
#endif
#if (defined(_WIN32) && (defined(LO_ENABLE_STREAMLINE_FG) || defined(LO_ENABLE_D3D12_FG) || defined(LO_ENABLE_VULKAN_FSR_FG))) || (defined(__APPLE__) && defined(LO_ENABLE_METALFX_FG))
        g_fgWindowSynchronization = false;
        g_fgWindowChange = 0;
#endif
#if defined(LO_ENABLE_D3D12_FG) && defined(_WIN32)
        if (g_d3dFg) g_d3dFg->Quiesce();
#endif
#if (defined(_WIN32) && (defined(LO_ENABLE_D3D12_FG) || defined(LO_ENABLE_STREAMLINE_FG) || defined(LO_ENABLE_VULKAN_FSR_FG))) || (defined(__APPLE__) && defined(LO_ENABLE_METALFX_FG))
        {
            std::lock_guard lock(g_fgSettingsMutex);
            g_fgAppliedConfig = {};
            g_fgSessionProvider = framegen::Provider::Off;
            g_fgFailedRequest.reset();
            g_fgStartupFailure = framegen::Provider::Off;
        }
#endif
#if defined(LO_ENABLE_STREAMLINE_FG) && defined(_WIN32)
        // FG releases its retained producer leases before renderer/device teardown.
        g_fgSession.reset();
#endif
        g_displayChanges.Reset();
#ifdef LO_GPU_PLUME
        g_fgPresent.CancelAll(frame_generation::HandoffCancel::Shutdown);
#endif
        g_available = false;
        g_initializing = false;
        g_selectedBackend = -1;
        PublishClearedDeviceCapability();
        renderer::Shutdown();
#ifdef LO_GPU_PLUME
        WaitForPresentGpu();
        if (GpuWorkStopped()) DrainGpuForShutdown();
        g_captureRetained.clear();
        if (g_captureCopy.buffer) g_captureCopy.buffer.reset();
        g_captureCopy = {};
        g_cpuFrame.reset(); g_cpuWidth = g_cpuHeight = 0;
        g_presentedSnapshot.reset(); g_snapshotWidth = g_snapshotHeight = 0; g_snapshotFormat=plume::RenderFormat::UNKNOWN;
        g_hdrCalibrationCache = {};
        settings::SetHdrCalibrationSceneAvailable(false);
        // Renderer shutdown above established a completed/lost-device teardown
        // boundary; a failed ordinary wait alone never releases these leases.
        g_fgPresent = FgPresentBridge{}; g_fgPresentSerial = 0;
        g_presentation.reset();
        g_hdrSwapchain = g_hdrSceneEnabled = false;
        g_hdrSwapchainRefused.reset();
        g_hdrOutput = {};
        g_presentationFormat = plume::RenderFormat::UNKNOWN;
        settings::SetHdrDisplayInfo({});
        g_hdrReportedActive.reset();
        g_uploadBuffer.reset();
#if defined(LO_ENABLE_D3D12_FG) && defined(_WIN32)
        // Release DLSS-G's NGX feature while both SDK sessions remain live.
        // Renderer and presentation work have already been drained.
        if (g_d3dFg) g_d3dFg->ReleaseFeatureAfterGpuDrain();
#endif
        g_swapChain.reset(); g_presentSemaphores.clear();
        if (g_temporalUpscaler) {
            g_temporalUpscaler->ShutdownAfterGpuDrain();
            if (!g_temporalUpscaler->ShutdownComplete()) {
                LOG_ERROR("video: SR shutdown incomplete; retaining unresolved resources and device");
                std::fflush(nullptr); std::_Exit(EXIT_FAILURE);
            }
        }
#if defined(LO_ENABLE_D3D12_FG) && defined(_WIN32)
        // Keep the FG SDK loaded until NGX SR has released its feature and
        // parameters. Its proxy vtables also outlive all swapchain references.
        // The device and queue are still live for FG shutdown here.
        g_d3dFg.reset();
#endif
#if defined(LO_ENABLE_VULKAN_FSR_FG) && defined(_WIN32)
        // WSI hooks and exclusive queues outlive the SDK swapchain and SR work.
        g_fsrVulkanFg.reset();
#endif
        g_releaseSemaphore.reset(); g_acquireSemaphore.reset();
        g_fence.reset(); g_commandList.reset(); g_queue.reset();
#if defined(LO_ENABLE_STREAMLINE_FG) && defined(_WIN32)
        // Streamline owns Vulkan children; its shutdown requires a live device.
        if (g_fgRuntime && !g_fgRuntime->Shutdown()) {
            LOG_ERROR("DLSS FG: Streamline shutdown failed; ending process before device replacement");
            std::fflush(nullptr); std::_Exit(EXIT_FAILURE);
        }
        if (g_fgDispatch) {
            g_fgDispatch->RestoreDeviceHooks();
            g_fgDispatch->RestoreCreationHooks();
        }
        g_fgDispatch.reset();
        g_fgRuntime.reset();
#endif
        g_device.reset(); g_interface.reset();
        g_submissionState = {};
        g_presentPending = false;
        g_temporalUpscaler.reset();
        g_dlssController.reset();
        g_hasPresentedImage = false; g_lastPresentedImage = 0; g_forceSwapResize = false;
#if defined(__ANDROID__)
        g_androidAppliedSurfaceChangeSerial = 0;
        g_androidSurfaceRebuildFailureLogged = false;
#endif
        g_presentationDisplay = {};
#endif
    }

    bool Init()
    {
        if (g_initAttempted)
            return g_available;
        g_initAttempted = true;
        gpu::SetFrameRateTarget(settings::GetConfig().frameRate);

        if (getenv("LO_HEADLESS"))
        {
            LOG_INFO("video: LO_HEADLESS set, no window");
            return false;
        }

        const auto configured = settings::GetConfig().graphicsBackend;
        const auto requested = backend::Requested(configured, getenv("LO_GRAPHICS_API"));
        if (!requested) {
            LOG_ERROR("video: invalid backend request '{}' (use auto, d3d12, vulkan, or dx11; DX11 is unsupported)",
                getenv("LO_GRAPHICS_API") ? getenv("LO_GRAPHICS_API") : "settings");
            return false;
        }

#if defined(LO_GPU_PLUME)
#if defined(LO_DLSS_SDK)
        constexpr bool ngxCompiled = true;
#else
        constexpr bool ngxCompiled = false;
#endif
        const auto& opti = optiscaler::Initialize(ngxCompiled);
        if (opti.requested) {
            if (opti.loaded)
                LOG_INFO("OptiScaler: {}. Select DLSS as the game input; configure the output in OptiScaler.", opti.reason);
            else
                LOG_WARNING("OptiScaler: {} (system error {})", opti.reason, opti.systemError);
        }
#endif

        auto createWindow = [] {
            g_windowDisplay = {};
            g_windowModeOverridden = false;
            if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
            {
                LOG_WARNING("video: SDL video init failed: {}", SDL_GetError());
                return false;
            }
            g_videoSubsystemOwned = true;

            // Background regression runs still render and capture the swap chain,
            // but must never show a window or take focus from the desktop user.
            const bool background = getenv("LO_BACKGROUND") != nullptr;
            const auto config=settings::GetConfig();
            uint32_t flags = SDL_WINDOW_RESIZABLE | (background ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN);
#if LO_PLATFORM_MACOS
            // macOS renders through plume's Metal backend (CAMetalLayer).
            flags |= SDL_WINDOW_METAL;
#elif !defined(_WIN32)
            flags |= SDL_WINDOW_VULKAN;
#endif
            // Fullscreen later uses the display the window was created on.
            const auto displays = QueryDisplays();
            const int display = ChosenDisplay(config, displays);
            LOG_INFO("video: displays: {}; configured=\"{}\"#{} chosen={}", DescribeDisplays(displays),
                config.displayName, config.displayIndex, display);
            const int position = display >= 0 ? int(SDL_WINDOWPOS_CENTERED_DISPLAY(display)) : int(SDL_WINDOWPOS_CENTERED);
            g_window = SDL_CreateWindow(lo_version::WindowTitle, position, position,
                config.width, config.height, flags);
            if (!g_window)
            {
                LOG_WARNING("video: window creation failed: {}", SDL_GetError());
                return false;
            }
#if defined(__ANDROID__)
            const auto nativeWindow = CurrentAndroidNativeWindow();
            g_androidNativeWindowIdentity = nativeWindow;
            g_androidSurfaceChangeSerial = 0;
            g_androidSurfaceReady = nativeWindow != 0;
            g_androidWasBackgrounded = false;
            LOG_INFO("video: Android native surface initial window={:#x}", nativeWindow);
#endif

            // The game never accepts host text entry. Keep SDL text input/IME
            // disabled so an active IME cannot consume gameplay key presses.
            SDL_StopTextInput();

            // Keep the pointer usable for mouse-driven host UI, then hide it
            // after brief inactivity while it remains over the game window.
            g_cursorManaged = !background;
            g_cursorHidden = false;
            if (g_cursorManaged) {
                SDL_ShowCursor(SDL_ENABLE);
                g_lastPointerActivity = std::chrono::steady_clock::now();
            }

            // This thread owns the SDL event loop from now on; the controller
            // subsystem is initialised here too so its message window (if any)
            // lives on the pumping thread.
            hid::Init();
            hid::SetExternalEventPump(true);
#ifdef _WIN32
            SDL_SysWMinfo info{};
            SDL_VERSION(&info.version);
            if (!SDL_GetWindowWMInfo(g_window, &info))
            {
                LOG_WARNING("video: native window lookup failed: {}", SDL_GetError());
                SDL_DestroyWindow(g_window);
                g_window = nullptr;
                return false;
            }
            g_nativeWindow = info.info.win.window;
            // SDL's Windows class loads the first RT_GROUP_ICON from this EXE.
            // Keep Explorer and the game window on the same shared resource.
            const auto resourceIcon = LoadIconW(GetModuleHandleW(nullptr), L"IDI_LOST_ODYSSEY_RECOMP");
            const auto windowIcon = reinterpret_cast<HICON>(GetClassLongPtrW(g_nativeWindow, GCLP_HICON));
            LOG_INFO("video: window icon resource match={}", resourceIcon && windowIcon == resourceIcon);
#endif
#if LO_PLATFORM_MACOS
            // plume's Metal swap chain presents to this view's CAMetalLayer.
            SDL_SysWMinfo info{};
            SDL_VERSION(&info.version);
            if (SDL_GetWindowWMInfo(g_window, &info))
                g_metalView = SDL_Metal_CreateView(g_window);
            if (!g_metalView)
            {
                LOG_WARNING("video: Metal view creation failed: {}", SDL_GetError());
                SDL_DestroyWindow(g_window);
                g_window = nullptr;
                return false;
            }
            g_cocoaWindow = info.info.cocoa.window;
            g_metalLayer = SDL_Metal_GetLayer(g_metalView);
#endif
            return true;
        };
#ifdef _WIN32
        // GPU waits, shader compilation and capture I/O must not starve Win32
        // messages. Create, pump and destroy SDL windows on their own thread.
        std::promise<bool> ready;
        auto initialized = ready.get_future();
        g_windowThread = std::jthread([createWindow, ready = std::move(ready)](std::stop_token stop) mutable {
            // The game resolution is a client-area pixel size. PMv2 prevents
            // Windows bitmap scaling; SDL's pixel policy keeps that size when
            // the window moves to a display with a different DPI.
            const window_pixels::Context pixels;
            struct Cleanup { ~Cleanup() { DestroyWindowResources(); } } cleanup;
            bool success = false;
            try {
                if (pixels.Ready()) success = createWindow();
                else LOG_ERROR("video: could not establish physical-pixel window coordinates");
            }
            catch (const std::exception& e) { LOG_ERROR("video: window initialization exception: {}", e.what()); }
            catch (...) { LOG_ERROR("video: window initialization exception"); }
            LOG_INFO("video: window thread {} (independent event pump)", GetCurrentThreadId());
            ready.set_value(success);
            if (!success) return;
            while (!stop.stop_requested())
            {
                PumpWindowEvents();
                // Bound latency even with no SDL events (native Debug Menu).
                MsgWaitForMultipleObjectsEx(0, nullptr, 8, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            }
        });
        if (!initialized.get())
        {
            g_windowThread.join();
            Shutdown(); g_initAttempted = true;
            return false;
        }
        LOG_INFO("video: render thread {}", GetCurrentThreadId());
#else
        // macOS: AppKit window work must run on the process main thread.
        bool created = false;
        os::main_thread::Run([&] { created = createWindow(); });
        if (!created) { Shutdown(); g_initAttempted = true; return false; }
#endif

#if defined(LO_GPU_PLUME)
        diagnostics::InstallPlumeLog();
        const auto selection = backend::Select(*requested, [](backend::Backend candidate) -> std::string {
            g_vulkan.store(candidate == backend::Backend::Vulkan);
            // Backend changes must also refresh the window thread's mode policy.
            g_reapplyWindow.store(true);
            g_initializing = true;
            LOG_INFO("video: trying {}", backend::Name(candidate));
#ifdef _WIN32
            if (g_vulkan) {
                g_dlssController = std::make_unique<dlss::Controller>(DlssApplicationDataPath(), DlssRuntimePath());
                g_temporalUpscaler = std::make_unique<TemporalUpscaler>(*g_dlssController);
#if defined(LO_ENABLE_STREAMLINE_FG) || defined(LO_ENABLE_VULKAN_FSR_FG)
                const auto fg = VulkanFgRequest();
                if (fg.error) LOG_ERROR("Vulkan FG: {}", fg.error);
#endif
#if defined(LO_ENABLE_STREAMLINE_FG)
                if (fg.Enabled() && fg.config.provider == framegen::Provider::Dlss) {
                    std::string reason;
                    g_fgRuntime = std::make_unique<dlss_fg::Runtime>();
                    if (!g_fgRuntime->Initialize(StreamlineRuntimePath(), reason)) {
                        LOG_ERROR("DLSS FG: initialization unavailable: {}", reason);
                        g_fgRuntime.reset();
                    } else {
                        g_fgDispatch = std::make_unique<dlss_fg::VulkanDispatch>(*g_fgRuntime, *g_dlssController);
                    }
                }
                g_interface = plume::CreateVulkanInterface(g_fgDispatch ? g_fgDispatch->Hooks() : g_dlssController->ExtensionHooks());
#else
                g_interface = plume::CreateVulkanInterface(g_dlssController->ExtensionHooks());
#endif
            } else {
                // d3d12.dll and dxgi.dll are delay-loaded. A missing DLL or export would
                // raise an SEH fault on first call, which Select cannot catch.
                const HMODULE d3d12 = LoadLibraryW(L"d3d12.dll");
                const HMODULE dxgi = LoadLibraryW(L"dxgi.dll");
                if (!d3d12 || !GetProcAddress(d3d12, "D3D12CreateDevice") ||
                    !GetProcAddress(d3d12, "D3D12SerializeRootSignature"))
                    return "d3d12.dll unavailable";
                if (!dxgi || !GetProcAddress(dxgi, "CreateDXGIFactory2")) return "dxgi.dll unavailable";
                g_dlssController = std::make_unique<dlss::Controller>(DlssApplicationDataPath(), DlssRuntimePath());
                g_temporalUpscaler = std::make_unique<TemporalUpscaler>(*g_dlssController);
                g_interface = plume::CreateD3D12Interface();
            }
#elif LO_PLATFORM_MACOS
            // NGX/FSR are D3D12/Vulkan only; on Metal the upscaler records MetalFX.
            g_metal.store(candidate == backend::Backend::Metal);
            g_temporalUpscaler = std::make_unique<TemporalUpscaler>(static_cast<dlss::Controller*>(nullptr));
            g_interface = plume::CreateMetalInterface();
#else
            g_dlssController = std::make_unique<dlss::Controller>(DlssApplicationDataPath(), DlssRuntimePath());
            g_temporalUpscaler = std::make_unique<TemporalUpscaler>(*g_dlssController);
            LoadCustomVulkanDriver();
            g_interface = plume::CreateVulkanInterface(g_window, g_dlssController->ExtensionHooks());
            if (!g_interface && RetryWithSystemVulkanDriver("instance creation"))
                g_interface = plume::CreateVulkanInterface(g_window, g_dlssController->ExtensionHooks());
#endif
            if (!g_interface) return "API/loader initialization failed";
#ifdef __ANDROID__
            LogVulkanPhysicalDevices(static_cast<plume::VulkanInterface*>(g_interface.get())->instance);
#endif
#if defined(LO_ENABLE_VULKAN_FSR_FG) && defined(_WIN32)
            if (g_vulkan) {
                const auto fg = VulkanFgRequest();
                static_cast<plume::VulkanInterface*>(g_interface.get())->enableFrameInterpolationFeatures =
                    fg.Enabled() && fg.config.provider == framegen::Provider::Fsr;
            }
#endif
            // The GPU setting names an adapter as this backend lists it. Plume
            // picks automatically for an empty name or an adapter it cannot open.
            std::string preferredGpu = settings::GetConfig().gpuDevice;
            const auto adapterNames = g_interface->getDeviceNames();
            {
                std::string listed;
                for (const auto& name : adapterNames) listed += (listed.empty() ? "\"" : ", \"") + name + "\"";
                LOG_INFO("video: {} adapters: {} configured=\"{}\"", backend::Name(candidate),
                    listed.empty() ? std::string("none") : listed, preferredGpu);
            }
            if (!preferredGpu.empty() && std::find(adapterNames.begin(), adapterNames.end(), preferredGpu) == adapterNames.end()) {
                LOG_WARNING("video: GPU \"{}\" is not listed by {}; using automatic selection", preferredGpu, backend::Name(candidate));
                preferredGpu.clear();
            }
            g_device = g_interface->createDevice(preferredGpu);
#if defined(__ANDROID__) && defined(LO_HAS_ADRENOTOOLS)
            if (!g_device && RetryWithSystemVulkanDriver("device creation")) {
                g_interface.reset();
                g_interface = plume::CreateVulkanInterface(g_window, g_dlssController->ExtensionHooks());
                if (!g_interface) return "API/loader initialization failed";
                LogVulkanPhysicalDevices(static_cast<plume::VulkanInterface*>(g_interface.get())->instance);
                g_device = g_interface->createDevice(preferredGpu);
            }
#endif
            const auto openedDevice = [&] {
                if (g_device) {
                    const uint64_t nextEpoch = g_deviceEpoch.fetch_add(1, std::memory_order_acq_rel) + 1;
                    frame_plan::ResetSizing(nextEpoch);
                    const auto& description = g_device->getDescription();
                    LOG_INFO("video device: backend={} name={} driver_raw={} vendor_enum={} type_enum={} reported_device_memory_bytes={}",
                        backend::Name(candidate), description.name, description.driverVersion,
                        uint32_t(description.vendor), uint32_t(description.type), description.dedicatedVideoMemory);
                }
                return backend::Inspect(candidate, g_device.get());
            };
            auto capabilities = openedDevice();
            if (!preferredGpu.empty()) {
                const auto missing = backend::Missing(candidate, capabilities);
                if (g_device && g_device->getDescription().name != preferredGpu) {
                    LOG_WARNING("video: GPU \"{}\" could not be opened; automatic selection chose \"{}\"",
                        preferredGpu, g_device->getDescription().name);
                } else if (!missing.empty()) { // Includes a failed creation.
                    // The chosen adapter must not end the only candidate (Linux/macOS).
                    LOG_WARNING("video: GPU \"{}\" is unusable ({}); using automatic selection", preferredGpu, missing);
                    g_device.reset();
                    g_device = g_interface->createDevice();
                    capabilities = openedDevice();
                }
            }
            {
                std::vector<std::string> names;
                for (const auto& name : g_interface->getDeviceNames())
                    if (std::find(names.begin(), names.end(), name) == names.end()) names.push_back(name);
                std::lock_guard lock(g_gpuNamesMutex);
                g_gpuDeviceNames = std::move(names);
                g_activeGpuDeviceName = g_device ? g_device->getDescription().name : std::string{};
            }
            g_textureCompressionBC.store(capabilities.textureCompressionBC, std::memory_order_relaxed);
            if (g_device && candidate == backend::Backend::Vulkan)
                LOG_INFO("vulkan limits: sets={} samplers={} sampled_images={} storage_buffers={} push_constants={} bc={}",
                    capabilities.boundSets, capabilities.samplers, capabilities.sampledImages,
                    capabilities.storageBuffers, capabilities.pushConstants, capabilities.textureCompressionBC ? 1 : 0);
            if (const auto missing = backend::Missing(candidate, capabilities); !missing.empty()) return missing;
            if (g_vulkan && g_dlssController) {
                bool retainNgxForFg = false;
#if defined(LO_ENABLE_STREAMLINE_FG) && defined(_WIN32)
                retainNgxForFg = g_fgDispatch && g_fgDispatch->FeatureSupported();
#endif
                g_dlssController->ProbeOnce(*static_cast<plume::VulkanInterface*>(g_interface.get()),
                    *static_cast<plume::VulkanDevice*>(g_device.get()), retainNgxForFg);
                LogDlssProbe(g_dlssController->Report());
            }
#ifdef _WIN32
            else if (g_dlssController) {
                g_dlssController->ProbeOnce(*static_cast<plume::D3D12Device*>(g_device.get()));
                LogDlssProbe(g_dlssController->Report());
            }
#endif
#if defined(LO_ENABLE_D3D12_FG) && defined(_WIN32)
            if (!g_vulkan) {
                const auto fg = frame_generation::ResolveD3D12Selection(settings::GetConfig(), std::getenv("LO_FG_PROVIDER"),
                    std::getenv("LO_FG_MODE"), std::getenv("LO_FG_MULTIPLIER"),
                    std::getenv("LO_FG_TARGET_FPS"), std::getenv("LO_DLSS_FG"), g_displayRefreshHz.load(std::memory_order_relaxed));
                if (fg.error) LOG_ERROR("D3D12 FG: {}", fg.error);
                if (fg.Enabled()) {
                    auto bridge = std::make_unique<frame_generation::D3D12Bridge>();
                    const auto runtime = D3D12FgRuntime(fg.config.provider);
                    std::string reason;
                    if (bridge->Initialize(*static_cast<plume::D3D12Device*>(g_device.get()), fg.config, runtime, reason)) {
                        g_d3dFg = std::move(bridge);
                        std::lock_guard lock(g_fgSettingsMutex);
                        g_fgAppliedConfig = fg.config; // Exact request used by Initialize, not a later monitor sample.
                        g_fgSessionProvider = fg.config.provider;
                    }
                    else LOG_ERROR("D3D12 FG: unavailable; ordinary presentation retained: {}", reason);
                }
            }
#endif
            g_queue = g_device->createCommandQueue(plume::RenderCommandListType::DIRECT);
            if (!g_queue) return "graphics queue creation failed";
#if defined(LO_ENABLE_STREAMLINE_FG) && defined(_WIN32)
            if (g_fgDispatch && g_fgDispatch->FeatureSupported()) {
                std::string reason;
                if (!g_fgDispatch->InstallDeviceHooks(static_cast<plume::VulkanInterface*>(g_interface.get())->instance,
                    static_cast<plume::VulkanDevice*>(g_device.get())->vk, reason)) return reason;
                g_fgSession = std::make_unique<dlss_fg::Session>(*g_fgRuntime,
                    *static_cast<plume::VulkanDevice*>(g_device.get()), *static_cast<plume::VulkanCommandQueue*>(g_queue.get()));
                if (!g_fgSession->Initialize()) return "DLSS FG presentation initialization failed";
                g_fgWindowSynchronization = true;
                std::lock_guard lock(g_fgSettingsMutex);
                g_fgSessionProvider = framegen::Provider::Dlss;
            }
#endif
#if defined(LO_ENABLE_VULKAN_FSR_FG) && defined(_WIN32)
            if (g_vulkan) {
                const auto fg = VulkanFgRequest();
                if (fg.Enabled() && fg.config.provider == framegen::Provider::Fsr) {
                    auto session = std::make_unique<fsr_fg::Session>();
                    std::string reason;
                    if (session->Initialize(*static_cast<plume::VulkanDevice*>(g_device.get()),
                        *static_cast<plume::VulkanCommandQueue*>(g_queue.get()),
                        FidelityFxRuntime("LO_FSR_VULKAN_FG_RUNTIME", "amd_fidelityfx_vk.dll"), reason)) {
                        g_fsrVulkanFg = std::move(session);
                        g_fgWindowSynchronization = true;
                        std::lock_guard lock(g_fgSettingsMutex);
                        g_fgSessionProvider = framegen::Provider::Fsr;
                    } else LOG_ERROR("Vulkan FSR FG: unavailable; ordinary presentation retained: {}", reason);
                }
            }
#endif
            g_commandList = g_queue->createCommandList();
            g_fence = g_device->createCommandFence();
            g_acquireSemaphore = g_device->createCommandSemaphore();
            g_releaseSemaphore = g_device->createCommandSemaphore();
            if (!g_commandList || !g_fence || !g_acquireSemaphore || !g_releaseSemaphore) return "command/synchronization initialization failed";
#if defined(__ANDROID__)
            const auto initialSurfaceSerial = g_androidSurfaceChangeSerial.load();
#endif
            const auto hdrConfig = settings::GetConfig();
            // Vulkan keeps the HDR swap chain while AA/upscaling pause the
            // scene per frame. D3D12 and Metal frame generation replace the
            // swap chain in SDR and skip entirely while an HDR one exists.
            // Without frame generation, ReconcileHdrSwapchain follows the
            // setting later on.
            g_hdrSwapchain = hdrConfig.hdr && (g_vulkan ? HdrFrameGenerationCompatible() : HdrConfigurationCompatible());
            g_hdrPaperWhiteNits = float(hdrConfig.hdrPaperWhiteNits);
            g_hdrPeakNits = float(hdrConfig.hdrPeakNits);
            if (!g_hdrSwapchain && hdrConfig.hdr && g_vulkan) {
                LOG_WARNING("HDR: SDR swap chain retained; frame generation keeps its SDR swap chain here (FSR FG, or DLSS-G with LO_HDR_FG=0)");
            } else if (!g_hdrSwapchain && hdrConfig.hdr) {
                LOG_WARNING("HDR: SDR swap chain retained; frame generation keeps its SDR swap chain on this backend");
            }
            g_swapChain = g_queue->createSwapChain(SwapChainDescription());
            if (!g_swapChain || g_swapChain->isEmpty()) return "window surface/swapchain initialization failed";
#if defined(__ANDROID__)
            g_androidAppliedSurfaceChangeSerial = initialSurfaceSerial;
#endif
#if defined(LO_ENABLE_D3D12_FG) && defined(_WIN32)
            if (g_d3dFg) g_fgWindowSynchronization = true;
#endif
#if defined(LO_ENABLE_STREAMLINE_FG) && defined(_WIN32)
            if (g_fgSession) {
                // Vulkan DLSS-G does not support VSync; guest frame pacing still
                // limits real frames independently of the SDK's generated frames.
                g_swapChain->setVsyncEnabled(false);
                if (g_swapChain->needsResize() && !g_swapChain->resize()) return "DLSS FG immediate swapchain resize failed";
                if (g_swapChain->isVsyncEnabled()) return "DLSS FG requires Vulkan immediate presentation support";
                LOG_INFO("DLSS FG: Vulkan immediate presentation enabled");
            }
#endif
#if defined(LO_ENABLE_VULKAN_FSR_FG) && defined(_WIN32)
            if (g_fsrVulkanFg && g_fsrVulkanFg->UsesProxySwapchain()) {
                // The FidelityFX Vulkan presenter paces real and generated frames
                // itself. Under FIFO its image acquire and present waits stack on
                // that pacing and hold the game at a quarter of the refresh rate.
                g_swapChain->setVsyncEnabled(false);
                if (g_swapChain->needsResize() && !g_swapChain->resize()) return "FSR FG immediate swapchain resize failed";
                if (g_swapChain->isVsyncEnabled())
                    LOG_WARNING("Vulkan FSR FG: immediate presentation unavailable; FIFO limits the real frame rate");
                else LOG_INFO("Vulkan FSR FG: Vulkan immediate presentation enabled");
            }
#endif
            if (g_temporalUpscaler && settings::GetConfig().upscaler == upscaling::Upscaler::Dlss) {
                const auto output = upscaling::ResolveOutputRegion({g_swapChain->getWidth(), g_swapChain->getHeight()});
                const upscaling::SizingKey key{g_deviceEpoch.load(std::memory_order_acquire), output.width, output.height,
                    upscaling::Upscaler::Dlss, output.x, output.y};
                auto sizing = upscaling::OutputSizing{};
                if (g_vulkan)
                    sizing = upscaling::SizingService::QueryOutputSizing(*g_temporalUpscaler,
                        *static_cast<plume::VulkanInterface*>(g_interface.get()), *static_cast<plume::VulkanDevice*>(g_device.get()), key);
#ifdef _WIN32
                else
                    sizing = g_temporalUpscaler->QuerySizing(*static_cast<plume::D3D12Device*>(g_device.get()), key);
#endif
                const auto qualityIndex = static_cast<size_t>(settings::GetConfig().dlssQuality);
                const auto& activeMode = sizing.modes[qualityIndex < sizing.modes.size() ? qualityIndex : 0];
                if (activeMode.state != upscaling::SizingState::Ready) {
                    LOG_ERROR("video: DLSS initial sizing failed stage=QueryOutputSizing quality={} state={} native_result={} output={}x{} device_epoch={}",
                        qualityIndex, uint32_t(activeMode.state),
                        activeMode.ngxResult ? std::to_string(*activeMode.ngxResult) : "unavailable",
                        output.width, output.height, key.deviceEpoch);
                }
                frame_plan::PublishSizing(sizing);
            }
            LogOutputPixels("created");
            g_uploadCapacity = uint64_t(kMaxWidth) * kMaxHeight * 4;
            g_uploadBuffer = g_device->createBuffer(plume::RenderBufferDesc::UploadBuffer(g_uploadCapacity));
            if (!g_uploadBuffer) return "presentation upload allocation failed";
            g_presentation = std::make_unique<Presentation>();
            if (!g_presentation->Init(g_device.get(), g_swapChain->getFormat())) return "presentation shader/pipeline initialization failed";
            g_presentationFormat = g_swapChain->getFormat();
            if (!getenv("LO_NO_RENDERER") && !renderer::Init()) return "renderer initialization failed";
            UpdateHdrOutput(true);
            return {};
        }, ResetGpu);
        LOG_INFO("video: backend selection {}; configured={} (unchanged)", selection.Describe(), backend::Name(configured));
        if (selection.selected) {
            g_selectedBackend = static_cast<int>(*selection.selected);
            g_available = true; g_initializing = false;
#if (defined(LO_ENABLE_STREAMLINE_FG) || defined(LO_ENABLE_VULKAN_FSR_FG)) && defined(_WIN32)
            if (*selection.selected == backend::Backend::Vulkan) {
                const auto fg = VulkanFgRequest();
                // A failed startup is unavailable, not a request to restart
                // endlessly. Starting Off can be enabled after a restart.
                std::lock_guard lock(g_fgSettingsMutex);
                if (fg.Enabled() && g_fgSessionProvider == framegen::Provider::Off) {
                    g_fgFailedRequest = fg.config;
                    g_fgStartupFailure = fg.config.provider;
                }
            }
#endif
#if defined(LO_ENABLE_D3D12_FG) && defined(_WIN32)
            if (*selection.selected == backend::Backend::D3D12) {
                const auto requestedFg = frame_generation::ResolveD3D12Selection(settings::GetConfig(),
                    std::getenv("LO_FG_PROVIDER"), std::getenv("LO_FG_MODE"),
                    std::getenv("LO_FG_MULTIPLIER"), std::getenv("LO_FG_TARGET_FPS"), std::getenv("LO_DLSS_FG"), g_displayRefreshHz.load(std::memory_order_relaxed));
                std::lock_guard lock(g_fgSettingsMutex);
                if (!g_d3dFg) {
                    g_fgAppliedConfig = {};
                    g_fgSessionProvider = framegen::Provider::Off;
                }
                if (requestedFg.Enabled() && !g_d3dFg) g_fgFailedRequest = requestedFg.config;
            }
#endif
            PublishOwnedDeviceCapability();
            LOG_INFO("video: {} on {}", backend::Name(*selection.selected), g_device->getDescription().name);
        } else {
            LOG_ERROR("video: no usable backend; guest startup aborted: {}", selection.Describe());
#if defined(__ANDROID__) && !defined(LO_VIDEO_SUBMISSION_UNIT)
            for (const auto& attempt : selection.attempts)
                if (attempt.backend == backend::Backend::Vulkan) ReportGraphicsFailureToActivity(attempt.error);
#endif
            Shutdown();
            g_initAttempted = true; // A repeated call cannot silently start another retry cycle.
        }
#endif
        return g_available;
    }

    void Shutdown()
    {
        const bool exiting = ExitRequested();
        if (exiting) LOG_INFO("video: shutdown stage=gpu-reset begin");
        ResetGpu();
        if (exiting) LOG_INFO("video: shutdown stage=gpu-reset complete");
        if (exiting) LOG_INFO("video: shutdown stage=window-stop begin");
#ifdef _WIN32
        // DXGI teardown above still needs the window thread to pump messages.
        g_windowThread.request_stop();
        if (g_windowThread.joinable()) g_windowThread.join();
        g_nativeWindow = nullptr;
        g_preparationWindow = nullptr;
        g_shaderProgress = 0;
#else
        os::main_thread::Run(DestroyWindowResources);
#endif
        if (exiting) LOG_INFO("video: shutdown stage=window-stop complete");
        g_available = false;
        g_initAttempted = false;
        hid::SetExternalEventPump(false);
    }

    bool FrameGenerationInputCaptureEnabled() {
#if defined(LO_GPU_PLUME) && defined(LO_ENABLE_D3D12_FG) && defined(_WIN32)
        if (g_d3dFg && g_d3dFg->Enabled()) return true;
#endif
#if defined(LO_GPU_PLUME) && defined(LO_ENABLE_STREAMLINE_FG) && defined(_WIN32)
        if (g_fgSession && g_fgSession->Requested()) return true;
#endif
#if defined(LO_GPU_PLUME) && defined(LO_ENABLE_VULKAN_FSR_FG) && defined(_WIN32)
        if (g_fsrVulkanFg && g_fsrVulkanFg->Requested()) return true;
#endif
#if defined(LO_GPU_PLUME) && defined(__APPLE__) && defined(LO_ENABLE_METALFX_FG)
        if (g_metalFg && g_metalFg->Requested()) return true;
#endif
        return false;
    }
    bool FrameGenerationAvailable() {
#if defined(LO_GPU_PLUME) && defined(LO_ENABLE_D3D12_FG) && defined(_WIN32)
        if (g_d3dFg) return g_d3dFg->Available();
#endif
#if defined(LO_GPU_PLUME) && defined(LO_ENABLE_STREAMLINE_FG) && defined(_WIN32)
        if (g_fgSession) return g_fgSession->Available();
#endif
#if defined(LO_GPU_PLUME) && defined(LO_ENABLE_VULKAN_FSR_FG) && defined(_WIN32)
        if (g_fsrVulkanFg) return g_fsrVulkanFg->Available();
#endif
#if defined(LO_GPU_PLUME) && defined(__APPLE__) && defined(LO_ENABLE_METALFX_FG)
        if (g_metalFg) return g_metalFg->Available();
#endif
        return false;
    }
    uint32_t GetFramePacingTarget(uint32_t nativeTarget, bool hostOverlay)
    {
        const bool requested = settings::GetConfig().variableRefreshRate;
        const auto refresh = g_displayRefreshHz.load(std::memory_order_relaxed);
        uint32_t multiplier = 1;
        float dynamicTarget = 0;
        bool dynamic = false;
#if defined(LO_GPU_PLUME) && defined(LO_ENABLE_D3D12_FG) && defined(_WIN32)
        if (requested && !hostOverlay && g_d3dFg) {
            std::lock_guard lock(g_fgSettingsMutex);
            const auto caps = g_d3dFg->Supported();
            if (!g_fgFailedRequest && caps.available && framegen::Select(g_fgAppliedConfig, caps).Enabled()) {
                dynamic = g_fgAppliedConfig.mode == framegen::Mode::Dynamic;
                dynamicTarget = g_fgAppliedConfig.targetFrameRate;
                if (!dynamic) multiplier = g_fgAppliedConfig.generatedFrames + 1;
            }
        }
#endif
#if defined(LO_GPU_PLUME) && defined(LO_ENABLE_STREAMLINE_FG) && defined(_WIN32)
        // Availability is a last-frame runtime observation, not evidence that
        // the monitor is using VRR. The multiplier is the applied request.
        if (requested && !hostOverlay && g_fgSession && g_fgSession->Available()) {
            std::lock_guard lock(g_fgSettingsMutex);
            multiplier = g_fgAppliedConfig.generatedFrames + 1;
        }
#endif
#if defined(LO_GPU_PLUME) && defined(LO_ENABLE_VULKAN_FSR_FG) && defined(_WIN32)
        if (requested && !hostOverlay && g_fsrVulkanFg && g_fsrVulkanFg->Available()) multiplier = 2;
#endif
#if defined(LO_GPU_PLUME) && defined(__APPLE__) && defined(LO_ENABLE_METALFX_FG)
        if (requested && !hostOverlay && g_metalFg && g_metalFg->Available()) multiplier = 2;
#endif
        const auto paced = dynamic ? vrr::DynamicPacingTarget(nativeTarget, requested, refresh, dynamicTarget)
            : vrr::PacingTarget(nativeTarget, requested, refresh, multiplier);
        if (!hostOverlay) {
            // Bounded change-only diagnostics. Keep the stored game target intact.
            static uint64_t previousKey = ~uint64_t(0);
            static auto nextReport = std::chrono::steady_clock::time_point{};
            const uint64_t key = uint64_t(requested) | (uint64_t(refresh) << 1) |
                (uint64_t(nativeTarget) << 12) | (uint64_t(paced) << 23) |
                (uint64_t(multiplier) << 34) | (uint64_t(dynamic) << 39);
            const auto now = std::chrono::steady_clock::now();
            if (key != previousKey && now >= nextReport) {
                previousKey = key;
                nextReport = now + std::chrono::seconds(1);
                LOG_INFO("VRR pacing: requested={} refresh_hz={} native_target={} host_cap={} fg_multiplier={} dynamic={} hardware_vrr=unverified",
                    requested, refresh, nativeTarget, paced, multiplier, dynamic);
            }
        }
        return paced;
    }

    DynamicFgOutputPacing GetDynamicFgOutputPacing(uint32_t nativeTarget)
    {
#if defined(LO_GPU_PLUME) && defined(LO_ENABLE_D3D12_FG) && defined(_WIN32)
        if (!nativeTarget || !settings::GetConfig().variableRefreshRate || !g_d3dFg)
            return {};
        const auto limit = vrr::OutputLimit(g_displayRefreshHz.load(std::memory_order_relaxed));
        if (!limit) return {};
        std::lock_guard lock(g_fgSettingsMutex);
        if (g_fgAppliedConfig.provider != framegen::Provider::Dlss ||
            g_fgAppliedConfig.mode != framegen::Mode::Dynamic)
            return {};
        const auto requested = vrr::DynamicTarget(g_fgAppliedConfig.targetFrameRate, true,
            g_displayRefreshHz.load(std::memory_order_relaxed));
        const auto budget = requested > 0 ? uint32_t(std::clamp(requested, 1.0f, float(limit))) : limit;
        return {budget, g_d3dFg->ActualPresents()};
#else
        (void)nativeTarget;
        return {};
#endif
    }

    FrameGenerationStatus GetFrameGenerationStatus() {
        FrameGenerationStatus status;
        status.environmentOverride = std::getenv("LO_FG_PROVIDER") || std::getenv("LO_FG_MODE") ||
            std::getenv("LO_FG_MULTIPLIER") || std::getenv("LO_FG_TARGET_FPS") || std::getenv("LO_DLSS_FG");
#if !defined(LO_VIDEO_SUBMISSION_UNIT)
        const auto saved = settings::GetConfig();
        const auto backend = SelectedBackend();
        const auto request = frame_generation::ResolveSelection(backend.value_or(saved.graphicsBackend), saved,
            std::getenv("LO_FG_PROVIDER"), std::getenv("LO_FG_MODE"),
            std::getenv("LO_FG_MULTIPLIER"), std::getenv("LO_FG_TARGET_FPS"), std::getenv("LO_DLSS_FG"), g_displayRefreshHz.load(std::memory_order_relaxed));
        status.requested = request.config.provider;
        status.requestedMultiplier = request.config.generatedFrames + 1;
#if defined(LO_GPU_PLUME) && ((defined(_WIN32) && (defined(LO_ENABLE_D3D12_FG) || defined(LO_ENABLE_STREAMLINE_FG) || defined(LO_ENABLE_VULKAN_FSR_FG))) || (defined(__APPLE__) && defined(LO_ENABLE_METALFX_FG)))
        {
            std::lock_guard lock(g_fgSettingsMutex);
            status.applied = g_fgAppliedConfig.provider;
            status.sessionProvider = g_fgSessionProvider;
            status.appliedMultiplier = g_fgAppliedConfig.generatedFrames + 1;
            status.phase = DeriveFrameGenerationPhase({
                .backendSelected = backend.has_value(),
                .providerFixedAtStartup = backend == backend::Backend::Vulkan,
                .request = request.config,
                .requestError = request.error != nullptr,
                .applied = g_fgAppliedConfig,
                .sessionProvider = g_fgSessionProvider,
                .failedRequest = g_fgFailedRequest,
                .startupFailure = g_fgStartupFailure,
            });
        }
#else
        status.phase = status.requested == framegen::Provider::Off && !request.error
            ? FrameGenerationPhase::Off : FrameGenerationPhase::Unavailable;
#endif
#endif
        return status;
    }
    bool ExitRequested() { return g_exitRequested.load(std::memory_order_acquire); }
    void RequestExit() {
        if (!g_exitRequested.exchange(true, std::memory_order_acq_rel))
            LOG_INFO("video: exit requested; GPU owner cleanup pending");
        g_commandProcessor.RequestStopForExit();
        RequestSkipShaderPreparation();
    }
    [[noreturn]] void FinishRequestedExit() {
        if (!ExitRequested()) {
            LOG_ERROR("video: exit without an owner-thread shutdown request");
            std::fflush(nullptr); std::_Exit(EXIT_FAILURE);
        }
        // All presentation stack/recording guards have unwound at this point.
        LOG_INFO("video: shutdown stage=capture-archive begin");
        renderer::WaitDebugCaptureArchive();
        LOG_INFO("video: shutdown stage=capture-archive complete");
        Shutdown(); // Failed native/SDK drains terminate with EXIT_FAILURE.
        LOG_INFO("video: owner shutdown complete native_ngx_cleanup=complete streamline_cleanup=complete exit_code=0");
        os::shaderlog::CloseForExit();
        std::fflush(nullptr);
        std::_Exit(EXIT_SUCCESS);
    }

    void PumpEvents()
    {
#ifndef _WIN32
        os::main_thread::Run(PumpWindowEvents);
#endif
    }

    void PumpIdleEvents()
    {
        // Runs on the main thread between video-thread requests. Events stay
        // queued for PumpWindowEvents; this only keeps the host app responsive.
        if (g_window && !ExitRequested())
            SDL_PumpEvents();
    }

    void SetShaderPreparationProgress(uint32_t completed, uint32_t total, PreparationStage stage, PreparationUnit unit)
    {
        g_shaderProgress.store((std::min<uint64_t>(total,kProgressMask) << 28) |
            std::min<uint64_t>(completed,kProgressMask) | (uint64_t(stage) << 56) | (uint64_t(unit) << 60));
    }
    static std::atomic<bool> g_skipShaderPreparation{false};
    bool ShaderPreparationSkipped() { return g_skipShaderPreparation.load(); }
    void RequestSkipShaderPreparation() {
        g_skipShaderPreparation.store(true);
        LOG_INFO("video: shader preparation skip requested");
    }
    void ResetShaderPreparationSkip() { g_skipShaderPreparation.store(ExitRequested()); }
    bool DisplayModeFailed() { return g_displayFailed.load(); }
    bool WindowModeOverridden() { return g_windowModeOverridden.load(); }
    uint64_t BeginDisplayChange(const settings::Config& config) {
        const auto ticket = g_displayChanges.Begin(config.width, config.height, uint32_t(config.windowMode));
        ++g_settingsDisplayEpoch;
        g_reapplyWindow = true;
        return ticket;
    }
    DisplayChangeResult QueryDisplayChange(uint64_t ticket) { return g_displayChanges.Query(ticket); }

    namespace {
#if defined(LO_GPU_PLUME) && !defined(_WIN32)
#if defined(__ANDROID__)
    static bool EnsureAndroidSurfaceSwapChain()
    {
        if (GpuWorkStopped() || (!g_available && !g_initializing) || !g_androidSurfaceReady.load())
            return false;
        const auto surfaceSerial = g_androidSurfaceChangeSerial.load();
        if (surfaceSerial == g_androidAppliedSurfaceChangeSerial && g_swapChain) return true;
        if (!g_queue || !g_window || !WaitForPresentGpu()) return false;
        g_swapChain.reset();
        g_presentSemaphores.clear();
        g_hasPresentedImage = false;
        g_lastPresentedImage = 0;
        // Same HDR choice as before the surface went away.
        g_swapChain = g_queue->createSwapChain(SwapChainDescription());
        if (!g_swapChain || g_swapChain->isEmpty()) {
            g_swapChain.reset();
            if (!g_androidSurfaceRebuildFailureLogged)
                LOG_WARNING("video: Android native surface swapchain recreation pending serial={}", surfaceSerial);
            g_androidSurfaceRebuildFailureLogged = true;
            return false;
        }
        g_androidAppliedSurfaceChangeSerial = surfaceSerial;
        g_androidSurfaceRebuildFailureLogged = false;
        g_presentationDisplay.nativeVsyncInitialized = false;
        LOG_INFO("video: Android native surface swapchain recreated window={:#x} serial={}",
            g_androidNativeWindowIdentity.load(), surfaceSerial);
        return true;
    }
#endif
    static bool UploadAndPresentPixels(const std::vector<uint32_t>& pixels, uint32_t width, uint32_t height,
                                       bool isMenu, uint64_t displayTicket, const PresentationOptions& presentationOptions,
                                       const gpu::present_capture::Ticket *captureTicket, gpu::present_capture::Result *captureResult);
    static bool RenderPreparationScreen(PreparationStage stage, PreparationUnit unit, uint32_t done, uint32_t total)
    {
#if defined(__ANDROID__)
        if (!EnsureAndroidSurfaceSwapChain()) return false;
#endif
        if (!g_swapChain || g_swapChain->isEmpty() || (!g_available && !g_initializing) || !g_presentation)
            return false;
        const uint32_t width = g_swapChain->getWidth();
        const uint32_t height = g_swapChain->getHeight();
        if (!width || !height)
            return false;

        static std::vector<uint32_t> s_prepPixels;
        const size_t pixelCount = size_t(width) * height;
        if (s_prepPixels.size() != pixelCount)
            s_prepPixels.resize(pixelCount);

        std::fill(s_prepPixels.begin(), s_prepPixels.end(), host_ui::MakeColor(255, 20, 24, 31));

        host_ui::Rasterizer r(s_prepPixels.data(), width, height);
        const int centerY = int(height) / 2;

        const auto* titleStr = PreparationTitle(stage);
        const float titleScale = (width >= 1280 && height >= 720) ? 2.0f : 1.5f;
        const int titleW = r.MeasureWString(titleStr, titleScale);
        r.DrawWString((int(width) - titleW) / 2, centerY - 85, titleStr, host_ui::MakeColor(255, 235, 238, 242), titleScale);

        const std::wstring detail = std::to_wstring(done) + L" / " + std::to_wstring(total) + PreparationSuffix(unit);
        const float detailScale = 1.0f;
        const int countW = r.MeasureWString(detail, detailScale);
        r.DrawWString((int(width) - countW) / 2, centerY - 35, detail, host_ui::MakeColor(255, 200, 205, 215), detailScale);

        const int barWidth = std::min(480, std::max(0, int(width) - 80));
        const int barHeight = 8;
        const int barX = (int(width) - barWidth) / 2;
        const int barY = centerY + 8;
        r.FillRect(barX, barY, barWidth, barHeight, host_ui::MakeColor(255, 51, 58, 70));
        const int fillW = int(total ? uint64_t(barWidth) * std::min(done, total) / total : 0);
        if (fillW > 0)
            r.FillRect(barX, barY, fillW, barHeight, host_ui::MakeColor(255, 111, 177, 218));

        const std::wstring line1 = L"The game will continue automatically.";
        const std::wstring line2 = L"Future launches reuse the shader cache.";
        const std::wstring line3 = L"Press ESC, Space, or Controller (B) to skip.";
        const uint32_t hintColor = host_ui::MakeColor(255, 157, 168, 184);
        const int hint1W = r.MeasureWString(line1, 1.0f);
        const int hint2W = r.MeasureWString(line2, 1.0f);
        const int hint3W = r.MeasureWString(line3, 1.0f);
        r.DrawWString((int(width) - hint1W) / 2, centerY + 45, line1, hintColor, 1.0f);
        r.DrawWString((int(width) - hint2W) / 2, centerY + 68, line2, hintColor, 1.0f);
        r.DrawWString((int(width) - hint3W) / 2, centerY + 91, line3, hintColor, 1.0f);

        return UploadAndPresentPixels(s_prepPixels, width, height, true, 0, PresentationOptions{}, nullptr, nullptr);
    }
#endif

    void PollDisplayRefresh()
    {
        const auto now = std::chrono::steady_clock::now();
        if (now < g_nextRefreshPoll) return;
        g_nextRefreshPoll = now + std::chrono::milliseconds(500);
        uint32_t refresh = 0;
#ifdef _WIN32
        // The display mode can change outside SDL's cache. Query the actual
        // monitor containing this window, not the primary one.
        MONITORINFOEXW monitor{};
        monitor.cbSize = sizeof(monitor);
        DEVMODEW mode{};
        mode.dmSize = sizeof(mode);
        if (g_nativeWindow && GetMonitorInfoW(MonitorFromWindow(g_nativeWindow, MONITOR_DEFAULTTONEAREST),
                reinterpret_cast<MONITORINFO*>(&monitor)) &&
            EnumDisplaySettingsW(monitor.szDevice, ENUM_CURRENT_SETTINGS, &mode))
            refresh = mode.dmDisplayFrequency;
#else
        const int display = SDL_GetWindowDisplayIndex(g_window);
        SDL_DisplayMode mode{};
        if (display >= 0 && SDL_GetCurrentDisplayMode(display, &mode) == 0 && mode.refresh_rate > 0)
            refresh = uint32_t(mode.refresh_rate);
#endif
        if (!vrr::OutputLimit(refresh)) refresh = 0; // Unknown, not an invented 60 Hz.
        g_displayRefreshHz.store(refresh, std::memory_order_relaxed);
    }

#if LO_PLATFORM_MACOS
    // Scaling filter "MetalFX": Apple's spatial upscaler brings the scene to the
    // size presentation would scale it to (aspect kept), so the normal pass then
    // runs 1:1 with its AA, RGB range and UI. Falls back to the source on failure.
    std::unique_ptr<plume::RenderTexture> g_metalFxOutput;
    uint32_t g_metalFxWidth = 0, g_metalFxHeight = 0;
    plume::RenderTexture* UpscaleWithMetalFx(plume::RenderTexture* source, uint32_t& width, uint32_t& height)
    {
        const uint32_t outputWidth = g_swapChain->getWidth(), outputHeight = g_swapChain->getHeight();
        if (!width || !height || !outputWidth || !outputHeight) return source;
        const double scale = std::min(double(outputWidth) / width, double(outputHeight) / height);
        const uint32_t targetWidth = std::min(outputWidth, uint32_t(std::lround(width * scale)));
        const uint32_t targetHeight = std::min(outputHeight, uint32_t(std::lround(height * scale)));
        if (targetWidth <= width && targetHeight <= height) return source; // Not an upscale.
        if (!g_metalFxOutput || g_metalFxWidth != targetWidth || g_metalFxHeight != targetHeight) {
            g_metalFxOutput = g_device->createTexture(plume::RenderTextureDesc::Texture2D(targetWidth, targetHeight, 1,
                kSwapChainFormat, plume::RenderTextureFlag::RENDER_TARGET | plume::RenderTextureFlag::UNORDERED_ACCESS));
            g_metalFxWidth = g_metalFxOutput ? targetWidth : 0;
            g_metalFxHeight = g_metalFxOutput ? targetHeight : 0;
            LOG_INFO("video: MetalFX spatial upscale {}x{} -> {}x{}", width, height, targetWidth, targetHeight);
        }
        if (!g_metalFxOutput) return source;
        g_commandList->barriers(plume::RenderBarrierStage::COMPUTE,
            plume::RenderTextureBarrier(source, plume::RenderTextureLayout::SHADER_READ));
        g_commandList->barriers(plume::RenderBarrierStage::COMPUTE,
            plume::RenderTextureBarrier(g_metalFxOutput.get(), plume::RenderTextureLayout::GENERAL));
        if (!plume::EncodeMetalFxSpatialScale(g_commandList.get(), source, g_metalFxOutput.get(), width, height)) {
            static bool reported = false;
            if (!std::exchange(reported, true)) LOG_WARNING("video: MetalFX unavailable; using the regular scaling filter");
            return source;
        }
        width = targetWidth;
        height = targetHeight;
        return g_metalFxOutput.get();
    }
#endif

    void PumpWindowEvents()
    {
        if (!g_window) return;
        // DXGI Present, fullscreen transitions and swapchain release may send
        // synchronous messages to this thread during GPU-owner cleanup. Keep
        // pumping until Shutdown has released the GPU and stops this thread.
        SDL_PumpEvents();
        if (ExitRequested()) {
            // Service native messages, but do not run UI/settings/display work
            // or accumulate input events after the renderer starts tearing down.
            SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
            return;
        }
        // Service close even during an outstanding FG window handshake. The
        // regular event loop below cannot run while that handshake is pending.
        PollDisplayRefresh();
        SDL_Event closeEvent{};
        if (SDL_PeepEvents(&closeEvent, 1, SDL_GETEVENT, SDL_QUIT, SDL_QUIT) > 0) {
            RequestExit();
            return;
        }
        static uint64_t shownProgress = 0;
        static auto lastProgressPaint = std::chrono::steady_clock::time_point{};
#if defined(__ANDROID__)
        static uint64_t shownSurfaceSerial = 0;
        const bool surfaceChangePending = shownSurfaceSerial != g_androidSurfaceChangeSerial.load();
#else
        const bool surfaceChangePending = false;
#endif
        const uint64_t progress = g_shaderProgress.load();
        const auto now = std::chrono::steady_clock::now();
        // Scanning can publish hundreds of updates per second. Keep UI updates
        // at 10 Hz, but show phase transitions and completion immediately.
        const bool phaseChanged = (progress >> 56) != (shownProgress >> 56);
        const uint32_t total = uint32_t((progress >> 28) & kProgressMask);
        const uint32_t done = uint32_t(progress & kProgressMask);
#if defined(__ANDROID__)
        // The virtual pad publishes JNI snapshots, not SDL controller events.
        if (total && !ShaderPreparationSkipped() &&
            (hid::android_touch::Snapshot().buttons & XAMINPUT_GAMEPAD_B))
            RequestSkipShaderPreparation();
#endif
        if (g_window && (progress != shownProgress || surfaceChangePending) &&
            (phaseChanged || done == total || now-lastProgressPaint >= std::chrono::milliseconds(100))) {
            const auto stage=PreparationStage((progress>>56)&15);
            const auto unit=PreparationUnit(progress>>60);
            const auto title = total ? fmt::format("Lost Odyssey Recompiled - {} {}/{}{}",
                PreparationTitleNarrow(stage), done, total, PreparationSuffixNarrow(unit))
                                     : std::string("Lost Odyssey Recompiled");
            SDL_SetWindowTitle(g_window, title.c_str());
#ifdef _WIN32
            // The window thread owns the preparation overlay and stays responsive
            // while the command processor scans resources or waits for DXC.
            if (total && g_nativeWindow) {
                if (!g_preparationWindow) {
                    WNDCLASSW cls{}; cls.lpfnWndProc=PreparationWindowProc;
                    cls.hInstance=GetModuleHandleW(nullptr); cls.lpszClassName=L"LOShaderPreparation";
                    cls.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512)); RegisterClassW(&cls);
                    g_preparationWindow=CreateWindowExW(WS_EX_NOACTIVATE,cls.lpszClassName,L"",WS_CHILD|WS_VISIBLE,
                        0,0,1,1,g_nativeWindow,nullptr,cls.hInstance,nullptr);
                }
                RECT rect{}; GetClientRect(g_nativeWindow,&rect);
                RECT childRect{}; GetClientRect(g_preparationWindow,&childRect);
                if (childRect.right!=rect.right || childRect.bottom!=rect.bottom)
                    MoveWindow(g_preparationWindow,0,0,rect.right,rect.bottom,FALSE);
                InvalidateRect(g_preparationWindow,nullptr,FALSE);
            } else if (g_preparationWindow) {
                DestroyWindow(g_preparationWindow); g_preparationWindow=nullptr;
            }
#elif defined(LO_GPU_PLUME)
#if defined(__ANDROID__)
            const bool canPaintPreparation = g_queue && (g_available || g_initializing) && g_presentation;
#else
            const bool canPaintPreparation = g_swapChain && !g_swapChain->isEmpty() &&
                (g_available || g_initializing) && g_presentation;
#endif
            [[maybe_unused]] bool preparationPainted = false;
            if (total && canPaintPreparation) {
                preparationPainted = RenderPreparationScreen(stage, unit, done, total);
            } else if (!total && shownProgress != 0 && canPaintPreparation
#if defined(__ANDROID__)
                && EnsureAndroidSurfaceSwapChain()
#endif
                && g_swapChain && !g_swapChain->isEmpty()) {
                const uint32_t width = g_swapChain->getWidth();
                const uint32_t height = g_swapChain->getHeight();
                if (width && height) {
                    static std::vector<uint32_t> s_clearPixels;
                    const size_t count = size_t(width) * height;
                    if (s_clearPixels.size() != count)
                        s_clearPixels.assign(count, host_ui::MakeColor(255, 0, 0, 0));
                    preparationPainted = UploadAndPresentPixels(s_clearPixels, width, height, true, 0,
                        PresentationOptions{}, nullptr, nullptr);
                }
            }
#if defined(__ANDROID__)
            if (preparationPainted || (!total && !shownProgress))
                shownSurfaceSerial = g_androidSurfaceChangeSerial.load();
#endif
#endif
            shownProgress = progress;
            lastProgressPaint = now;
        }
        if (!g_window)
            return;
        if (settings::restart::Requested()) {
            renderer::WaitDebugCaptureArchive();
#if defined(_WIN32) || LO_PLATFORM_POSIX
            if (settings::restart::LaunchWaitingChild()) {
                RequestExit();
                return;
            }
#else
            settings::restart::ReportLaunchFailure();
#endif
        }
        auto config=settings::GetConfig();
        auto& state = g_windowDisplay;
        const auto settingsEpoch = g_settingsDisplayEpoch.load();
        if (settingsEpoch != state.settingsEpoch) {
            state.settingsEpoch = settingsEpoch;
            state.shortcutMode.reset();
            g_windowModeOverridden = false;
            state.shortcutTicket = 0;
        }
        if (state.shortcutTicket) {
            const auto result = g_displayChanges.Query(state.shortcutTicket);
            if (result != DisplayChangeResult::Pending) {
                state.shortcutTicket = 0;
                if (result == DisplayChangeResult::Failed) {
                    state.shortcutMode = state.shortcutPrevious;
                    g_windowModeOverridden = state.shortcutMode.has_value();
                    const auto restored = state.shortcutMode.value_or(config.windowMode);
                    g_displayChanges.Begin(config.width, config.height, uint32_t(restored));
                    g_reapplyWindow = true;
                    LOG_WARNING("video: fullscreen shortcut failed; restoring previous window mode");
                }
            }
        }
        if (state.shortcutMode) config.windowMode = *state.shortcutMode;
        const bool reapply = g_reapplyWindow.exchange(false);
        // Startup already created the window on the saved display.
        const bool displayChanged = state.initialized &&
            (config.displayName != state.applied.displayName || config.displayIndex != state.applied.displayIndex);
        if(reapply || displayChanged || !state.initialized || config.width!=state.applied.width || config.height!=state.applied.height || config.windowMode!=state.applied.windowMode) {
#if (defined(_WIN32) && (defined(LO_ENABLE_STREAMLINE_FG) || defined(LO_ENABLE_D3D12_FG) || defined(LO_ENABLE_VULKAN_FSR_FG))) || (defined(__APPLE__) && defined(LO_ENABLE_METALFX_FG))
            if (g_fgWindowSynchronization && g_fgWindowChange.load() != 2) {
                g_fgWindowChange = 1;
                g_reapplyWindow = true;
                return; // Presentation thread acknowledges before SDL changes the surface.
            }
#endif
            const auto ticket = g_displayChanges.WindowTicket(config.width, config.height, uint32_t(config.windowMode));
            // SDL operations remain on the message-owning thread. Hidden tests
            // must never change the user's desktop display mode.
            const auto mode=getenv("LO_BACKGROUND")?settings::WindowMode::Windowed:config.windowMode;
            const bool wasWindowed = !state.initialized || state.applied.windowMode == settings::WindowMode::Windowed;
            const bool sizeChanged = !state.initialized || config.width != state.applied.width || config.height != state.applied.height;
            if (displayChanged) {
                const auto displays = QueryDisplays();
                const int target = ChosenDisplay(config, displays);
                const int current = SDL_GetWindowDisplayIndex(g_window);
                if (target >= 0 && target != current) {
                    // SDL only records the position of a fullscreen window, and
                    // fullscreen covers the display the window is on: leave it,
                    // move, and let the mode below enter it again.
                    if (SDL_GetWindowFlags(g_window) & SDL_WINDOW_FULLSCREEN) SDL_SetWindowFullscreen(g_window, 0);
                    SDL_SetWindowPosition(g_window, SDL_WINDOWPOS_CENTERED_DISPLAY(target), SDL_WINDOWPOS_CENTERED_DISPLAY(target));
                    state.placement.valid = false; // The windowed rectangle was on the old display.
                    LOG_INFO("video: window moved from display {} to display {} \"{}\"; displays: {}",
                        current, target, displays[size_t(target)].name, DescribeDisplays(displays));
                } else {
                    LOG_INFO("video: display choice \"{}\"#{} -> {}; window stays on display {}",
                        config.displayName, config.displayIndex, target, current);
                }
            }
            if (wasWindowed && mode != settings::WindowMode::Windowed) state.placement.Capture(g_window);
            int result=SDL_SetWindowFullscreen(g_window,mode==settings::WindowMode::Borderless?SDL_WINDOW_FULLSCREEN_DESKTOP:0);
            if (result == 0 && mode == settings::WindowMode::Windowed) {
                if ((!wasWindowed || reapply) && !sizeChanged && state.placement.valid) state.placement.Restore(g_window);
                else if (sizeChanged) SDL_SetWindowSize(g_window,config.width,config.height);
            }
#ifdef _WIN32
            // SDL owns the fullscreen transition. A failed bounds repair must
            // not roll the shortcut back to windowed.
            if (result == 0 && mode == settings::WindowMode::Borderless)
                window_mode::FitBorderless(g_nativeWindow);
#endif
            g_displayFailed=result!=0;
            g_displayMode.store(int(mode));
            LOG_INFO("video: window mode={} display={} result={}", int(mode), SDL_GetWindowDisplayIndex(g_window), result);
            state.applied=config; state.initialized=true;
            g_nextRefreshPoll = {}; // Re-query after a mode transition on the next window pump.
            g_windowResizeRequested = true;
            g_displayChanges.WindowComplete(ticket, result == 0);
#if (defined(_WIN32) && (defined(LO_ENABLE_STREAMLINE_FG) || defined(LO_ENABLE_D3D12_FG) || defined(LO_ENABLE_VULKAN_FSR_FG))) || (defined(__APPLE__) && defined(LO_ENABLE_METALFX_FG))
            g_fgWindowChange = 0;
#endif
        }
        debug_menu::Update();
        hid::PumpHostInput();

        if (g_cursorManaged && !g_cursorHidden &&
            (SDL_GetWindowFlags(g_window) & SDL_WINDOW_MOUSE_FOCUS) &&
            g_lastPointerActivity != std::chrono::steady_clock::time_point{} &&
            now - g_lastPointerActivity >= kCursorIdleTimeout) {
            SetGameCursorHidden(true);
        }

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
#if defined(__ANDROID__)
            if (event.type == SDL_APP_WILLENTERBACKGROUND || event.type == SDL_APP_DIDENTERBACKGROUND) {
                g_androidWasBackgrounded = true;
                g_androidSurfaceReady = false;
            }
            if (event.type == SDL_APP_DIDENTERFOREGROUND ||
                (event.type == SDL_WINDOWEVENT && event.window.windowID == SDL_GetWindowID(g_window) &&
                 (event.window.event == SDL_WINDOWEVENT_RESIZED || event.window.event == SDL_WINDOWEVENT_RESTORED ||
                  event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED))) {
                const auto nativeWindow = CurrentAndroidNativeWindow();
                if (nativeWindow) {
                    const auto previous = g_androidNativeWindowIdentity.exchange(nativeWindow);
                    if (previous != nativeWindow || g_androidWasBackgrounded) {
                        const auto serial = g_androidSurfaceChangeSerial.fetch_add(1) + 1;
                        LOG_INFO("video: Android native surface changed old={:#x} new={:#x} serial={}",
                            previous, nativeWindow, serial);
                        g_androidWasBackgrounded = false;
                    }
                }
                g_androidSurfaceReady = nativeWindow != 0;
            }
#endif
            if (event.type == SDL_DISPLAYEVENT ||
                (event.type == SDL_WINDOWEVENT &&
                 (event.window.event == SDL_WINDOWEVENT_MOVED || event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                  event.window.event == SDL_WINDOWEVENT_DISPLAY_CHANGED || event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)))
                g_nextRefreshPoll = {};
            if (event.type == SDL_DISPLAYEVENT &&
                (event.display.event == SDL_DISPLAYEVENT_CONNECTED || event.display.event == SDL_DISPLAYEVENT_DISCONNECTED))
                LOG_INFO("video: displays changed: {}", DescribeDisplays(QueryDisplays())); // Refreshes the menu's list.
            const bool pointerActivity =
                event.type == SDL_MOUSEMOTION ||
                event.type == SDL_MOUSEBUTTONDOWN ||
                event.type == SDL_MOUSEBUTTONUP ||
                event.type == SDL_MOUSEWHEEL;
            if (pointerActivity) {
                g_lastPointerActivity = std::chrono::steady_clock::now();
                SetGameCursorHidden(false);
            }
            if (event.type == SDL_WINDOWEVENT && event.window.windowID == SDL_GetWindowID(g_window)) {
                if (event.window.event == SDL_WINDOWEVENT_ENTER) {
                    g_lastPointerActivity = std::chrono::steady_clock::now();
                    SetGameCursorHidden(false);
                }
                else if (event.window.event == SDL_WINDOWEVENT_LEAVE ||
                         event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    // Never leave the OS pointer hidden when the user leaves the game.
                    SetGameCursorHidden(false);
                }
                else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                    // Reassert the gameplay input contract after focus changes.
                    SDL_StopTextInput();
                }
            }
            if (g_shaderProgress.load() != 0) {
                if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                    if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_SPACE ||
                        event.key.keysym.sym == SDLK_b) {
                        RequestSkipShaderPreparation();
                    }
                } else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                    if (event.cbutton.button == SDL_CONTROLLER_BUTTON_B || event.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
                        RequestSkipShaderPreparation();
                    }
                }
            }
            if (event.type == SDL_KEYUP && event.key.keysym.scancode == state.consumedKey) {
                state.consumedKey = SDL_SCANCODE_UNKNOWN;
                continue;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.scancode == state.consumedKey) continue;
            if (event.type == SDL_KEYDOWN && window_mode::IsToggleChord(event.key) &&
                window_mode::TargetsGameWindow(event.key, SDL_GetWindowID(g_window))) {
                state.consumedKey = event.key.keysym.scancode ? event.key.keysym.scancode : SDL_SCANCODE_RETURN;
                const auto next = config.windowMode == settings::WindowMode::Windowed
                    ? settings::WindowMode::Borderless : settings::WindowMode::Windowed;
                const auto ticket = g_displayChanges.TryBegin(config.width, config.height, uint32_t(next));
                if (ticket) {
                    state.shortcutPrevious = state.shortcutMode;
                    state.shortcutMode = next;
                    g_windowModeOverridden = true;
                    state.shortcutTicket = ticket;
                    g_reapplyWindow = true;
                    LOG_INFO("video: Alt+Enter requested window mode {}", uint32_t(next));
                } else {
                    LOG_WARNING("video: Alt+Enter ignored; display change still pending");
                }
                continue;
            }
            if (event.type == SDL_KEYDOWN && window_mode::TargetsGameWindow(event.key, SDL_GetWindowID(g_window)) &&
                !debug_menu::IsOverlayVisible()) {
                uint32_t key = uint32_t(event.key.keysym.sym);
                if (event.key.keysym.sym >= SDLK_KP_1 && event.key.keysym.sym <= SDLK_KP_9)
                    key = '1' + uint32_t(event.key.keysym.sym - SDLK_KP_1);
                else if (event.key.keysym.sym == SDLK_KP_0) key = '0';
                else if (event.key.keysym.sym == SDLK_KP_ENTER) key = 13;
                if (settings::CalibrationKey(key)) { hid::ClearKeyboardState(); continue; }
            }
            if (event.type == SDL_MOUSEMOTION && !debug_menu::IsOverlayVisible()) {
                int w=0,h=0; SDL_GetWindowSize(g_window,&w,&h);
                const float scale=std::min(w/1280.0f,h/720.0f);
                if (scale>0) settings::PointerDrag((event.motion.x-(w-1280*scale)*0.5f)/scale,
                    (event.motion.y-(h-720*scale)*0.5f)/scale, (event.motion.state & SDL_BUTTON_LMASK) != 0);
            }
            if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT)
                settings::PointerDrag(0,0,false);
            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
                hid::HandleKeyboardEvent(event.key.keysym.scancode, event.type == SDL_KEYDOWN);
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                state.consumedKey = SDL_SCANCODE_UNKNOWN;
                hid::ClearKeyboardState();
            }
            if (event.type == SDL_WINDOWEVENT && event.window.windowID == SDL_GetWindowID(g_window) &&
                (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || event.window.event == SDL_WINDOWEVENT_DISPLAY_CHANGED ||
#if defined(__ANDROID__)
                 event.window.event == SDL_WINDOWEVENT_RESIZED ||
#endif
                 event.window.event == SDL_WINDOWEVENT_RESTORED)) {
#ifdef _WIN32
                if (!getenv("LO_BACKGROUND") && state.applied.windowMode == settings::WindowMode::Borderless)
                    window_mode::FitBorderless(g_nativeWindow);
#endif
                g_windowResizeRequested = true;
            }
            if(event.type==SDL_MOUSEBUTTONDOWN) {
                if (!debug_menu::IsOverlayVisible()) {
                    int w=0,h=0; SDL_GetWindowSize(g_window,&w,&h);
                    const float scale=std::min(w/1280.0f,h/720.0f);
                    if(scale>0) settings::PointerClick((event.button.x-(w-1280*scale)*0.5f)/scale,
                        (event.button.y-(h-720*scale)*0.5f)/scale,event.button.button==SDL_BUTTON_RIGHT);
                }
            }
            if (event.type == SDL_KEYDOWN)
                LOG_INFO("video key: {} name: '{}' repeat {}", event.key.keysym.sym, SDL_GetKeyName(event.key.keysym.sym), event.key.repeat);
            if (event.type == SDL_KEYDOWN && !event.key.repeat && event.key.keysym.sym == SDLK_F1)
            {
                LOG_INFO("[host_ui] F1 key triggered!");
                debug_menu::Toggle();
            }
            else if (event.type == SDL_KEYDOWN && !event.key.repeat && debug_menu::IsOverlayVisible())
            {
                switch (event.key.keysym.sym)
                {
                case SDLK_UP: debug_menu::HandleInput(debug_menu::InputAction::Up); break;
                case SDLK_DOWN: debug_menu::HandleInput(debug_menu::InputAction::Down); break;
                case SDLK_LEFT: debug_menu::HandleInput(debug_menu::InputAction::Left); break;
                case SDLK_RIGHT: debug_menu::HandleInput(debug_menu::InputAction::Right); break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER: debug_menu::HandleInput(debug_menu::InputAction::Confirm); break;
                case SDLK_ESCAPE: debug_menu::HandleInput(debug_menu::InputAction::Cancel); break;
                case SDLK_TAB:
                case SDLK_q: debug_menu::HandleInput(debug_menu::InputAction::PrevTab); break;
                case SDLK_e: debug_menu::HandleInput(debug_menu::InputAction::NextTab); break;
                default: break;
                }
            }
            if (event.type == SDL_CONTROLLERDEVICEADDED || event.type == SDL_CONTROLLERDEVICEREMOVED)
                hid::HandleControllerEvent(event.type, event.cdevice.which);
            if (event.type == SDL_QUIT)
            {
                RequestExit();
                return;
            }
        }
    }

#ifdef LO_GPU_PLUME
    // Only the command/presentation thread may replace D3D12 FG ownership.
#if defined(_WIN32) && defined(LO_ENABLE_D3D12_FG)
    static bool ReconcileD3D12FrameGeneration()
    {
        if (g_vulkan || !g_device || !g_queue || !g_swapChain) return true;
        if (g_hdrSwapchain) return true; // SDR-only SDK presentation requires a restart with HDR disabled.
        // The window thread owns this handshake. It may be changing the SDL
        // surface after observing the quiescent acknowledgement.
        if (g_fgWindowChange.load() != 0) return true;
        const auto request = frame_generation::ResolveD3D12Selection(settings::GetConfig(),
            std::getenv("LO_FG_PROVIDER"), std::getenv("LO_FG_MODE"),
            std::getenv("LO_FG_MULTIPLIER"), std::getenv("LO_FG_TARGET_FPS"), std::getenv("LO_DLSS_FG"), g_displayRefreshHz.load(std::memory_order_relaxed));
        const framegen::Config desired = request.Enabled() ? request.config : framegen::Config{};
        framegen::Config applied;
        {
            std::lock_guard lock(g_fgSettingsMutex);
            applied = g_fgAppliedConfig;
            if (g_fgFailedRequest && *g_fgFailedRequest != request.config) g_fgFailedRequest.reset();
            if (g_fgFailedRequest) return true;
        }
        if (desired == applied) return true;
        if (!renderer::DrainForFrameGenerationReconfigure() || !WaitForPresentGpu()) {
            LOG_ERROR("D3D12 FG: could not drain renderer/presentation for settings change");
            return false;
        }
        if (g_d3dFg && desired.provider == framegen::Provider::Off) {
            g_d3dFg->Suspend();
            g_fgPresent.CancelAll(frame_generation::HandoffCancel::DisplayChange);
            renderer::CancelFgHandoffs();
            renderer::SetFrameGenerationInputCaptureEnabled(false);
            g_captureRetained.clear();
            g_captureCopy = {};
            std::lock_guard lock(g_fgSettingsMutex);
            g_fgAppliedConfig = {};
            g_fgFailedRequest.reset();
            LOG_INFO("D3D12 FG: disabled; Streamline retained for live DLSS SR");
            return true;
        }
        if (g_d3dFg && g_d3dFg->Provider() == framegen::Provider::Dlss &&
            desired.provider != framegen::Provider::Dlss) {
            g_d3dFg->Suspend();
            g_fgPresent.CancelAll(frame_generation::HandoffCancel::DisplayChange);
            renderer::CancelFgHandoffs();
            renderer::SetFrameGenerationInputCaptureEnabled(false);
            g_captureRetained.clear();
            g_captureCopy = {};
            std::lock_guard lock(g_fgSettingsMutex);
            g_fgAppliedConfig = {};
            g_fgFailedRequest = request.config;
            LOG_WARNING("D3D12 FG: switch from DLSS to {} requires restart while Streamline is retained for DLSS SR",
                framegen::ProviderName(desired.provider));
            return true;
        }
        if (g_d3dFg && desired.provider == g_d3dFg->Provider()) {
            std::string reason;
            if (!g_d3dFg->Reconfigure(desired, reason)) {
                LOG_ERROR("D3D12 FG: reconfiguration unavailable: {}", reason);
                std::lock_guard lock(g_fgSettingsMutex);
                g_fgFailedRequest = request.config;
                return true;
            }
            std::lock_guard lock(g_fgSettingsMutex);
            g_fgAppliedConfig = desired;
            g_fgFailedRequest.reset();
            renderer::SetFrameGenerationInputCaptureEnabled(true);
            LOG_INFO("D3D12 FG: enabled provider={} multiplier={} without swapchain replacement",
                framegen::ProviderName(desired.provider), desired.generatedFrames + 1);
            return true;
        }

        // The old SDK swapchain and all of its backbuffers must die before the
        // session DLL unloads. Keep the Plume queue wrapper address: renderer
        // command lists borrow it for future submissions.
        if (g_d3dFg) g_d3dFg->Quiesce();
        g_fgPresent.CancelAll(frame_generation::HandoffCancel::DisplayChange);
        renderer::CancelFgHandoffs();
        g_captureRetained.clear();
        g_captureCopy = {};
        g_presentedSnapshot.reset();
        g_snapshotWidth = g_snapshotHeight = 0;
        g_snapshotFormat = plume::RenderFormat::UNKNOWN;
        g_swapChain.reset();
        g_presentSemaphores.clear();
        g_d3dFg.reset();

        std::string reason;
        if (desired.provider != framegen::Provider::Off) {
            auto bridge = std::make_unique<frame_generation::D3D12Bridge>();
            const auto runtime = D3D12FgRuntime(desired.provider);
            if (bridge->Initialize(*static_cast<plume::D3D12Device*>(g_device.get()), desired, runtime, reason))
                g_d3dFg = std::move(bridge);
            else LOG_ERROR("D3D12 FG: provider switch unavailable: {}", reason);
        }
        auto* queue = static_cast<plume::D3D12CommandQueue*>(g_queue.get());
        bool queueReady = queue->recreateNativeQueue();
        if (!queueReady && g_d3dFg) {
            LOG_ERROR("D3D12 FG: provider queue creation failed; returning to ordinary presentation");
            g_d3dFg.reset();
            queueReady = queue->recreateNativeQueue();
        }
        if (!queueReady) {
            LOG_ERROR("D3D12 FG: native queue replacement failed");
            StopGpuWork(int32_t(E_FAIL));
            return false;
        }
        auto createSwapchain = [&] {
            g_swapChain = g_queue->createSwapChain(
                plume::RenderSwapChainDesc(g_nativeWindow, kSwapChainFormat, kSwapChainBuffers));
            return g_swapChain && !g_swapChain->isEmpty();
        };
        bool swapReady = createSwapchain();
        if (!swapReady && g_d3dFg) {
            g_swapChain.reset();
            LOG_ERROR("D3D12 FG: provider swapchain creation failed; returning to ordinary presentation");
            g_d3dFg.reset();
            if (queue->recreateNativeQueue()) swapReady = createSwapchain();
        }
        if (!swapReady) {
            LOG_ERROR("D3D12 FG: ordinary swapchain recovery failed");
            StopGpuWork(int32_t(E_FAIL));
            return false;
        }
        g_fgWindowSynchronization = bool(g_d3dFg);
        renderer::SetFrameGenerationInputCaptureEnabled(bool(g_d3dFg));
        g_presentationDisplay = {};
        g_forceSwapResize = true;
        g_hasPresentedImage = false;
        g_lastPresentedImage = 0;
        g_fgPresentSerial = 0;
        {
            std::lock_guard lock(g_fgSettingsMutex);
            g_fgAppliedConfig = g_d3dFg ? desired : framegen::Config{};
            g_fgSessionProvider = g_d3dFg ? desired.provider : framegen::Provider::Off;
            g_fgFailedRequest = desired.provider != framegen::Provider::Off && !g_d3dFg
                ? std::optional(request.config) : std::nullopt;
        }
        LOG_INFO("D3D12 FG: provider switch requested={} applied={} multiplier={} swapchain_replaced=1",
            framegen::ProviderName(desired.provider), g_d3dFg ? framegen::ProviderName(desired.provider) : "off",
            desired.generatedFrames + 1);
        return true;
    }
#endif
#if defined(_WIN32) && (defined(LO_ENABLE_STREAMLINE_FG) || defined(LO_ENABLE_VULKAN_FSR_FG))
    static bool ReconcileVulkanFrameGeneration()
    {
        if (!g_vulkan || g_fgWindowChange.load() != 0) return true;
        const auto request = VulkanFgRequest();
        auto desired = request.Enabled() ? request.config : framegen::Config{};
        {
            std::lock_guard lock(g_fgSettingsMutex);
            if (g_fgFailedRequest && *g_fgFailedRequest != request.config) g_fgFailedRequest.reset();
            if (g_fgFailedRequest || desired == g_fgAppliedConfig) return true;
            if (g_fgSessionProvider == framegen::Provider::Off) return true;
            // Each SDK owns its WSI hooks. Switching providers needs a restart;
            // stop the old feature now so it cannot masquerade as the new one.
            if (request.Enabled() && request.config.provider != g_fgSessionProvider) {
                desired = {};
                if (desired == g_fgAppliedConfig) return true;
            }
        }
        if (!renderer::DrainForFrameGenerationReconfigure() || !WaitForPresentGpu()) return false;
        g_fgPresent.CancelAll(frame_generation::HandoffCancel::DisplayChange);
        renderer::CancelFgHandoffs();
        std::string reason;
        bool applied = false;
#if defined(LO_ENABLE_STREAMLINE_FG)
        if (g_fgSession) applied = g_fgSession->Reconfigure(desired, reason);
#endif
#if defined(LO_ENABLE_VULKAN_FSR_FG)
        if (g_fsrVulkanFg) applied = g_fsrVulkanFg->Reconfigure(desired, reason);
#endif
        renderer::SetFrameGenerationInputCaptureEnabled(applied && desired.provider != framegen::Provider::Off);
        {
            std::lock_guard lock(g_fgSettingsMutex);
            g_fgAppliedConfig = applied ? desired : framegen::Config{};
            g_fgFailedRequest = !applied || request.error ? std::optional(request.config) : std::nullopt;
        }
        if (!applied || request.error)
            LOG_ERROR("Vulkan FG: unavailable; ordinary rendering retained: {}", request.error ? request.error : reason);
        else
            LOG_INFO("Vulkan FG: provider={} multiplier={} input_capture={}",
                uint32_t(desired.provider), desired.generatedFrames + 1, desired.provider != framegen::Provider::Off);
        return true;
    }
#endif

#if defined(__APPLE__) && defined(LO_ENABLE_METALFX_FG)
    static bool ReconcileMetalFrameGeneration()
    {
        if (!g_metal) return true;
        if (g_hdrSwapchain) return true;
        const auto request = MetalFgRequest();
        const auto desired = request.Enabled() ? request.config : framegen::Config{};
        {
            std::lock_guard lock(g_fgSettingsMutex);
            if (g_fgFailedRequest && *g_fgFailedRequest != request.config) g_fgFailedRequest.reset();
            if (g_fgFailedRequest || desired == g_fgAppliedConfig) return true;
        }
        if (!renderer::DrainForFrameGenerationReconfigure() || !WaitForPresentGpu()) return false;
        g_fgPresent.CancelAll(frame_generation::HandoffCancel::DisplayChange);
        renderer::CancelFgHandoffs();
        std::string reason;
        if (!g_metalFg && request.Enabled()) {
            auto session = std::make_unique<metalfx_fg::Session>();
            if (session->Initialize(g_device.get(), reason)) g_metalFg = std::move(session);
        }
        const bool applied = g_metalFg ? g_metalFg->Reconfigure(desired, reason) : desired.provider == framegen::Provider::Off;
        renderer::SetFrameGenerationInputCaptureEnabled(applied && request.Enabled());
        {
            std::lock_guard lock(g_fgSettingsMutex);
            g_fgAppliedConfig = applied ? desired : framegen::Config{};
            g_fgSessionProvider = g_metalFg ? framegen::Provider::MetalFx : framegen::Provider::Off;
            g_fgFailedRequest = applied ? std::nullopt : std::optional(request.config);
        }
        if (!applied) LOG_ERROR("MetalFX FG: unavailable; ordinary presentation retained: {}", reason);
        return true;
    }
#endif

#if LO_PLATFORM_MACOS
    // plume's HDR_LINEAR chain puts the CAMetalLayer in EDR with an extended
    // linear sRGB colorspace; an SDR chain on the same layer needs both back to
    // the defaults a fresh SDR start has. AppKit layers change on the main thread.
    static void ResetMetalLayerToSdr()
    {
        void* layer = g_metalLayer;
        if (!layer) return;
        os::main_thread::Run([layer] {
            auto object = static_cast<id>(layer);
            reinterpret_cast<void (*)(id, SEL, void*)>(objc_msgSend)(object, sel_registerName("setColorspace:"), nullptr);
            reinterpret_cast<void (*)(id, SEL, BOOL)>(objc_msgSend)(object,
                sel_registerName("setWantsExtendedDynamicRangeContent:"), NO);
        });
    }
#endif

    // HDR follows the setting without a restart: the swap chain is replaced in
    // the other format, and the resize in PreparePresentation rebuilds the
    // presentation pipelines for it. Frame generation owns or hooks the swap
    // chain, so while it is selected, or once a session started in this
    // process, the HDR choice waits for a restart as before.
    static bool ReconcileHdrSwapchain()
    {
        const bool desired = settings::GetConfig().hdr;
        if (desired == g_hdrSwapchain || !g_queue || !g_swapChain) {
            g_hdrSwapchainRefused.reset();
            return true;
        }
        if (g_hdrSwapchainRefused == desired) return true;
        const auto fg = frame_generation::ResolveSelection(g_metal ? backend::Backend::Metal :
            g_vulkan ? backend::Backend::Vulkan : backend::Backend::D3D12,
            settings::GetConfig(), std::getenv("LO_FG_PROVIDER"), std::getenv("LO_FG_MODE"),
            std::getenv("LO_FG_MULTIPLIER"), std::getenv("LO_FG_TARGET_FPS"), std::getenv("LO_DLSS_FG"));
        bool session = false;
#if defined(LO_ENABLE_D3D12_FG) && defined(_WIN32)
        session |= bool(g_d3dFg);
#endif
#if defined(LO_ENABLE_STREAMLINE_FG) && defined(_WIN32)
        session |= bool(g_fgSession);
#endif
#if defined(LO_ENABLE_VULKAN_FSR_FG) && defined(_WIN32)
        session |= bool(g_fsrVulkanFg);
#endif
#if defined(__APPLE__) && defined(LO_ENABLE_METALFX_FG)
        session |= bool(g_metalFg);
#endif
#if (defined(_WIN32) && (defined(LO_ENABLE_D3D12_FG) || defined(LO_ENABLE_STREAMLINE_FG) || defined(LO_ENABLE_VULKAN_FSR_FG))) || (defined(__APPLE__) && defined(LO_ENABLE_METALFX_FG))
        {
            std::lock_guard lock(g_fgSettingsMutex);
            session |= g_fgSessionProvider != framegen::Provider::Off;
        }
#endif
        if (fg.Enabled() || session) return true;
        if (!renderer::DrainForFrameGenerationReconfigure() || !WaitForPresentGpu()) {
            LOG_ERROR("HDR: could not drain renderer/presentation for swap chain replacement");
            return false;
        }
        g_fgPresent.CancelAll(frame_generation::HandoffCancel::DisplayChange);
        renderer::CancelFgHandoffs();
        g_captureRetained.clear();
        g_captureCopy = {};
        g_presentedSnapshot.reset();
        g_snapshotWidth = g_snapshotHeight = 0;
        g_snapshotFormat = plume::RenderFormat::UNKNOWN;
        // The frozen scene copies keep the format they were made in.
        g_hdrCalibrationCache = {};
        settings::SetHdrCalibrationSceneAvailable(false);
        g_swapChain.reset();
        g_presentSemaphores.clear();
        g_hdrSwapchain = desired;
#if LO_PLATFORM_MACOS
        if (!g_hdrSwapchain) ResetMetalLayerToSdr();
#endif
        g_swapChain = g_queue->createSwapChain(SwapChainDescription());
        if (!g_swapChain && g_hdrSwapchain) {
            // Keep presenting in SDR and stop retrying until the setting changes.
            LOG_ERROR("HDR: HDR swap chain creation failed; SDR retained until the setting changes");
            g_hdrSwapchainRefused = desired;
            g_hdrSwapchain = false;
#if LO_PLATFORM_MACOS
            ResetMetalLayerToSdr();
#endif
            g_swapChain = g_queue->createSwapChain(SwapChainDescription());
        }
        if (!g_swapChain) {
            LOG_ERROR("HDR: swap chain replacement failed");
            StopGpuWork(-1);
            return false;
        }
        // As after a frame generation swap chain replacement: re-apply the
        // display mode and vsync policy, and resize before the next acquire.
        g_presentationDisplay = {};
        g_forceSwapResize = true;
        g_hasPresentedImage = false;
        g_lastPresentedImage = 0;
        g_fgPresentSerial = 0;
        g_hdrReportedActive.reset();
        LOG_INFO("HDR: swap chain replaced hdr={} format={}", g_hdrSwapchain, uint32_t(g_swapChain->getFormat()));
        return true;
    }

    // Sole presentation-thread entry for mode changes and swap-chain recovery.
    // Both guest frames and host-only frames call this BEFORE reading dimensions
    // or rasterizing UI. The returned ticket belongs to these prepared operations.
    static bool PreparePresentation(uint64_t& displayTicket, uint32_t& width, uint32_t& height)
    {
#if (defined(_WIN32) && (defined(LO_ENABLE_STREAMLINE_FG) || defined(LO_ENABLE_D3D12_FG) || defined(LO_ENABLE_VULKAN_FSR_FG))) || (defined(__APPLE__) && defined(LO_ENABLE_METALFX_FG))
        if (g_fgWindowChange.load() == 1) {
#if defined(LO_ENABLE_STREAMLINE_FG)
            if (g_fgSession) g_fgSession->Quiesce();
#endif
#if defined(LO_ENABLE_VULKAN_FSR_FG) && defined(_WIN32)
            if (g_fsrVulkanFg) g_fsrVulkanFg->Quiesce();
#endif
#if defined(LO_ENABLE_D3D12_FG)
            if (g_d3dFg) g_d3dFg->Quiesce();
#endif
            g_fgWindowChange = 2;
        }
#endif
        if (!g_available || GpuWorkStopped())
            return false;
#if defined(__ANDROID__)
        if (!EnsureAndroidSurfaceSwapChain()) return false;
#endif
        if (!g_swapChain) return false;
        UpdateHdrOutput();
        if (!ReconcileHdrSwapchain()) return false;
        // Shared by real game frames and paused host overlays. Saving a new
        // cap applies here before any swapchain acquire, even while paused.
        gpu::SetFrameRateTarget(settings::GetConfig().frameRate);
#if defined(_WIN32) && defined(LO_ENABLE_D3D12_FG)
        if (!ReconcileD3D12FrameGeneration()) return false;
#endif
#if defined(_WIN32) && (defined(LO_ENABLE_STREAMLINE_FG) || defined(LO_ENABLE_VULKAN_FSR_FG))
        if (!ReconcileVulkanFrameGeneration()) return false;
#endif
#if defined(__APPLE__) && defined(LO_ENABLE_METALFX_FG)
        if (!ReconcileMetalFrameGeneration()) return false;
#endif
        const auto nativeTarget = gpu::GetFrameRateTarget();
        auto& nativePolicy = g_presentationDisplay;
        if (!nativePolicy.nativeVsyncInitialized) {
            nativePolicy.nativeVsyncBaseline = g_swapChain->isVsyncEnabled();
            nativePolicy.nativeVsyncRequested = nativePolicy.nativeVsyncBaseline;
            nativePolicy.nativeVsyncInitialized = true;
            nativePolicy.nativeVsyncReportPending = true;
        }
        bool forceImmediate = false;
#if defined(LO_ENABLE_STREAMLINE_FG) && defined(_WIN32)
        // Retain the existing Vulkan DLSS-G requirement at every native cap.
        forceImmediate = bool(g_fgSession);
#endif
#if defined(LO_ENABLE_VULKAN_FSR_FG) && defined(_WIN32)
        // The FidelityFX Vulkan presenter paces its own presents (see startup).
        forceImmediate = forceImmediate || (g_fsrVulkanFg && g_fsrVulkanFg->UsesProxySwapchain());
#endif
#if LO_PLATFORM_MACOS
        // Metal keeps display sync: the compositor needs it and it never tears.
        // Adaptive sync and targets above the 60 Hz guest clock present each
        // drawable for at least one game frame instead, so ProMotion displays
        // refresh at the game's cadence. 1 ms of slack keeps a frame from
        // slipping to the next refresh.
        const bool nativeVsync = true;
        (void)forceImmediate;
        {
            const uint32_t paced = GetFramePacingTarget(nativeTarget);
            const bool adaptive = settings::GetConfig().variableRefreshRate || frame_rate::NeedsImmediate(nativeTarget);
            uint32_t presentationMultiplier = 1;
#if defined(LO_ENABLE_METALFX_FG)
            if (g_metalFg && g_metalFg->Requested()) presentationMultiplier = 2;
#endif
            const double duration = (adaptive || presentationMultiplier > 1) && paced
                ? std::max(0.0, 1.0 / (double(paced) * presentationMultiplier) - 0.001) : 0.0;
            if (duration != nativePolicy.metalMinimumPresentDuration) {
                plume::SetMetalMinimumPresentDuration(g_swapChain.get(), duration);
                nativePolicy.metalMinimumPresentDuration = duration;
                LOG_INFO("video: Metal minimum present duration {:.3f} ms (pacing target {} FPS)", duration * 1000.0, paced);
            }
        }
#else
        const bool nativeVsync = vrr::HostVsyncEnabled(nativeTarget,
            nativePolicy.nativeVsyncBaseline, forceImmediate, settings::GetConfig().variableRefreshRate);
#endif
        if (nativeVsync != nativePolicy.nativeVsyncRequested) {
            // Plume marks Vulkan's swapchain for resize; the existing transaction
            // below quiesces FG, cancels leases and waits before replacing images.
            // Cache the request, not isVsyncEnabled(), to avoid per-frame retries
            // when the WSI cannot supply immediate presentation.
            g_swapChain->setVsyncEnabled(nativeVsync);
            nativePolicy.nativeVsyncRequested = nativeVsync;
            nativePolicy.nativeVsyncReportPending = true;
        }
        displayTicket = g_displayChanges.PresentationTicket();
        if (g_windowResizeRequested.exchange(false)) g_forceSwapResize = true;
        if (displayTicket && displayTicket != g_presentationDisplay.resizedTicket) {
            g_forceSwapResize = true;
            g_presentationDisplay.resizedTicket = displayTicket;
        }
        // Empty is recoverable: minimized Vulkan surfaces may have zero extent.
        // Never return for isEmpty() before giving resize() a chance to recover.
#if defined(__ANDROID__)
        // Android WSI can mandate a native currentExtent larger than SDL's
        // requested size. Plume's needsResize() compares those two extents and
        // would rebuild the same swapchain on every frame. SDL resize events
        // set g_forceSwapResize; preserve out-of-date and present-mode requests.
        const auto* androidSwap = static_cast<const plume::VulkanSwapChain*>(g_swapChain.get());
        const bool backendNeedsResize = androidSwap->surfaceOutOfDate ||
            androidSwap->requiredPresentMode != androidSwap->createdPresentMode;
#else
        const bool backendNeedsResize = g_swapChain->needsResize();
#endif
        if (g_forceSwapResize || g_swapChain->isEmpty() || backendNeedsResize) {
#if defined(LO_ENABLE_D3D12_FG) && defined(_WIN32)
            if (g_d3dFg) g_d3dFg->Quiesce();
#endif
#if defined(LO_ENABLE_STREAMLINE_FG) && defined(_WIN32)
            if (g_fgSession) g_fgSession->Quiesce();
#endif
#if defined(LO_ENABLE_VULKAN_FSR_FG) && defined(_WIN32)
            if (g_fsrVulkanFg) g_fsrVulkanFg->Quiesce();
#endif
            g_fgPresent.CancelAll(frame_generation::HandoffCancel::DisplayChange);
            renderer::CancelFgHandoffs();
            if (!WaitForPresentGpu()) return false;
#if defined(__APPLE__) && defined(LO_ENABLE_METALFX_FG)
            if (g_metalFg) g_metalFg->SuspendAfterHostDrain();
#endif
            if (!g_swapChain->resize()) {
                // Zero extent is transient. Retain the pending transaction and
                // resize request until the window has a drawable extent again.
                if (g_swapChain->getWidth() && g_swapChain->getHeight()) {
                    if (g_forceSwapResize) g_displayFailed = true;
                    g_displayChanges.Complete(displayTicket, false);
                }
                return false;
            }
            g_forceSwapResize = false;
            g_hasPresentedImage = false;
            g_presentedSnapshot.reset();
            g_snapshotFormat = plume::RenderFormat::UNKNOWN;
            g_snapshotWidth = g_snapshotHeight = 0;
            if (g_presentationFormat != g_swapChain->getFormat()) {
                auto replacement = std::make_unique<Presentation>();
                if (!replacement->Init(g_device.get(), g_swapChain->getFormat())) {
                    LOG_ERROR("HDR: presentation pipeline rebuild failed after swapchain format change");
                    g_displayFailed = true;
                    return false;
                }
                g_presentation = std::move(replacement);
                g_presentationFormat = g_swapChain->getFormat();
            }
            UpdateHdrOutput(true);
            LogOutputPixels("resized");
        }
        if (g_swapChain->isEmpty())
            return false;
        if (nativePolicy.nativeVsyncReportPending) {
            const bool reportedVsync = g_swapChain->isVsyncEnabled();
            LOG_INFO("native presentation: target={} requested_vsync={} reported_vsync={} guest_refresh={} clocks=unchanged",
                nativeTarget, nativePolicy.nativeVsyncRequested, reportedVsync, frame_rate::kGuestRefreshHz);
            if (!nativePolicy.nativeVsyncRequested && reportedVsync) {
                LOG_WARNING("native presentation: immediate mode unavailable; display synchronization may limit native FPS");
                // Accept the backend's fallback instead of leaving a mismatched
                // required/created mode that could rebuild on every frame.
                g_swapChain->setVsyncEnabled(reportedVsync);
            }
            nativePolicy.nativeVsyncReportPending = false;
        }
        width = g_swapChain->getWidth();
        height = g_swapChain->getHeight();
        return width != 0 && height != 0;
    }

    // Publish pixels, dimensions and provenance together on the presentation thread.
    static void CacheCpuFrame(const std::vector<uint32_t>& pixels, uint32_t width, uint32_t height)
    {
        g_pixels = pixels;
        g_frameWidth = width;
        g_frameHeight = height;
        g_frameOnGpu = false;
    }

    // The scene resolve to copy into copies[next] for the calibration previews,
    // (re)allocating both copies on a size change; null when there is none. The caller
    // records RecordSceneCopy and, once submitted, calls SceneCopySubmitted.
    static plume::RenderTexture* PrepareSceneCopy(uint32_t address, uint64_t ordinal, uint32_t width, uint32_t height)
    {
        auto& cache = g_hdrCalibrationCache;
        const bool allocate = !cache.copies[0] || cache.width != width || cache.height != height;
        // The copies keep the format they were made with: the HDR twin when the
        // first frame has one, otherwise the SDR resolve presentation shows. A
        // frame without the twin the copies need is skipped.
        uint32_t sourceWidth = 0, sourceHeight = 0, sourceFormat = 0;
        plume::RenderTexture* source = allocate || cache.extended
            ? renderer::AcquireResolvedWrite(address, ordinal, true, sourceWidth, sourceHeight, sourceFormat) : nullptr;
        const bool extended = source != nullptr;
        if (!source && ((!allocate && cache.extended) || !(source = renderer::AcquireResolvedWrite(address, ordinal, false,
                sourceWidth, sourceHeight, sourceFormat)) || plume::RenderFormat(sourceFormat) != kSwapChainFormat))
            return nullptr;
        if (sourceWidth < width || sourceHeight < height) return nullptr;
        if (allocate) {
            for (auto& copy : cache.copies)
                copy = g_device->createTexture(plume::RenderTextureDesc::Texture2D(width, height, 1,
                    extended ? plume::RenderFormat::R16G16B16A16_FLOAT : kSwapChainFormat));
            cache.width = width;
            cache.height = height;
            cache.extended = extended;
            cache.filled[0] = cache.filled[1] = false;
        }
        return cache.copies[0] && cache.copies[1] ? source : nullptr;
    }

    static void RecordSceneCopy(plume::RenderTexture* source)
    {
        const auto& cache = g_hdrCalibrationCache;
        auto* target = cache.copies[cache.next].get();
        g_commandList->barriers(plume::RenderBarrierStage::COPY,
            plume::RenderTextureBarrier(source, plume::RenderTextureLayout::COPY_SOURCE));
        g_commandList->barriers(plume::RenderBarrierStage::COPY,
            plume::RenderTextureBarrier(target, plume::RenderTextureLayout::COPY_DEST));
        const plume::RenderBox sceneBox(0, 0, int32_t(cache.width), int32_t(cache.height), 0, 1);
        g_commandList->copyTextureRegion(plume::RenderTextureCopyLocation::Subresource(target),
            plume::RenderTextureCopyLocation::Subresource(source), 0, 0, 0, &sceneBox);
    }

    static void SceneCopySubmitted(std::chrono::steady_clock::time_point now)
    {
        auto& cache = g_hdrCalibrationCache;
        cache.filled[cache.next] = true;
        cache.next ^= 1;
        cache.copied = now;
    }

    // Copies stop refreshing and the previews may sample the older one.
    static void FreezeScene()
    {
        auto& cache = g_hdrCalibrationCache;
        if (cache.ready) return;
        const uint32_t pick = cache.filled[cache.next] ? cache.next : cache.next ^ 1;
        if (!cache.filled[pick]) return;
        cache.scene = cache.copies[pick].get();
        cache.ready = true;
        settings::SetHdrCalibrationSceneAvailable(true);
        static uint32_t logs = 0;
        if (logs++ < 8)
            LOG_INFO("calibration: frozen game scene {}x{} extended={}", cache.width, cache.height, cache.extended);
    }

    static bool UploadAndPresentPixels(const std::vector<uint32_t>& pixels, uint32_t width, uint32_t height,
                                       bool isMenu, uint64_t displayTicket, const PresentationOptions& presentationOptions,
                                       const gpu::present_capture::Ticket *captureTicket, gpu::present_capture::Result *captureResult)
    {
#if defined(__ANDROID__)
        if (!EnsureAndroidSurfaceSwapChain()) return false;
#endif
        if (GpuWorkStopped() || (!g_available && !g_initializing) || !g_swapChain || g_swapChain->isEmpty())
            return false;
        g_fgPresent.CancelAll(frame_generation::HandoffCancel::AlternatePresent);
#if defined(__APPLE__) && defined(LO_ENABLE_METALFX_FG)
        // Only an active request has interpolation history to interrupt. With FG
        // Off the session records nothing, so host-only frames do not wait.
        if (g_metalFg && g_metalFg->Requested()) {
            if (!WaitForPresentGpu()) return false;
            g_metalFg->SuspendAfterHostDrain();
        }
#endif
#if defined(LO_ENABLE_D3D12_FG) && defined(_WIN32)
        if (g_d3dFg) g_d3dFg->Quiesce();
#endif
#if defined(LO_ENABLE_STREAMLINE_FG) && defined(_WIN32)
        if (g_fgSession) g_fgSession->Prepare({}, width, height, 0, VK_FORMAT_UNDEFINED);
#endif
#if defined(LO_ENABLE_VULKAN_FSR_FG) && defined(_WIN32)
        if (g_fsrVulkanFg) g_fsrVulkanFg->Quiesce();
#endif
        renderer::CancelFgHandoffs();
        DisplayCompletion completion(g_displayChanges, displayTicket);
        if (!width || !height || size_t(width) > std::numeric_limits<size_t>::max() / height ||
            pixels.size() != size_t(width) * height || width > (UINT32_MAX - 255u) / 4u) {
            return false;
        }
        // PreparePresentation has already finalized the output size. Do not
        // resize a second time after the menu has been rasterized for that size.

        // Upload the untiled pixels; rows must be 256-byte aligned for D3D12.
        const uint32_t rowPitch = (width * 4 + 255) & ~255u;
        const uint64_t requiredBytes = uint64_t(rowPitch) * height;
        try {
            if (!WaitForPresentGpu()) return false;
            // A host menu opened straight over gameplay freezes the last copy.
            if (isMenu) FreezeScene();
            // Stage allocations before acquiring an image or opening a command
            // list. Failed resizing keeps the previous usable resources intact.
            std::unique_ptr<plume::RenderBuffer> upload;
            if (!g_uploadBuffer || requiredBytes > g_uploadCapacity) {
                upload = g_device->createBuffer(plume::RenderBufferDesc::UploadBuffer(requiredBytes));
                if (!upload) return false;
            }
            std::unique_ptr<plume::RenderTexture> cpuFrame;
            if (g_presentation && (!g_cpuFrame || g_cpuWidth != width || g_cpuHeight != height)) {
                cpuFrame = g_device->createTexture(plume::RenderTextureDesc::Texture2D(width, height, 1, kSwapChainFormat));
                if (!cpuFrame) return false;
            }
            auto* uploadBuffer = upload ? upload.get() : g_uploadBuffer.get();
            auto* mapped = static_cast<uint8_t*>(uploadBuffer->map());
            if (!mapped) return false;
            for (uint32_t y = 0; y < height; ++y)
                memcpy(mapped + size_t(y) * rowPitch, &pixels[size_t(y) * width], size_t(width) * 4);
            uploadBuffer->unmap();
            if (upload) { g_uploadBuffer = std::move(upload); g_uploadCapacity = requiredBytes; }
            if (cpuFrame) { g_cpuFrame = std::move(cpuFrame); g_cpuWidth = width; g_cpuHeight = height; }
        } catch (const std::exception& error) {
            LOG_WARNING("video: CPU presentation resource preparation failed: {}", error.what());
            return false;
        }

        uint32_t imageIndex = 0;
        if (!g_swapChain->acquireTexture(g_acquireSemaphore.get(), &imageIndex))
        {
            return false;
        }
        plume::RenderTexture* backBuffer = g_swapChain->getTexture(imageIndex);
        const uint32_t copyWidth = std::min(width, g_swapChain->getWidth());
        const uint32_t copyHeight = std::min(height, g_swapChain->getHeight());

        if (!BeginGpuCommands(g_commandList.get())) return false;
        auto* uploadTarget=g_presentation?g_cpuFrame.get():backBuffer;
        g_commandList->barriers(plume::RenderBarrierStage::COPY, plume::RenderTextureBarrier(uploadTarget, plume::RenderTextureLayout::COPY_DEST));
        plume::RenderBox box(0, 0, int32_t(copyWidth), int32_t(copyHeight), 0, 1);
        g_commandList->copyTextureRegion(
            plume::RenderTextureCopyLocation::Subresource(uploadTarget),
            plume::RenderTextureCopyLocation::PlacedFootprint(g_uploadBuffer.get(), kSwapChainFormat, width, height, 1, rowPitch / 4),
            0, 0, 0, g_presentation?nullptr:&box);
        // This upload came from guest tiled memory, not the processed GPU resolve.
        // GPU-only scene-AA provenance cannot authorize skipping its legacy AA.
        if(g_presentation) {
            auto options = isMenu ? PresentationOptions{} : presentationOptions;
            const auto calibration = settings::GetHdrCalibration();
            const auto brightness = settings::GetBrightnessCalibration();
            const bool hdrPage = isMenu && calibration.open && g_hdrOutput.active;
            const bool brightnessPage = isMenu && brightness.open;
            if (hdrPage || brightnessPage) {
                // Both pages share the 1280x720 layout's preview rectangle and
                // show the menu's unsaved brightness and gamma.
                const float scale = std::min(width/1280.0f,height/720.0f);
                const float x = (width-1280*scale)*0.5f, y = (height-720*scale)*0.5f;
                options.hdrCalibration = hdrPage;
                options.brightnessPreview = brightnessPage;
                options.displayBrightness = brightness.brightness;
                options.displayGamma = brightness.gamma;
                options.calibrationRect[0] = (x+160*scale)/width;
                options.calibrationRect[1] = (y+150*scale)/height;
                options.calibrationRect[2] = (x+1120*scale)/width;
                options.calibrationRect[3] = (y+470*scale)/height;
                const bool scenePreview = hdrPage ? calibration.scenePreview && calibration.sceneAvailable
                                                  : brightness.scenePreview && brightness.sceneAvailable;
                if (scenePreview && g_hdrCalibrationCache.ready) {
                    options.calibrationScene = g_hdrCalibrationCache.scene;
                    options.calibrationSceneExtended = g_hdrCalibrationCache.extended;
                    options.calibrationExpandRgbRange = brightness.expandRgbRange;
                    options.calibrationDisplayGammaRamp = true;
                }
            }
            g_presentation->Draw(g_commandList.get(),g_cpuFrame.get(),backBuffer,width,height,
                g_swapChain->getWidth(),g_swapChain->getHeight(),options);
        }
        RecordPresentedSnapshot(backBuffer);
        QueuePresentCapture(backBuffer, captureTicket);
        PreparePresentImage(backBuffer);
        if (!EndGpuCommands(g_commandList.get())) return false;

        const plume::RenderCommandList* lists[] = { g_commandList.get() };
        plume::RenderCommandSemaphore* waitSemaphore = g_acquireSemaphore.get();
        plume::RenderCommandSemaphore* signalSemaphore = PresentSemaphore(imageIndex);
        uint64_t submissionSerial = 0; int32_t submitResult = 0;
#ifndef _WIN32
        const uint64_t d3dSignal = 0;
#else
        const uint64_t d3dSignal = g_vulkan || !g_fence ? 0 : static_cast<plume::D3D12CommandFence *>(g_fence.get())->fenceValue;
#endif
        g_captureCopy.d3dFenceValue = d3dSignal;
        FgSubmitStart();
        const bool submitted = SubmitPresentationBatch(lists, 1, &waitSemaphore, 1, &signalSemaphore, 1,
            g_fence.get(), &submissionSerial, &submitResult);
        FgHostSubmitted(submitted, submissionSerial, submitResult);
        if (!submitted) { LOG_ERROR("video: present submit failed raw_result={}", submitResult); ReadPresentCapture(captureResult, false, false, 0); return false; }
        FgPresentStart();
        const bool presented = g_swapChain->present(imageIndex, &signalSemaphore, 1);
        FgPresented(presented);
        if (presented) ++g_completedPresentCount;
        g_fgPresentSerial = submissionSerial;
        g_presentPending = true;
        g_lastPresentedImage=imageIndex; g_hasPresentedImage=true;
        completion.Complete(presented && !g_displayFailed.load());
        ReadPresentCapture(captureResult, true, presented, submissionSerial);
        return presented;
    }
#endif

    } // namespace

    uint64_t CompletedPresentCount() { return g_completedPresentCount; }
#if defined(LO_GPU_PLUME)
    void SetPresentCaptureCompletionFault(bool fail) { g_captureCompletionFault = fail; }
    uint64_t PresentCaptureAllocationCount() { return g_captureAllocs; }
    uint64_t PresentCaptureCopyCount() { return g_captureCopies; }
    uint64_t PresentCaptureMapCount() { return g_captureMaps; }
    size_t PresentCaptureRetainedBuffers() { return g_captureRetained.size() + (g_captureCopy.buffer ? 1 : 0); }
#endif

    void PresentFrontbuffer(uint32_t physicalAddress, uint32_t width, uint32_t height, uint32_t copyDestInfo,
        const gpu::present_capture::Ticket *capture, gpu::present_capture::Result *captureResult)
    {
        struct CaptureExit {
            const gpu::present_capture::Ticket *ticket;
            gpu::present_capture::Result *result;
            ~CaptureExit() {
#if defined(LO_GPU_PLUME)
                const bool gpuQueued = g_captureCopy.queued;
#else
                const bool gpuQueued = false;
#endif
                gpu::present_capture::FinishPresentCaptureExit(ticket, result, gpuQueued, [&] {
#if defined(LO_GPU_PLUME)
                    ReadPresentCapture(result, false, false, 0);
#endif
                });
            }
        } captureExit{capture, captureResult};
        if (width == 0 || height == 0 || width > kMaxWidth || height > kMaxHeight)
            return;

#ifdef LO_GPU_PLUME
        uint64_t displayTicket = 0;
        uint32_t menuWidth = 1280, menuHeight = 720;
        // Preserve headless CPU diagnostics; a live swap chain uses the same
        // preparation transaction as overlay-only presentation.
        if (g_available && !PreparePresentation(displayTicket, menuWidth, menuHeight))
            return;
        ServicePendingDlssSizing();
        renderer::SetOutputSize(menuWidth, menuHeight);
        const auto presentationConfig = settings::GetConfig();
        PresentationOptions presentationOptions{
            presentationConfig.antialiasing == 3 ? Antialiasing::SMAA : static_cast<Antialiasing>(presentationConfig.antialiasing),
            presentationConfig.scalingQuality ? ScalingFilter::Bicubic : ScalingFilter::Bilinear,
            presentationConfig.expandRgbRange};
        // Guest frames get the display gamma ramp the game wrote (#179) and the
        // saved brightness and gamma; host menus and status screens use the
        // default options without them.
        presentationOptions.displayGammaRamp = true;
        presentationOptions.displayAdjust = true;
        presentationOptions.displayBrightness = presentationConfig.displayBrightness;
        presentationOptions.displayGamma = presentationConfig.displayGamma;
        // Debug / test trigger: auto-open overlay after N frames if LO_AUTO_OVERLAY is set
        static int s_autoOverlayCountdown = []() {
            const char* env = getenv("LO_AUTO_OVERLAY");
            return env ? atoi(env) : -1;
        }();
        if (s_autoOverlayCountdown > 0) {
            if (--s_autoOverlayCountdown == 0) {
                LOG_INFO("[host_ui] LO_AUTO_OVERLAY triggered debug overlay!");
                debug_menu::Toggle();
                if (getenv("LO_AUTO_TAB")) {
                    debug_menu::HandleInput(debug_menu::InputAction::NextTab);
                }
            }
        }

        const bool hasSettings = settings::DrawMenu(g_menuPixels, g_menuRevision, menuWidth, menuHeight);
        const bool hasDebug = debug_menu::IsOverlayVisible();
        const bool menu = hasSettings || hasDebug;
        std::vector<uint32_t> menuPresentBuffer;
        if (hasDebug) {
            static host_ui::PixelBuffer s_debugOverlayBuf;
            s_debugOverlayBuf.Resize(1280, 720);
            s_debugOverlayBuf.Clear(0x00000000);
            host_ui::Rasterizer r(s_debugOverlayBuf);
            debug_menu::RenderOverlay(r);

            // Blend the logical 720p debug overlay onto the current output-sized menu buffer.
            if (!hasSettings) {
                if (size_t(menuWidth) > std::numeric_limits<size_t>::max() / size_t(menuHeight)) {
                    menuPresentBuffer.clear();
                }
                else {
                    menuPresentBuffer.assign(size_t(menuWidth) * menuHeight, host_ui::MakeColor(204, 16, 16, 24));
                }
            }
            else {
                menuPresentBuffer = g_menuPixels;
            }
            host_ui::CompositeScaled(s_debugOverlayBuf, menuWidth, menuHeight, menuPresentBuffer);
        }
        else if (hasSettings) {
            menuPresentBuffer = g_menuPixels;
        }
        // Fast path: the frontbuffer was resolved on the GPU, copy it straight
        // into the swap chain. LO_PRESENT_CPU=1 forces the untiling path below.
        static const bool cpuPresent = getenv("LO_PRESENT_CPU") != nullptr;
        if (renderer::SuppressPresent())
            return;
        if (g_available && !cpuPresent && !menu)
        {
            uint32_t rw = 0, rh = 0, rf = 0;
            frame_plan::FramePlan sourcePlan;
            frame_generation::ResolvedHandoff fgHandoff;
            plume::RenderTexture* source = renderer::AcquireResolvedSurface(physicalAddress & 0x1FFFFFFF, rw, rh, rf,
                &sourcePlan, &fgHandoff);
            if (source && plume::RenderFormat(rf) == kSwapChainFormat)
            {
                uint32_t sourceWidth=width, sourceHeight=height;
                renderer::ScaleResolvedSize(physicalAddress & 0x1FFFFFFF, sourceWidth, sourceHeight);
                sourceWidth=std::min(sourceWidth,rw); sourceHeight=std::min(sourceHeight,rh);
                bool hdrScene = false;
                plume::RenderTexture* hdrGainSource = nullptr;
                uint32_t hdrGainValidWidth = 0, hdrGainValidHeight = 0, hdrGainWidth = 0, hdrGainHeight = 0;
                if (g_hdrSceneEnabled) {
                    uint32_t hdrWidth = 0, hdrHeight = 0;
                    auto* hdrSource = renderer::AcquireHdrResolvedSurface(physicalAddress & 0x1FFFFFFF, hdrWidth, hdrHeight);
                    // MetalFX spatial scaling below outputs RGBA8; keep the SDR
                    // frame as its input and apply the scene as a gain afterwards.
                    bool metalFxUpscale = false;
#if LO_PLATFORM_MACOS
                    if (g_presentation && g_swapChain && settings::GetConfig().scalingQuality == settings::ScalingMetalFx &&
                        sourceWidth && sourceHeight) {
                        const double scale = std::min(double(g_swapChain->getWidth()) / sourceWidth, double(g_swapChain->getHeight()) / sourceHeight);
                        metalFxUpscale = std::min(g_swapChain->getWidth(), uint32_t(std::lround(sourceWidth * scale))) > sourceWidth ||
                            std::min(g_swapChain->getHeight(), uint32_t(std::lround(sourceHeight * scale))) > sourceHeight;
                    }
#endif
                    if (hdrSource && metalFxUpscale && hdrWidth >= sourceWidth && hdrHeight >= sourceHeight) {
                        hdrGainSource = hdrSource;
                        hdrGainWidth = hdrWidth; hdrGainHeight = hdrHeight;
                        hdrGainValidWidth = sourceWidth; hdrGainValidHeight = sourceHeight;
                    } else if (hdrSource && hdrWidth >= sourceWidth && hdrHeight >= sourceHeight) {
                        source = hdrSource;
                        hdrScene = true;
                        static uint32_t admitted = 0;
                        if (admitted++ < 8)
                            LOG_INFO("HDR: extended scene selected address={:#x} source={}x{} valid={}x{}",
                                physicalAddress, hdrWidth, hdrHeight, sourceWidth, sourceHeight);
                    } else if (!hdrSource && (hdrSource = renderer::AcquireHdrGainSurface(physicalAddress & 0x1FFFFFFF, hdrWidth, hdrHeight))) {
                        // Pre-upscale scene: applied as a highlight gain to the
                        // final frame after any spatial upscale below.
                        hdrGainSource = hdrSource;
                        hdrGainWidth = hdrWidth; hdrGainHeight = hdrHeight;
                        hdrGainValidWidth = sourcePlan.cpuSerial && sourcePlan.width ? std::min(sourcePlan.width, hdrWidth) : hdrWidth;
                        hdrGainValidHeight = sourcePlan.cpuSerial && sourcePlan.height ? std::min(sourcePlan.height, hdrHeight) : hdrHeight;
                    }
                }
                g_frameWidth = sourceWidth;
                g_frameHeight = sourceHeight;
                g_frontbufferPhysical = physicalAddress & 0x1FFFFFFF;
                g_frameOnGpu = true;
                if (g_swapChain->isEmpty())
                    return;
                DisplayCompletion completion(g_displayChanges, displayTicket);
                if (!WaitForPresentGpu()) return;
                // Calibration previews: scene frames refresh the copy (no earlier
                // frame still samples it after the wait); the first frame without
                // the scene freezes it.
                plume::RenderTexture* sceneCopySource = nullptr;
                uint64_t sceneOrdinal = 0;
                const auto now = std::chrono::steady_clock::now();
                if (renderer::ResolvedScene(physicalAddress & 0x1FFFFFFF, sceneOrdinal)) {
                    if (g_hdrCalibrationCache.ready) {
                        // A new stretch of gameplay starts with fresh copies.
                        g_hdrCalibrationCache.ready = false;
                        g_hdrCalibrationCache.scene = nullptr;
                        g_hdrCalibrationCache.filled[0] = g_hdrCalibrationCache.filled[1] = false;
                        settings::SetHdrCalibrationSceneAvailable(false);
                    }
                    if (now - g_hdrCalibrationCache.copied >= std::chrono::milliseconds(250))
                        sceneCopySource = PrepareSceneCopy(physicalAddress & 0x1FFFFFFF, sceneOrdinal, sourceWidth, sourceHeight);
                }
                else FreezeScene();
#if (defined(_WIN32) && (defined(LO_ENABLE_STREAMLINE_FG) || defined(LO_ENABLE_D3D12_FG) || defined(LO_ENABLE_VULKAN_FSR_FG))) || (defined(__APPLE__) && defined(LO_ENABLE_METALFX_FG))
                frame_generation::CompositeHandoff composite;
                double fgProducerWaitMs = 0.0;
                if (FrameGenerationInputCaptureEnabled()) {
                    const auto fgAcquireBegin = std::chrono::steady_clock::now();
#if defined(__APPLE__) && defined(LO_ENABLE_METALFX_FG)
                    // The first Metal FG path drains producer command buffers;
                    // queue submission order alone does not order untracked textures.
                    if (g_metalFg && !renderer::DrainForFrameGenerationReconfigure()) return;
#endif
                    const bool matched = renderer::AcquireFgCompositeInputs(physicalAddress & 0x1FFFFFFF, composite) &&
                        composite.ReadyForOrderedSubmission() && composite.outputWidth == sourceWidth && composite.outputHeight == sourceHeight &&
                        (!g_vulkan || dlss_fg::FullFramePresentation(sourceWidth, sourceHeight,
                            g_swapChain->getWidth(), g_swapChain->getHeight()));
                    if (std::getenv("LO_MV_LOG") && composite.frame % 120 == 119)
                        LOG_INFO("video FG admission: frame={} matched={} window_change={} source={}x{} swapchain={}x{} composite={}x{}",
                            composite.frame, matched, g_fgWindowChange.load(), sourceWidth, sourceHeight,
                            g_swapChain->getWidth(), g_swapChain->getHeight(), composite.outputWidth, composite.outputHeight);
                    if (!matched || g_fgWindowChange.load() != 0) composite = {};
                    fgProducerWaitMs = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - fgAcquireBegin).count();
                }
#endif
                uint32_t imageIndex = 0;
                if (!g_swapChain->acquireTexture(g_acquireSemaphore.get(), &imageIndex))
                {
                    return;
                }
                plume::RenderTexture* backBuffer = g_swapChain->getTexture(imageIndex);
                const uint32_t copyWidth = std::min(sourceWidth, g_swapChain->getWidth());
                const uint32_t copyHeight = std::min(sourceHeight, g_swapChain->getHeight());

                // Staging the exact resolve is independent of ordinary present.
                // This does not make inputs CPU/GPU consumable before completion.
                struct FgRecordingExit {
                    ~FgRecordingExit() {
#if defined(LO_ENABLE_STREAMLINE_FG) && defined(_WIN32)
                        if (g_fgSession) g_fgSession->CancelUnsubmitted(g_commandList.get());
#endif
#if defined(LO_ENABLE_VULKAN_FSR_FG) && defined(_WIN32)
                        if (g_fsrVulkanFg) g_fsrVulkanFg->CancelUnsubmitted(g_commandList.get());
#endif
#if defined(LO_ENABLE_D3D12_FG) && defined(_WIN32)
                        if (g_d3dFg) g_d3dFg->CancelUnsubmitted(g_commandList.get());
#endif
#if defined(__APPLE__) && defined(LO_ENABLE_METALFX_FG)
                        if (g_metalFg && g_metalFg->HasUnsubmitted(g_commandList.get()))
                            g_metalFg->CancelUnsubmitted(g_commandList.get(), renderer::DrainForFrameGenerationReconfigure());
#endif
                        g_fgPresent.CancelRecording();
                    }
                } fgExit;
                if (fgHandoff.packet && g_fgPresent.Select(fgHandoff, g_deviceEpoch.load(), sourceWidth, sourceHeight,
                    g_swapChain->getWidth(), g_swapChain->getHeight())) {
                    const auto& id = fgHandoff.selected;
                    LOG_INFO("video: FG selected owner={} frame={} epoch={} source={}:{} resolve={} target={}:{} producer_serial={} resolve_serial={} pending=1 ui=unavailable",
                        id.owner, id.frame, id.historyEpoch, id.sourceAllocation, id.sourceGeneration, id.resolve,
                        id.allocation, id.generation, fgHandoff.packet->producer->producerSerial, fgHandoff.packet->resolveSerial);
                }
                if (!BeginGpuCommands(g_commandList.get())) return;
                if (sceneCopySource) RecordSceneCopy(sceneCopySource);
#if defined(LO_ENABLE_D3D12_FG) && defined(_WIN32)
                if (g_d3dFg && g_d3dFg->Enabled()) {
                    g_d3dFg->PrepareAfterHostDrain(composite,
                        *static_cast<plume::D3D12SwapChain*>(g_swapChain.get()),
                        *static_cast<plume::D3D12CommandList*>(g_commandList.get()), g_deviceEpoch.load());
                    framegen::Config applied;
                    { std::lock_guard lock(g_fgSettingsMutex); applied = g_fgAppliedConfig; }
                    const auto caps = g_d3dFg->Supported();
                    if (caps.available && !framegen::Select(applied, caps).Enabled()) {
                        std::lock_guard lock(g_fgSettingsMutex);
                        g_fgFailedRequest = applied;
                    }
                }
#endif
#if defined(LO_ENABLE_STREAMLINE_FG) && defined(_WIN32)
                if (g_fgSession) {
                    auto* swap = static_cast<plume::VulkanSwapChain*>(g_swapChain.get());
                    // Borrowed swapchain texture wrappers leave imageFormat undefined.
                    // Use the format actually passed to vkCreateSwapchainKHR.
                    g_fgSession->Prepare(composite.producer, g_swapChain->getWidth(), g_swapChain->getHeight(),
                        uint32_t(swap->textures.size()), swap->createInfo.imageFormat, g_commandList.get(), fgProducerWaitMs);
                }
#endif
#if defined(LO_ENABLE_VULKAN_FSR_FG) && defined(_WIN32)
                if (g_fsrVulkanFg) g_fsrVulkanFg->PrepareAfterHostDrain(composite,
                    *static_cast<plume::VulkanSwapChain*>(g_swapChain.get()),
                    *static_cast<plume::VulkanCommandList*>(g_commandList.get()));
#endif
#if LO_PLATFORM_MACOS
                if (g_presentation && settings::GetConfig().scalingQuality == settings::ScalingMetalFx)
                    source = UpscaleWithMetalFx(source, sourceWidth, sourceHeight);
#endif
                if (g_presentation && hdrGainSource) {
                    if (auto* gained = g_presentation->ComposeHdrGain(g_commandList.get(), source, hdrGainSource,
                            sourceWidth, sourceHeight, hdrGainValidWidth, hdrGainValidHeight, hdrGainWidth, hdrGainHeight)) {
                        source = gained;
                        hdrScene = true;
                        static uint32_t gainLogs = 0;
                        if (gainLogs++ < 8)
                            LOG_INFO("HDR: highlight gain applied address={:#x} scene={}x{} (valid {}x{}) output={}x{}",
                                physicalAddress, hdrGainWidth, hdrGainHeight, hdrGainValidWidth, hdrGainValidHeight, sourceWidth, sourceHeight);
                    }
                }
                // Periodic tally of how each presented frame reached HDR (or did not).
                {
                    static uint32_t directFrames = 0, gainFrames = 0, sdrFrames = 0;
                    if (hdrScene && hdrGainSource) ++gainFrames;
                    else if (hdrScene) ++directFrames;
                    else if (g_hdrSceneEnabled) ++sdrFrames;
                    if (g_hdrSceneEnabled && (directFrames + gainFrames + sdrFrames) % 600 == 0)
                        LOG_INFO("HDR: presented frames direct={} gain={} sdr={}", directFrames, gainFrames, sdrFrames);
                }
                if(g_presentation) {
                    const auto decision = frame_plan::ResolvePresentationDecision(&sourcePlan,
                        renderer::SceneAAApplied(physicalAddress & 0x1FFFFFFF), uint32_t(presentationOptions.antialiasing),
                        uint32_t(presentationOptions.scalingFilter));
                    // Scene AA already applied: the HDR path takes Draw (not
                    // DrawComposited) and must not run the pass a second time.
                    PresentationOptions sourceOptions{decision.bypassAA ? Antialiasing::Off :
                        decision.requestedAA == 3 ? Antialiasing::SMAA : static_cast<Antialiasing>(decision.requestedAA),
                        decision.scalingQuality ? ScalingFilter::Bicubic : ScalingFilter::Bilinear,
                        presentationOptions.expandRgbRange, hdrScene};
                    sourceOptions.displayGammaRamp = presentationOptions.displayGammaRamp;
                    sourceOptions.displayAdjust = presentationOptions.displayAdjust;
                    sourceOptions.displayBrightness = presentationOptions.displayBrightness;
                    sourceOptions.displayGamma = presentationOptions.displayGamma;
                    // With scene AA already applied this is DrawComposited's pass.
                    g_presentation->Draw(g_commandList.get(),source,backBuffer,sourceWidth,sourceHeight,
                        g_swapChain->getWidth(),g_swapChain->getHeight(),sourceOptions);
                }
                else {
                    g_commandList->barriers(plume::RenderBarrierStage::COPY, plume::RenderTextureBarrier(backBuffer, plume::RenderTextureLayout::COPY_DEST));
                    plume::RenderBox box(0, 0, int32_t(copyWidth), int32_t(copyHeight), 0, 1);
                    g_commandList->copyTextureRegion(plume::RenderTextureCopyLocation::Subresource(backBuffer),
                        plume::RenderTextureCopyLocation::Subresource(source), 0, 0, 0, &box);
                }
                RecordPresentedSnapshot(backBuffer);
                QueuePresentCapture(backBuffer, capture);
#if defined(__APPLE__) && defined(LO_ENABLE_METALFX_FG)
                const bool metalGenerated = g_metalFg && g_metalFg->Record(composite, g_commandList.get(), backBuffer);
#endif
#ifndef _WIN32
                const uint64_t d3dSignal = 0;
#else
                const uint64_t d3dSignal = g_vulkan || !g_fence ? 0 : static_cast<plume::D3D12CommandFence *>(g_fence.get())->fenceValue;
#endif
                g_captureCopy.d3dFenceValue = d3dSignal;
                PreparePresentImage(backBuffer);
                if (!EndGpuCommands(g_commandList.get())) return;

                const plume::RenderCommandList* lists[] = { g_commandList.get() };
                plume::RenderCommandSemaphore* waitSemaphore = g_acquireSemaphore.get();
                plume::RenderCommandSemaphore* signalSemaphore = PresentSemaphore(imageIndex);
                uint64_t submissionSerial = 0; int32_t submitResult = 0;
                FgSubmitStart();
                const bool submitted = SubmitPresentationBatch(lists, 1, &waitSemaphore, 1, &signalSemaphore, 1,
                    g_fence.get(), &submissionSerial, &submitResult);
                FgHostSubmitted(submitted, submissionSerial, submitResult);
                if (!submitted) { LOG_ERROR("video: GPU presentation submit failed raw_result={}", submitResult); ReadPresentCapture(captureResult, false, false, 0); return; }
                g_fgPresent.Submitted(g_deviceEpoch.load(), submissionSerial);
                if (sceneCopySource) SceneCopySubmitted(now);
                FgPresentStart();
                const bool presented = g_swapChain->present(imageIndex, &signalSemaphore, 1);
                FgPresented(presented);
                if (!presented) {
                    g_fgPresent.CancelAll(frame_generation::HandoffCancel::DisplayChange);
                    renderer::CancelFgHandoffs();
                }
                if (presented) ++g_completedPresentCount;
                g_fgPresentSerial = submissionSerial;
                g_presentPending = true;
                g_lastPresentedImage=imageIndex; g_hasPresentedImage=true;
#if defined(__APPLE__) && defined(LO_ENABLE_METALFX_FG)
                bool realPresented = presented;
                if (g_metalFg && metalGenerated && presented) {
                    // Present the generated image first, then the retained real
                    // image. Guest simulation and renderer frame IDs advance once.
                    const auto presentOriginal = [&]() -> bool {
                        if (!WaitForPresentGpu()) return false;
                        uint32_t realIndex = 0;
                        if (!g_swapChain->acquireTexture(g_acquireSemaphore.get(), &realIndex)) return false;
                        auto* realBuffer = g_swapChain->getTexture(realIndex);
                        if (!BeginGpuCommands(g_commandList.get())) return false;
                        g_metalFg->RecordOriginal(g_commandList.get(), realBuffer);
                        PreparePresentImage(realBuffer);
                        if (!EndGpuCommands(g_commandList.get())) return false;
                        auto* ready = PresentSemaphore(realIndex);
                        uint64_t realSerial = 0; int32_t realResult = 0;
                        FgSubmitStart();
                        const bool submittedReal = SubmitPresentationBatch(lists, 1, &waitSemaphore, 1,
                            &ready, 1, g_fence.get(), &realSerial, &realResult);
                        FgHostSubmitted(submittedReal, realSerial, realResult);
                        if (!submittedReal) return false;
                        g_fgPresentSerial = realSerial; g_presentPending = true;
                        const bool accepted = g_swapChain->present(realIndex, &ready, 1);
                        if (accepted) ++g_completedPresentCount;
                        g_lastPresentedImage = realIndex; g_hasPresentedImage = accepted;
                        return accepted;
                    };
                    realPresented = presentOriginal();
                }
                if (g_metalFg) {
                    g_metalFg->FinishPresent(realPresented, metalGenerated);
                    if (g_metalFg->Failed()) {
                        renderer::SetFrameGenerationInputCaptureEnabled(false);
                        // As for Vulkan FSR: mark the current MetalFX request so a
                        // failure before any apply does not make reconciliation loop.
                        const auto request = MetalFgRequest().config;
                        if (request.provider == framegen::Provider::MetalFx) {
                            std::lock_guard lock(g_fgSettingsMutex);
                            g_fgFailedRequest = request;
                        }
                    }
                }
                completion.Complete(realPresented && !g_displayFailed.load());
                ReadPresentCapture(captureResult, true, realPresented, submissionSerial);
#else
                completion.Complete(presented && !g_displayFailed.load());
                ReadPresentCapture(captureResult, true, presented, submissionSerial);
#endif
                return;
            }
        }
#endif
        g_frameOnGpu = false;

#ifdef LO_GPU_PLUME
        if (menu)
        {
            CacheCpuFrame(menuPresentBuffer, menuWidth, menuHeight);
            UploadAndPresentPixels(menuPresentBuffer, menuWidth, menuHeight, true, displayTicket, presentationOptions, capture, captureResult);
            return;
        }
#endif
        {
        const uint32_t pitchBlocks = (width + 31) & ~31u;
        const uint32_t endian = copyDestInfo & 7;
        const uint8_t* src = static_cast<const uint8_t*>(g_memory.Translate(0xA0000000u + (physicalAddress & 0x1FFFFFFF)));
        g_pixels.resize(size_t(width) * height);
        g_frameWidth = width;
        g_frameHeight = height;
        for (uint32_t y = 0; y < height; y++)
        {
            uint32_t* dst = &g_pixels[size_t(y) * width];
            for (uint32_t x = 0; x < width; x++)
            {
                uint32_t offset = TiledOffset2D(x, y, pitchBlocks, 2);
                uint32_t v;
                memcpy(&v, src + offset, 4);
                dst[x] = GpuSwap(v, endian);
            }
        }
        }

#ifdef LO_GPU_PLUME
        UploadAndPresentPixels(g_pixels, width, height, false, displayTicket, presentationOptions, capture, captureResult);
#endif
    }

    bool IsHostOverlayActive()
    {
        return debug_menu::IsOverlayVisible() || settings::IsOpen();
    }

    void PresentHostOverlay()
    {
#ifdef LO_GPU_PLUME
        if (!IsHostOverlayActive())
            return;
        uint64_t displayTicket = 0;
        uint32_t menuWidth = 0, menuHeight = 0;
        if (!PreparePresentation(displayTicket, menuWidth, menuHeight))
            return;
        renderer::SetOutputSize(menuWidth, menuHeight);
        if (settings::GetConfig().variableRefreshRate) {
            static FramePacer overlayPacer;
            static DeadlineWait overlayWait;
            overlayWait.Until(overlayPacer.Schedule(std::chrono::steady_clock::now(), GetFramePacingTarget(60, true)));
        }

        const bool hasSettings = settings::DrawMenu(g_menuPixels, g_menuRevision, menuWidth, menuHeight);
        const bool hasDebug = debug_menu::IsOverlayVisible();
        if (!hasSettings && !hasDebug)
            return;

        std::vector<uint32_t> menuPresentBuffer;
        if (hasDebug) {
            static host_ui::PixelBuffer s_debugOverlayBuf;
            s_debugOverlayBuf.Resize(1280, 720);
            s_debugOverlayBuf.Clear(0x00000000);
            host_ui::Rasterizer r(s_debugOverlayBuf);
            debug_menu::RenderOverlay(r);

            if (!hasSettings) {
                if (size_t(menuWidth) > std::numeric_limits<size_t>::max() / size_t(menuHeight))
                    menuPresentBuffer.clear();
                else
                    menuPresentBuffer.assign(size_t(menuWidth) * menuHeight, host_ui::MakeColor(204, 16, 16, 24));
            } else {
                menuPresentBuffer = g_menuPixels;
            }
            host_ui::CompositeScaled(s_debugOverlayBuf, menuWidth, menuHeight, menuPresentBuffer);
        }
        else {
            menuPresentBuffer = g_menuPixels;
        }

        if (menuPresentBuffer.empty())
            return;

        CacheCpuFrame(menuPresentBuffer, menuWidth, menuHeight);
        UploadAndPresentPixels(menuPresentBuffer, menuWidth, menuHeight, true, displayTicket, PresentationOptions{}, nullptr, nullptr);
#endif
    }

    static bool WritePpm(const char* path, const std::vector<uint32_t>& pixels, uint32_t width, uint32_t height,
        bool bgra = false)
    {
        FILE* f = fopen(path, "wb");
        if (!f)
            return false;
        fprintf(f, "P6\n%u %u\n255\n", width, height);
        std::vector<uint8_t> row(size_t(width) * 3);
        for (uint32_t y = 0; y < height; y++)
        {
            for (uint32_t x = 0; x < width; x++)
            {
                uint32_t p = pixels[size_t(y) * width + x];
                row[x * 3 + 0] = uint8_t(p >> (bgra ? 16 : 0));
                row[x * 3 + 1] = uint8_t(p >> 8);
                row[x * 3 + 2] = uint8_t(p >> (bgra ? 0 : 16));
            }
            fwrite(row.data(), 1, row.size(), f);
        }
        fclose(f);
        return true;
    }

#ifdef LO_GPU_PLUME
    // Opt-in diagnostic replay input, preserving the frozen scene before peak
    // mapping. The payload is tightly packed RGBA16F extended gamma, not a PNG
    // or an output-encoded HDR image. Normal screenshots never read it back.
    static bool SaveHdrCalibrationCapture(const char* path)
    {
        const auto& scene = g_hdrCalibrationCache;
        if (!scene.ready || !scene.scene || !scene.extended || !WaitForPresentGpu()) return false;
        const uint32_t pitch = (scene.width * 8 + 255) & ~255u;
        auto readback = g_device->createBuffer(plume::RenderBufferDesc::ReadbackBuffer(uint64_t(pitch) * scene.height));
        if (!readback || !BeginGpuCommands(g_commandList.get())) return false;
        g_commandList->barriers(plume::RenderBarrierStage::COPY,
            plume::RenderTextureBarrier(scene.scene, plume::RenderTextureLayout::COPY_SOURCE));
        g_commandList->copyTextureRegion(plume::RenderTextureCopyLocation::PlacedFootprint(readback.get(),
            plume::RenderFormat::R16G16B16A16_FLOAT, scene.width, scene.height, 1, pitch / 8),
            plume::RenderTextureCopyLocation::Subresource(scene.scene));
        g_commandList->barriers(plume::RenderBarrierStage::GRAPHICS,
            plume::RenderTextureBarrier(scene.scene, plume::RenderTextureLayout::SHADER_READ));
        if (!EndGpuCommands(g_commandList.get())) return false;
        const plume::RenderCommandList* lists[] = {g_commandList.get()};
        uint64_t serial = 0; int32_t result = 0;
        if (!SubmitPresentationBatch(lists, 1, nullptr, 0, nullptr, 0, g_fence.get(), &serial, &result)) return false;
        g_fgPresentSerial = serial;
        g_presentPending = true;
        if (!WaitForGpuFence(g_fence.get())) { DrainGpuForShutdown(); return false; }
        g_presentPending = false;
        const auto* data = static_cast<const uint8_t*>(readback->map());
        if (!data) return false;
        FILE* file = fopen(path, "wb");
        bool saved = false;
        if (file) {
            saved = fprintf(file, "LOHDR1 %u %u\n", scene.width, scene.height) > 0;
            for (uint32_t y = 0; y < scene.height && saved; ++y)
                saved = fwrite(data + size_t(y) * pitch, 8, scene.width, file) == scene.width;
            saved = fclose(file) == 0 && saved;
        }
        readback->unmap();
        return saved;
    }
#endif
    bool SaveScreenshot(const char* path)
    {
#ifdef LO_GPU_PLUME
        if (GpuWorkStopped()) return false;
        if (const char* capture = getenv("LO_HDR_CALIBRATION_CAPTURE"))
            LOG_INFO("HDR calibration: frozen source export={} saved={}", capture, SaveHdrCalibrationCapture(capture));
        if(getenv("LO_SCREENSHOT_PRESENTED") && g_hasPresentedImage && g_swapChain) {
            const auto format=g_vulkan ? g_snapshotFormat : g_swapChain->getFormat();
            const bool fp16Frame = format == plume::RenderFormat::R16G16B16A16_FLOAT;
            const bool pqFrame = format == plume::RenderFormat::R10G10B10A2_UNORM || format == plume::RenderFormat::B10G10R10A2_UNORM;
            const bool hdrFrame = fp16Frame || pqFrame;
            const float outputScale = g_vulkan ? g_snapshotOutputScale : g_hdrOutput.scale;
            // Vulkan negotiates real SDR surfaces, so only D3D12/Metal keep an inactive linear output.
            const bool srgbPreview = !g_vulkan && g_hdrOutput.linear && !g_hdrOutput.active;
            if(format!=plume::RenderFormat::R8G8B8A8_UNORM &&
                format!=plume::RenderFormat::B8G8R8A8_UNORM && !hdrFrame) return false;
            const uint32_t w=g_vulkan ? g_snapshotWidth : g_swapChain->getWidth();
            const uint32_t h=g_vulkan ? g_snapshotHeight : g_swapChain->getHeight();
            const uint32_t bytesPerPixel = fp16Frame ? 8u : 4u;
            const uint32_t pitch=(w*bytesPerPixel+255)&~255u;
            if(!w || !h) return false;
            auto readback=g_device->createBuffer(plume::RenderBufferDesc::ReadbackBuffer(uint64_t(pitch)*h));
            auto* frame=g_vulkan ? g_presentedSnapshot.get() : g_swapChain->getTexture(g_lastPresentedImage);
            if(!frame)return false;
            if (!WaitForPresentGpu()) return false;
            if (!BeginGpuCommands(g_commandList.get())) return false;
            g_commandList->barriers(plume::RenderBarrierStage::COPY,plume::RenderTextureBarrier(frame,plume::RenderTextureLayout::COPY_SOURCE));
            g_commandList->copyTextureRegion(plume::RenderTextureCopyLocation::PlacedFootprint(readback.get(),format,w,h,1,pitch/bytesPerPixel),plume::RenderTextureCopyLocation::Subresource(frame));
            if(!g_vulkan) g_commandList->barriers(plume::RenderBarrierStage::NONE,plume::RenderTextureBarrier(frame,plume::RenderTextureLayout::PRESENT));
            if (!EndGpuCommands(g_commandList.get())) return false; const plume::RenderCommandList* lists[]={g_commandList.get()};
            uint64_t submissionSerial = 0; int32_t submitResult = 0;
            const bool submitted = SubmitPresentationBatch(lists, 1, nullptr, 0, nullptr, 0,
                g_fence.get(), &submissionSerial, &submitResult);
            if (!submitted) { LOG_ERROR("video: screenshot submit failed raw_result={}", submitResult); return false; }
            g_fgPresentSerial = submissionSerial;
            g_presentPending = true;
            if (!WaitForGpuFence(g_fence.get())) { DrainGpuForShutdown(); return false; }
            g_presentPending = false;
            std::vector<uint32_t> pixels(size_t(w)*h); const auto* data=static_cast<const uint8_t*>(readback->map());
            if (!data) return false;
            float maximumLinear = 0.0f;
            uint64_t aboveWhite = 0;
            for(uint32_t y=0;y<h;y++) {
                if (fp16Frame) {
                    const auto* row = reinterpret_cast<const uint16_t*>(data + size_t(y) * pitch);
                    for (uint32_t x=0;x<w;x++) {
                        const float red = hdr::DecodeHalf(row[x*4]);
                        const float green = hdr::DecodeHalf(row[x*4+1]);
                        const float blue = hdr::DecodeHalf(row[x*4+2]);
                        const float maximum = std::max({red, green, blue});
                        maximumLinear = std::max(maximumLinear, maximum);
                        aboveWhite += maximum > outputScale;
                        pixels[size_t(y)*w+x] = hdr::PreviewRgba(red, green, blue, outputScale, srgbPreview);
                    }
                } else if (pqFrame) {
                    const auto* row = reinterpret_cast<const uint32_t*>(data + size_t(y)*pitch);
                    for (uint32_t x=0;x<w;x++) {
                        const auto rgb = hdr::DecodePq10(row[x], format == plume::RenderFormat::B10G10R10A2_UNORM);
                        const float maximum = std::max({rgb[0],rgb[1],rgb[2]});
                        maximumLinear = std::max(maximumLinear,maximum);
                        aboveWhite += maximum > outputScale;
                        pixels[size_t(y)*w+x] = hdr::PreviewRgba(rgb[0],rgb[1],rgb[2],outputScale);
                    }
                } else memcpy(pixels.data()+size_t(y)*w,data+size_t(y)*pitch,w*4);
            }
            if (hdrFrame) LOG_INFO("HDR screenshot: SDR preview={} linear_max={} reference_white={} pixels_above_white={}",
                path, maximumLinear, outputScale, aboveWhite);
            readback->unmap(); return WritePpm(path,pixels,w,h,format==plume::RenderFormat::B8G8R8A8_UNORM);
        }
#endif
        // LO_SCREENSHOT_RESOLVED=1: also dump every GPU-resolved surface (HDR
        // scene buffers etc.) as <path>_<address>.ppm for renderer debugging.
        static const bool dumpResolved = getenv("LO_SCREENSHOT_RESOLVED") != nullptr;
        if (dumpResolved)
        {
            {
                std::string p = path;
                size_t dot = p.rfind('.');
                renderer::DumpRenderTargets(p.substr(0, dot).c_str());
            }
            std::vector<uint32_t> pixels;
            for (uint32_t address : renderer::GetResolvedAddresses())
            {
                uint32_t w = 0, h = 0;
                if (renderer::ReadbackResolvedSurface(address, pixels, w, h))
                {
                    std::string p = path;
                    size_t dot = p.rfind('.');
                    p = p.substr(0, dot) + fmt::format("_{:x}", address) + (dot == std::string::npos ? "" : p.substr(dot));
                    WritePpm(p.c_str(), pixels, w, h);
                }
            }
        }
        if (g_frameOnGpu && !renderer::ReadbackResolvedSurface(g_frontbufferPhysical, g_pixels, g_frameWidth, g_frameHeight))
            return false;
        if (g_pixels.empty() || g_pixels.size() != size_t(g_frameWidth) * g_frameHeight)
            return false;
        FILE* f = fopen(path, "wb");
        if (!f)
            return false;
        fprintf(f, "P6\n%u %u\n255\n", g_frameWidth, g_frameHeight);
        std::vector<uint8_t> row(size_t(g_frameWidth) * 3);
        for (uint32_t y = 0; y < g_frameHeight; y++)
        {
            for (uint32_t x = 0; x < g_frameWidth; x++)
            {
                uint32_t p = g_pixels[size_t(y) * g_frameWidth + x];
                row[x * 3 + 0] = uint8_t(p);
                row[x * 3 + 1] = uint8_t(p >> 8);
                row[x * 3 + 2] = uint8_t(p >> 16);
            }
            fwrite(row.data(), 1, row.size(), f);
        }
        fclose(f);
        return true;
    }
#endif
}
