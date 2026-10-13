#pragma once
#include <cstdint>
#include <vector>

// CPU renderer for the "Achievement unlocked" toast: a dark pill that grows out of a
// ring-of-light circle whose centre flips between an orb and the achievement icon, then
// collapses again. No GPU, game or SDL dependency; the caller uploads Frame::pixels as an
// overlay every presented frame. All shapes are our own drawing.
namespace ui::achievement_toast {
struct Content {
    // Cache key together with the scale: the caller bumps it for every new toast and whenever it
    // re-renders the text/icon (e.g. at a new scale). Buffer addresses are never used as a key.
    uint64_t revision = 0;
    // Straight-alpha RGBA8 pixels, byte order R,G,B,A (uint32 = R | G<<8 | B<<16 | A<<24), prepared by the caller.
    const uint32_t* icon = nullptr; uint32_t iconWidth = 0, iconHeight = 0;       // achievement icon, 64x64
    const uint32_t* title = nullptr; uint32_t titleWidth = 0, titleHeight = 0;    // "Achievement unlocked" text, white on transparent
    const uint32_t* detail = nullptr; uint32_t detailWidth = 0, detailHeight = 0; // "10G - Defeated Cave Worm", white on transparent
};

struct Frame {
    std::vector<uint32_t> pixels; // straight-alpha RGBA8 as above, width*height; empty when !visible
    uint32_t width = 0, height = 0;
    // Where the frame's top-left goes relative to the toast anchor (the bottom-centre point of the
    // toast's resting box); the caller adds the anchor position.
    int offsetX = 0, offsetY = 0;
    bool visible = false; // false before start and after the end
};

// Total seconds from start until the toast is gone.
double Duration();

// t = seconds since the toast started (wall clock); scale = output pixels per 720p pixel
// (1.0 at 720p, 1.5 at 1080p, 3.0 at 4K). The text images are drawn by the caller at the same
// scale; the renderer only places/clips them.
// The frame size and offsets depend only on (content sizes, scale), not on t, so a texture can
// be reused for the whole toast. Static layers are cached per thread, keyed by
// (revision, scale) (the text image sizes are also compared as a safety net); the text images
// themselves are read on every call.
Frame Render(const Content& content, double t, float scale);

// The chime, mono float samples at sampleRate, peak about 0.5.
std::vector<float> SynthesizeChime(uint32_t sampleRate = 48000);
}
