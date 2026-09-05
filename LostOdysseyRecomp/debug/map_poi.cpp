#include <stdafx.h>
#include <cmath>
#include "map_poi.h"

namespace
{
bool Address(uint32_t p) { return p >= 0x100000 && p < 0x7BFF0000 && !(p & 3); }
std::wstring Name(uint8_t* base, uint32_t object, bool suffix = true)
{
    if (!Address(object)) return {};
    const uint32_t names = PPC_LOAD_U32(0x833690D0), count = PPC_LOAD_U32(0x833690D4);
    const uint32_t index = PPC_LOAD_U32(object + 0x2C), number = PPC_LOAD_U32(object + 0x30);
    if (!Address(names) || count > 1000000 || index >= count) return {};
    const uint32_t entry = PPC_LOAD_U32(names + index * 4);
    if (!Address(entry)) return {};
    std::wstring result;
    for (uint32_t i = 0; i < 128; ++i)
    {
        const auto c = PPC_LOAD_U16(entry + 16 + i * 2);
        if (!c)
        {
            if (suffix && number) result += L"_" + std::to_wstring(number - 1);
            return result;
        }
        result += wchar_t(c);
    }
    return {};
}
debug_menu::Position PositionAt(uint8_t* base, uint32_t actor)
{
    return {std::bit_cast<float>(PPC_LOAD_U32(actor + 0xF8)),
        std::bit_cast<float>(PPC_LOAD_U32(actor + 0xFC)),
        std::bit_cast<float>(PPC_LOAD_U32(actor + 0x100))};
}
bool Valid(debug_menu::Position p)
{
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) &&
        std::abs(p.x) < debug_menu::MaxTeleportCoordinate &&
        std::abs(p.y) < debug_menu::MaxTeleportCoordinate &&
        std::abs(p.z) < debug_menu::MaxTeleportCoordinate &&
        (p.x != 0 || p.y != 0 || p.z != 0);
}
}

std::vector<debug_menu::LiveMapPoi> debug_menu::ScanMapPois(uint8_t* base, uint32_t world)
{
    std::vector<LiveMapPoi> result;
    if (!Address(world)) return result;
    // Live UWorld.Levels and ULevel.Actors, including streamed script/lvd levels.
    const uint32_t levels = PPC_LOAD_U32(world + 0x44), levelCount = PPC_LOAD_U32(world + 0x48);
    if (!Address(levels) || !levelCount || levelCount > 128) return result;
    struct Anchor { Position position; bool spawn; };
    std::vector<Anchor> anchors;
    uint32_t visited = 0;
    for (uint32_t l = 0; l < levelCount; ++l)
    {
        const uint32_t level = PPC_LOAD_U32(levels + l * 4);
        if (!Address(level) || Name(base, PPC_LOAD_U32(level + 0x34), false) != L"Level") continue;
        const auto outer = PPC_LOAD_U32(level + 0x28);
        const auto package = Address(outer) ? Name(base, PPC_LOAD_U32(outer + 0x28)) : L"";
        const uint32_t actors = PPC_LOAD_U32(level + 0x3C), count = PPC_LOAD_U32(level + 0x40);
        if (!Address(actors) || count > 16384 || visited + count > 65536) continue;
        visited += count;
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t actor = PPC_LOAD_U32(actors + i * 4);
            if (!Address(actor) || PPC_LOAD_U32(actor + 0x28) != level ||
                (PPC_LOAD_U32(actor + 0x70) & 0x02000000) ||
                (PPC_LOAD_U32(actor + 0x74) & 0x00080000)) continue;
            const auto type = Name(base, PPC_LOAD_U32(actor + 0x34), false);
            const bool anchor = type == L"fdDummyPoint" || type == L"PlayerStart";
            std::wstring category;
            if (type == L"fgGimmickSavePoint") category = L"存档点";
            else if (type == L"fnMapJumpPoint") category = L"出口附近";
            else if (type == L"fgGimmickAttackPoint") category = L"机关互动点";
            else if (type == L"fgGimmickTouchPoint" || type == L"fiItem") category = L"拾取 / 触碰点";
            else if (type == L"PlayerStart" && package != L"Entry") category = L"地图入口";
            if (category.empty() && !anchor) continue;
            const auto position = PositionAt(base, actor);
            if (!Valid(position) || (type == L"PlayerStart" && package == L"Entry")) continue;
            if (anchor) anchors.push_back({position, type == L"PlayerStart"});
            if (category.empty()) continue;
            const auto name = Name(base, actor);
            if (name.empty() || result.size() >= 512) continue;
            result.push_back({actor, level, PPC_LOAD_U32(actor + 4), package + L"/" + name, category, position});
        }
    }
    for (auto& poi : result)
    {
        if (poi.category == L"地图入口") continue;
        const bool save = poi.category == L"存档点";
        const bool exit = poi.category == L"出口附近";
        const Anchor* best = nullptr;
        float bestDistance = exit ? 256.0f * 256.0f : 32.0f * 32.0f;
        if (save || exit)
            for (const auto& anchor : anchors)
            {
                if (anchor.spawn != exit || std::abs(anchor.position.z - poi.position.z) > 200) continue;
                const float dx = anchor.position.x - poi.position.x, dy = anchor.position.y - poi.position.y;
                const float distance = dx * dx + dy * dy;
                if (distance < bestDistance) { best = &anchor; bestDistance = distance; }
            }
        // Save markers are at the floor/effect origin. Prefer a real nearby
        // character anchor. Other interaction markers get modest clearance;
        // the normal SetLocation collision check and subsequent physics remain.
        if (best) poi.position = best->position;
        else poi.position.z += 100.0f;
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return std::tie(a.category, a.key) < std::tie(b.category, b.key);
    });
    return result;
}
