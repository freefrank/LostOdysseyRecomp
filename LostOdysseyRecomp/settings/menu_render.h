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
    bool operator==(const MenuRow &) const = default;
};
struct MenuSnapshot
{
    int tab = 0, row = 0;
    uint32_t language = 0;
    std::vector<MenuRow> rows;
    std::wstring help;
    uint64_t revision = 0;
};
// Render glyphs at output resolution, fitting the existing 1280x720 logical layout.
bool RasterizeMenu(const MenuSnapshot &snapshot, uint32_t width, uint32_t height,
                   std::vector<uint32_t> &pixels);
}
