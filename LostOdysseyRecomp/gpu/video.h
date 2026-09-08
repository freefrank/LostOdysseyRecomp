#pragma once

#include <cstdint>

namespace plume
{
    struct RenderDevice;
    struct RenderCommandQueue;
}

// Host presentation layer: SDL window + plume render device. Owned by the
// command processor thread; on Windows the SDL/Debug Menu windows have a
// dedicated owner thread so GPU stalls cannot block their message pump.
namespace gpu::video
{
    // Shared with the draw backend (nullptr when no device is available).
    plume::RenderDevice* GetDevice();
    bool IsVulkan();
    plume::RenderCommandQueue* GetQueue();

    // Creates the window and the render device. Safe to call repeatedly;
    // returns false when no device is available (the game keeps running
    // headless in that case).
    bool Init();
    void Shutdown();

    // Drains messages on non-Windows hosts; Windows pumps on its window thread.
    void PumpEvents();
    bool DisplayModeFailed();
    // Updates the title on the window owner thread. total=0 restores the title.
    enum class PreparationStage : uint32_t { Shaders, Pipelines, CacheValidation, IndexedExtraction, FallbackScan };
    enum class PreparationUnit : uint32_t { Shaders, Pipelines, Files, MiB, Entries };
    void SetShaderPreparationProgress(uint32_t completed, uint32_t total,
        PreparationStage stage = PreparationStage::Shaders, PreparationUnit unit = PreparationUnit::Shaders);

    // Untiles the guest frontbuffer (a tiled 32bpp texture written by the
    // GPU resolve) into an upload buffer and presents it.
    void PresentFrontbuffer(uint32_t physicalAddress, uint32_t width, uint32_t height, uint32_t copyDestInfo);

    // Writes the last untiled frontbuffer as a binary PPM (for offline inspection).
    bool SaveScreenshot(const char* path);

    // Xenos 2D tiled texture addressing (Xenia texture_address.xesli,
    // XenosTextureTiledAddress2D). Coordinates are in blocks, pitch in blocks
    // (multiple of 32), returns a byte offset.
    uint32_t TiledOffset2D(uint32_t x, uint32_t y, uint32_t pitchBlocks, uint32_t bytesPerBlockLog2);
}
