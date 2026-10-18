#include <stdafx.h>

#include "gpu/rtl_text.h"
#include "cpu/ppc_context.h"
#include "kernel/heap.h"
#include "kernel/memory.h"

#include <algorithm>
#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

// Right-to-left text from a language pack. The pack stores each line already shaped and
// reversed (presentation forms, U+FB50..U+FEFF), so the game draws it left to right from
// the text's x. Here a left-anchored Arabic text is moved so it ends on the right:
//   - every line of a block ends where its widest line ends;
//   - three or more texts that share a left edge and scale in nearby rows (a list) end where
//     the widest of them ends;
//   - styled texts on a screen whose box edge is known (kEdges) end at that edge, and so do text
//     blocks in such a box (kBlockEdges); a text wider than that room keeps its start;
//   - a text the game types in letter by letter appears whole (the reveal runs left to right).
// The previous frame tells which texts share a column. The game's own per-line x positions
// (text object +280/+296/+300, used for centring; each replaces x) carry the shift.
// Every hook here passes straight to the game's code unless the selected language pack is
// right to left (language.ini direction=rtl).

extern "C" PPC_FUNC(__imp__sub_82314CD8);
extern "C" PPC_FUNC(__imp__sub_8230B6D8);
extern "C" PPC_FUNC(__imp__sub_8230C118);
extern "C" PPC_FUNC(__imp__sub_82319F70);
extern "C" PPC_FUNC(__imp__sub_828088E0);
// sub_8230BA20 (style lookup) and sub_8232DB40 (icon lookup) come from ppc_recomp_shared.h.

