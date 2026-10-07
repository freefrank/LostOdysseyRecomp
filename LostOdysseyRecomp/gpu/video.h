#pragma once

#include <cstddef>
#include <cstdint>
#include "backend_selection.h"
#include "display_change.h"
#include "present_capture.h"
#include "upscaling_plan.h"
#include "frame_generation_status.h"
#include "../../shared/frame_generation/core.h"
namespace settings { struct Config; }

namespace plume
{
    struct RenderDevice;
    struct RenderCommandQueue;
    struct RenderCommandList;
    struct RenderCommandFence;
}
namespace gpu::dlss { class Controller; }
namespace gpu { class TemporalUpscaler; }

// Host presentation layer: SDL window + plume render device. Owned by the
// command processor thread; on Windows the SDL/Debug Menu windows have a
// dedicated owner thread so GPU stalls cannot block their message pump.
namespace gpu::video
{
    // Shared with the draw backend (nullptr when no device is available).
    plume::RenderDevice* GetDevice();
    bool IsVulkan();
    bool IsMetal();
    // SPIR-V shaders and the Vulkan descriptor layout: Vulkan, and Metal (whose
    // plume backend translates SPIR-V to MSL).
    bool UsesSpirv();
    // The game window's height in points (logical pixels), or 0 when no window
    // exists. On Retina displays the drawable is larger than this.
    uint32_t LogicalOutputHeight();
    // The device samples BC1-BC3 images (false on Mali Vulkan drivers, #214).
    bool TextureCompressionBC();
    // Actual committed backend; absent before readiness or after shutdown.
    std::optional<backend::Backend> SelectedBackend();
    // Adapter names the running backend lists (duplicates removed) and the one
    // in use; empty before device creation. Safe to call from the menu thread.
    std::vector<std::string> GpuDeviceNames();
    std::string ActiveGpuDeviceName();
    // Latest committed device capability. Callers receive a copy and do not
    // read NGX reports or device pointers. The device owner publishes it.
    upscaling::BackendDeviceSnapshot BackendDeviceState();
    plume::RenderCommandQueue* GetQueue();
#if defined(LO_GPU_PLUME)
    // The renderer borrows the persistent controller. Video remains responsible
    // for its device lifetime and final drained shutdown.
    dlss::Controller* GetDlssController();
    TemporalUpscaler* GetTemporalUpscaler();
    bool GpuWorkStopped();
    bool BeginGpuCommands(plume::RenderCommandList* list);
    bool EndGpuCommands(plume::RenderCommandList* list);
    void StopGpuWork(int32_t nativeResult);
    // Caller must own a fence attached to a successful submission. Failure
    // never authorizes retirement, descriptor reuse or CPU readback.
    bool WaitForGpuFence(plume::RenderCommandFence* fence);
    // Shutdown only: device loss authorizes disposal, not successful completion.
    // An unprovable drain terminates without running native resource destructors.
    void DrainGpuForShutdown();
    // Native queue submission. The serial advances only after the backend's
    // checked submission and fence signal succeed. D3D12 may execute commands
    // before a later Signal failure; that case requires retaining their uses.
    bool SubmitRendererBatch(const plume::RenderCommandList* const* lists, uint32_t count,
        plume::RenderCommandFence* fence, uint64_t& submissionSerial, int32_t& rawResult,
        bool* executionMayBeInFlight = nullptr);
#endif

