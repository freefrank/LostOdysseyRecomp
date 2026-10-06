#include <stdafx.h>
#include <os/logger.h>
#include <algorithm>
#include <atomic>
#include <map>
#include <optional>
#include <vector>
#include <gpu/pipeline_cache.h>
#include "battle_menu.h"
#include "map_info.h"

extern "C" PPC_FUNC(__imp__sub_82A0D648);
namespace {
std::mutex mutex;
std::map<std::wstring, uint32_t> definitions;
debug_menu::MapInfo snapshot;
std::chrono::steady_clock::time_point lastUpdate;
// Map IDs present in the world at the last update; a new one is a map load.
std::vector<uint32_t> presentMaps;
std::atomic<void (*)(uint32_t)> sceneListener{nullptr};
// Scene of the last loader request until it is on screen; the requested battle.
std::atomic<uint32_t> pendingScene{0}, battleScene{0};
// The snapshot's map as a scene tag, readable without the lock once a frame.
std::atomic<uint32_t> currentMapScene{0};
std::atomic<std::chrono::steady_clock::rep> mapSceneUpdated{0};
uint32_t MapScene(uint32_t id) { return gpu::pipeline_cache::SceneTag(gpu::pipeline_cache::kSceneMap, id); }
void NotifySceneLoad(uint32_t tag) {
    if (const auto listener = sceneListener.load()) listener(tag);
}

bool Address(uint32_t p) { return p >= 0x100000 && p < 0x7BFF0000 && !(p & 3); }
std::wstring Text(uint8_t* base, uint32_t p, uint32_t limit = 256) {
    if (p < 0x100000 || p >= 0x7BFF0000 || (p & 1)) return {};
    std::wstring result;
    for (uint32_t i = 0; i < limit; ++i) {
        const auto c = PPC_LOAD_U16(p + i * 2);
        if (!c) return result;
        result += wchar_t(c);
    }
    return {};
}
std::wstring Key(std::wstring value) {
    for (auto& c : value) if (c >= L'A' && c <= L'Z') c += L'a' - L'A';
    return value;
}
std::wstring ObjectName(uint8_t* base, uint32_t object) {
    if (!Address(object)) return {};
    const auto names = PPC_LOAD_U32(0x833690D0), count = PPC_LOAD_U32(0x833690D4);
    const auto index = PPC_LOAD_U32(object + 0x2C);
    if (!Address(names) || count > 1000000 || index >= count) return {};
    const auto entry = PPC_LOAD_U32(names + index * 4);
    return Address(entry) ? Key(Text(base, entry + 16)) : std::wstring{};
}
}

PPC_FUNC(sub_82A0D648) {
    // Native map-definition insertion: key r4, row r5, FString package at +4.
    const auto row = ctx.r5.u32;
    if (Address(row)) {
        const auto package = Key(Text(base, PPC_LOAD_U32(row + 4)));
        if (!package.empty()) {
            std::lock_guard lock(mutex);
            definitions[package] = ctx.r4.u32;
        }
    }
    __imp__sub_82A0D648(ctx, base);
}

void debug_menu::UpdateMapInfo(uint8_t* base) {
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard lock(mutex);
    if (now - lastUpdate < std::chrono::milliseconds(250)) return;
    lastUpdate = now;
    MapInfo current;
    std::vector<uint32_t> present;
    const auto world = PPC_LOAD_U32(0x83318744);
    if (Address(world)) {
        const auto levels = PPC_LOAD_U32(world + 0x44), count = PPC_LOAD_U32(world + 0x48);
        if (Address(levels) && count <= 256) {
            for (uint32_t i = 0; i < count; ++i) {
                auto object = PPC_LOAD_U32(levels + i * 4);
                // ULevel -> UWorld -> package. Names are matched to native definitions.
                for (unsigned depth = 0; depth < 4 && Address(object); ++depth) {
                    const auto package = ObjectName(base, object);
                    const auto found = definitions.find(package);
                    if (found != definitions.end()) {
                        if (std::find(present.begin(), present.end(), found->second) == present.end()) {
                            present.push_back(found->second);
                            current.package = package;
                        }
                        break;
                    }
                    object = PPC_LOAD_U32(object + 0x28);
                }
            }
        }
    }
    // A map that appears in the world is being loaded, even while a streaming
    // transition still holds the previous one.
    for (const uint32_t id : present)
        if (std::find(presentMaps.begin(), presentMaps.end(), id) == presentMaps.end())
            NotifySceneLoad(MapScene(id));
    presentMaps = present;
    // Several maps at once is an ambiguous streaming transition.
    if (present.size() == 1) {
        current.available = true;
        current.id = present.front();
    } else {
        current.package.clear();
    }
    if (current.available) {
        // Live localized FString array indexed by map-definition ID, not font text ID.
        const auto data = PPC_LOAD_U32(0x832C9728), count = PPC_LOAD_U32(0x832C972C);
        if (Address(data) && count <= 4096 && current.id < count) {
            const auto item = data + current.id * 12;
            const auto length = PPC_LOAD_U32(item + 4);
            if (length > 0 && length <= 512)
                current.name = Text(base, PPC_LOAD_U32(item), length);
        }
    }
    // Map changes are always logged, once per resolved map and once when it is
    // lost: they place runtime reports such as temporal suspects without a capture.
    static bool loggedAvailable = false;
    static uint32_t loggedId = ~0u;
    static bool loggedNamed = false;
    const bool changed = current.available != snapshot.available ||
        current.id != snapshot.id || current.name != snapshot.name;
    const bool edge = current.available != loggedAvailable ||
        (current.available && (current.id != loggedId || (!loggedNamed && !current.name.empty())));
    if ((changed && getenv("LO_TRACE_MAP_INFO")) || edge) {
        const auto name = std::filesystem::path(current.name).u8string();
        const auto package = std::filesystem::path(current.package).u8string();
        LOG_INFO("current map available={} id={} name={} package={}", current.available, current.id,
            reinterpret_cast<const char*>(name.c_str()), reinterpret_cast<const char*>(package.c_str()));
        loggedAvailable = current.available;
        loggedId = current.id;
        loggedNamed = !current.name.empty();
    }
    currentMapScene = current.available ? MapScene(current.id) : 0;
    mapSceneUpdated = now.time_since_epoch().count();
    snapshot = std::move(current);
}

