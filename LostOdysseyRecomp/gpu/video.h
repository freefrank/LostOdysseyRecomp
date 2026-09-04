#pragma once

#include <cstdint>

// Host presentation layer: SDL window + plume render device. Owned by the
// command processor thread, which is the only thread that calls into it.
namespace gpu::video
{
    // Creates the window and the render device. Safe to call repeatedly;
    // returns false when no device is available (the game keeps running
    // headless in that case).
    bool Init();
    void Shutdown();

    // Drains window messages. Must be called from the thread that called Init.
    void PumpEvents();

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
