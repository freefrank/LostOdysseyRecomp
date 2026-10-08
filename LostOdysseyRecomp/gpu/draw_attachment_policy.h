#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace gpu::draw_attachment {
// Keep a color-only draw (including a masked/discarding one) on its original
// raster grid. With depth bound and no writable color components, the depth
// attachment alone defines that grid; an unrelated color mapping must not
// constrain its framebuffer. The caller keeps the original pixel shader and
// depth/stencil state, and performs explicit EDRAM alias clears separately.
constexpr bool DepthOnly(uint32_t colorWriteMask, bool hasDepth) {
    return (colorWriteMask & 0xFu) == 0 && hasDepth;
}
// A depth-only draw whose color target has the depth target's size may keep
// that color attachment bound (zero write mask) instead of switching to a
// depth-only framebuffer. Apple GPUs store and reload tiles at every encoder
// break, so this is ~15% faster on Metal; on Turnip's sysmem passes it was ~9%
// slower. LO_DEPTH_ONLY_KEEP_COLOR=0/1 overrides the platform default.
inline bool KeepColorForDepthOnly() {
    static const bool keep = [] {
        const char* value = std::getenv("LO_DEPTH_ONLY_KEEP_COLOR");
#if defined(__APPLE__)
        return !value || std::strcmp(value, "0") != 0;
#else
        return value && std::strcmp(value, "1") == 0;
#endif
    }();
    return keep;
}
}