    // Finite startup transaction: window -> device/caps -> presentation -> renderer.
    // Failure cleans resources before fallback. False aborts ordinary guest startup;
    // explicit LO_HEADLESS / LO_NO_RENDERER remain diagnostic opt-outs.
    bool Init();
    void Shutdown();
    // Window thread publishes a request; only the command/presentation owner
    // may destroy GPU resources and finish process exit.
    bool ExitRequested();
    // GPU owner-thread queries. Capture is required even while a failed FG
    // configuration is blocked, so the next valid configuration can recover.
    bool FrameGenerationInputCaptureEnabled();
    bool FrameGenerationAvailable();
    // GPU owner only. Keeps the configured native/guest target independent of
    // the display/FG-adjusted host deadline. No SDL calls on the render thread.
    uint32_t GetFramePacingTarget(uint32_t nativeTarget, bool hostOverlay = false);
    struct DynamicFgOutputPacing { uint32_t outputFps = 0; uint64_t actualPresents = 0; };
    // GPU owner only. Cumulative SDK presents are observations, not physical scanout.
    DynamicFgOutputPacing GetDynamicFgOutputPacing(uint32_t nativeTarget);
    struct FrameGenerationStatus {
        FrameGenerationPhase phase = FrameGenerationPhase::Off;
        framegen::Provider requested = framegen::Provider::Off;
        framegen::Provider applied = framegen::Provider::Off;
        framegen::Provider sessionProvider = framegen::Provider::Off;
        uint32_t requestedMultiplier = 2;
        uint32_t appliedMultiplier = 2;
        bool environmentOverride = false;
    };
    // Ready means the SDK session and swapchain exist, not that a generated
    // frame was observed. Safe to query from the menu/window thread.
    FrameGenerationStatus GetFrameGenerationStatus();
    void RequestExit();
    [[noreturn]] void FinishRequestedExit();

    // Drains messages on non-Windows hosts; Windows pumps on its window thread.
    void PumpEvents();
    // macOS main-thread idle pump while the video thread owns event handling.
    void PumpIdleEvents();
    bool DisplayModeFailed();
    // Alt+Enter is session-only; saving Display settings takes precedence.
    bool WindowModeOverridden();
    // Called after saving Current(). Forces an actual retry even for the same
    // mode. Completion includes the window operation and one presented frame.
    uint64_t BeginDisplayChange(const settings::Config& config);
    DisplayChangeResult QueryDisplayChange(uint64_t ticket);
    // Updates the title on the window owner thread. total=0 restores the title.
    enum class PreparationStage : uint32_t { Shaders, Pipelines, CacheValidation, IndexedExtraction, FallbackScan, CachedShaders };
    enum class PreparationUnit : uint32_t { Shaders, Pipelines, Files, MiB, Entries };
    void SetShaderPreparationProgress(uint32_t completed, uint32_t total,
        PreparationStage stage = PreparationStage::Shaders, PreparationUnit unit = PreparationUnit::Shaders);
    bool ShaderPreparationSkipped();
    void RequestSkipShaderPreparation();
    void ResetShaderPreparationSkip();

    // Untiles the guest frontbuffer (a tiled 32bpp texture written by the
    // GPU resolve) into an upload buffer and presents it.
    void PresentFrontbuffer(uint32_t physicalAddress, uint32_t width, uint32_t height, uint32_t copyDestInfo,
        const present_capture::Ticket *capture = nullptr, present_capture::Result *captureResult = nullptr);
#if defined(LO_GPU_PLUME)
    // Observation for the capture fixture. Production frames leave these at zero
    // unless an explicit ticket queued a readback.
    void SetPresentCaptureCompletionFault(bool fail);
    uint64_t PresentCaptureAllocationCount();
    uint64_t PresentCaptureCopyCount();
    uint64_t PresentCaptureMapCount();
    size_t PresentCaptureRetainedBuffers();
#endif
    // Waits the independent presentation submission before renderer resources
    // referenced by it are retired. Called on the command processor thread.
    bool WaitForPresentGpu();
    // Presents the host menu overlay (settings or debug overlay) on the presentation thread.
    bool IsHostOverlayActive();
    void PresentHostOverlay();
    // Monotonic successful swap-chain present calls; read on the command thread.
    // Early returns and failed presents do not advance this counter.
    uint64_t CompletedPresentCount();

    // Writes the last untiled frontbuffer as a binary PPM (for offline inspection).
    bool SaveScreenshot(const char* path);

    // Xenos 2D tiled texture addressing (Xenia texture_address.xesli,
    // XenosTextureTiledAddress2D). Coordinates are in blocks, pitch in blocks
    // (multiple of 32), returns a byte offset.
    uint32_t TiledOffset2D(uint32_t x, uint32_t y, uint32_t pitchBlocks, uint32_t bytesPerBlockLog2);
}
