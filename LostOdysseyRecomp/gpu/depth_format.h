#pragma once
#include <cstdint>

namespace gpu {
// A float intermediate can round (1 * 0xFFFFFF + 0.5) to 0x1000000,
// which loses all depth bits when packed above the stencil byte.
constexpr uint32_t PackDepth24Unorm(float depth) {
  if (!(depth > 0.0f))
    return 0;
  if (depth >= 1.0f)
    return 0xFFFFFF;
  return uint32_t(double(depth) * 16777215.0 + 0.5);
}
} // namespace gpu
