#pragma once
#include <cstdint>
#include <string>

struct PPCContext;

// Live mod reload. After the Mods page saves a changed list, the current disc's
// rebuilt files are prepared for the new mods (modding/text_overlay.h) while the
// game runs. Once the player walks a field map with nothing loading, the game
// reloads that disc's index through its own disc-change path, asked for the disc
// it already has. Areas loaded from then on use the new files; what is already
// on screen stays until the player changes area.
namespace mod_hot_reload
{
enum class Phase { Idle, Preparing, Waiting, Reloading, Done, Unchanged, Failed };
struct Status
{
    Phase phase = Phase::Idle;
    uint32_t done = 0, total = 0; // packages, while Preparing
    std::string error;
};
// Re-reads the mods and starts preparing. False when mods are off (LO_MODS=0).
bool Request();
Status GetStatus();
// Engine tick on the game thread. playerInField: the player controls a pawn on
// a field map (debug/teleport.cpp).
void Tick(PPCContext &ctx, uint8_t *base, bool playerInField);
}
