#include <version.h>
#include <stdafx.h>
#include "video.h"
#if defined(LO_GPU_PLUME)
#include "backend_device.h"
#include "vulkan_submission_state.h"
#include "vulkan_command_recording.h"
#include "dlss_ngx.h"
#endif
#include "renderer.h"
#include "presentation.h"
#include "command_processor.h"
#include "frame_plan.h"
#include <settings/config.h>
#include <settings/menu.h>
#include <settings/restart.h>
#include <kernel/memory.h>
#include <os/logger.h>
#include <os/shader_log.h>
#include <os/user_paths.h>
#include <hid/hid.h>
#include <debug/battle_menu.h>
#include <debug/menu_overlay.h>
#include <host_ui/host_ui.h>
#include <host_ui/rasterizer.h>

#include <SDL.h>
#include <SDL_syswm.h>
#include "window_pixels.h"
#include "window_mode.h"

#ifdef LO_GPU_PLUME
#include <plume_render_interface.h>
#include <plume_vulkan.h>
#include "diagnostic_log.h"
#ifdef _WIN32
#include <plume_d3d12.h>
#endif
#endif

#include <vector>
#include <future>
#include <thread>
#include <atomic>

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

        SDL_Window* g_window = nullptr;
        bool g_videoSubsystemOwned = false;
        // Pair only this lifecycle's reference, on its window-owning thread.
        // HID or other SDL clients retain their independent subsystem references.
        void DestroyWindowResources()
        {
            if (g_window) { SDL_DestroyWindow(g_window); g_window = nullptr; }
            if (g_videoSubsystemOwned) {
                SDL_QuitSubSystem(SDL_INIT_VIDEO);
                g_videoSubsystemOwned = false;
            }
        }
        std::atomic<uint64_t> g_shaderProgress{0};
        std::atomic<bool> g_vulkan{false};
        std::atomic<uint64_t> g_deviceEpoch{0};
        constexpr uint64_t kProgressMask = (1ull << 28) - 1;
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
        std::atomic<uint64_t> g_displaySize{0};
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
        bool g_initAttempted = false;
        bool g_available = false;
        bool g_initializing = false;
        std::atomic<int> g_selectedBackend{-1};

        std::vector<uint32_t> g_pixels;   // last untiled frame, R8G8B8A8
        uint32_t g_frameWidth = 0, g_frameHeight = 0;
        bool g_frameOnGpu = false;        // last frame came straight from a resolved surface
        uint32_t g_frontbufferPhysical = 0;

