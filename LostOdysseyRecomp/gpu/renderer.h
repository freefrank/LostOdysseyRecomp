#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>
#include <string>
#include "present_capture.h"

namespace plume { struct RenderTexture; }
namespace gpu::frame_plan { struct FramePlan; }
namespace gpu::frame_plan { enum class SurfaceRole : uint32_t; }

// Xenos draw backend on plume: turns the command processor's register state
// plus a DRAW_INDX packet into host draws, emulates EDRAM render targets as
// host textures and writes resolves back into guest memory. Everything runs
// on the command processor thread.
namespace gpu::renderer
{
    struct DrawInfo
    {
        uint32_t primitiveType = 0;     // xenos::PrimitiveType
        uint32_t indexCount = 0;        // VGT_DRAW_INITIATOR.num_indices
        bool indexed = false;           // source select kDMA
        uint32_t indexBase = 0;         // guest physical address of the index buffer
        uint32_t indexBufferWords = 0;  // VGT_DMA_SIZE.num_words
        uint32_t indexEndian = 0;       // VGT_DMA_SIZE.swap_mode
        bool index32 = false;           // VGT_DRAW_INITIATOR.index_size
    };

    // Initialises against the presenter's device; returns false without a GPU.
    bool Init();
    void Shutdown();
    // Report the actual swapchain extent; the next renderer frame applies Auto.
    void SetOutputSize(uint32_t width, uint32_t height);
    // The aspect attached to the renderer's current allocation epoch. Guest
    // camera hooks use this rather than a concurrently resized swapchain.
    float ActiveOutputAspect();
    // The command processor commits only tagged CPU frame plans. Renderer
    // allocation consumes this immutable plan instead of the window size.
    void SelectFramePlan(const frame_plan::FramePlan& plan);
    void RegisterCatalogSurface(frame_plan::SurfaceRole role, uint32_t surfaceInfo, uint32_t colorInfo);
    // Ordered host-private movie command, consumed on the command processor
    // thread after the movie helper has drawn its safe-area destination.
    void ClearMovieBars(uint32_t surfaceInfo, uint32_t colorInfo,
        float x, float y, float width, float height, float safeLeft, float safeRight);
    // Convert the guest frontbuffer content extent to this surface's physical
    // pixels (storage padding remains excluded).
    void ScaleResolvedSize(uint32_t physicalAddress, uint32_t& width, uint32_t& height);
    // UI thread requests; render thread captures the next three complete frames.
    void RequestDebugCapture();
    std::wstring DebugCaptureStatus();
    bool DebugCaptureBusy();
    // Fixes this XE_SWAP's guest export and ticket before presentation.
    void PrepareDebugCaptureFrame(uint32_t frontbuffer, uint32_t swap, present_capture::Ticket &ticket);
    // Writes the final pre-present image, then advances or archives. No capture is a no-op.
    void CompleteDebugCaptureFrame(const present_capture::Result &presented);
    // Starts the next frame directory only after the current export has finished.
    void PollDebugCapture();
    // Normal window close waits for the in-flight archive before process exit.
    void WaitDebugCaptureArchive();

    // Called for every DRAW_INDX / DRAW_INDX_2 after the registers were updated.
    void Draw(const DrawInfo& info);

    // Called on XE_SWAP before the frontbuffer is presented: finishes all work.
    void Flush();
    // Record the frontbuffer COPY_SOURCE barrier on the still-open swap list.
    // Must run before Flush so Present does not submit a second command list.
    void PreparePresent(uint32_t physicalAddress);
    // A failed allocation epoch must not fall through to stale CPU frontbuffer data.
    bool SuppressPresent();

    // Guest memory range was written by the GPU (resolve) or is known dirty.
    void InvalidateGuestRange(uint32_t physicalAddress, uint32_t size);

    // Resolves stay on the GPU: the surface last resolved to a guest physical
    // address lives in a host texture. PreparePresent records the COPY_SOURCE
    // barrier on the swap list; AcquireResolvedSurface then hands that texture
    // over (format is a plume::RenderFormat) without a second Flush, or returns
    // nullptr when nothing was resolved there.
    plume::RenderTexture* AcquireResolvedSurface(uint32_t physicalAddress, uint32_t& width, uint32_t& height, uint32_t& format,
        frame_plan::FramePlan* sourcePlan = nullptr);
    // Same renderer/presentation thread, after XE_SWAP Flush and acquisition.
    // True only for a full resolve of the actual processed scene target in the
    // just-completed frame; stale surfaces and unrecognized paths return false.
    bool SceneAAApplied(uint32_t physicalAddress);
    // Reads such a surface back as R8G8B8A8 pixels (screenshots, debugging).
    bool ReadbackResolvedSurface(uint32_t physicalAddress, std::vector<uint32_t>& pixels, uint32_t& width, uint32_t& height);
    // Physical addresses of every surface currently held (debugging dumps).
    std::vector<uint32_t> GetResolvedAddresses();
    // Writes every colour render target as <prefix>_rt_<base>_<fmt>_<w>x<h>.ppm (debugging).
    void DumpRenderTargets(const char* prefix);
#if defined(LO_RENDERER_P2_SELFTEST)
    // Guarded runtime bootstrap for the scene-copy promotion GPU fixture.
    int RunSceneCopyPromotionSelfTest(const std::filesystem::path& evidenceDirectory);
#endif
}
