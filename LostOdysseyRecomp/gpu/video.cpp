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
#include "../../shared/frame_generation/environment.h"
#include "frame_generation_settings.h"
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
#include <settings/menu.h>
#include <settings/restart.h>
#include <kernel/memory.h>
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
#endif
#include <os/logger.h>

#if defined(LO_GPU_PLUME) || defined(LO_VIDEO_SUBMISSION_UNIT)
#include <plume_render_interface.h>
#include <plume_vulkan.h>
#ifdef _WIN32
#include <plume_d3d12.h>
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
            if (g_window) { SDL_DestroyWindow(g_window); g_window = nullptr; }
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
        std::mutex g_fgSettingsMutex;
        framegen::Config g_fgAppliedConfig{};
        framegen::Provider g_fgSessionProvider = framegen::Provider::Off;
        std::optional<framegen::Config> g_fgFailedRequest;
#endif
#if defined(LO_ENABLE_STREAMLINE_FG) && defined(_WIN32)
        std::unique_ptr<dlss_fg::Runtime> g_fgRuntime;
        std::unique_ptr<dlss_fg::VulkanDispatch> g_fgDispatch;
        std::unique_ptr<dlss_fg::Session> g_fgSession;
#endif
#if defined(_WIN32) && (defined(LO_ENABLE_STREAMLINE_FG) || defined(LO_ENABLE_D3D12_FG))
        std::atomic<bool> g_fgWindowSynchronization{false};
        std::atomic<int> g_fgWindowChange{0}; // 0 idle, 1 requested, 2 GPU quiescent
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
        FgPresentBridge g_fgPresent;
        uint64_t g_fgPresentSerial = 0;
