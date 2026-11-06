#pragma once
#include <cstdint>

namespace render_fence
{
    // True while a guest thread has waited at least 250 ms on a render command
    // fence (FRenderCommandFence::Wait, 0x82322478): a loading screen whose
    // rendering thread is draining queued commands (cpu/poll_wait.cpp).
    bool LongWait();
    // Load boost: during a long fence wait the rendering thread runs one
    // queued load command per loading-screen frame, and each frame waits for
    // the GPU, which waits for two vblanks (30 fps). The command processor
    // then delivers vblanks at LoadBoostVblankHz (LO_LOAD_VBLANK_HZ, default
    // 480), stops pacing, and shows about 30 loading-screen frames a second.
    // Off by default: no shorter loads in testing, and the loading wheel
    // flickered. LO_LOAD_BOOST=1 turns it on.
    bool LoadBoost();
    uint32_t LoadBoostVblankHz();
    // Rendering thread, around each queued command (gpu/frame_plan.cpp hooks
    // at 0x824857C8 / 0x824857F8): timed only during a fence wait.
    void CommandBegin(uint32_t command);
    void CommandEnd();
}
