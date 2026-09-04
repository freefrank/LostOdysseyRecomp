#include <stdafx.h>
#include "video.h"
#include <kernel/memory.h>
#include <os/logger.h>

#include <SDL.h>
#include <SDL_syswm.h>

#ifdef LO_GPU_PLUME
#include <plume_render_interface.h>
#ifdef _WIN32
#include <plume_d3d12.h>
#endif
#endif

#include <vector>

#ifdef LO_GPU_PLUME
namespace plume
{
    // Defined in plume_d3d12.cpp / plume_vulkan.cpp but not exported by a header.
    std::unique_ptr<RenderInterface> CreateD3D12Interface();
    std::unique_ptr<RenderInterface> CreateVulkanInterface();
}
#endif

namespace gpu::video
{
    namespace
    {
        constexpr uint32_t kMaxWidth = 1920;
        constexpr uint32_t kMaxHeight = 1080;

        SDL_Window* g_window = nullptr;
        bool g_initAttempted = false;
        bool g_available = false;

        std::vector<uint32_t> g_pixels;   // last untiled frame, R8G8B8A8
        uint32_t g_frameWidth = 0, g_frameHeight = 0;

#ifdef LO_GPU_PLUME
        std::unique_ptr<plume::RenderInterface> g_interface;
        std::unique_ptr<plume::RenderDevice> g_device;
        std::unique_ptr<plume::RenderCommandQueue> g_queue;
        std::unique_ptr<plume::RenderCommandList> g_commandList;
        std::unique_ptr<plume::RenderCommandFence> g_fence;
        std::unique_ptr<plume::RenderCommandSemaphore> g_acquireSemaphore;
        std::unique_ptr<plume::RenderCommandSemaphore> g_releaseSemaphore;
        std::unique_ptr<plume::RenderSwapChain> g_swapChain;
        std::unique_ptr<plume::RenderBuffer> g_uploadBuffer;
        constexpr plume::RenderFormat kSwapChainFormat = plume::RenderFormat::R8G8B8A8_UNORM;
        constexpr uint32_t kSwapChainBuffers = 3;
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
    }

    plume::RenderDevice* GetDevice()
    {
#ifdef LO_GPU_PLUME
        return g_available ? g_device.get() : nullptr;
#else
        return nullptr;
#endif
    }

    plume::RenderCommandQueue* GetQueue()
    {
#ifdef LO_GPU_PLUME
        return g_available ? g_queue.get() : nullptr;
#else
        return nullptr;
#endif
    }

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

        if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
        {
            LOG_WARNING("video: SDL video init failed: {}", SDL_GetError());
            return false;
        }

        g_window = SDL_CreateWindow("Lost Odyssey Recompiled", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            1280, 720, SDL_WINDOW_SHOWN);
        if (!g_window)
        {
            LOG_WARNING("video: window creation failed: {}", SDL_GetError());
            return false;
        }

#ifdef LO_GPU_PLUME
        SDL_SysWMinfo wmInfo{};
        SDL_VERSION(&wmInfo.version);
        SDL_GetWindowWMInfo(g_window, &wmInfo);

#ifdef _WIN32
        g_interface = plume::CreateD3D12Interface();
        plume::RenderWindow renderWindow = wmInfo.info.win.window;
#else
        plume::RenderWindow renderWindow{};
#endif
        if (!g_interface)
        {
            LOG_WARNING("video: no render interface available");
            return false;
        }

        g_device = g_interface->createDevice();
        if (!g_device)
        {
            LOG_WARNING("video: device creation failed");
            return false;
        }

        g_queue = g_device->createCommandQueue(plume::RenderCommandListType::DIRECT);
        g_commandList = g_queue->createCommandList();
        g_fence = g_device->createCommandFence();
        g_acquireSemaphore = g_device->createCommandSemaphore();
        g_releaseSemaphore = g_device->createCommandSemaphore();
        g_swapChain = g_queue->createSwapChain(plume::RenderSwapChainDesc(renderWindow, kSwapChainFormat, kSwapChainBuffers));
        g_uploadBuffer = g_device->createBuffer(plume::RenderBufferDesc::UploadBuffer(kMaxWidth * kMaxHeight * 4));

