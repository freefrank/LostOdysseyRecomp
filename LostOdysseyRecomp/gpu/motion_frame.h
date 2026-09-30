#pragma once
#include "temporal_frame_inputs.h"
#include <cstdint>
namespace plume { struct RenderTexture; }
namespace gpu::temporal {
// Borrowed resources. Producer and consumer run on the same ordered graphics
// queue; the producer retains them through the renderer's normal submission fence.
struct MotionFrameView {
    plume::RenderTexture* velocity = nullptr;
    plume::RenderTexture* depths = nullptr; // R32G32_FLOAT: actual current / replayed previous reverse depth
    plume::RenderTexture* reactive = nullptr; // R8_UNORM: 1 invalid, 0 reusable
    uint64_t frame = ~0ull, epoch = 0, depthAllocation = 0;
    uint32_t width = 0, height = 0;
    bool ready = false;
    MotionState state = MotionState::Unavailable;
};
}
