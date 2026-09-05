#pragma once
#include "teleport.h"

namespace debug_menu
{
    struct LiveMapPoi
    {
        uint32_t actor{}, level{}, objectIndex{};
        std::wstring key, category;
        Position position{};
    };
    // Call only on the guest game thread; only actors in this world's loaded
    // Level arrays are considered. No global UObject scans or fixed addresses.
    std::vector<LiveMapPoi> ScanMapPois(uint8_t* base, uint32_t world);
}
