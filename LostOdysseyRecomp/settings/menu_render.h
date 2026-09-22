#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace settings
{
namespace menu_assets { struct Assets; }
// Visible list slots: rows start at y=150 with height 43 and clip at y=640,
// so (640 - 150) / 43 == 11 rows fit without scrolling.
inline constexpr int kMenuVisibleRows = 11;
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
    // Hidden rows keep their logical index (input dispatch stays stable) but
    // are skipped by navigation, drawing and mouse hit-testing.
    bool hidden = false;
    bool operator==(const MenuRow &) const = default;
};
struct MenuSnapshot
{
    int tab = 0, row = 0;
    // First visible slot when rows overflow the list area. Publish keeps the
    // focused row inside [scroll, scroll + kMenuVisibleRows).
    int scroll = 0;
    uint32_t language = 0;
    std::vector<MenuRow> rows;
    std::wstring help;
    // Graphics-tab DLSS status. Empty on other tabs, so those pages keep one help line.
    std::wstring notice;
    std::wstring dialogTitle, dialogMessage;
    std::vector<std::wstring> dialogChoices;
    int dialogSelection = 0;
    uint64_t revision = 0;
    std::shared_ptr<const menu_assets::Assets> assets;
};
// Render glyphs at output resolution, fitting the existing 1280x720 logical layout.
bool RasterizeMenu(const MenuSnapshot &snapshot, uint32_t width, uint32_t height,
                   std::vector<uint32_t> &pixels);
}
