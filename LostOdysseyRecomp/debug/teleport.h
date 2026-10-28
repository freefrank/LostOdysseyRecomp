#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace debug_menu
{
    // A deliberate debug input limit, not a claim about the engine world size.
    inline constexpr float MaxTeleportCoordinate = 1000000.0f;
    struct Position { float x, y, z; };
    struct MapPoi
    {
        uint64_t id{};
        std::wstring label;
        Position position{};
    };
    struct TeleportSnapshot
    {
        bool available = false;
        Position current{};
        bool bookmarkAvailable = false;
        Position bookmark{};
        std::wstring status;
        uint64_t poiRevision{};
        std::vector<MapPoi> pois;
    };

    // UI requests are consumed on the guest game thread.
    TeleportSnapshot GetTeleportSnapshot();
    bool TeleportRequestPending();
    bool RequestTeleport(Position position);
    bool RequestTeleportOffset(Position offset);
    bool RequestSavePosition();
    bool RequestRestorePosition();
    bool RequestPoiTeleport(uint64_t id);
    // Queues a map jump through the game's own RequestMapJump. Returns nullptr
    // when queued, otherwise a translations.h status key naming the refusal.
    const wchar_t* RequestMapJump(const char* map);
    // The original event-debug room (location 295, on every disc).
    inline const wchar_t* RequestDebugEventRoom() { return RequestMapJump("z0g_9_scrw"); }
}