debug_menu::MapInfo debug_menu::GetMapInfo() {
    std::lock_guard lock(mutex);
    if (std::chrono::steady_clock::now() - lastUpdate > std::chrono::seconds(2)) return {};
    return snapshot;
}

void debug_menu::PollSceneLoads(uint8_t* base) {
    // The loader's request slots (see patches/encounter_defer.cpp): phase at +0,
    // FString name at +4, ID at +16. Slot 0 is the map jump, slot 1 the battle.
    constexpr uint32_t MapSlot = 0x832631F0 + 3168, BattleSlot = MapSlot + 88;
    static int32_t mapPhase = 0, battlePhase = 0;
    static uint32_t battleId = ~0u;
    static bool battleStarted = false;
    const auto phase = static_cast<int32_t>(PPC_LOAD_U32(MapSlot));
    if (phase > 0 && mapPhase <= 0) {
        const auto name = Key(Text(base, PPC_LOAD_U32(MapSlot + 4), 64));
        std::optional<uint32_t> id;
        {
            std::lock_guard lock(mutex);
            if (const auto found = definitions.find(name); found != definitions.end()) id = found->second;
        }
        const auto text = std::filesystem::path(name).u8string();
        LOG_INFO("scene load: map {} id={}", reinterpret_cast<const char*>(text.c_str()), id ? std::to_string(*id) : "-");
        if (id) {
            pendingScene = MapScene(*id);
            NotifySceneLoad(MapScene(*id));
        }
    }
    mapPhase = phase;

    const auto requested = static_cast<int32_t>(PPC_LOAD_U32(BattleSlot));
    const uint32_t id = PPC_LOAD_U32(BattleSlot + 16);
    if (requested > 0 && (battlePhase <= 0 || id != battleId)) {
        const auto tag = gpu::pipeline_cache::SceneTag(gpu::pipeline_cache::kSceneBattle, id);
        LOG_INFO("scene load: battle id={}", int32_t(id));
        battleId = id;
        battleScene = tag;
        pendingScene = tag;
        battleStarted = false;
        NotifySceneLoad(tag);
    }
    battlePhase = requested;

    // A requested map stays the loading scene until it is the current map, a
    // battle until its field is back.
    uint32_t pending = pendingScene;
    const uint32_t map = currentMapScene;
    if (pending >> 28 == gpu::pipeline_cache::kSceneBattle) {
        if (CurrentBattle()) battleStarted = true;
        else if (battleStarted && map) pendingScene.compare_exchange_strong(pending, 0u);
    } else if (pending && map == pending) {
        pendingScene.compare_exchange_strong(pending, 0u);
    }
}

uint32_t debug_menu::CurrentSceneTag() {
    if (CurrentBattle()) return battleScene;
    // Same staleness rule as GetMapInfo.
    const auto updated = std::chrono::steady_clock::time_point(std::chrono::steady_clock::duration(mapSceneUpdated.load()));
    const uint32_t map = std::chrono::steady_clock::now() - updated > std::chrono::seconds(2) ? 0 : currentMapScene.load();
    return map ? map : pendingScene.load();
}

void debug_menu::SetSceneLoadListener(void (*listener)(uint32_t)) {
    sceneListener = listener;
}
