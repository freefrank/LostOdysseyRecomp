#include <stdafx.h>
#include "companion.h"

#include <debug/cheats.h>
#include <debug/map_info.h>
#include <debug/menu_animations.h>
#include <debug/teleport.h>
#include <hid/android_touch.h>
#include <host_ui/host_ui.h>
#include <os/json.h>
#include <os/logger.h>
#include <settings/menu.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <mutex>
#include <span>
#include <string_view>

#if defined(__ANDROID__)
#include <jni.h>
#endif

namespace companion
{
namespace
{
namespace data = debug_menu::cheats::data;

// Same guest slots as discord/game_state.cpp (desktop only, so repeated here).
constexpr uint32_t Engine = 0x83315FB4, World = 0x83318744;
constexpr uint32_t Title = 0x832644EC, DiscManager = 0x832640B0;
constexpr uint32_t StorageMenu = 0x832688E8;
constexpr size_t MaxPois = 64;

std::mutex g_mutex;
std::string g_json = "{}";
std::string g_body; // last snapshot without its serial
uint64_t g_serial = 0;
// The party block is only readable in the field. Battles show the last field
// values, marked as such, instead of an empty panel.
debug_menu::cheats::Snapshot g_lastParty;
bool g_haveParty = false;

bool Object(uint32_t p) { return p >= 0x100000 && p < 0x7BFF0000 && !(p & 3); }

uint32_t Word(const uint8_t* base, uint32_t p)
{
    return uint32_t(base[p]) << 24 | uint32_t(base[p + 1]) << 16 |
        uint32_t(base[p + 2]) << 8 | base[p + 3];
}

std::string Utf8(std::wstring_view text)
{
    std::string out;
    out.reserve(text.size() * 3);
    for (size_t i = 0; i < text.size(); ++i)
    {
        uint32_t c = uint32_t(text[i]);
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < text.size() &&
            uint32_t(text[i + 1]) >= 0xDC00 && uint32_t(text[i + 1]) <= 0xDFFF)
            c = 0x10000 + ((c - 0xD800) << 10) + (uint32_t(text[++i]) - 0xDC00);
        else if ((c >= 0xD800 && c <= 0xDFFF) || c > 0x10FFFF)
            c = 0xFFFD;
        // The retail font's private control/glyph codes are not printable.
        if (c < 0x20 || (c >= 0xE000 && c <= 0xF8FF)) continue;
        if (c < 0x80) out += char(c);
        else if (c < 0x800)
        {
            out += char(0xC0 | c >> 6);
            out += char(0x80 | (c & 0x3F));
        }
        else if (c < 0x10000)
        {
            out += char(0xE0 | c >> 12);
            out += char(0x80 | ((c >> 6) & 0x3F));
            out += char(0x80 | (c & 0x3F));
        }
        else
        {
            out += char(0xF0 | c >> 18);
            out += char(0x80 | ((c >> 12) & 0x3F));
            out += char(0x80 | ((c >> 6) & 0x3F));
            out += char(0x80 | (c & 0x3F));
        }
    }
    return out;
}

// Catalog names carry Cheat Table emphasis such as "!!!Slot Seeds!!!".
std::string CatalogName(const wchar_t* name)
{
    std::wstring_view view = name ? name : L"";
    while (!view.empty() && view.front() == L'!') view.remove_prefix(1);
    while (!view.empty() && view.back() == L'!') view.remove_suffix(1);
    return Utf8(view);
}

nlohmann::json Lookup(std::span<const data::Entry> list, uint32_t value)
{
    if (!value || value == 0xFFFF) return nullptr;
    for (const auto& e : list)
        if (e.value == value) return {{"id", value}, {"name", CatalogName(e.name)}};
    return {{"id", value}, {"name", nullptr}};
}

bool FieldEvent(const uint8_t* base)
{
    // Same local-player control flag as discord/game_state.cpp: cleared while
    // a field event or dialogue owns the player.
    const auto engine = Word(base, Engine);
    if (!Object(engine)) return false;
    const auto players = Word(base, engine + 0x2B8), count = Word(base, engine + 0x2BC);
    if (!Object(players) || !count || count > 4) return false;
    const auto player = Word(base, players);
    if (!Object(player)) return false;
    const auto controller = Word(base, player + 0x40);
    return Object(controller) && !(Word(base, controller + 0x5C8) & 0x08000000);
}

const char* Mode(const uint8_t* base)
{
    uint32_t worldMode = 0, activeWorldMode = 0;
    const auto world = Word(base, World);
    if (Object(world))
    {
        worldMode = Word(base, world + 472);
        activeWorldMode = Word(base, world + 476);
    }
    if (Word(base, debug_menu::menu_animations::CampMenu) > 1 ||
        Word(base, StorageMenu + 20) > 1 || settings::IsOpen())
        return "menu";
    if (Word(base, Title) != 0) return "title";
    if (worldMode == 3 || activeWorldMode == 3) return "battle";
    if (FieldEvent(base)) return "event";
    return "field";
}

nlohmann::json Characters(const debug_menu::cheats::Snapshot& s)
{
    auto list = nlohmann::json::array();
    for (unsigned i = 0; i < s.characters.size(); ++i)
    {
        const auto& c = s.characters[i];
        if (!c.available) continue;
        auto accessories = nlohmann::json::array();
        for (unsigned j = 0; j < std::min<unsigned>(c.accessoryCount, 8); ++j)
        {
            auto entry = Lookup(data::Accessories, c.accessories[j]);
            if (!entry.is_null()) accessories.push_back(std::move(entry));
        }
        list.push_back({
            {"id", i},
            {"name", Utf8(data::Characters[i])},
            {"hp", std::lround(c.hp)}, {"maxHp", std::lround(c.maxHp)},
            {"mp", std::lround(c.mp)}, {"maxMp", std::lround(c.maxMp)},
            {"exp", std::clamp(c.exp, 0.0f, 100.0f)},
            {"backRow", c.row != 0},
            {"weapon", Lookup(data::Weapons, c.weapon)},
            {"ring", Lookup(data::Rings, c.ring)},
            {"accessories", std::move(accessories)},
            {"accessorySlots", c.accessoryCount},
            {"skills", c.learnedSkills},
        });
    }
    return list;
}

template <size_t N>
nlohmann::json Inventory(std::span<const data::Entry> list, const std::array<uint16_t, N>& counts)
{
    // Duplicate catalog names (two "Healing Herb" slots) are merged.
    auto out = nlohmann::json::array();
    std::vector<std::pair<std::string, unsigned>> merged;
    for (size_t i = 0; i < N && i < list.size(); ++i)
    {
        if (!counts[i]) continue;
        auto name = CatalogName(list[i].name);
        auto it = std::find_if(merged.begin(), merged.end(), [&](auto& e) { return e.first == name; });
        if (it == merged.end()) merged.emplace_back(std::move(name), counts[i]);
        else it->second += counts[i];
    }
    for (auto& [name, count] : merged) out.push_back({{"name", name}, {"count", count}});
    return out;
}

// Category names debug/map_poi.cpp gives its points of interest.
constexpr std::pair<std::wstring_view, const char*> PoiCategories[] = {
    {L"存档点", "save"},                 // save point
    {L"出口附近", "exit"},           // near an exit
    {L"机关互动点", "gimmick"}, // mechanism
    {L"拾取 / 触碰点", "pickup"}, // pickup / touch point
    {L"地图入口", "entrance"},       // map entrance
};

nlohmann::json MapState()
{
    const auto t = debug_menu::GetTeleportSnapshot();
    nlohmann::json map = {{"available", t.available}};
    if (!t.available) return map;
    map["x"] = t.current.x;
    map["y"] = t.current.y;
    map["z"] = t.current.z;
    map["revision"] = t.poiRevision;
    auto pois = nlohmann::json::array();
    for (const auto& p : t.pois)
    {
        if (pois.size() >= MaxPois) break;
        // 64-bit ids do not survive JavaScript numbers: send them as text. The label is
        // "<debug-menu category> <n>" (debug/teleport.cpp); send the category as a code the
        // panel translates and numbers itself.
        const char* category = "other";
        for (const auto& [text, code] : PoiCategories)
            if (p.label.rfind(text, 0) == 0) { category = code; break; }
        pois.push_back({{"id", std::to_string(p.id)}, {"cat", category},
            {"x", p.position.x}, {"y", p.position.y}, {"z", p.position.z}});
    }
    map["pois"] = std::move(pois);
    return map;
}
}

