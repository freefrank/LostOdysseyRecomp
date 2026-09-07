#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace plume { struct RenderTexture; }

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
    // UI thread requests; render thread captures the next complete frame.
    void RequestDebugCapture();
    std::wstring DebugCaptureStatus();
    bool DebugCaptureBusy();
    void FinishDebugCapture(uint32_t frontbuffer);

    // Called for every DRAW_INDX / DRAW_INDX_2 after the registers were updated.
    void Draw(const DrawInfo& info);

    // Called on XE_SWAP before the frontbuffer is presented: finishes all work.
    void Flush();

    // Guest memory range was written by the GPU (resolve) or is known dirty.
    void InvalidateGuestRange(uint32_t physicalAddress, uint32_t size);

    // Resolves stay on the GPU: the surface last resolved to a guest physical
    // address lives in a host texture. AcquireResolvedSurface flushes pending
    // work and hands that texture over in COPY_SOURCE layout (format is a
    // plume::RenderFormat), or returns nullptr when nothing was resolved there.
    plume::RenderTexture* AcquireResolvedSurface(uint32_t physicalAddress, uint32_t& width, uint32_t& height, uint32_t& format);
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
}
