#include "game_state.h"

#if !defined(__ANDROID__) && !defined(__SWITCH__) && !defined(__NX__)
#include <debug/map_info.h>
#include <debug/menu_animations.h>
#include <movie/movie_player.h>
#include <settings/menu.h>

#include <chrono>
#include <string_view>

namespace discord_rpc
{
namespace
{
constexpr uint32_t Engine = 0x83315FB4, World = 0x83318744;
constexpr uint32_t Title = 0x832644EC, DiscManager = 0x832640B0;
constexpr uint32_t StorageMenu = 0x832688E8;

bool Object(uint32_t p) { return p >= 0x100000 && p < 0x7BFF0000 && !(p & 3); }

uint32_t Word(const uint8_t* base, uint32_t p)
{
    return uint32_t(base[p]) << 24 | uint32_t(base[p + 1]) << 16 |
        uint32_t(base[p + 2]) << 8 | base[p + 3];
}

std::string Utf8(std::wstring_view text)
{
    // MapInfo copies the game's UTF-16 code units into wchar_t on every host.
    // Convert those units explicitly, including surrogate pairs on Linux/macOS.
    std::string out;
    out.reserve(text.size() * 3);
    for (size_t i = 0; i < text.size(); ++i)
    {
        uint32_t c = text[i];
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < text.size() &&
            text[i + 1] >= 0xDC00 && text[i + 1] <= 0xDFFF)
            c = 0x10000 + ((c - 0xD800) << 10) + (text[++i] - 0xDC00);
        else if ((c >= 0xD800 && c <= 0xDFFF) || c > 0x10FFFF)
            c = 0xFFFD;
        // Never send the retail font's private control/glyph codes to Discord.
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

bool FieldEvent(uint8_t* base)
{
    // Same local-player path and control flag as patches/encounter_defer.cpp.
    // Script DD clears this bit while a field event/dialogue owns the player;
    // title, camp, movies and battles are classified separately above it.
    const auto engine = Word(base, Engine);
    if (!Object(engine)) return false;
    const auto players = Word(base, engine + 0x2B8), count = Word(base, engine + 0x2BC);
    if (!Object(players) || !count || count > 4) return false;
    const auto player = Word(base, players);
    if (!Object(player)) return false;
    const auto controller = Word(base, player + 0x40);
    return Object(controller) && !(Word(base, controller + 0x5C8) & 0x08000000);
}
}

Snapshot game_state::Sample(uint8_t* base, std::string_view previousLocation)
{
    Snapshot snapshot;
    if (!base) return snapshot;
    // The native disc manager's current disc, already used by mod_hot_reload.
    const int disc = int(base[DiscManager + 8]) << 8 | base[DiscManager + 9];
    snapshot.disc = disc >= 1 && disc <= 4 ? disc : 0;

    Observed observed;
    const auto world = Word(base, World);
    if (Object(world))
    {
        observed.worldMode = Word(base, world + 472);
        observed.activeWorldMode = Word(base, world + 476);
    }
    observed.title = Word(base, Title) != 0;
    observed.movie = movie::GuestPlaying();
    // Native camp states 0/1 are inactive; include opening/closing and children.
    observed.menu = Word(base, debug_menu::menu_animations::CampMenu) > 1 ||
        Word(base, StorageMenu + 20) > 1 || settings::IsOpen();
    if (!observed.movie && !observed.menu && !observed.title &&
        observed.worldMode != 3 && observed.activeWorldMode != 3)
        observed.fieldEvent = FieldEvent(base);
    snapshot.mode = Classify(observed);
    // The live, localized retail map name is shared with the runtime's map log.
    // Never send package names, player names, save filenames or debug labels.
    if (snapshot.mode != Mode::Title)
    {
        debug_menu::MapInfo map;
        if (!debug_menu::TryGetMapInfo(map)) snapshot.location = previousLocation;
        else if (map.available) snapshot.location = Utf8(map.name);
    }
    return snapshot;
}

void Tick(uint8_t* base)
{
    if (!Enabled()) return;
    using Clock = std::chrono::steady_clock;
    static Clock::time_point next{};
    static std::string location;
    const auto now = Clock::now();
    if (now < next) return;
    next = now + std::chrono::seconds(1);
    auto snapshot = game_state::Sample(base, location);
    location = snapshot.location;
    Publish(snapshot);
}
}
#endif
