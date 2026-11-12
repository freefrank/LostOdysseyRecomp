#pragma once

// Switch: optional Lossless Scaling frame generation (LSFG 2x) on the
// presenter's own Vulkan device, after NetherSX2_nx's same-device bridge to
// lsfg-vk (thirdparty/lsfg-vk, GPL-3.0-or-later). The user supplies their own
// Lossless.dll (from the Steam version of Lossless Scaling) at
// <data root>/lsfg/Lossless.dll; nothing from it ships with the port.
//
// LO_FRAMEGEN=1 turns it on. Each real frame then presents one generated
// in-between frame and the real one, both through FIFO at the display rate,
// so a steady 30 fps game shows 60 frames per second.
//   LO_FRAMEGEN_MULTIPLIER=3       two generated frames (20 fps game, 60 shown)
//   LO_FRAMEGEN_LOCK=1             x2 with every image held two refreshes (15/30)
//   LO_FRAMEGEN_FLOW=<0.125..0.5>  motion estimation scale (default 0.25)
//   LO_FRAMEGEN_PERF=0             quality mode (default: performance mode)

#include <cstdint>

namespace plume
{
struct RenderSwapChain;
struct RenderCommandSemaphore;
}

namespace gpu::lsfg
{
#if defined(LO_VIDEO_SUBMISSION_UNIT)
// Host unit tests build video.cpp without the frame generation backend.
inline bool Enabled() { return false; }
inline bool Present(plume::RenderSwapChain*, uint32_t, plume::RenderCommandSemaphore*, bool&) { return false; }
inline void Reset() {}
inline void BeforeAcquire() {}
#else
// LO_FRAMEGEN=1 and not given up after a failure.
bool Enabled();

// Presents swapchain image `imageIndex` (ready when `waitSemaphore` signals)
// with a generated frame before it. Returns false when frame generation did
// not handle the present (disabled, unsupported swapchain, setup failure);
// the caller then presents normally. `presented` reports the real frame.
bool Present(plume::RenderSwapChain* swapChain, uint32_t imageIndex, plume::RenderCommandSemaphore* waitSemaphore,
             bool& presented);

// Releases the frame generation resources (swapchain recreation, shutdown).
void Reset();

// Call before acquiring a swapchain image: waits until the previous frame's
// generated and real images (presented on a worker thread) are queued, since
// the Switch allows only one acquired image at a time.
void BeforeAcquire();
#endif
}