namespace
{
constexpr uint32_t kIconTable = 0x83265300;  // icon/style table the draw code passes to its lookups
constexpr uint32_t kUnitScale = 0x82007784;  // the 1.0 the draw code uses when +292 is set
constexpr uint32_t kMaxLines = 64;

bool ValidRange(uint32_t address, uint32_t size)
{
    return g_memory.base && address >= 0x10000 && uint64_t(address) + size <= 0x100000000ull;
}
uint32_t LoadWord(uint32_t address)
{
    return ValidRange(address, 4) ? __builtin_bswap32(*reinterpret_cast<const uint32_t*>(g_memory.base + address)) : 0;
}
int32_t LoadInt(uint32_t address) { return int32_t(LoadWord(address)); }
uint16_t LoadHalf(uint32_t address)
{
    return ValidRange(address, 2) ? __builtin_bswap16(*reinterpret_cast<const uint16_t*>(g_memory.base + address)) : 0;
}
void StoreWord(uint32_t address, uint32_t value)
{
    if (ValidRange(address, 4))
        *reinterpret_cast<uint32_t*>(g_memory.base + address) = __builtin_bswap32(value);
}
float LoadFloat(uint32_t address) { return std::bit_cast<float>(LoadWord(address)); }
void StoreFloat(uint32_t address, float value) { StoreWord(address, std::bit_cast<uint32_t>(value)); }

std::atomic<bool> gEnabled{false};
bool Enabled() { return gEnabled.load(std::memory_order_relaxed); }

bool IsArabicForm(uint16_t c) { return (c >= 0xFB50 && c <= 0xFDFF) || (c >= 0xFE70 && c <= 0xFEFC); }

// Whether a guest TArray<char16_t> (data, count) holds an Arabic form, read in place.
bool GuestHasArabic(uint32_t array)
{
    const uint32_t data = LoadWord(array), count = LoadWord(array + 4);
    if (!data || count == 0 || count > 4096 || !ValidRange(data, count * 2))
        return false;
    for (uint32_t i = 0; i < count; ++i)
        if (IsArabicForm(LoadHalf(data + i * 2)))
            return true;
    return false;
}

// A zero-terminated guest string of at most limit characters.
std::u16string GuestText(uint32_t at, uint32_t limit)
{
    std::u16string text;
    for (uint32_t i = 0; i < limit && ValidRange(at + i * 2, 2); ++i)
    {
        const uint16_t c = LoadHalf(at + i * 2);
        if (!c) break;
        text.push_back(char16_t(c));
    }
    return text;
}
bool HasArabic(const std::u16string &text) { return std::any_of(text.begin(), text.end(), [](char16_t c) { return IsArabicForm(c); }); }

// A left-anchored Arabic text drawn this frame.
struct Draw
{
    uint32_t object = 0;
    float anchor = 0, top = 0, width = 0, scale = 0;
};

struct Layout
{
    std::vector<float> lines;  // width of each line, in screen units
};

std::mutex gMutex;
std::vector<Draw> gCurrent, gPrevious;
std::unordered_set<uint32_t> gSeen;
uint32_t gOffsets = 0;  // guest float[kMaxLines]

// A column: last frame's texts at this x and scale (rows: their y and right end), linked to this
// text through rows at most kRowGap apart. Three or more make a list, which ends where its widest
// text ends; fewer (a label pair) keep their own end.
struct Row
{
    float y = 0, right = 0;
};
constexpr float kRowGap = 60;

float ColumnRight(float x, float y, float width, const std::vector<Row> &rows)
{
    float right = x + width, top = y, bottom = y;
    size_t members = 1;
    std::vector<bool> taken(rows.size());
    for (bool grew = true; grew;)
    {
        grew = false;
        for (size_t i = 0; i < rows.size(); ++i)
            if (!taken[i] && rows[i].y >= top - kRowGap && rows[i].y <= bottom + kRowGap)
            {
                taken[i] = true;
                top = std::min(top, rows[i].y);
                bottom = std::max(bottom, rows[i].y);
                right = std::max(right, rows[i].right);
                ++members;
                grew = true;
            }
    }
    return members >= 3 ? right : x + width;
}

// Text blocks in a box the game does not keep the edge of: a block starting at x (canvas 1280x720)
// with its top between top and bottom ends at right.
struct BlockEdge
{
    float x, top, bottom, right;
};
constexpr BlockEdge kBlockEdges[] = {
    {227, 536, 546, 1050}, // tutorial pages (camp menu, Help): the box under the picture, clear of its A button
};


// Line widths the way the draw code advances a left-anchored text (sub_82314CD8).
Layout Measure(PPCContext &ctx, uint8_t *base, uint32_t object, float scale)
{
    Layout layout;
    layout.lines.push_back(0);
    const uint32_t records = LoadWord(object + 196), count = LoadWord(object + 200);
    const uint32_t font = LoadWord(object + 168);
    const float kerning = float(LoadInt(font + 108)), spacing = LoadFloat(object + 260);
    const float guestScale = LoadFloat(object + 180) * float(ctx.f1.f64);
    if (!ValidRange(records, count * 20))
        return layout;
    for (uint32_t i = 0; i < count; ++i)
    {
        const uint32_t record = records + i * 20;
        const int32_t code = LoadInt(record), value = LoadInt(record + 4), width = LoadInt(record + 8);
        float advance = 0;
        if (code >= 0 && width >= 0)
            advance = std::trunc((float(width) + kerning + spacing) * scale);
        else if (code == -1 && value == 480)
        {
            layout.lines.push_back(0);
            continue;
        }
        else if (code == -1 && width < -1)
            advance = std::trunc((kerning + spacing - float(width)) * guestScale);
        else if (code == -2)
        {
            PPCContext call = ctx;
            call.r3.u64 = kIconTable;
            call.r4.u64 = uint32_t(value);
            sub_8232DB40(call, base);
            if (const uint32_t icon = call.r3.u32)
                advance = std::trunc(float(LoadInt(icon + 24)) + kerning + spacing);
        }
        layout.lines.back() += advance;
    }
    return layout;
}

// Typing reveal and fade (+216 above zero and +272 = 1): record i at or past +312 is drawn at the opacity
// +248 - step x i (step +228 per record index when +276 is set and +220 = 1, else a fixed step per record),
// and drawing stops at the first record at zero. A line stored in display order would appear from its
// left, so every record of an Arabic text takes the first one's opacity (indexed order, step 0): the text
// appears whole and still fades out with +248.
constexpr uint32_t kZero = 0x82000E50; // the 0 the draw code compares +216 with

bool Typing(uint32_t object) { return LoadFloat(object + 216) > LoadFloat(kZero) && LoadWord(object + 272) == 1; }
} // namespace

