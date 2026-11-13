#pragma once
#include <cstdint>

union PPCRegister { uint64_t u64 = 0; uint32_t u32; };
struct PPCContext { PPCRegister r3, r4; uint64_t lr = 0; };
#define PPC_FUNC(name) void name(PPCContext& ctx, uint8_t* base)
