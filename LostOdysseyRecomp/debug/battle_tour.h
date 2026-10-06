#pragma once
#include <cstdint>

// Harness hook for recording pipeline recipes battle by battle.
// LO_DEBUG_BATTLE_FILE holds one command, "serial battle <formation>" or
// "serial list"; a higher serial runs it. The formation starts through the
// game's own walking-encounter path (see no_encounters.cpp), so it needs a
// controllable player in the field.
namespace debug_menu::battle_tour
{
    // Engine tick (guest game thread).
    void Poll(uint8_t* base);
    // Walking encounter hooks (guest game thread).
    bool Pending();
    bool Take(int32_t& formation);
}