// Draws one LO text object: r3 = the object, f1/f2 = the canvas scale.
PPC_FUNC(sub_82314CD8)
{
    const uint32_t object = ctx.r3.u32;
    const bool leftAnchored = LoadWord(object + 224) != 1 && LoadWord(object + 280) == 0;
    if (!Enabled() || !leftAnchored || !GuestHasArabic(object + 236))
    {
        __imp__sub_82314CD8(ctx, base);
        return;
    }
    const float scale = LoadWord(object + 292) ? LoadFloat(kUnitScale) : LoadFloat(object + 180) * float(ctx.f1.f64);
    const float x = float(int32_t(LoadFloat(object + 4))), y = float(int32_t(LoadFloat(object + 8)));

    std::unique_lock lock(gMutex);
    if (!gSeen.insert(object).second)
    {
        gPrevious = std::move(gCurrent);
        gCurrent.clear();
        gSeen.clear();
        gSeen.insert(object);
    }
    Layout layout = Measure(ctx, base, object, scale);
    const float natural = *std::max_element(layout.lines.begin(), layout.lines.end());

    float right = x + natural;
    if (natural > 0)
    {
        std::vector<Row> rows;
        for (const Draw &other : gPrevious)
            if (other.object != object && std::abs(other.anchor - x) <= 2 && std::abs(other.scale - scale) < 0.01f)
                rows.push_back({other.top, other.anchor + other.width});
        right = ColumnRight(x, y, natural, rows);
        for (const BlockEdge &edge : kBlockEdges)
            if (std::abs(edge.x - x) <= 1 && y >= edge.top && y <= edge.bottom)
                right = std::max(right, edge.right);
    }

    gCurrent.push_back({object, x, y, natural, scale});

    if (natural <= 0 || layout.lines.size() > kMaxLines)
    {
        lock.unlock();
        __imp__sub_82314CD8(ctx, base);
        return;
    }
    if (!gOffsets)
        gOffsets = g_memory.MapVirtual(g_userHeap.Alloc(kMaxLines * 4));

    for (size_t i = 0; i < layout.lines.size(); ++i)
        StoreFloat(gOffsets + uint32_t(i) * 4, right - layout.lines[i]); // the draw code uses it in place of x
    const uint32_t savedCentre = LoadWord(object + 280), savedOffsets = LoadWord(object + 296), savedCount = LoadWord(object + 300);
    StoreWord(object + 280, 1);
    StoreWord(object + 296, gOffsets);
    StoreWord(object + 300, uint32_t(layout.lines.size()));
    lock.unlock();
    const bool typing = Typing(object);
    const uint32_t savedIndexed = LoadWord(object + 276), savedOrder = LoadWord(object + 220), savedStep = LoadWord(object + 228);
    if (typing)
    {
        StoreWord(object + 276, 1);
        StoreWord(object + 220, 1);
        StoreFloat(object + 228, 0);
    }
    __imp__sub_82314CD8(ctx, base);
    if (typing)
    {
        StoreWord(object + 276, savedIndexed);
        StoreWord(object + 220, savedOrder);
        StoreWord(object + 228, savedStep);
    }
    StoreWord(object + 280, savedCentre);
    StoreWord(object + 296, savedOffsets);
    StoreWord(object + 300, savedCount);
}

// Styled text: r5 = style id, whose record (sub_8230BA20) holds a format string at +8 (when +12 is
// set), the x margin at +20 and the horizontal alignment at +36 (1 left, 2 right, else centred);
// r8 = the element it sits in, whose box size sub_8232A948 gives. Right alignment puts the text's end
// at margin + box width, so an Arabic text flips to it only when it starts at its box's left edge
// (a button, a list heading); a label placed inside a wider panel keeps its place.
constexpr int32_t kMaxFlipMargin = 12;
constexpr float kSlashWidth = 16; // BigNum '/' (12) and a little air, before the style's scale

