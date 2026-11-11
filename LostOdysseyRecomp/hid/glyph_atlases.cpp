#include "glyph_atlases.h"
#include "hid.h"
#include "input_map.h"
// Header-only miniz; the runtime links its definitions from updater/stage.cpp.
#include <miniz.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <mutex>
#include <string_view>
#include "prompt_atlases_packed.inl"

namespace hid::glyphs
{
namespace
{
using packed::AtlasWidth;
using packed::AtlasHeight;
using packed::Cell;

// Inflates a packed grey + alpha image into RGBA.
std::shared_ptr<Atlas> Unpack(const uint8_t *data, size_t size, uint32_t width, uint32_t height)
{
    std::vector<uint8_t> greyAlpha(size_t(width) * height * 2);
    const size_t written = tinfl_decompress_mem_to_mem(greyAlpha.data(), greyAlpha.size(), data, size,
                                                       TINFL_FLAG_PARSE_ZLIB_HEADER);
    if (written != greyAlpha.size()) return {};
    auto atlas = std::make_shared<Atlas>();
    atlas->width = width;
    atlas->height = height;
    atlas->rgba.resize(size_t(width) * height * 4);
    for (size_t i = 0; i < size_t(width) * height; ++i)
    {
        const uint8_t grey = greyAlpha[i * 2];
        atlas->rgba[i * 4] = atlas->rgba[i * 4 + 1] = atlas->rgba[i * 4 + 2] = grey;
        atlas->rgba[i * 4 + 3] = greyAlpha[i * 2 + 1];
    }
    return atlas;
}

// SDL scancode (USB HID usage) to the key sheet's cap name; "" when it has none.
std::string_view KeyName(uint16_t scancode)
{
    static const std::map<uint16_t, std::string_view> names = [] {
        std::map<uint16_t, std::string_view> m;
        for (uint16_t i = 0; i < 26; ++i) m[uint16_t(4 + i)] = packed::KeyNames[24 + i];   // A-Z
        for (uint16_t i = 0; i < 9; ++i) m[uint16_t(30 + i)] = packed::KeyNames[51 + i];   // 1-9
        m[39] = "0";
        for (uint16_t i = 0; i < 12; ++i) m[uint16_t(58 + i)] = packed::KeyNames[i];       // F1-F12
        for (uint16_t i = 0; i < 12; ++i) m[uint16_t(104 + i)] = packed::KeyNames[12 + i]; // F13-F24
        const std::pair<uint16_t, std::string_view> others[] = {
            {40, "Return"}, {41, "Escape"}, {42, "Backspace"}, {43, "Tab"}, {44, "Space"}, {45, "Minus"},
            {46, "Equals"}, {47, "LBracket"}, {48, "RBracket"}, {49, "Backslash"}, {51, "Semicolon"}, {52, "Quote"},
            {53, "Backtick"}, {54, "Comma"}, {55, "Period"}, {56, "Slash"}, {57, "CapsLock"}, {70, "PrintScreen"},
            {71, "ScrollLock"}, {72, "Pause"}, {73, "Insert"}, {74, "Home"}, {75, "PageUp"}, {76, "Delete"},
            {77, "End"}, {78, "PageDown"}, {79, "Right"}, {80, "Left"}, {81, "Down"}, {82, "Up"}, {83, "NumLock"},
            {84, "NumpadSlash"}, {85, "NumpadStar"}, {86, "NumpadMinus"}, {87, "NumpadPlus"}, {88, "NumpadEnter"},
            {89, "Numpad1"}, {90, "Numpad2"}, {91, "Numpad3"}, {92, "Numpad4"}, {93, "Numpad5"}, {94, "Numpad6"},
            {95, "Numpad7"}, {96, "Numpad8"}, {97, "Numpad9"}, {98, "Numpad0"}, {224, "Control"}, {225, "Shift"},
            {226, "Alt"}, {228, "Control"}, {229, "Shift"}, {230, "Alt"}};
        for (const auto &[code, name] : others) m[code] = name;
        return m;
    }();
    const auto it = names.find(scancode);
    return it == names.end() ? std::string_view{} : it->second;
}

// The cap of a key; the blank cap (the last) for an unbound or unlisted key.
int CapIndex(uint16_t scancode)
{
    const auto name = KeyName(scancode);
    for (size_t i = 0; !name.empty() && i < std::size(packed::KeyNames); ++i)
        if (packed::KeyNames[i] == name) return int(i);
    return int(std::size(packed::KeyNames)) - 1;
}

// Copies cell (fromColumn, fromRow) of `source` into cell (column, row) of `atlas`.
void CopyCell(const Atlas &source, Atlas &atlas, uint32_t fromColumn, uint32_t fromRow, uint32_t column, uint32_t row)
{
    for (uint32_t y = 0; y < Cell; ++y)
        std::copy_n(&source.rgba[(size_t(fromRow * Cell + y) * source.width + fromColumn * Cell) * 4], Cell * 4,
                    &atlas.rgba[(size_t(row * Cell + y) * atlas.width + column * Cell) * 4]);
}

// Draws key cap `index` scaled into atlas[x, y, size x size] (area average,
// alpha over).
void DrawCap(const Atlas &sheet, Atlas &atlas, int index, int x, int y, int size)
{
    const uint32_t sx = uint32_t(index % packed::KeyColumns) * packed::KeyCap;
    const uint32_t sy = uint32_t(index / packed::KeyColumns) * packed::KeyCap;
    const double cap = packed::KeyCap;
    for (int py = 0; py < size; ++py)
    {
        const double y0 = sy + py * cap / size, y1 = sy + (py + 1) * cap / size;
        for (int px = 0; px < size; ++px)
        {
            const double x0 = sx + px * cap / size, x1 = sx + (px + 1) * cap / size;
            double sum[4]{}, area = 0;
            for (int yy = int(y0); yy < int(std::ceil(y1)); ++yy)
            {
                const double wy = std::min(y1, yy + 1.0) - std::max(y0, double(yy));
                for (int xx = int(x0); xx < int(std::ceil(x1)); ++xx)
                {
                    const double w = (std::min(x1, xx + 1.0) - std::max(x0, double(xx))) * wy;
                    const uint8_t *p = &sheet.rgba[(size_t(yy) * sheet.width + xx) * 4];
                    const double a = p[3] / 255.0 * w;
                    for (int c = 0; c < 3; ++c) sum[c] += p[c] * a;
                    sum[3] += a;
                    area += w;
                }
            }
            if (area <= 0 || sum[3] <= 0) continue;
            const int ox = x + px, oy = y + py;
            if (ox < 0 || oy < 0 || ox >= int(atlas.width) || oy >= int(atlas.height)) continue;
            uint8_t *d = &atlas.rgba[(size_t(oy) * atlas.width + ox) * 4];
            const double a = sum[3] / area, da = d[3] / 255.0, out = a + da * (1 - a);
            for (int c = 0; c < 3; ++c)
                d[c] = uint8_t(std::clamp((sum[c] / sum[3] * a + d[c] * da * (1 - a)) / std::max(out, 1e-6), 0.0, 255.0));
            d[3] = uint8_t(std::clamp(out * 255, 0.0, 255.0));
        }
    }
}

std::shared_ptr<Atlas> ComposeKeyboard(const Atlas &sheet, const input_map::KeyboardMap &keys)
{
    using namespace input_map;
    auto atlas = std::make_shared<Atlas>();
    atlas->width = AtlasWidth;
    atlas->height = AtlasHeight;
    atlas->rgba.assign(size_t(AtlasWidth) * AtlasHeight * 4, 0);
    auto cap = [&](int input) { return CapIndex(input >= 0 ? keys[size_t(input)] : NoKey); };
    // One key in a cell; four in a cross for a stick or the D-pad.
    auto key = [&](int column, int row, int input) {
        DrawCap(sheet, *atlas, cap(input), column * int(Cell) + 10, row * int(Cell) + 10, 124);
    };
    auto cross = [&](int column, int row, int up, int left, int down, int right) {
        const int x = column * int(Cell), y = row * int(Cell), s = 46;
        DrawCap(sheet, *atlas, cap(up), x + 49, y + 24, s);
        DrawCap(sheet, *atlas, cap(left), x + 3, y + 72, s);
        DrawCap(sheet, *atlas, cap(down), x + 49, y + 72, s);
        DrawCap(sheet, *atlas, cap(right), x + 95, y + 72, s);
    };
    // The game's 7x3 grid (hid/controller_atlas_glyphs.h).
    key(0, 0, LeftShoulder); key(1, 0, RightShoulder); key(2, 0, LeftTrigger); key(3, 0, RightTrigger);
    cross(4, 0, StickUp, StickLeft, StickDown, StickRight);
    cross(5, 0, -1, -1, -1, -1); // the right stick has no keys
    key(6, 0, LeftThumb);
    key(0, 1, RightThumb); key(1, 1, -1);
    cross(2, 1, DpadUp, DpadLeft, DpadDown, DpadRight);
    key(3, 1, Back); key(4, 1, Start); key(5, 1, DpadUp); key(6, 1, DpadDown);
    key(0, 2, A); key(1, 2, B); key(2, 2, X); key(3, 2, Y); key(4, 2, DpadLeft); key(5, 2, DpadRight);
    return atlas;
}

struct Cache
{
    std::mutex mutex;
    bool unpacked = false;
    std::shared_ptr<const Atlas> switchPositions, switchLabels, steamDeck, sheet, keyboard;
    input_map::KeyboardMap keyboardKeys{};
};
Cache &State()
{
    static Cache cache;
    return cache;
}
}

std::shared_ptr<const Atlas> FamilyAtlas(prompts::Family family)
{
    if (family == prompts::Family::Xbox || family == prompts::Family::PlayStation) return {};
    auto &cache = State();
    std::lock_guard lock(cache.mutex);
    if (!cache.unpacked)
    {
        cache.unpacked = true;
        const auto positions = Unpack(packed::Switch, sizeof(packed::Switch), AtlasWidth, AtlasHeight);
        if (positions)
        {
            // hid reads Nintendo pads by label: the cell of Xbox A (bottom)
            // gets the icon of the button printed A (right), and so on.
            auto labels = std::make_shared<Atlas>(*positions);
            CopyCell(*positions, *labels, 1, 2, 0, 2);
            CopyCell(*positions, *labels, 0, 2, 1, 2);
            CopyCell(*positions, *labels, 3, 2, 2, 2);
            CopyCell(*positions, *labels, 2, 2, 3, 2);
            cache.switchLabels = labels;
        }
        cache.switchPositions = positions;
        cache.steamDeck = Unpack(packed::SteamDeck, sizeof(packed::SteamDeck), AtlasWidth, AtlasHeight);
        cache.sheet = Unpack(packed::KeySheet, sizeof(packed::KeySheet), packed::KeySheetWidth, packed::KeySheetHeight);
    }
    if (family == prompts::Family::Switch) return PromptsByPosition() ? cache.switchPositions : cache.switchLabels;
    if (family == prompts::Family::SteamDeck) return cache.steamDeck;
    if (!cache.sheet) return {};
    const auto keys = KeyboardMapSnapshot();
    if (!cache.keyboard || keys != cache.keyboardKeys)
    {
        cache.keyboardKeys = keys;
        cache.keyboard = ComposeKeyboard(*cache.sheet, keys);
    }
    return cache.keyboard;
}
}