#ifdef LO_GPU_PLUME
        // This outlives g_interface because Plume retains the copied hook
        // userdata until VulkanInterface destruction.
        std::unique_ptr<dlss::Controller> g_dlssController;
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
        std::unique_ptr<Presentation> g_presentation;
        std::unique_ptr<plume::RenderTexture> g_cpuFrame;
        std::unique_ptr<plume::RenderTexture> g_presentedSnapshot;
        uint32_t g_snapshotWidth=0,g_snapshotHeight=0;
        uint32_t g_cpuWidth=0,g_cpuHeight=0;
        uint32_t g_lastPresentedImage=0;
        bool g_hasPresentedImage=false;
        bool g_presentPending=false;
        bool g_forceSwapResize=false;
        submission::VulkanState g_submissionState;
        struct PresentationDisplayState {
            uint64_t resizedTicket = 0;
#ifdef _WIN32
            int appliedMode = -1;
            uint64_t appliedSize = 0, appliedTicket = 0;
#endif
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
            if (GpuWorkStopped()) {
                if (rawResult) *rawResult = g_submissionState.Failure();
                return false;
            }
            if (!g_vulkan || !g_queue || !lists || !count || !fence) {
                StopGpuWork(submission::VulkanState::InvalidState); return false;
            }
            auto* queue = static_cast<plume::VulkanCommandQueue*>(g_queue.get());
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
                    [&] { return int32_t(vkResetFences(queue->device->vk, 1, &nativeFence->vk)); },
                    [&] { return int32_t(vkQueueSubmit(queue->queue->vk, 1, &submit, nativeFence->vk)); },
                    acceptedSerial, result);
            }
            if (rawResult) *rawResult = result;
            if (serial) *serial = acceptedSerial;
            if (!submitted) LOG_ERROR("video: Vulkan submission stopped raw_vk={}", result);
            return submitted;
        }

        std::filesystem::path DlssApplicationDataPath()
        {
            return os::user_paths::UsePortableLayout() ? std::filesystem::path("cache") / "ngx"
                : os::user_paths::DataDir() / "cache" / "ngx";
        }

        std::filesystem::path DlssRuntimePath()
        {
            const char* override = std::getenv("LO_DLSS_RUNTIME_PATH");
            return override && *override ? std::filesystem::path(override) : std::filesystem::current_path();
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

        bool WaitForPresentGpuImpl()
        {
            if (g_presentPending) {
                if (!g_queue || !g_fence || !WaitForGpuFence(g_fence.get())) return false;
                g_presentPending = false;
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
            if(!g_presentedSnapshot || w!=g_snapshotWidth || h!=g_snapshotHeight) {
                g_presentedSnapshot=g_device->createTexture(plume::RenderTextureDesc::Texture2D(w,h,1,kSwapChainFormat));
                g_snapshotWidth=w;g_snapshotHeight=h;
            }
            if(!g_presentedSnapshot) return;
            g_commandList->barriers(plume::RenderBarrierStage::COPY,plume::RenderTextureBarrier(frame,plume::RenderTextureLayout::COPY_SOURCE));
            g_commandList->barriers(plume::RenderBarrierStage::COPY,plume::RenderTextureBarrier(g_presentedSnapshot.get(),plume::RenderTextureLayout::COPY_DEST));
            g_commandList->copyTexture(g_presentedSnapshot.get(),frame);
        }
#endif

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

        // Owner thread only. Report() is read here, after NGX has returned,
        // and the four fields are published as one value.
        void PublishOwnedDeviceCapability()
        {
            upscaling::BackendDeviceSnapshot snapshot;
            const bool vulkan = g_vulkan.load(std::memory_order_acquire);
            snapshot.backend = vulkan ? backend::Backend::Vulkan : backend::Backend::D3D12;
            snapshot.deviceEpoch = g_deviceEpoch.load(std::memory_order_acquire);
#if defined(LO_GPU_PLUME)
            snapshot.deviceReady = g_available && g_device != nullptr;
            snapshot.dlssAvailable = vulkan && g_dlssController &&
                g_dlssController->Report().state == dlss::ProbeState::Available;
#else
            snapshot.deviceReady = false;
            snapshot.dlssAvailable = false;
#endif
            upscaling::PublishDeviceCapability(snapshot);
        }

        void PublishClearedDeviceCapability()
        {
            upscaling::BackendDeviceSnapshot snapshot;
            snapshot.backend = g_vulkan.load(std::memory_order_acquire) ? backend::Backend::Vulkan : backend::Backend::D3D12;
            snapshot.deviceEpoch = g_deviceEpoch.load(std::memory_order_acquire);
            snapshot.deviceReady = false;
            snapshot.dlssAvailable = false;
            upscaling::PublishDeviceCapability(snapshot);
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
#if defined(LO_GPU_PLUME)
    dlss::Controller* GetDlssController() { return g_vulkan ? g_dlssController.get() : nullptr; }
    bool GpuWorkStopped() { return g_vulkan && g_submissionState.Stopped(); }
    void StopGpuWork(int32_t nativeResult) {
        if (!g_submissionState.Stopped())
            LOG_ERROR("video: native GPU work stopped raw_vk={}; device restart required", nativeResult);
        g_submissionState.Stop(nativeResult);
    }
    bool BeginGpuCommands(plume::RenderCommandList* list) {
        if (GpuWorkStopped()) return false;
        if (!list) { StopGpuWork(submission::VulkanState::InvalidState); return false; }
        if (!g_vulkan) { list->begin(); return true; }
        const auto result = submission::BeginCommands(*static_cast<plume::VulkanCommandList*>(list));
        if (result != VK_SUCCESS) StopGpuWork(int32_t(result));
        return result == VK_SUCCESS;
    }
    bool EndGpuCommands(plume::RenderCommandList* list) {
        if (GpuWorkStopped()) return false;
        if (!list) { StopGpuWork(submission::VulkanState::InvalidState); return false; }
        if (!g_vulkan) { list->end(); return true; }
        const auto result = submission::EndCommands(*static_cast<plume::VulkanCommandList*>(list));
        if (result != VK_SUCCESS) StopGpuWork(int32_t(result));
        return result == VK_SUCCESS;
    }
    bool WaitForGpuFence(plume::RenderCommandFence* fence) {
        if (!g_queue || !fence) { StopGpuWork(submission::VulkanState::InvalidState); return false; }
        if (!g_vulkan) { g_queue->waitForCommandFence(fence); return true; }
        auto* queue = static_cast<plume::VulkanCommandQueue*>(g_queue.get());
        auto* nativeFence = static_cast<plume::VulkanCommandFence*>(fence);
        const bool complete = g_submissionState.WaitSubmitted([&] {
            return int32_t(vkWaitForFences(queue->device->vk, 1, &nativeFence->vk, VK_TRUE, UINT64_MAX));
        });
        if (!complete) LOG_ERROR("video: GPU fence wait failed raw_vk={}; resources retained", g_submissionState.Failure());
        return complete;
    }
    void DrainGpuForShutdown() {
        if (!g_vulkan || !g_device) return;
        const auto result = vkDeviceWaitIdle(static_cast<plume::VulkanDevice*>(g_device.get())->vk);
        if (result == VK_SUCCESS) {
            if (g_dlssController) g_dlssController->ReleaseCompletedThrough(g_submissionState.LastSubmission());
        } else if (result == VK_ERROR_DEVICE_LOST) {
            StopGpuWork(int32_t(result));
            if (g_dlssController) g_dlssController->AbandonUsesAfterDeviceLoss();
        } else {
            // No proven completion or lost-device disposal boundary. Never free
            // resources still referenced by native work. OS process teardown is
            // safer than running their destructors against an undrained device.
            LOG_ERROR("video: shutdown drain failed raw_vk={}; terminating without unsafe GPU destruction", int32_t(result));
            std::fflush(nullptr);
            std::_Exit(EXIT_FAILURE);
        }
    }

    bool SubmitRendererBatch(const plume::RenderCommandList* const* lists, uint32_t count,
        plume::RenderCommandFence* fence, uint64_t& submissionSerial, int32_t& rawVkResult)
    {
        return SubmitVulkan(lists, count, nullptr, 0, nullptr, 0, fence, &submissionSerial, &rawVkResult);
    }
#endif

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
    upscaling::BackendDeviceSnapshot BackendDeviceState()
    {
        return upscaling::PublishedDeviceCapability();
    }
#if defined(LO_GPU_PLUME)
    // Called by video's GPU-owning presentation path. TakeSizingRequest releases
    // the CPU cache lock before NGX initialization and capability work begins.
    static void ServicePendingDlssSizing()
    {
        if (!g_vulkan || !g_dlssController || !g_interface || !g_device) return;
        const auto key = frame_plan::TakeSizingRequest();
        if (!key || key->deviceEpoch != g_deviceEpoch.load(std::memory_order_acquire)) return;
        frame_plan::PublishSizing(upscaling::SizingService::QueryOutputSizing(*g_dlssController,
            *static_cast<plume::VulkanInterface*>(g_interface.get()), *static_cast<plume::VulkanDevice*>(g_device.get()), *key));
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
    std::optional<backend::Backend> SelectedBackend() {
        const auto selected = g_selectedBackend.load();
        return selected < 0 ? std::nullopt : std::optional(static_cast<backend::Backend>(selected));
    }

    // The command thread calls this only before guest startup, or after all
    // rendering has stopped. The window/event thread is deliberately retained
    // between candidates; device children are destroyed before their parents.
    static void ResetGpu() {
        g_displayChanges.Reset();
        g_available = false;
        g_initializing = false;
        g_selectedBackend = -1;
        PublishClearedDeviceCapability();
        renderer::Shutdown();
#ifdef LO_GPU_PLUME
        WaitForPresentGpu();
        if (g_dlssController && g_vulkan)
            g_dlssController->ShutdownAfterGpuDrain();
        g_cpuFrame.reset(); g_cpuWidth = g_cpuHeight = 0;
        g_presentedSnapshot.reset(); g_snapshotWidth = g_snapshotHeight = 0;
        g_presentation.reset();
        g_uploadBuffer.reset();
#ifdef _WIN32
        if (g_swapChain && !g_vulkan) {
            auto* swap = static_cast<plume::D3D12SwapChain*>(g_swapChain.get());
            if (swap->d3d) swap->d3d->SetFullscreenState(FALSE, nullptr);
        }
#endif
        g_swapChain.reset(); g_presentSemaphores.clear();
        g_releaseSemaphore.reset(); g_acquireSemaphore.reset();
        g_fence.reset(); g_commandList.reset(); g_queue.reset();
        g_device.reset(); g_interface.reset();
        g_submissionState = {};
        g_presentPending = false;
        g_dlssController.reset();
        g_hasPresentedImage = false; g_lastPresentedImage = 0; g_forceSwapResize = false;
        g_presentationDisplay = {};
#endif
    }

    bool Init()
    {
        if (g_initAttempted)
            return g_available;
        g_initAttempted = true;

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
#ifndef _WIN32
            flags |= SDL_WINDOW_VULKAN;
#endif
            g_window = SDL_CreateWindow(lo_version::WindowTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                config.width, config.height, flags);
            if (!g_window)
            {
                LOG_WARNING("video: window creation failed: {}", SDL_GetError());
                return false;
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
        if (!createWindow()) { Shutdown(); g_initAttempted = true; return false; }
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
                g_interface = plume::CreateVulkanInterface(g_dlssController->ExtensionHooks());
            } else {
                g_dlssController.reset();
                g_interface = plume::CreateD3D12Interface();
            }
#else
            g_dlssController = std::make_unique<dlss::Controller>(DlssApplicationDataPath(), DlssRuntimePath());
            g_interface = plume::CreateVulkanInterface(g_window, g_dlssController->ExtensionHooks());
#endif
            if (!g_interface) return "API/loader initialization failed";
            g_device = g_interface->createDevice();
            if (g_device) {
                const uint64_t nextEpoch = g_deviceEpoch.fetch_add(1, std::memory_order_acq_rel) + 1;
                frame_plan::ResetSizing(nextEpoch);
            }
            if (g_device) {
                const auto& description = g_device->getDescription();
                LOG_INFO("video device: backend={} name={} driver_raw={} vendor_enum={} type_enum={} reported_device_memory_bytes={}",
                    backend::Name(candidate), description.name, description.driverVersion,
                    uint32_t(description.vendor), uint32_t(description.type), description.dedicatedVideoMemory);
            }
            if (const auto missing = backend::Missing(candidate, backend::Inspect(candidate, g_device.get())); !missing.empty()) return missing;
            if (g_vulkan && g_dlssController) {
                g_dlssController->ProbeOnce(*static_cast<plume::VulkanInterface*>(g_interface.get()),
                    *static_cast<plume::VulkanDevice*>(g_device.get()));
                LogDlssProbe(g_dlssController->Report());
            }
            g_queue = g_device->createCommandQueue(plume::RenderCommandListType::DIRECT);
            if (!g_queue) return "graphics queue creation failed";
            g_commandList = g_queue->createCommandList();
            g_fence = g_device->createCommandFence();
            g_acquireSemaphore = g_device->createCommandSemaphore();
            g_releaseSemaphore = g_device->createCommandSemaphore();
            if (!g_commandList || !g_fence || !g_acquireSemaphore || !g_releaseSemaphore) return "command/synchronization initialization failed";
#ifdef _WIN32
            g_swapChain = g_queue->createSwapChain(plume::RenderSwapChainDesc(g_nativeWindow, kSwapChainFormat, kSwapChainBuffers));
#else
            g_swapChain = g_queue->createSwapChain(plume::RenderSwapChainDesc(g_window, kSwapChainFormat, kSwapChainBuffers));
#endif
            if (!g_swapChain || g_swapChain->isEmpty()) return "window surface/swapchain initialization failed";
            if (g_vulkan && g_dlssController && settings::GetConfig().upscaler == upscaling::Upscaler::Dlss) {
                const auto output = upscaling::ResolveOutputRegion({g_swapChain->getWidth(), g_swapChain->getHeight()});
                const upscaling::SizingKey key{g_deviceEpoch.load(std::memory_order_acquire), output.width, output.height};
                const auto sizing = upscaling::SizingService::QueryOutputSizing(*g_dlssController,
                    *static_cast<plume::VulkanInterface*>(g_interface.get()), *static_cast<plume::VulkanDevice*>(g_device.get()), key);
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
            if (!getenv("LO_NO_RENDERER") && !renderer::Init()) return "renderer initialization failed";
            return {};
        }, ResetGpu);
        LOG_INFO("video: backend selection {}; configured={} (unchanged)", selection.Describe(), backend::Name(configured));
        if (selection.selected) {
            g_selectedBackend = static_cast<int>(*selection.selected);
            g_available = true; g_initializing = false;
            PublishOwnedDeviceCapability();
            LOG_INFO("video: {} on {}", backend::Name(*selection.selected), g_device->getDescription().name);
        } else {
            LOG_ERROR("video: no usable backend; guest startup aborted: {}", selection.Describe());
            Shutdown();
            g_initAttempted = true; // A repeated call cannot silently start another retry cycle.
        }
#endif
        return g_available;
    }

    void Shutdown()
    {
        ResetGpu();
#ifdef _WIN32
        g_windowThread.request_stop();
        if (g_windowThread.joinable()) g_windowThread.join();
        g_nativeWindow = nullptr;
        g_preparationWindow = nullptr;
        g_shaderProgress = 0;
#else
        DestroyWindowResources();
#endif
        g_available = false;
        g_initAttempted = false;
        hid::SetExternalEventPump(false);
    }

    void PumpEvents()
    {
#ifndef _WIN32
        PumpWindowEvents();
#endif
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
    void ResetShaderPreparationSkip() { g_skipShaderPreparation.store(false); }
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
    static bool UploadAndPresentPixels(const std::vector<uint32_t>& pixels, uint32_t width, uint32_t height,
                                       bool isMenu, uint64_t displayTicket, const PresentationOptions& presentationOptions);
    static void RenderPreparationScreen(PreparationStage stage, PreparationUnit unit, uint32_t done, uint32_t total)
    {
        if (!g_swapChain || g_swapChain->isEmpty() || (!g_available && !g_initializing) || !g_presentation)
            return;
        const uint32_t width = g_swapChain->getWidth();
        const uint32_t height = g_swapChain->getHeight();
        if (!width || !height)
            return;

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

        UploadAndPresentPixels(s_prepPixels, width, height, true, 0, PresentationOptions{});
    }
#endif

    void PumpWindowEvents()
    {
        static uint64_t shownProgress = 0;
        static auto lastProgressPaint = std::chrono::steady_clock::time_point{};
        const uint64_t progress = g_shaderProgress.load();
        const auto now = std::chrono::steady_clock::now();
        // Scanning can publish hundreds of updates per second. Keep UI updates
        // at 10 Hz, but show phase transitions and completion immediately.
        const bool phaseChanged = (progress >> 56) != (shownProgress >> 56);
        const uint32_t total = uint32_t((progress >> 28) & kProgressMask);
        const uint32_t done = uint32_t(progress & kProgressMask);
        if (g_window && progress != shownProgress &&
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
            if (total && g_swapChain && !g_swapChain->isEmpty() && (g_available || g_initializing) && g_presentation) {
                RenderPreparationScreen(stage, unit, done, total);
            } else if (!total && shownProgress != 0 && g_swapChain && !g_swapChain->isEmpty() && (g_available || g_initializing) && g_presentation) {
                const uint32_t width = g_swapChain->getWidth();
                const uint32_t height = g_swapChain->getHeight();
                if (width && height) {
                    static std::vector<uint32_t> s_clearPixels;
                    const size_t count = size_t(width) * height;
                    if (s_clearPixels.size() != count)
                        s_clearPixels.assign(count, host_ui::MakeColor(255, 0, 0, 0));
                    UploadAndPresentPixels(s_clearPixels, width, height, true, 0, PresentationOptions{});
                }
            }
#endif
            shownProgress = progress;
            lastProgressPaint = now;
        }
        if (!g_window)
            return;
        if (settings::restart::Requested()) {
            renderer::WaitDebugCaptureArchive();
#ifdef _WIN32
            if (settings::restart::LaunchWaitingChild()) {
                os::shaderlog::CloseForExit();
                fflush(nullptr);
                std::_Exit(0);
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
        if(reapply || !state.initialized || config.width!=state.applied.width || config.height!=state.applied.height || config.windowMode!=state.applied.windowMode) {
            const auto ticket = g_displayChanges.WindowTicket(config.width, config.height, uint32_t(config.windowMode));
            // SDL operations remain on the message-owning thread. Hidden tests
            // must never change the user's desktop display mode.
            const auto mode=getenv("LO_BACKGROUND")?settings::WindowMode::Windowed:config.windowMode;
            const bool wasWindowed = !state.initialized || state.applied.windowMode == settings::WindowMode::Windowed;
            const bool sizeChanged = !state.initialized || config.width != state.applied.width || config.height != state.applied.height;
            if (wasWindowed && mode != settings::WindowMode::Windowed) state.placement.Capture(g_window);
            int result=SDL_SetWindowFullscreen(g_window,mode==settings::WindowMode::Borderless?SDL_WINDOW_FULLSCREEN_DESKTOP:(g_vulkan && mode==settings::WindowMode::Exclusive?SDL_WINDOW_FULLSCREEN:0));
            if (result == 0 && mode == settings::WindowMode::Windowed) {
                if ((!wasWindowed || reapply) && !sizeChanged && state.placement.valid) state.placement.Restore(g_window);
                else if (sizeChanged) SDL_SetWindowSize(g_window,config.width,config.height);
            }
            else if (result == 0 && mode == settings::WindowMode::Exclusive)
                SDL_SetWindowSize(g_window,config.width,config.height);
#ifdef _WIN32
            // SDL owns the fullscreen transition. A failed bounds repair must
            // not roll the shortcut back to windowed.
            if (result == 0 && mode == settings::WindowMode::Borderless)
                window_mode::FitBorderless(g_nativeWindow);
#endif
            g_displayFailed=result!=0;
            g_displaySize.store(uint64_t(config.width)<<32|config.height);
            g_displayMode.store(int(mode));
            state.applied=config; state.initialized=true;
            g_windowResizeRequested = true;
            g_displayChanges.WindowComplete(ticket, result == 0);
        }
        debug_menu::Update();
        hid::PumpHostInput();
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
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
            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
                hid::HandleKeyboardEvent(event.key.keysym.scancode, event.type == SDL_KEYDOWN);
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                state.consumedKey = SDL_SCANCODE_UNKNOWN;
                hid::ClearKeyboardState();
            }
            if (event.type == SDL_WINDOWEVENT && event.window.windowID == SDL_GetWindowID(g_window) &&
                (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || event.window.event == SDL_WINDOWEVENT_DISPLAY_CHANGED ||
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
                LOG_INFO("video: window closed, exiting");
                renderer::WaitDebugCaptureArchive();
                os::shaderlog::CloseForExit();
                fflush(stdout);
                std::_Exit(0);
            }
        }
    }

#ifdef LO_GPU_PLUME
    // Sole presentation-thread entry for mode changes and swap-chain recovery.
    // Both guest frames and host-only frames call this BEFORE reading dimensions
    // or rasterizing UI. The returned ticket belongs to these prepared operations.
    static bool PreparePresentation(uint64_t& displayTicket, uint32_t& width, uint32_t& height)
    {
        if (!g_available || !g_swapChain || GpuWorkStopped())
            return false;
        displayTicket = g_displayChanges.PresentationTicket();
        if (g_windowResizeRequested.exchange(false)) g_forceSwapResize = true;
        if (displayTicket && displayTicket != g_presentationDisplay.resizedTicket) {
            g_forceSwapResize = true;
            g_presentationDisplay.resizedTicket = displayTicket;
        }
#ifdef _WIN32
        const int mode = g_displayMode.load();
        const uint64_t size = g_displaySize.load();
        auto& applied = g_presentationDisplay;
        if (!g_vulkan && mode >= 0 && (mode != applied.appliedMode || size != applied.appliedSize ||
            (displayTicket && displayTicket != applied.appliedTicket))) {
            if (!WaitForPresentGpu()) return false;
            auto* swap = static_cast<plume::D3D12SwapChain*>(g_swapChain.get());
            const plume::WindowPixelContext pixels;
            HRESULT result = swap->d3d->SetFullscreenState(FALSE, nullptr);
            if (SUCCEEDED(result) && mode == int(settings::WindowMode::Exclusive)) {
                DXGI_MODE_DESC target{};
                target.Width = uint32_t(size >> 32); target.Height = uint32_t(size);
                target.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                result = swap->d3d->ResizeTarget(&target);
                if (SUCCEEDED(result)) result = swap->d3d->SetFullscreenState(TRUE, nullptr);
            }
            BOOL exclusive = FALSE;
            const HRESULT queryResult = swap->d3d->GetFullscreenState(&exclusive, nullptr);
            const bool modeApplied = SUCCEEDED(result) && SUCCEEDED(queryResult) &&
                bool(exclusive) == (mode == int(settings::WindowMode::Exclusive));
            if (applied.appliedMode == int(settings::WindowMode::Exclusive) && mode != applied.appliedMode)
                g_reapplyWindow = true;
            LOG_INFO("display mode: requested={} exclusive={} result={:#x}", mode, bool(exclusive), uint32_t(result));
            // Flip-model buffers must be resized even for an equal-size transition.
            g_forceSwapResize = true;
            applied.appliedMode = mode; applied.appliedSize = size; applied.appliedTicket = displayTicket;
            if (!modeApplied) {
                g_displayFailed = true;
                g_displayChanges.Complete(displayTicket, false);
                return false;
            }
        }
#endif
        // Empty is recoverable: minimized Vulkan surfaces may have zero extent.
        // Never return for isEmpty() before giving resize() a chance to recover.
        if (g_forceSwapResize || g_swapChain->isEmpty() || g_swapChain->needsResize()) {
            if (!WaitForPresentGpu()) return false;
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
            LogOutputPixels("resized");
        }
        if (g_swapChain->isEmpty())
            return false;
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

    static bool UploadAndPresentPixels(const std::vector<uint32_t>& pixels, uint32_t width, uint32_t height,
                                       bool isMenu, uint64_t displayTicket, const PresentationOptions& presentationOptions)
    {
        if (GpuWorkStopped() || (!g_available && !g_initializing) || !g_swapChain || g_swapChain->isEmpty())
            return false;
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
        if(g_presentation) g_presentation->Draw(g_commandList.get(),g_cpuFrame.get(),backBuffer,width,height,
            g_swapChain->getWidth(),g_swapChain->getHeight(),isMenu ? PresentationOptions{} : presentationOptions);
        RecordPresentedSnapshot(backBuffer);
        g_commandList->barriers(plume::RenderBarrierStage::NONE, plume::RenderTextureBarrier(backBuffer, plume::RenderTextureLayout::PRESENT));
        if (!EndGpuCommands(g_commandList.get())) return false;

        const plume::RenderCommandList* lists[] = { g_commandList.get() };
        plume::RenderCommandSemaphore* waitSemaphore = g_acquireSemaphore.get();
        plume::RenderCommandSemaphore* signalSemaphore = PresentSemaphore(imageIndex);
        uint64_t submissionSerial = 0; int32_t submitResult = 0;
        const bool submitted = g_vulkan ? SubmitVulkan(lists, 1, &waitSemaphore, 1, &signalSemaphore, 1,
            g_fence.get(), &submissionSerial, &submitResult)
            : (g_queue->executeCommandLists(lists, 1, &waitSemaphore, 1, &signalSemaphore, 1, g_fence.get()), true);
        if (!submitted) { LOG_ERROR("video: present submit failed raw_vk={}", submitResult); return false; }
        const bool presented = g_swapChain->present(imageIndex, &signalSemaphore, 1);
        if (presented) ++g_completedPresentCount;
        g_presentPending = true;
        g_lastPresentedImage=imageIndex; g_hasPresentedImage=true;
        completion.Complete(presented && !g_displayFailed.load());
        return presented;
    }
#endif

    } // namespace

    uint64_t CompletedPresentCount() { return g_completedPresentCount; }

    void PresentFrontbuffer(uint32_t physicalAddress, uint32_t width, uint32_t height, uint32_t copyDestInfo)
    {
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
        gpu::SetFrameRateTarget(presentationConfig.frameRate);
        const PresentationOptions presentationOptions{
            presentationConfig.antialiasing == 3 ? Antialiasing::SMAA : static_cast<Antialiasing>(presentationConfig.antialiasing),
            presentationConfig.scalingQuality ? ScalingFilter::Bicubic : ScalingFilter::Bilinear};
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
            plume::RenderTexture* source = renderer::AcquireResolvedSurface(physicalAddress & 0x1FFFFFFF, rw, rh, rf, &sourcePlan);
            if (source && plume::RenderFormat(rf) == kSwapChainFormat)
            {
                uint32_t sourceWidth=width, sourceHeight=height;
                renderer::ScaleResolvedSize(physicalAddress & 0x1FFFFFFF, sourceWidth, sourceHeight);
                sourceWidth=std::min(sourceWidth,rw); sourceHeight=std::min(sourceHeight,rh);
                g_frameWidth = sourceWidth;
                g_frameHeight = sourceHeight;
                g_frontbufferPhysical = physicalAddress & 0x1FFFFFFF;
                g_frameOnGpu = true;
                if (g_swapChain->isEmpty())
                    return;
                DisplayCompletion completion(g_displayChanges, displayTicket);
                if (!WaitForPresentGpu()) return;
                uint32_t imageIndex = 0;
                if (!g_swapChain->acquireTexture(g_acquireSemaphore.get(), &imageIndex))
                {
                    return;
                }
                plume::RenderTexture* backBuffer = g_swapChain->getTexture(imageIndex);
                const uint32_t copyWidth = std::min(sourceWidth, g_swapChain->getWidth());
                const uint32_t copyHeight = std::min(sourceHeight, g_swapChain->getHeight());

                if (!BeginGpuCommands(g_commandList.get())) return;
                if(g_presentation) {
                    const auto decision = frame_plan::ResolvePresentationDecision(&sourcePlan,
                        renderer::SceneAAApplied(physicalAddress & 0x1FFFFFFF), uint32_t(presentationOptions.antialiasing),
                        uint32_t(presentationOptions.scalingFilter));
                    const PresentationOptions sourceOptions{decision.requestedAA == 3 ? Antialiasing::SMAA :
                        static_cast<Antialiasing>(decision.requestedAA), decision.scalingQuality ? ScalingFilter::Bicubic : ScalingFilter::Bilinear};
                    if(decision.bypassAA)
                        g_presentation->DrawComposited(g_commandList.get(),source,backBuffer,sourceWidth,sourceHeight,
                            g_swapChain->getWidth(),g_swapChain->getHeight(),sourceOptions.scalingFilter);
                    else g_presentation->Draw(g_commandList.get(),source,backBuffer,sourceWidth,sourceHeight,
                        g_swapChain->getWidth(),g_swapChain->getHeight(),sourceOptions);
                }
                else {
                    g_commandList->barriers(plume::RenderBarrierStage::COPY, plume::RenderTextureBarrier(backBuffer, plume::RenderTextureLayout::COPY_DEST));
                    plume::RenderBox box(0, 0, int32_t(copyWidth), int32_t(copyHeight), 0, 1);
                    g_commandList->copyTextureRegion(plume::RenderTextureCopyLocation::Subresource(backBuffer),
                        plume::RenderTextureCopyLocation::Subresource(source), 0, 0, 0, &box);
                }
                RecordPresentedSnapshot(backBuffer);
                g_commandList->barriers(plume::RenderBarrierStage::NONE, plume::RenderTextureBarrier(backBuffer, plume::RenderTextureLayout::PRESENT));
                if (!EndGpuCommands(g_commandList.get())) return;

                const plume::RenderCommandList* lists[] = { g_commandList.get() };
                plume::RenderCommandSemaphore* waitSemaphore = g_acquireSemaphore.get();
                plume::RenderCommandSemaphore* signalSemaphore = PresentSemaphore(imageIndex);
                uint64_t submissionSerial = 0; int32_t submitResult = 0;
                const bool submitted = g_vulkan ? SubmitVulkan(lists, 1, &waitSemaphore, 1, &signalSemaphore, 1,
                    g_fence.get(), &submissionSerial, &submitResult)
                    : (g_queue->executeCommandLists(lists, 1, &waitSemaphore, 1, &signalSemaphore, 1, g_fence.get()), true);
                if (!submitted) { LOG_ERROR("video: GPU presentation submit failed raw_vk={}", submitResult); return; }
                const bool presented = g_swapChain->present(imageIndex, &signalSemaphore, 1);
                if (presented) ++g_completedPresentCount;
                g_presentPending = true;
                g_lastPresentedImage=imageIndex; g_hasPresentedImage=true;
                completion.Complete(presented && !g_displayFailed.load());
                return;
            }
        }
#endif
        g_frameOnGpu = false;

#ifdef LO_GPU_PLUME
        if (menu)
        {
            CacheCpuFrame(menuPresentBuffer, menuWidth, menuHeight);
            UploadAndPresentPixels(menuPresentBuffer, menuWidth, menuHeight, true, displayTicket, presentationOptions);
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
        UploadAndPresentPixels(g_pixels, width, height, false, displayTicket, presentationOptions);
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
        UploadAndPresentPixels(menuPresentBuffer, menuWidth, menuHeight, true, displayTicket, PresentationOptions{});
#endif
    }

    static bool WritePpm(const char* path, const std::vector<uint32_t>& pixels, uint32_t width, uint32_t height)
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
                row[x * 3 + 0] = uint8_t(p);
                row[x * 3 + 1] = uint8_t(p >> 8);
                row[x * 3 + 2] = uint8_t(p >> 16);
            }
            fwrite(row.data(), 1, row.size(), f);
        }
        fclose(f);
        return true;
    }

    bool SaveScreenshot(const char* path)
    {
#ifdef LO_GPU_PLUME
        if (GpuWorkStopped()) return false;
        if(getenv("LO_SCREENSHOT_PRESENTED") && g_hasPresentedImage && g_swapChain) {
            const uint32_t w=g_swapChain->getWidth(),h=g_swapChain->getHeight(),pitch=(w*4+255)&~255u;
            if(!w || !h) return false;
            auto readback=g_device->createBuffer(plume::RenderBufferDesc::ReadbackBuffer(uint64_t(pitch)*h));
            auto* frame=g_vulkan ? g_presentedSnapshot.get() : g_swapChain->getTexture(g_lastPresentedImage);
            if(!frame)return false;
            if (!WaitForPresentGpu()) return false;
            if (!BeginGpuCommands(g_commandList.get())) return false;
            g_commandList->barriers(plume::RenderBarrierStage::COPY,plume::RenderTextureBarrier(frame,plume::RenderTextureLayout::COPY_SOURCE));
            g_commandList->copyTextureRegion(plume::RenderTextureCopyLocation::PlacedFootprint(readback.get(),kSwapChainFormat,w,h,1,pitch/4),plume::RenderTextureCopyLocation::Subresource(frame));
            if(!g_vulkan) g_commandList->barriers(plume::RenderBarrierStage::NONE,plume::RenderTextureBarrier(frame,plume::RenderTextureLayout::PRESENT));
            if (!EndGpuCommands(g_commandList.get())) return false; const plume::RenderCommandList* lists[]={g_commandList.get()};
            uint64_t submissionSerial = 0; int32_t submitResult = 0;
            const bool submitted = g_vulkan ? SubmitVulkan(lists, 1, nullptr, 0, nullptr, 0,
                g_fence.get(), &submissionSerial, &submitResult)
                : (g_queue->executeCommandLists(lists,1,nullptr,0,nullptr,0,g_fence.get()), true);
            if (!submitted) { LOG_ERROR("video: screenshot submit failed raw_vk={}", submitResult); return false; }
            g_presentPending = true;
            if (!WaitForGpuFence(g_fence.get())) { DrainGpuForShutdown(); return false; }
            g_presentPending = false;
            std::vector<uint32_t> pixels(size_t(w)*h); const auto* data=static_cast<const uint8_t*>(readback->map());
            for(uint32_t y=0;y<h;y++) memcpy(pixels.data()+size_t(y)*w,data+size_t(y)*pitch,w*4);
            readback->unmap(); return WritePpm(path,pixels,w,h);
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
}
