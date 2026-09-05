#pragma once

#include <cstdint>

namespace GuestAddressSpace
{
// A and C alias the same 512 MiB; E starts one physical page later.
// Keep these OS mappings coherent even for direct recompiled base+address loads.
uint8_t* Allocate();
void Release(uint8_t* base);
}