        LOG_INFO("video: {} on {}", "D3D12", g_device->getDescription().name);
        g_available = true;
#endif
        return g_available;
    }

    void Shutdown()
    {
#ifdef LO_GPU_PLUME
        if (g_queue && g_fence)
        {
            // Nothing in flight after the last present's fence wait.
        }
        g_uploadBuffer.reset();
        g_swapChain.reset();
        g_releaseSemaphore.reset();
        g_acquireSemaphore.reset();
        g_fence.reset();
        g_commandList.reset();
        g_queue.reset();
        g_device.reset();
        g_interface.reset();
#endif
        if (g_window)
        {
            SDL_DestroyWindow(g_window);
            g_window = nullptr;
        }
        g_available = false;
    }

    void PumpEvents()
    {
        if (!g_window)
            return;
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                LOG_INFO("video: window closed, exiting");
                fflush(stdout);
                std::_Exit(0);
            }
        }
    }

    void PresentFrontbuffer(uint32_t physicalAddress, uint32_t width, uint32_t height, uint32_t copyDestInfo)
    {
        if (width == 0 || height == 0 || width > kMaxWidth || height > kMaxHeight)
            return;

        // Untile: 32bpp blocks, pitch rounded up to a 32-block macro tile.
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

#ifdef LO_GPU_PLUME
        if (!g_available)
            return;

        if (g_swapChain->needsResize())
            g_swapChain->resize();
        if (g_swapChain->isEmpty())
            return;

        // Upload the untiled pixels; rows must be 256-byte aligned for D3D12.
        const uint32_t rowPitch = (width * 4 + 255) & ~255u;
        auto* mapped = static_cast<uint8_t*>(g_uploadBuffer->map());
        for (uint32_t y = 0; y < height; y++)
            memcpy(mapped + size_t(y) * rowPitch, &g_pixels[size_t(y) * width], size_t(width) * 4);
        g_uploadBuffer->unmap();

        uint32_t imageIndex = 0;
        if (!g_swapChain->acquireTexture(g_acquireSemaphore.get(), &imageIndex))
            return;
        plume::RenderTexture* backBuffer = g_swapChain->getTexture(imageIndex);
        const uint32_t copyWidth = std::min(width, g_swapChain->getWidth());
        const uint32_t copyHeight = std::min(height, g_swapChain->getHeight());

        g_commandList->begin();
        g_commandList->barriers(plume::RenderBarrierStage::COPY, plume::RenderTextureBarrier(backBuffer, plume::RenderTextureLayout::COPY_DEST));
        plume::RenderBox box(0, 0, int32_t(copyWidth), int32_t(copyHeight), 0, 1);
        g_commandList->copyTextureRegion(
            plume::RenderTextureCopyLocation::Subresource(backBuffer),
            plume::RenderTextureCopyLocation::PlacedFootprint(g_uploadBuffer.get(), kSwapChainFormat, width, height, 1, rowPitch / 4),
            0, 0, 0, &box);
        g_commandList->barriers(plume::RenderBarrierStage::NONE, plume::RenderTextureBarrier(backBuffer, plume::RenderTextureLayout::PRESENT));
        g_commandList->end();

        const plume::RenderCommandList* lists[] = { g_commandList.get() };
        plume::RenderCommandSemaphore* waitSemaphore = g_acquireSemaphore.get();
        plume::RenderCommandSemaphore* signalSemaphore = g_releaseSemaphore.get();
        g_queue->executeCommandLists(lists, 1, &waitSemaphore, 1, &signalSemaphore, 1, g_fence.get());
        g_swapChain->present(imageIndex, &signalSemaphore, 1);
        g_queue->waitForCommandFence(g_fence.get());
#endif
    }

    bool SaveScreenshot(const char* path)
    {
        if (g_pixels.empty())
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
