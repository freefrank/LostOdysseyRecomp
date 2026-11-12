#pragma once

#include <cstdint>

struct PPCContext;

namespace debug_menu::soft_reset
{
// The overlay queues an action; only the guest thread executes the transition.
bool Request();
const wchar_t* Status();
const wchar_t* TakeRefusal();
void Tick(PPCContext& ctx, uint8_t* base);
}
