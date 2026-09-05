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
}
