#pragma once

// XMA hardware decoder emulation, modelled on Xenia's XmaDecoder /
// XmaContextFake (BSD). The title only imports XMACreateContext and
// XMAReleaseContext; everything else happens through the 320-entry context
// array in physical memory and the MMIO registers at 0x7FEA0000, which the
// statically linked XAudio driver pokes directly (stwbrx / lwbrx). Guest MMIO
// is plain memory for us, so a worker thread polls the kick/lock/clear
// register words instead of trapping the accesses.
//
// Assemble XMA frames across packet/input-buffer boundaries and decode them
// with Xenia's FFmpeg XMAFRAMES codec. The guest consumes big-endian int16
// subframes from its output ring.

#include <cstdint>

namespace apu::xma
{
    constexpr uint32_t kContextCount = 320;
    constexpr uint32_t kContextSize = 64;
    constexpr uint32_t kRegisterBase = 0x7FEA0000;

    void Init();
    void Shutdown();

    // XMACreateContext: guest virtual address of a free context, or 0.
    uint32_t AllocateContext();
    // XMAReleaseContext.
    void ReleaseContext(uint32_t guestAddress);
}
