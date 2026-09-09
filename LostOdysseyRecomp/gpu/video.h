#pragma once

#include <cstdint>
#include "backend_selection.h"
#include "display_change.h"
namespace settings { struct Config; }

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
    // Actual committed backend; absent before readiness or after shutdown.
    std::optional<backend::Backend> SelectedBackend();
    plume::RenderCommandQueue* GetQueue();

    // Finite startup transaction: window -> device/caps -> presentation -> renderer.
    // Failure cleans resources before fallback. False aborts ordinary guest startup;
    // explicit LO_HEADLESS / LO_NO_RENDERER remain diagnostic opt-outs.
    bool Init();
    void Shutdown();

    // Drains messages on non-Windows hosts; Windows pumps on its window thread.
    void PumpEvents();
    bool DisplayModeFailed();
    // Called after saving Current(). Forces an actual retry even for the same
    // mode. Completion includes the window operation and one presented frame.
    uint64_t BeginDisplayChange(const settings::Config& config);
    DisplayChangeResult QueryDisplayChange(uint64_t ticket);
    // Updates the title on the window owner thread. total=0 restores the title.
    enum class PreparationStage : uint32_t { Shaders, Pipelines, CacheValidation, IndexedExtraction, FallbackScan, CachedShaders };
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
