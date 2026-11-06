#pragma once
#include <cstdint>

namespace gpu::cinematic_bars
{
// Called from the existing engine tick; no object enumeration or guest calls.
void UpdatePause(uint8_t* base);
// Presentation consumes host atomics, never follows guest objects.
bool Active();
}
