#include <stdafx.h>
#include "command_processor.h"
#include "ppc_mmio.h"
#include <apu/xma.h>
#include <os/logger.h>

// MMIO stores from recompiled code. Values arrive in guest (native) order;
// the memory mirror keeps the big-endian image so plain guest loads still see
// what was written.

static inline bool IsGpuRegister(uint32_t ea)
{
    return ea - gpu::MMIO_BASE < 0x10000;
}

extern "C" void LoMmioStore32(uint8_t* base, uint32_t ea, uint32_t value)
{
    if (apu::xma::WriteCommand(ea, __builtin_bswap32(value))) return;
    *(volatile uint32_t*)(base + ea) = __builtin_bswap32(value);
    if (IsGpuRegister(ea))
        gpu::g_commandProcessor.MmioWrite32(ea, value);
    else if (ea >= 0x7FC00000 && ea < 0x80000000)
    {
        static int logged = 0;
        if (logged++ < 20)
            LOG_KERNEL("non-GPU MMIO store {:#x} = {:#x}", ea, value);
    }
}

extern "C" void LoMmioStore8(uint8_t* base, uint32_t ea, uint8_t value)
{
    *(volatile uint8_t*)(base + ea) = value;
}

extern "C" void LoMmioStore64(uint8_t* base, uint32_t ea, uint64_t value)
{
    *(volatile uint64_t*)(base + ea) = __builtin_bswap64(value);
    if (IsGpuRegister(ea))
    {
        gpu::g_commandProcessor.MmioWrite32(ea, uint32_t(value >> 32));
        gpu::g_commandProcessor.MmioWrite32(ea + 4, uint32_t(value));
    }
}
