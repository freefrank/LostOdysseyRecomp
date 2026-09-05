#pragma once
#include <cstdint>

namespace apu::xma
{
// Xenia XmaContext::TrySetupNextLoop: packet advancement may pass loop_end,
// so equality is insufficient. Retain the input buffer until loops finish.
inline bool RestartLoop(uint32_t start, uint32_t end, bool bufferEnded,
                        uint32_t& readOffset, uint32_t& count)
{
    if (!count || start >= end || (!bufferEnded && readOffset < end)) return false;
    readOffset = start;
    if (count != 255) --count;
    return true;
}
}
