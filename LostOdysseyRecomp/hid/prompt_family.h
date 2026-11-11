#pragma once

#include <cstddef>
#include <cstdint>

namespace hid::prompts
{
// The button icons shown: the game's own (Xbox), its PlayStation repaint, or
// the built-in Switch, Steam Deck or keyboard atlases (hid/glyph_atlases.h).
// The numbers are the button_prompts setting minus one (0 is Auto).
enum class Family : uint8_t { Xbox, PlayStation, Switch, SteamDeck, Keyboard };
inline constexpr int FamilyCount = 5;
inline const char *FamilyName(Family family)
{
    constexpr const char *names[FamilyCount] = {"Xbox", "PlayStation", "Switch", "Steam Deck", "keyboard"};
    return names[std::size_t(family) < FamilyCount ? std::size_t(family) : 0];
}
}
