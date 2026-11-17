#pragma once
#include <cstdint>

// Harness hook for recording pipeline recipes battle by battle.
// LO_DEBUG_BATTLE_FILE holds one command, "serial battle <formation> [stage]",
// "serial victory" (the debug menu's Victory) or "serial list"; a higher
// serial runs it. The formation starts through the game's own walking
// encounter path (see no_encounters.cpp), so it needs a controllable player
// in the field. A stage (battle map package) replaces the formation's own.
namespace debug_menu::battle_tour
{
    // Formation table: TArray (data, count) at this address with 160-byte entries;
    // RequestBattle 0x828278A0 indexes it with the formation id.
    inline constexpr uint32_t FormationArray = 0x83264978 + 128;
    inline constexpr uint32_t FormationBytes = 160;
    // Engine tick (guest game thread).
    void Poll(uint8_t* base);
    // Walking encounter hooks (guest game thread).
    bool Pending();
    bool Take(uint8_t* base, int32_t& formation);
    void RestoreStage(uint8_t* base);
}
