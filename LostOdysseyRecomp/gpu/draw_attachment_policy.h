#pragma once
#include <cstdint>

namespace gpu::draw_attachment {
// Keep a color-only draw (including a masked/discarding one) on its original
// raster grid. With depth bound and no writable color components, the depth
// attachment alone defines that grid; an unrelated color mapping must not
// constrain its framebuffer. The caller keeps the original pixel shader and
// depth/stencil state, and performs explicit EDRAM alias clears separately.
constexpr bool DepthOnly(uint32_t colorWriteMask, bool hasDepth) {
    return (colorWriteMask & 0xFu) == 0 && hasDepth;
}
}
