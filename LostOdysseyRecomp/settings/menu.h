#pragma once
#include <cstdint>
#include <vector>
namespace settings
{
// Called by input polling before returning the guest-facing controller state.
bool FilterInput(uint16_t &buttons, int16_t leftX, int16_t leftY);
// Snapshot rendered on the presentation thread, never accessing guest memory.
bool DrawMenu(std::vector<uint32_t> &pixels, uint64_t &revision);
void PointerClick(float x, float y, bool reverse);
} // namespace settings
