#pragma once
#include <chrono>
#include <cstdint>
#include <vector>

// Settings → Graphics → Performance overlay over game frames, laid out like
// the Steam Deck's MangoHud bar. FPS: a corner block with the frame rate and
// frame time. Detailed: a strip across the top with the upscaler and its
// resolution, the backend with FPS, frame time and a frame-time graph, frame
// generation's presented FPS, and the process CPU and GPU load. Values are
// averaged over about half a second and the image is redrawn only then.
// Presentation thread only.
namespace ui::performance_overlay {
inline constexpr uint32_t Off = 0, Fps = 1, Detailed = 2;

struct Image {
    std::vector<uint32_t> pixels; // straight-alpha RGBA8 (R in the low byte)
    uint32_t width = 0, height = 0;
    int x = 0, y = 0;      // top-left in the output
    uint64_t revision = 0; // changes whenever the pixels do
};

// What presentation knows when the readout refreshes.
struct Sources {
    const char* backend = "";        // VULKAN, D3D12 or METAL
    const char* upscaler = "NATIVE"; // DLSS, FSR, XeSS, MetalFX or NATIVE
    uint32_t renderWidth = 0, renderHeight = 0;
    uint32_t outputWidth = 0, outputHeight = 0; // the upscaler's output; 0 without one
    bool frameGeneration = false;    // a provider generates frames
    uint64_t presentedFrames = 0;    // its cumulative presents, generated frames included
    uint64_t gpuBusy = 0;            // gpu::gpu_busy::Total()
};

// Once per presented game frame while the overlay is on. True when the
// readout refreshes on this frame; the caller then passes Sources to Refresh.
bool Frame(uint32_t mode, std::chrono::steady_clock::time_point now, uint32_t outputWidth, uint32_t outputHeight);
void Refresh(const Sources& sources);
// The image to draw; empty before the first refresh.
const Image& Current();
// The overlay was turned off: drops the image and stops GPU timing.
void Stop();
}
