#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace settings
{
struct MenuRow
{
    std::wstring name, value;
    bool enabled = true;
    // Ordered exactly like the left/right input cycle. The renderer uses the
    // list to recreate the original game's horizontal option cells.
    std::vector<std::wstring> choices;
    int selectedChoice = 0;
    // Non-negative values render the original Min/Max volume slider instead
    // of option cells.
    int sliderPercent = -1;
    // Confirmation-button choices use the original colored A/B key legend.
    bool controllerButtons = false;
    bool operator==(const MenuRow &) const = default;
};
struct MenuSnapshot
{
    int tab = 0, row = 0;
    uint32_t language = 0;
    std::vector<MenuRow> rows;
    std::wstring help;
    std::wstring dialogTitle, dialogMessage;
    std::vector<std::wstring> dialogChoices;
    int dialogSelection = 0;
    uint64_t revision = 0;
};
// Render glyphs at output resolution, fitting the existing 1280x720 logical layout.
bool RasterizeMenu(const MenuSnapshot &snapshot, uint32_t width, uint32_t height,
                   std::vector<uint32_t> &pixels);
}