PPC_FUNC(sub_8230B6D8)
{
    if (!Enabled())
    {
        __imp__sub_8230B6D8(ctx, base);
        return;
    }
    PPCContext call = ctx;
    call.r3.u64 = kIconTable;
    call.r4.u64 = ctx.r5.u32;
    sub_8230BA20(call, base);
    const uint32_t style = call.r3.u32;
    const std::u16string format = style && LoadWord(style + 12) ? GuestText(LoadWord(style + 8), 256) : std::u16string();
    const bool arabic = HasArabic(format);
    float boxWidth = 0;
    if (arabic && ctx.r8.u32)
    {
        static uint32_t scratch = 0;
        if (!scratch)
            scratch = g_memory.MapVirtual(g_userHeap.Alloc(16));
        PPCContext size = ctx;
        size.r3.u64 = ctx.r8.u32;
        size.r4.u64 = scratch;
        size.r5.u64 = scratch + 4;
        size.f1.f64 = size.f2.f64 = LoadFloat(kUnitScale);
        StoreFloat(scratch, 0);
        sub_8232A948(size, base);
        boxWidth = LoadFloat(scratch);
    }
    const int32_t margin = style ? LoadInt(style + 20) : 0;
    const bool flip = arabic && LoadWord(style + 36) == 1 && boxWidth > 0 && margin >= 0 && margin <= kMaxFlipMargin;
    // "current / maximum": three styles on one row, the maximum right-aligned in a field wide
    // enough for four digits, so a short maximum stands far from its "/". It is drawn from just
    // after the "/" instead (right-to-left packs only, so other languages keep the game's layout).
    // A "/" style is remembered by element and row; the maximum may be drawn first, so the pairing
    // holds from the next frame.
    int32_t closeUp = -1;
    if (style && LoadWord(style + 12))
    {
        static std::mutex slashMutex;
        // (element, row, "/" end); rows of one pair may differ by a few pixels.
        static std::vector<std::tuple<uint32_t, int32_t, int32_t>> slashes;
        const uint32_t element = ctx.r8.u32;
        const int32_t row = LoadInt(style + 24);
        std::lock_guard lock(slashMutex);
        if (format == u"/" && LoadWord(style + 36) == 1)
        {
            const int32_t end = margin + int32_t(std::lround(kSlashWidth * LoadFloat(style + 28)));
            std::erase_if(slashes, [&](const auto &s) { return std::get<0>(s) == element && std::get<1>(s) == row; });
            if (slashes.size() < 256)
                slashes.emplace_back(element, row, end);
        }
        else if (format == u"%d" && LoadWord(style + 36) == 2)
            for (const auto &[e, r, end] : slashes)
                if (e == element && std::abs(r - row) <= 6 && margin > end && margin - end <= 100)
                    closeUp = end;
    }
    const uint32_t savedAlign = style ? LoadWord(style + 36) : 0;
    if (flip)
        StoreWord(style + 36, 2);
    if (closeUp >= 0)
    {
        StoreWord(style + 36, 1);
        StoreWord(style + 20, uint32_t(closeUp));
    }
    __imp__sub_8230B6D8(ctx, base);
    if (flip || closeUp >= 0)
        StoreWord(style + 36, savedAlign);
    if (closeUp >= 0)
        StoreWord(style + 20, uint32_t(margin));
}

// Every styled text (sub_8230B6D8, sub_82339950, sub_82895F30, sub_828962A0) ends here: r3 = the
// text renderer holding the string, f1/f2 = scale, f3/f4 = x/y after the style's alignment, r8 = the
// string. Arabic texts that start at the same x (a menu's commands, a column of labels) end where
// the widest of them ends; widths come from the game's own measure (sub_8230BAE0) and the column
// from the previous frame.
namespace
{
struct StyledDraw
{
    float x = 0, y = 0, width = 0, scale = 0;
    std::u16string text;
};
std::vector<StyledDraw> gStyledCurrent, gStyledPrevious;
uint32_t gMeasure = 0;

// Box edges the game does not keep: a left-anchored text starting at x (canvas 1280x720) with this
// scale, between top and bottom, ends at right. Measured from the menus' frames.
struct Edge
{
    float x, scale, top, bottom, right;
};
constexpr Edge kEdges[] = {
    {81, 1.00f, 140, 500, 276},   // command buttons (main menu, Items submenu)
    {415, 0.90f, 140, 620, 655},  // item names, before the quantity
    {127, 1.00f, 640, 670, 1196}, // help bar
    {374, 1.00f, 35, 45, 480},    // header: gold caption
    {506, 1.00f, 35, 45, 591},    // header: play time caption
    {631, 1.00f, 35, 60, 850},    // header: location caption and name
    {320, 1.00f, 110, 120, 820},  // "in the battle party" caption over the party bar
};
// Labels in a slot sized for two Latin letters (LV): drawn smaller to stay clear of the number.
const std::pair<std::u16string_view, float> kTightLabels[] = {{u"\uFEEF\uFEEE\uFE98\uFEB4\uFEE3", 0.78f}};
// Labels that move to the other side of their box (canvas x of their new left edge).
const std::pair<std::u16string_view, float> kPlaced[] = {
    {u"\uFE93\uFEAA\uFECB\uFE8E\uFEB4\uFEE3", 1215}, // help caption: right of the help bar (bar ends at 1206)
};
} // namespace

