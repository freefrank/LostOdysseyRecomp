#pragma once

#include <cstdint>

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

    // Called for every DRAW_INDX / DRAW_INDX_2 after the registers were updated.
    void Draw(const DrawInfo& info);

    // Called on XE_SWAP before the frontbuffer is presented: finishes all work.
    void Flush();

    // Guest memory range was written by the GPU (resolve) or is known dirty.
    void InvalidateGuestRange(uint32_t physicalAddress, uint32_t size);
}
