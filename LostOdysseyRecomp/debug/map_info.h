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
// Owned, serialised game-thread observation for bounded performance probes.
// A stale (>2 s) observation is returned unavailable with serial zero.
struct MapObservation { MapInfo info; uint64_t serial = 0; };
MapObservation ObserveMapInfo();
}
