// The D3D runtime's shader constant packets (sub_823C6CB8: r3 = the device, r4 = a 64-bit mask of dirty 64-byte
// blocks, highest bit first, r5 = the first register, r6 = the constants). For each run of set bits it pads the
// command buffer's write pointer (device +48) with 0x80000000 words to 12 bytes past a 16-byte boundary, writes a
// header ((words - 1) << 16 | register) and copies the run's blocks. The recompiled copy takes every 16 bytes through
// a shuffle into the vector registers in memory and back: 2.1% of the game's rendering thread in the busy street.
//
// Native when every run fits before the buffer's end (device +52), as the recompiled check asks; otherwise, or for an
// empty mask, the recompiled function runs (its sub_823D1E48 call grows the buffer). Same words, same copies, and
// r4, r5, r8, r9, r10 as its return leaves them (r9 and r10 can hold the padding word).
// LO_NATIVE_CONSTANT_PACKETS=0 keeps the recompiled code; 2 runs both and compares (first 20 differences, a count
// every 2^20 calls).
#include <stdafx.h>
#include <kernel/memory.h>
#include <os/logger.h>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" PPC_FUNC(__imp__sub_823C6CB8);

namespace
{
    int NativeConstantPacketsMode()
    {
        static const int mode = []()
            {
                const char* value = std::getenv("LO_NATIVE_CONSTANT_PACKETS");
                return value ? std::atoi(value) : 1;
            }();
        return mode;
    }

    constexpr uint64_t kPadding = 0xFFFFFFFF80000000ull; // lis r25,-32768

    struct Result
    {
        uint64_t r4, r5, r8, r9, r10;
    };

    // Writes go through `store`, `copy`: the guest's memory, or a check buffer.
    template <class Store, class Copy>
    bool Run(uint8_t* base, const PPCContext& ctx, Result& out, Store&& store, Copy&& copy)
    {
        const uint32_t device = ctx.r3.u32;
        uint64_t mask = ctx.r4.u64, r5 = ctx.r5.u64, source = ctx.r6.u64 - 4;
        uint64_t r4 = PPC_LOAD_U32(device + 48), r8 = 0, r9 = 0, r10 = 0;
        if (!mask) return false;
        do
        {
            const uint64_t zeros = uint64_t(__builtin_clzll(mask));
            r9 = PPC_LOAD_U32(device + 52);
            r8 = zeros;
            r10 = (zeros << 6) & 0xFFFFFFC0u;
            source += r10;
            const uint64_t r27 = ((zeros << 4) & 0xFFFFFFF0u) + r5;
            mask <<= zeros;
            const uint64_t run = ~mask ? uint64_t(__builtin_clzll(~mask)) : 64;
            const uint64_t words = (run << 4) & 0xFFFFFFF0u;
            if (uint32_t((((words + 3) << 2) & 0xFFFFFFFCu) + r4) >= uint32_t(r9)) return false;
            const uint32_t misalign = uint32_t(r4) & 15;
            const uint32_t pads = misalign == 4 ? 1 : misalign == 0 ? 2 : misalign == 12 ? 3 : 0;
            if (misalign == 0 || misalign == 12) r10 = kPadding;
            if (misalign == 12) r9 = kPadding;
            for (uint32_t p = 0; p < pads; ++p, r4 += 4) store(uint32_t(r4 + 4), 0x80000000u);
            r5 = words + r27;
            store(uint32_t(r4 + 4), uint32_t(((uint32_t(words - 1) << 16) & 0xFFFF0000u) | r27));
            r4 += 4;
            for (uint64_t block = 0; block < run; ++block)
            {
                copy((uint32_t(r4) + 4) & ~0xFu, (uint32_t(source) + 4) & ~0xFu);
                source += 64;
                r4 += 64;
                mask <<= 1;
            }
        } while (mask);
        out = {r4, r5, r8, r9, r10};
        return true;
    }

    void Apply(PPCContext& ctx, const Result& r)
    {
        ctx.r4.u64 = r.r4;
        ctx.r5.u64 = r.r5;
        ctx.r8.u64 = r.r8;
        ctx.r9.u64 = r.r9;
        ctx.r10.u64 = r.r10;
    }

    void Check(PPCContext& ctx, uint8_t* base)
    {
        static std::atomic<uint32_t> calls, differences;
        const uint32_t device = ctx.r3.u32;
        const uint32_t start = PPC_LOAD_U32(device + 48);
        // The native writes into a copy of the buffer from its write pointer on.
        std::vector<uint8_t> buffer(size_t(PPC_LOAD_U32(device + 52) - start) + 64);
        auto at = [&](uint32_t address) { return buffer.data() + (address - start); };
        Result native{};
        const bool planned = Run(base, ctx, native,
            [&](uint32_t address, uint32_t value) { const uint32_t be = ByteSwap(value); std::memcpy(at(address), &be, 4); },
            [&](uint32_t destination, uint32_t from) { std::memcpy(at(destination), base + from, 64); });
        __imp__sub_823C6CB8(ctx, base);
        if (!planned) return;
        const uint32_t n = calls.fetch_add(1) + 1;
        const uint32_t end = uint32_t(native.r4);
        const bool bytes = PPC_LOAD_U32(device + 48) == end && std::memcmp(base + start + 4, at(start + 4), end - start) == 0;
        const bool registers = ctx.r4.u64 == native.r4 && ctx.r5.u64 == native.r5 && ctx.r8.u64 == native.r8 &&
            ctx.r9.u64 == native.r9 && ctx.r10.u64 == native.r10;
        if ((!bytes || !registers) && differences.fetch_add(1) < 20)
            LOG_ERROR("[constant packets] sub_823C6CB8 differs: device {:08X} bytes {} registers {} r4 {:X}/{:X} r9 {:X}/{:X} r10 {:X}/{:X}",
                device, bytes, registers, native.r4, ctx.r4.u64, native.r9, ctx.r9.u64, native.r10, ctx.r10.u64);
        if ((n & 0xFFFFF) == 0)
            LOG_ERROR("[constant packets] sub_823C6CB8 checked {} calls, {} differ", n, differences.load());
    }
}

PPC_FUNC(sub_823C6CB8)
{
    const int mode = NativeConstantPacketsMode();
    if (mode == 0)
    {
        __imp__sub_823C6CB8(ctx, base);
        return;
    }
    if (mode == 2)
    {
        Check(ctx, base);
        return;
    }
    // Plan first: nothing is written unless every run fits.
    Result result{};
    if (!Run(base, ctx, result, [](uint32_t, uint32_t) {}, [](uint32_t, uint32_t) {}))
    {
        __imp__sub_823C6CB8(ctx, base);
        return;
    }
    Run(base, ctx, result,
        [&](uint32_t address, uint32_t value) { PPC_STORE_U32(address, value); },
        [&](uint32_t destination, uint32_t from) { std::memcpy(base + destination, base + from, 64); });
    PPC_STORE_U32(ctx.r3.u32 + 48, uint32_t(result.r4));
    Apply(ctx, result);
}
