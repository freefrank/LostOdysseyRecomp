#pragma once
#include <cstdint>

union PPCRegister
{
    uint64_t u64 = 0;
    uint32_t u32;
};
struct PPCContext
{
    PPCRegister r3, r4;
    struct { double f64 = 0; } f1;
};
#define PPC_FUNC(name) void name(PPCContext& ctx, uint8_t* base)

namespace fixture
{
uint8_t Load8(uint32_t address);
uint16_t Load16(uint32_t address);
uint32_t Load32(uint32_t address);
}
#define PPC_LOAD_U8(address) fixture::Load8(uint32_t(address))
#define PPC_LOAD_U16(address) fixture::Load16(uint32_t(address))
#define PPC_LOAD_U32(address) fixture::Load32(uint32_t(address))
