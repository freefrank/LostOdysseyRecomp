#pragma once
#include <cstdint>
#include <string>

namespace debug_menu {
struct MapInfo {
    bool available{};
    uint32_t id{};
    std::wstring name, package;
};
// Update on the guest game thread; UI receives an owned snapshot only.
void UpdateMapInfo(uint8_t* base);
MapInfo GetMapInfo();

// Scenes as gpu::pipeline_cache scene tags (map definition ID or battle ID).
// Engine tick, after UpdateMapInfo: watches the loader's map and battle requests.
void PollSceneLoads(uint8_t* base);
// The battle being fought, else the current map, else the scene being loaded;
// 0 when unknown. Any thread.
uint32_t CurrentSceneTag();
// Called on the game thread when a map or battle starts loading.
void SetSceneLoadListener(void (*listener)(uint32_t tag));
}