PPC_FUNC(sub_8230C118)
{
    const std::u16string text = Enabled() ? GuestText(ctx.r8.u32, 256) : std::u16string();
    if (!HasArabic(text))
    {
        __imp__sub_8230C118(ctx, base);
        return;
    }
    for (const auto &[label, factor] : kTightLabels)
        if (text == label)
        {
            ctx.f1.f64 *= factor;
            ctx.f2.f64 *= factor;
        }
    if (!gMeasure)
        gMeasure = g_memory.MapVirtual(g_userHeap.Alloc(16));
    // Letter spacing (renderer +2076, set for the pause title) would split joined letters: none for
    // Arabic, in the measure and the draw, then back for the texts after it.
    const uint32_t spacingAddress = ctx.r3.u32 + 2076;
    const uint32_t spacing = LoadWord(spacingAddress);
    StoreWord(spacingAddress, 0);
    // The measure reads the renderer's own copy of the string (+4): give it the one being drawn.
    PPCContext store = ctx;
    store.r4.u64 = ctx.r8.u32;
    sub_8230BAB8(store, base);
    PPCContext measure = ctx;
    measure.r4.u64 = gMeasure;
    measure.r5.u64 = gMeasure + 4;
    StoreWord(gMeasure, 0);
    sub_8230BAE0(measure, base);
    const float width = float(LoadInt(gMeasure));
    const float x = float(ctx.f3.f64), y = float(ctx.f4.f64), scale = float(ctx.f1.f64);

    std::unique_lock lock(gMutex);
    // A text drawn again at the same place starts a new frame.
    if (std::any_of(gStyledCurrent.begin(), gStyledCurrent.end(),
                    [&](const StyledDraw &d) { return d.x == x && d.y == y && d.text == text; }) ||
        gStyledCurrent.size() >= 512)
    {
        gStyledPrevious = std::move(gStyledCurrent);
        gStyledCurrent.clear();
    }
    gStyledCurrent.push_back({x, y, width, scale, text});
    std::vector<Row> rows;
    for (const StyledDraw &other : gStyledPrevious)
        if (std::abs(other.x - x) <= 1 && std::abs(other.scale - scale) < 0.01f && other.y != y)
            rows.push_back({other.y, other.x + other.width});
    float right = ColumnRight(x, y, width, rows);
    // A screen whose box edge is known: the text (and its list) ends there instead, unless it is
    // wider than the room before that edge (the same caption on another screen with a longer
    // text): then it keeps its start and runs past the edge rather than over what is on its left.
    for (const Edge &edge : kEdges)
        if (std::abs(edge.x - x) <= 1 && std::abs(edge.scale - scale) < 0.01f && y >= edge.top && y <= edge.bottom)
            right = std::max(edge.right, right);
    lock.unlock();
    if (width > 0) // 0 until the renderer has its font (C118 sets it on first use)
        ctx.f3.f64 = right - width;
    for (const auto &[label, left] : kPlaced)
        if (text == label)
            ctx.f3.f64 = left;
    __imp__sub_8230C118(ctx, base);
    StoreWord(spacingAddress, spacing);
}

// The other text loop (title screen, sub_82352A88 and about twenty more): r3 = canvas, f1/f2 = x/y,
// f3/f4 = scale, r6 = the string, r7 = the font; returns the advance in r3. It steps by each
// glyph's width alone, without the font's kerning, and the pack's Arabic glyphs are padded by
// that kerning so the other loops join them: here they would stand apart. Arabic text is drawn
// one character at a time, stepping by width + kerning as those loops do.
namespace
{
uint32_t gCharacter = 0; // guest char16_t[2]
}

