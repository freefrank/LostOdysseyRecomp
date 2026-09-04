#pragma once

// Force-included into every recompiled translation unit (see
// LostOdysseyRecompLib/CMakeLists.txt). XenonRecomp emits PPC_MM_STORE_* for
// stores that follow an eieio, which is how the D3D layer pokes GPU registers
// at 0x7FC80000 (CP_RB_WPTR etc.). Route those through the command processor.

#include <cstdint>

extern "C" void LoMmioStore32(uint8_t* base, uint32_t ea, uint32_t value);
extern "C" void LoMmioStore8(uint8_t* base, uint32_t ea, uint8_t value);
extern "C" void LoMmioStore64(uint8_t* base, uint32_t ea, uint64_t value);

#define PPC_MM_STORE_U8(x, y)  LoMmioStore8(base, (x), (y))
#define PPC_MM_STORE_U32(x, y) LoMmioStore32(base, (x), (y))
#define PPC_MM_STORE_U64(x, y) LoMmioStore64(base, (x), (y))