#endif
        std::unique_ptr<plume::RenderTexture> g_cpuFrame;
        std::unique_ptr<plume::RenderTexture> g_presentedSnapshot;
        uint32_t g_snapshotWidth=0,g_snapshotHeight=0;
        plume::RenderFormat g_snapshotFormat=plume::RenderFormat::UNKNOWN;
        uint32_t g_cpuWidth=0,g_cpuHeight=0;
        uint32_t g_lastPresentedImage=0;
        bool g_hasPresentedImage=false;
        bool g_presentPending=false;
        bool g_forceSwapResize=false;
        struct PresentCaptureCopy {
            bool queued = false;
            std::string failure;
            gpu::present_capture::Ticket ticket{};
            std::unique_ptr<plume::RenderBuffer> buffer;
            uint32_t width = 0, height = 0, pitch = 0;
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
                if (!g_queue || !g_fence || !WaitForGpuFence(g_fence.get())) {
                    g_fgPresent.WaitFailed();
                    return false;
                }
                const auto completed = g_fgPresent.Completed(g_deviceEpoch.load(), g_fgPresentSerial);
                if (completed) LOG_INFO("video: FG diagnostic completed={} present_serial={} provider_ready=0 ui=unavailable",
                    completed, g_fgPresentSerial);
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
            const auto format=g_swapChain->getFormat();
            if(!g_presentedSnapshot || w!=g_snapshotWidth || h!=g_snapshotHeight || format!=g_snapshotFormat) {
                g_presentedSnapshot=g_device->createTexture(plume::RenderTextureDesc::Texture2D(w,h,1,format));
                g_snapshotWidth=w;g_snapshotHeight=h;g_snapshotFormat=format;
            }
            if(!g_presentedSnapshot) return;
            g_commandList->barriers(plume::RenderBarrierStage::COPY,plume::RenderTextureBarrier(frame,plume::RenderTextureLayout::COPY_SOURCE));
            g_commandList->barriers(plume::RenderBarrierStage::COPY,plume::RenderTextureBarrier(g_presentedSnapshot.get(),plume::RenderTextureLayout::COPY_DEST));
            g_commandList->copyTexture(g_presentedSnapshot.get(),frame);
        }

        void PreparePresentImage(plume::RenderTexture* image) {
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
        }
        void FgHostSubmitted(bool success, uint64_t serial, int32_t nativeResult) {
#if defined(LO_ENABLE_D3D12_FG) && defined(_WIN32)
            if (g_d3dFg) g_d3dFg->HostSubmitted(success, serial);
#endif
#if defined(LO_ENABLE_STREAMLINE_FG) && defined(_WIN32)
            if (g_fgSession) g_fgSession->HostSubmitted(success, serial, nativeResult);
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
            const uint32_t width = g_swapChain->getWidth(), height = g_swapChain->getHeight();
            if (!width || !height || width > (UINT32_MAX - 255u) / 4u)
            {
                g_captureCopy.failure = "swapchain_extent";
                return;
            }
            const uint32_t pitch = (width * 4 + 255u) & ~255u;
            auto buffer = g_device->createBuffer(plume::RenderBufferDesc::ReadbackBuffer(uint64_t(pitch) * height));
            ++g_captureAllocs;
            if (!buffer)
            {
                g_captureCopy.failure = "allocation_failed";
                return;
            }
            g_commandList->barriers(plume::RenderBarrierStage::COPY, plume::RenderTextureBarrier(frame, plume::RenderTextureLayout::COPY_SOURCE));
            g_commandList->copyTextureRegion(
                plume::RenderTextureCopyLocation::PlacedFootprint(buffer.get(), kSwapChainFormat, width, height, 1, pitch / 4),
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
            result->backend = g_vulkan ? "Vulkan" : "D3D12";
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
            if (g_vulkan)
            {
                complete = WaitForGpuFence(g_fence.get());
                if (complete)
                {
                    result->hasSubmissionSerial = true;
                    result->submissionSerial = serial;
                    g_fgPresent.Completed(g_deviceEpoch.load(), serial);
                    g_presentPending = false;
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
                    g_presentPending = false;
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
            for (uint32_t y = 0; y < result->height; ++y)
                memcpy(result->pixels.data() + size_t(y) * result->width, mapped + size_t(y) * g_captureCopy.pitch, size_t(result->width) * 4);
            g_captureCopy.buffer->unmap();
            g_captureCopy.buffer.reset();
            g_captureCopy.queued = false;
            result->available = true;
            result->reason = "swapchain_readback";
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
            snapshot.backend = vulkan ? backend::Backend::Vulkan : backend::Backend::D3D12;
            snapshot.deviceEpoch = g_deviceEpoch.load(std::memory_order_acquire);
#if defined(LO_GPU_PLUME) && !defined(LO_VIDEO_SUBMISSION_UNIT)
            snapshot.deviceReady = g_available && g_device != nullptr;
            snapshot.dlssAvailable = g_dlssController &&
                g_dlssController->Report().state == dlss::ProbeState::Available;
            snapshot.fsrAvailable = snapshot.deviceReady &&
                (vulkan ? LO_HAS_FSR : LO_HAS_FSR_D3D12);
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
            snapshot.backend = g_vulkan.load(std::memory_order_acquire) ? backend::Backend::Vulkan : backend::Backend::D3D12;
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
                g_vulkan ? "Vulkan" : "D3D12", nativeResult);
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
        std::fflush(nullptr);
        std::_Exit(EXIT_FAILURE);
    }
#endif

    bool SubmitRendererBatch(const plume::RenderCommandList* const* lists, uint32_t count,
        plume::RenderCommandFence* fence, uint64_t& submissionSerial, int32_t& rawResult,
        bool* executionMayBeInFlight)
    {
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
    std::optional<backend::Backend> SelectedBackend() {
        const auto selected = g_selectedBackend.load();
        return selected < 0 ? std::nullopt : std::optional(static_cast<backend::Backend>(selected));
    }

    // The command thread calls this only before guest startup, or after all
    // rendering has stopped. The window/event thread is deliberately retained
    // between candidates; device children are destroyed before their parents.
    static void ResetGpu() {
#if defined(_WIN32) && (defined(LO_ENABLE_STREAMLINE_FG) || defined(LO_ENABLE_D3D12_FG))
        g_fgWindowSynchronization = false;
        g_fgWindowChange = 0;
#endif
#if defined(LO_ENABLE_D3D12_FG) && defined(_WIN32)
        if (g_d3dFg) g_d3dFg->Quiesce();
        {
            std::lock_guard lock(g_fgSettingsMutex);
            g_fgAppliedConfig = {};
            g_fgSessionProvider = framegen::Provider::Off;
            g_fgFailedRequest.reset();
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
        // Renderer shutdown above established a completed/lost-device teardown
        // boundary; a failed ordinary wait alone never releases these leases.
        g_fgPresent = FgPresentBridge{}; g_fgPresentSerial = 0;
        g_presentation.reset();
        g_uploadBuffer.reset();
#if defined(LO_ENABLE_D3D12_FG) && defined(_WIN32)
        // Release DLSS-G's NGX feature while both SDK sessions remain live.
        // Renderer and presentation work have already been drained.
        if (g_d3dFg) g_d3dFg->ReleaseFeatureAfterGpuDrain();
#endif
#ifdef _WIN32
        if (g_swapChain && !g_vulkan) {
            auto* swap = static_cast<plume::D3D12SwapChain*>(g_swapChain.get());
            if (swap->d3d) swap->d3d->SetFullscreenState(FALSE, nullptr);
        }
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
                g_temporalUpscaler = std::make_unique<TemporalUpscaler>(*g_dlssController);
#if defined(LO_ENABLE_STREAMLINE_FG)
                const auto fg = framegen::ParseEnvironment(std::getenv("LO_FG_PROVIDER"),
                    std::getenv("LO_FG_MODE"), std::getenv("LO_FG_MULTIPLIER"),
                    std::getenv("LO_FG_TARGET_FPS"), std::getenv("LO_DLSS_FG"));
                if (fg.error) LOG_ERROR("FG: {}", fg.error);
                const bool legacyFg = fg.Enabled() && fg.config.provider == framegen::Provider::Dlss &&
                    fg.config.mode == framegen::Mode::Fixed && fg.config.generatedFrames == 1;
                if (fg.Enabled() && !legacyFg) LOG_ERROR("FG: selected mode requires the D3D12 backend; Vulkan FG disabled");
                if (legacyFg) {
                    std::string reason;
                    g_fgRuntime = std::make_unique<dlss_fg::Runtime>();
                    if (!g_fgRuntime->Initialize(DlssRuntimePath(), reason)) {
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
                g_dlssController = std::make_unique<dlss::Controller>(DlssApplicationDataPath(), DlssRuntimePath());
                g_temporalUpscaler = std::make_unique<TemporalUpscaler>(*g_dlssController);
                g_interface = plume::CreateD3D12Interface();
            }
#else
            g_dlssController = std::make_unique<dlss::Controller>(DlssApplicationDataPath(), DlssRuntimePath());
            g_temporalUpscaler = std::make_unique<TemporalUpscaler>(*g_dlssController);
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
                    const char* fsrPath = std::getenv("LO_FSR_FG_RUNTIME");
                    const auto runtime = fg.config.provider == framegen::Provider::Fsr
                        ? (fsrPath && *fsrPath ? std::filesystem::path(fsrPath) : DlssRuntimePath() / "amd_fidelityfx_dx12.dll")
                        : DlssRuntimePath();
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
            }
#endif
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
            if (!getenv("LO_NO_RENDERER") && !renderer::Init()) return "renderer initialization failed";
            return {};
        }, ResetGpu);
        LOG_INFO("video: backend selection {}; configured={} (unchanged)", selection.Describe(), backend::Name(configured));
        if (selection.selected) {
            g_selectedBackend = static_cast<int>(*selection.selected);
            g_available = true; g_initializing = false;
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
        DestroyWindowResources();
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
        if (g_fgSession) return true;
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
        // The existing Vulkan session is fixed 2x. Availability is a last-frame
        // runtime observation, not evidence that the monitor is using VRR.
        if (requested && !hostOverlay && g_fgSession && g_fgSession->Available()) multiplier = 2;
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
#if defined(LO_ENABLE_D3D12_FG) && defined(_WIN32) && defined(LO_GPU_PLUME) && !defined(LO_VIDEO_SUBMISSION_UNIT)
        const auto request = frame_generation::ResolveD3D12Selection(settings::GetConfig(),
            std::getenv("LO_FG_PROVIDER"), std::getenv("LO_FG_MODE"),
            std::getenv("LO_FG_MULTIPLIER"), std::getenv("LO_FG_TARGET_FPS"), std::getenv("LO_DLSS_FG"), g_displayRefreshHz.load(std::memory_order_relaxed));
        status.requested = request.config.provider;
        status.requestedMultiplier = request.config.generatedFrames + 1;
        {
            std::lock_guard lock(g_fgSettingsMutex);
            status.applied = g_fgAppliedConfig.provider;
            status.sessionProvider = g_fgSessionProvider;
            status.appliedMultiplier = g_fgAppliedConfig.generatedFrames + 1;
            const auto backend = SelectedBackend();
            if (request.error || (backend && *backend != backend::Backend::D3D12) ||
                (g_fgFailedRequest && *g_fgFailedRequest == request.config))
                status.phase = request.config.provider == framegen::Provider::Off && !request.error
                    ? FrameGenerationPhase::Off : FrameGenerationPhase::Unavailable;
            else if (!backend || request.config != g_fgAppliedConfig)
                status.phase = request.config.provider == framegen::Provider::Off && !backend
                    ? FrameGenerationPhase::Off : FrameGenerationPhase::Pending;
            else
                status.phase = status.applied == framegen::Provider::Off
                    ? FrameGenerationPhase::Off : FrameGenerationPhase::Ready;
        }
#elif !defined(LO_VIDEO_SUBMISSION_UNIT)
        status.requested = settings::GetConfig().frameGenerationProvider;
        status.phase = status.requested == framegen::Provider::Off
            ? FrameGenerationPhase::Off : FrameGenerationPhase::Unavailable;
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
    static bool UploadAndPresentPixels(const std::vector<uint32_t>& pixels, uint32_t width, uint32_t height,
                                       bool isMenu, uint64_t displayTicket, const PresentationOptions& presentationOptions,
                                       const gpu::present_capture::Ticket *captureTicket, gpu::present_capture::Result *captureResult);
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

        UploadAndPresentPixels(s_prepPixels, width, height, true, 0, PresentationOptions{}, nullptr, nullptr);
    }
#endif

    void PollDisplayRefresh()
    {
        const auto now = std::chrono::steady_clock::now();
        if (now < g_nextRefreshPoll) return;
        g_nextRefreshPoll = now + std::chrono::milliseconds(500);
        uint32_t refresh = 0;
#ifdef _WIN32
        // DXGI exclusive fullscreen can change the mode outside SDL's cache.
        // Query the actual monitor containing this window, not the primary one.
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
                    UploadAndPresentPixels(s_clearPixels, width, height, true, 0, PresentationOptions{}, nullptr, nullptr);
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
#if defined(_WIN32) || defined(__linux__)
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
        if(reapply || !state.initialized || config.width!=state.applied.width || config.height!=state.applied.height || config.windowMode!=state.applied.windowMode) {
#if defined(_WIN32) && (defined(LO_ENABLE_STREAMLINE_FG) || defined(LO_ENABLE_D3D12_FG))
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
            g_nextRefreshPoll = {}; // Re-query after a mode transition on the next window pump.
            g_windowResizeRequested = true;
            g_displayChanges.WindowComplete(ticket, result == 0);
#if defined(_WIN32) && (defined(LO_ENABLE_STREAMLINE_FG) || defined(LO_ENABLE_D3D12_FG))
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
            if (event.type == SDL_DISPLAYEVENT ||
                (event.type == SDL_WINDOWEVENT &&
                 (event.window.event == SDL_WINDOWEVENT_MOVED || event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                  event.window.event == SDL_WINDOWEVENT_DISPLAY_CHANGED || event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)))
                g_nextRefreshPoll = {};
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
            LOG_WARNING("D3D12 FG: switch from DLSS to FSR requires restart while Streamline is retained for DLSS SR");
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
                desired.provider == framegen::Provider::Dlss ? "dlss" : "fsr", desired.generatedFrames + 1);
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
        auto* oldSwap = static_cast<plume::D3D12SwapChain*>(g_swapChain.get());
        if (oldSwap->d3d) oldSwap->d3d->SetFullscreenState(FALSE, nullptr);
        g_swapChain.reset();
        g_presentSemaphores.clear();
        g_d3dFg.reset();

        std::string reason;
        if (desired.provider != framegen::Provider::Off) {
            auto bridge = std::make_unique<frame_generation::D3D12Bridge>();
            const char* fsrPath = std::getenv("LO_FSR_FG_RUNTIME");
            const auto runtime = desired.provider == framegen::Provider::Fsr
                ? (fsrPath && *fsrPath ? std::filesystem::path(fsrPath) : DlssRuntimePath() / "amd_fidelityfx_dx12.dll")
                : DlssRuntimePath();
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
            desired.provider == framegen::Provider::Dlss ? "dlss" : desired.provider == framegen::Provider::Fsr ? "fsr" : "off",
            g_d3dFg ? (desired.provider == framegen::Provider::Dlss ? "dlss" : "fsr") : "off",
            desired.generatedFrames + 1);
        return true;
    }
#endif
    // Sole presentation-thread entry for mode changes and swap-chain recovery.
    // Both guest frames and host-only frames call this BEFORE reading dimensions
    // or rasterizing UI. The returned ticket belongs to these prepared operations.
    static bool PreparePresentation(uint64_t& displayTicket, uint32_t& width, uint32_t& height)
    {
#if defined(_WIN32) && (defined(LO_ENABLE_STREAMLINE_FG) || defined(LO_ENABLE_D3D12_FG))
        if (g_fgWindowChange.load() == 1) {
#if defined(LO_ENABLE_STREAMLINE_FG)
            if (g_fgSession) g_fgSession->Quiesce();
#endif
#if defined(LO_ENABLE_D3D12_FG)
            if (g_d3dFg) g_d3dFg->Quiesce();
#endif
            g_fgWindowChange = 2;
        }
#endif
        if (!g_available || !g_swapChain || GpuWorkStopped())
            return false;
        // Shared by real game frames and paused host overlays. Saving a new
        // cap applies here before any swapchain acquire, even while paused.
        gpu::SetFrameRateTarget(settings::GetConfig().frameRate);
#if defined(_WIN32) && defined(LO_ENABLE_D3D12_FG)
        if (!ReconcileD3D12FrameGeneration()) return false;
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
        const bool nativeVsync = vrr::HostVsyncEnabled(nativeTarget,
            nativePolicy.nativeVsyncBaseline, forceImmediate, settings::GetConfig().variableRefreshRate);
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
#ifdef _WIN32
        const int mode = g_displayMode.load();
        const uint64_t size = g_displaySize.load();
        auto& applied = g_presentationDisplay;
        if (!g_vulkan && mode >= 0 && (mode != applied.appliedMode || size != applied.appliedSize ||
            (displayTicket && displayTicket != applied.appliedTicket))) {
            if (!WaitForPresentGpu()) return false;
#if defined(LO_ENABLE_D3D12_FG)
            if (g_d3dFg) g_d3dFg->Quiesce();
#endif
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
#if defined(LO_ENABLE_D3D12_FG) && defined(_WIN32)
            if (g_d3dFg) g_d3dFg->Quiesce();
#endif
#if defined(LO_ENABLE_STREAMLINE_FG) && defined(_WIN32)
            if (g_fgSession) g_fgSession->Quiesce();
#endif
            g_fgPresent.CancelAll(frame_generation::HandoffCancel::DisplayChange);
            renderer::CancelFgHandoffs();
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

    static bool UploadAndPresentPixels(const std::vector<uint32_t>& pixels, uint32_t width, uint32_t height,
                                       bool isMenu, uint64_t displayTicket, const PresentationOptions& presentationOptions,
                                       const gpu::present_capture::Ticket *captureTicket, gpu::present_capture::Result *captureResult)
    {
        if (GpuWorkStopped() || (!g_available && !g_initializing) || !g_swapChain || g_swapChain->isEmpty())
            return false;
        g_fgPresent.CancelAll(frame_generation::HandoffCancel::AlternatePresent);
#if defined(LO_ENABLE_D3D12_FG) && defined(_WIN32)
        if (g_d3dFg) g_d3dFg->Quiesce();
#endif
#if defined(LO_ENABLE_STREAMLINE_FG) && defined(_WIN32)
        if (g_fgSession) g_fgSession->Prepare({}, width, height, 0, VK_FORMAT_UNDEFINED);
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
        const PresentationOptions presentationOptions{
            presentationConfig.antialiasing == 3 ? Antialiasing::SMAA : static_cast<Antialiasing>(presentationConfig.antialiasing),
            presentationConfig.scalingQuality ? ScalingFilter::Bicubic : ScalingFilter::Bilinear,
            presentationConfig.expandRgbRange};
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
                g_frameWidth = sourceWidth;
                g_frameHeight = sourceHeight;
                g_frontbufferPhysical = physicalAddress & 0x1FFFFFFF;
                g_frameOnGpu = true;
                if (g_swapChain->isEmpty())
                    return;
                DisplayCompletion completion(g_displayChanges, displayTicket);
                if (!WaitForPresentGpu()) return;
#if defined(_WIN32) && (defined(LO_ENABLE_STREAMLINE_FG) || defined(LO_ENABLE_D3D12_FG))
                frame_generation::CompositeHandoff composite;
                double fgProducerWaitMs = 0.0;
                if (FrameGenerationInputCaptureEnabled()) {
                    const auto fgAcquireBegin = std::chrono::steady_clock::now();
                    const bool matched = renderer::AcquireFgCompositeInputs(physicalAddress & 0x1FFFFFFF, composite) &&
                        composite.ReadyForOrderedSubmission() && composite.outputWidth == sourceWidth && composite.outputHeight == sourceHeight &&
                        (!g_vulkan || (sourceWidth == g_swapChain->getWidth() && sourceHeight == g_swapChain->getHeight()));
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
#if defined(LO_ENABLE_D3D12_FG) && defined(_WIN32)
                        if (g_d3dFg) g_d3dFg->CancelUnsubmitted(g_commandList.get());
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
                if(g_presentation) {
                    const auto decision = frame_plan::ResolvePresentationDecision(&sourcePlan,
                        renderer::SceneAAApplied(physicalAddress & 0x1FFFFFFF), uint32_t(presentationOptions.antialiasing),
                        uint32_t(presentationOptions.scalingFilter));
                    const PresentationOptions sourceOptions{decision.requestedAA == 3 ? Antialiasing::SMAA :
                        static_cast<Antialiasing>(decision.requestedAA),
                        decision.scalingQuality ? ScalingFilter::Bicubic : ScalingFilter::Bilinear,
                        presentationOptions.expandRgbRange};
                    if(decision.bypassAA)
                        g_presentation->DrawComposited(g_commandList.get(),source,backBuffer,sourceWidth,sourceHeight,
                            g_swapChain->getWidth(),g_swapChain->getHeight(),sourceOptions.scalingFilter,
                            sourceOptions.expandRgbRange);
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
                QueuePresentCapture(backBuffer, capture);
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
                completion.Complete(presented && !g_displayFailed.load());
                ReadPresentCapture(captureResult, true, presented, submissionSerial);
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

    bool SaveScreenshot(const char* path)
    {
#ifdef LO_GPU_PLUME
        if (GpuWorkStopped()) return false;
        if(getenv("LO_SCREENSHOT_PRESENTED") && g_hasPresentedImage && g_swapChain) {
            const auto format=g_vulkan ? g_snapshotFormat : g_swapChain->getFormat();
            if(format!=plume::RenderFormat::R8G8B8A8_UNORM &&
                format!=plume::RenderFormat::B8G8R8A8_UNORM) return false;
            const uint32_t w=g_vulkan ? g_snapshotWidth : g_swapChain->getWidth();
            const uint32_t h=g_vulkan ? g_snapshotHeight : g_swapChain->getHeight();
            const uint32_t pitch=(w*4+255)&~255u;
            if(!w || !h) return false;
            auto readback=g_device->createBuffer(plume::RenderBufferDesc::ReadbackBuffer(uint64_t(pitch)*h));
            auto* frame=g_vulkan ? g_presentedSnapshot.get() : g_swapChain->getTexture(g_lastPresentedImage);
            if(!frame)return false;
            if (!WaitForPresentGpu()) return false;
            if (!BeginGpuCommands(g_commandList.get())) return false;
            g_commandList->barriers(plume::RenderBarrierStage::COPY,plume::RenderTextureBarrier(frame,plume::RenderTextureLayout::COPY_SOURCE));
            g_commandList->copyTextureRegion(plume::RenderTextureCopyLocation::PlacedFootprint(readback.get(),format,w,h,1,pitch/4),plume::RenderTextureCopyLocation::Subresource(frame));
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
            for(uint32_t y=0;y<h;y++) memcpy(pixels.data()+size_t(y)*w,data+size_t(y)*pitch,w*4);
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
