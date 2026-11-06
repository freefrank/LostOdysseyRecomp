// The D3D runtime's shader constant copy (sub_827ACD90: r3 = device, r4 = first register, r5 = source, r6 = count of
// vec4s, r7 = dirty bits). It copies count * 16 bytes from an unaligned source into the device's constant shadow at
// device + (r4 + 120) * 16, then ORs r7 into the 64-bit dirty mask at the device's start. Each 16 bytes take two
// shuffled loads, an OR and a shuffled store through the vector registers in memory in the recompiled loop: 2.9% of
// the game's rendering thread in the busy street, which sets Guest Main's pace there. The bytes go through unswapped,
// so here it's a copy, in the guest's order (four vec4s loaded before they are stored, then one at a time).
//
// General registers as the recompiled code leaves them: r4 = 16; with more than three vec4s r5 = 32 and r6 = 48;
// r8, r9 and r10 as its loops end. Vector registers v0 and v7-v13 (volatile) aren't reproduced.
// LO_NATIVE_CONSTANT_COPY=0 keeps the recompiled code; 2 runs both and logs differences (first 20, and a count every
// 2^20 calls).
#include <stdafx.h>
#include <kernel/memory.h>
#include <os/logger.h>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" PPC_FUNC(__imp__sub_827ACD90);

namespace
{
    int NativeConstantCopyMode()
    {
        static const int mode = []()
            {
                const char* value = std::getenv("LO_NATIVE_CONSTANT_COPY");
                return value ? std::atoi(value) : 1;
            }();
        return mode;
    }

    struct Registers
    {
        uint64_t r4, r5, r6, r8, r9, r10;
    };

    // The copy and the registers the recompiled loops leave.
    Registers Copy(uint8_t* base, const PPCContext& ctx)
    {
        uint32_t count = ctx.r6.u32;
        uint64_t source = ctx.r5.u64;
        uint64_t destination = ctx.r3.u64 + (uint64_t(ctx.r4.u32 + 120u) << 4 & 0xFFFFFFF0u);
        Registers out{16, ctx.r5.u64, ctx.r6.u64, 128, ctx.r6.u64, 0};
        uint8_t block[64];
        if (count > 3)
        {
            for (uint32_t groups = ((count - 4) >> 2) + 1; groups; --groups)
            {
                std::memcpy(block, base + uint32_t(source), 64);
                std::memcpy(base + (uint32_t(destination) & ~0xFu), block, 64);
                source += 64;
                destination += 64;
                out.r9 -= 4;
            }
            out.r5 = 32;
            out.r6 = 48;
            out.r8 = 0;
        }
        for (uint32_t left = uint32_t(out.r9); left; --left)
        {
            out.r8 = out.r6 = source;
            std::memcpy(block, base + uint32_t(source), 16);
            std::memcpy(base + (uint32_t(destination) & ~0xFu), block, 16);
            source += 16;
            destination += 16;
            out.r9 = left - 1;
        }
        out.r10 = destination;
        const uint32_t device = ctx.r3.u32;
        PPC_STORE_U64(device, PPC_LOAD_U64(device) | ctx.r7.u64);
        return out;
    }

    void Store(PPCContext& ctx, const Registers& r)
    {
        ctx.r4.u64 = r.r4;
        ctx.r5.u64 = r.r5;
        ctx.r6.u64 = r.r6;
        ctx.r8.u64 = r.r8;
        ctx.r9.u64 = r.r9;
        ctx.r10.u64 = r.r10;
    }

    // Runs the recompiled code and compares what it left with the copy computed beforehand.
    void Check(PPCContext& ctx, uint8_t* base)
    {
        static uint32_t calls, differences;
        const uint32_t count = ctx.r6.u32 > 4096 ? 0 : ctx.r6.u32;
        const uint32_t device = ctx.r3.u32;
        const uint32_t destination = (ctx.r3.u32 + ((ctx.r4.u32 + 120u) << 4)) & ~0xFu;
        std::vector<uint8_t> source(base + ctx.r5.u32, base + ctx.r5.u32 + size_t(count) * 16);
        const uint64_t dirty = PPC_LOAD_U64(device) | ctx.r7.u64;
        // The native copy for its registers, then the memory as it was, for the recompiled code to write.
        PPCContext copy = ctx;
        std::vector<uint8_t> saved(base + destination, base + destination + size_t(count) * 16);
        const uint64_t savedDirty = PPC_LOAD_U64(device);
        const Registers native = Copy(base, copy);
        std::memcpy(base + destination, saved.data(), saved.size());
        PPC_STORE_U64(device, savedDirty);
        __imp__sub_827ACD90(ctx, base);
        ++calls;
        const bool bytes = std::memcmp(base + destination, source.data(), source.size()) == 0;
        const bool registers = ctx.r4.u64 == native.r4 && ctx.r5.u64 == native.r5 && ctx.r6.u64 == native.r6 &&
            ctx.r8.u64 == native.r8 && ctx.r9.u64 == native.r9 && ctx.r10.u64 == native.r10;
        if ((!bytes || !registers || PPC_LOAD_U64(device) != dirty) && ++differences <= 20)
            LOG_ERROR("[constant copy] sub_827ACD90 differs: device {:08X} count {} bytes {} registers {} dirty {:016X}/{:016X}",
                device, count, bytes, registers, dirty, PPC_LOAD_U64(device));
        if ((calls & 0xFFFFF) == 0)
            LOG_ERROR("[constant copy] sub_827ACD90 checked {} calls, {} differ", calls, differences);
    }
}

PPC_FUNC(sub_827ACD90)
{
    const int mode = NativeConstantCopyMode();
    if (mode == 0)
    {
        __imp__sub_827ACD90(ctx, base);
        return;
    }
    if (mode == 2)
    {
        Check(ctx, base);
        return;
    }
    Store(ctx, Copy(base, ctx));
}
