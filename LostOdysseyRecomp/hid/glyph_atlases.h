#pragma once
#include "prompt_family.h"
#include <cstdint>
#include <memory>
#include <vector>

// Button icon atlases for the families the game has no artwork for: Switch,
// Steam Deck and keyboard. They use the game's 7x3 grid of 36-pixel cells
// (hid/controller_atlas_glyphs.h), which Icon_Page_0 and the font's icon page
// share, at 4x (1024x512). The artwork is built in (prompt_atlases_packed.inl,
// Xelu's CC0 prompts; tools/prompt_atlases). The keyboard atlas is composed
// from its key caps and the keyboard layout in use.
namespace hid::glyphs
{
struct Atlas
{
    uint32_t width = 0, height = 0;
    std::vector<uint8_t> rgba; // straight RGBA8, rows without padding
};
// The family's atlas; null for Xbox and PlayStation, which keep the game's
// atlas and its PlayStation repaint. Switch atlases follow how hid reads the
// pad: by label (the button printed A is A) unless Steam Input keeps
// positions. A new pointer means new pixels (the keyboard layout or the
// Switch reading changed). Thread-safe.
std::shared_ptr<const Atlas> FamilyAtlas(prompts::Family family);
}