void Tick(uint8_t* base)
{
#if defined(__ANDROID__)
    constexpr bool Enabled = true;
#else
    // Only the Android Presentation reads the snapshot; desktop pays nothing.
    constexpr bool Enabled = false;
#endif
    if constexpr (!Enabled) return;
    if (!base) return;
    using Clock = std::chrono::steady_clock;
    static Clock::time_point next{};
    const auto now = Clock::now();
    if (now < next) return;
    next = now + std::chrono::milliseconds(200);

    nlohmann::json state;
    const char* mode = Mode(base);
    const int disc = int(base[DiscManager + 8]) << 8 | base[DiscManager + 9];
    state["mode"] = mode;
    state["disc"] = disc >= 1 && disc <= 4 ? disc : 0;
    state["paused"] = host_ui::IsGamePaused();

    if (std::string_view(mode) != "title")
    {
        debug_menu::MapInfo map;
        if (debug_menu::TryGetMapInfo(map) && map.available) state["location"] = Utf8(map.name);
    }

    const auto party = debug_menu::cheats::session.Get();
    if (party.available)
    {
        g_lastParty = party;
        g_haveParty = true;
    }
    state["partyLive"] = party.available;
    if (g_haveParty && std::string_view(mode) != "title")
    {
        const auto& s = g_lastParty;
        state["gold"] = s.gold;
        auto members = nlohmann::json::array();
        for (auto id : s.party)
            if (id < 9 && s.characters[id].available) members.push_back(id);
        state["party"] = std::move(members);
        state["characters"] = Characters(s);
        state["items"] = Inventory<std::size(data::Items)>(data::Items, s.items);
        state["materials"] = Inventory<std::size(data::Materials)>(data::Materials, s.materials);
    }
    state["map"] = MapState();

    std::string json;
    try
    {
        json = state.dump(-1, ' ', true, nlohmann::json::error_handler_t::replace);
    }
    catch (const std::exception& e)
    {
        LOG_WARNING("companion: snapshot not serialised: {}", e.what());
        return;
    }
    std::lock_guard lock(g_mutex);
    // The serial changes only with the content, so the panel redraws (and
    // replaces its touch targets) only when something actually changed.
    if (json == g_body) return;
    g_body = json;
    g_json = std::move(json);
    g_json.insert(1, "\"serial\":" + std::to_string(++g_serial) + ",");
}

