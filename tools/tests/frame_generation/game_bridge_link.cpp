#include "gpu/frame_generation_d3d12.h"
int main() {
    // Links every out-of-line production bridge method and both selected SDK
    // adapters without initializing a device, SDK runtime, window, or game.
    gpu::frame_generation::D3D12Bridge bridge;
    return bridge.Available() ? 1 : 0;
}