PPC_FUNC(sub_82319F70)
{
    const uint32_t font = ctx.r7.u32;
    const int32_t kerning = Enabled() && font ? LoadInt(font + 108) : 0;
    const std::u16string text = kerning < 0 && kerning >= -32 ? GuestText(ctx.r6.u32, 1024) : std::u16string();
    if (!HasArabic(text))
    {
        __imp__sub_82319F70(ctx, base);
        return;
    }
    if (!gCharacter)
        gCharacter = g_memory.MapVirtual(g_userHeap.Alloc(4));
    const double left = ctx.f1.f64;
    double x = left;
    for (const char16_t c : text)
    {
        StoreWord(gCharacter, uint32_t(c) << 16);
        PPCContext call = ctx;
        call.r6.u64 = gCharacter;
        call.f1.f64 = x;
        __imp__sub_82319F70(call, base);
        if (const int32_t advance = call.r3.s32; advance > 0)
            x += advance + std::trunc(kerning * ctx.f3.f64);
    }
    ctx.r3.u64 = uint64_t(int64_t(std::lround(x - left)));
}
// A per-character text element (the Thousand Years of Dreams pages, sub_827FBC28): r3 = the element.
// The game lays its characters out from Postion (+16/+20) to the right, wrapping at Width (+104),
// and reveals them left to right (delays at +152). A language pack stores each Arabic piece as lines
// joined by '\n'; here each line ends at the box's right edge, lines stack one under another, and the
// reveal runs from the top line's right end. Elements in the anchored mode (+132) are left as they are.
PPC_FUNC(sub_828088E0)
{
    const uint32_t element = ctx.r3.u32;
    __imp__sub_828088E0(ctx, base);
    if (!Enabled())
        return;
    const uint32_t font = LoadWord(element), characters = LoadWord(element + 120), count = LoadWord(element + 124);
    const uint32_t sprites = LoadWord(element + 140), spriteCount = LoadWord(element + 144);
    const uint32_t delays = LoadWord(element + 152), delayCount = LoadWord(element + 156);
    if (!font || !characters || count < 2 || count > 4096 || spriteCount + 1 != count || (LoadWord(element + 132) >> 24) != 0 ||
        !ValidRange(characters, count * 2) || !ValidRange(sprites, spriteCount * 4))
        return;
    std::u16string text;
    for (uint32_t i = 0; i + 1 < count; ++i)
        text.push_back(char16_t(LoadHalf(characters + i * 2)));
    if (!HasArabic(text))
        return;
    constexpr float kHalf = 0.5f; // the layout's centre factor (0x8201F9F0)
    const float widthScale = LoadFloat(font + 112), heightScale = LoadFloat(font + 116);
    const float kerning = float(LoadInt(font + 108)) * widthScale;
    const float right = LoadFloat(element + 16) + LoadFloat(element + 104), top = LoadFloat(element + 20);
    // Lines: [begin, end) character ranges, their widths and the line step.
    std::vector<std::pair<uint32_t, uint32_t>> lines{{0, 0}};
    std::vector<float> widths{0};
    float step = 0;
    for (uint32_t i = 0; i < spriteCount; ++i)
    {
        const uint32_t sprite = LoadWord(sprites + i * 4);
        if (!ValidRange(sprite, 144))
            return;
        if (text[i] == u'\n')
        {
            lines.back().second = i;
            lines.push_back({i + 1, i + 1});
            widths.push_back(0);
            continue;
        }
        lines.back().second = i + 1;
        widths.back() += LoadFloat(sprite + 36) * widthScale + kerning;
        step = std::max(step, LoadFloat(sprite + 40) * heightScale);
    }
    const float pitch = LoadFloat(element + 12);
    uint32_t rank = 0;
    for (size_t line = 0; line < lines.size(); ++line)
    {
        float pen = right - widths[line];
        const float y = top + float(line) * step;
        std::vector<uint32_t> order;
        for (uint32_t i = lines[line].first; i < lines[line].second; ++i)
        {
            const uint32_t sprite = LoadWord(sprites + i * 4);
            const float width = LoadFloat(sprite + 36);
            StoreFloat(sprite, pen + width * kHalf);
            StoreFloat(sprite + 4, y + LoadFloat(sprite + 40) * kHalf);
            pen += width * widthScale + kerning;
            order.push_back(i);
        }
        // Right to left within the line.
        for (auto it = order.rbegin(); it != order.rend(); ++it, ++rank)
            if (*it < delayCount && ValidRange(delays + *it * 4, 4))
                StoreFloat(delays + *it * 4, -pitch * float(rank));
    }
    // The line breaks themselves: off screen.
    for (uint32_t i = 0; i < spriteCount; ++i)
        if (text[i] == u'\n')
        {
            const uint32_t sprite = LoadWord(sprites + i * 4);
            StoreFloat(sprite, -4096.0f);
            StoreFloat(sprite + 4, -4096.0f);
        }
}

namespace rtl_text
{
void SetEnabled(bool enabled) { gEnabled.store(enabled, std::memory_order_relaxed); }
}