std::string StateJson()
{
    std::lock_guard lock(g_mutex);
    return g_json;
}

bool HandleAction(const std::string& text)
{
    const auto action = nlohmann::json::parse(text, nullptr, false);
    if (!action.is_object() || !action.contains("type") || !action["type"].is_string()) return false;
    const auto type = action["type"].get<std::string>();
    if (type == "teleportPoi" && action.contains("id") && action["id"].is_string())
    {
        try
        {
            const uint64_t id = std::stoull(action["id"].get<std::string>());
            const bool queued = debug_menu::RequestPoiTeleport(id);
            LOG_INFO("companion: POI teleport {} {}", id, queued ? "queued" : "refused");
            return queued;
        }
        catch (...)
        {
            return false;
        }
    }
    if (type == "releaseButtons")
    {
        hid::android_touch::SetCompanion(0, 0, 0);
        return true;
    }
    return false;
}
}

#if defined(__ANDROID__)
extern "C" JNIEXPORT jstring JNICALL
Java_io_github_freefrank_lostodyssey_DualScreenCompanion_nativeState(JNIEnv* env, jclass)
{
    // ensure_ascii keeps the text plain ASCII, safe for NewStringUTF.
    return env->NewStringUTF(companion::StateJson().c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_freefrank_lostodyssey_DualScreenCompanion_nativeButtons(
    JNIEnv*, jclass, jint buttons, jint leftTrigger, jint rightTrigger)
{
    hid::android_touch::SetCompanion(buttons, leftTrigger, rightTrigger);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_github_freefrank_lostodyssey_DualScreenCompanion_nativeAction(JNIEnv* env, jclass, jstring json)
{
    if (!json) return JNI_FALSE;
    const char* chars = env->GetStringUTFChars(json, nullptr);
    if (!chars) return JNI_FALSE;
    std::string text(chars);
    env->ReleaseStringUTFChars(json, chars);
    return companion::HandleAction(text) ? JNI_TRUE : JNI_FALSE;
}
#endif
