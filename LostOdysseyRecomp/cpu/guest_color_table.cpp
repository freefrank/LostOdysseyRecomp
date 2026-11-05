// FLinearColor from an FColor (sub_822A42B8: r3 = the four floats out, r4 = the B, G, R, A bytes): each channel is
// its byte times a scale, raised to a power, with the VMX log and exp estimates and their special cases. Every
// operation is per lane and the scale and power are the function's own statics (two vectors it fills on its first
// call, flags at 0x8336D980), so each output float depends only on its byte. 1.7% of the game's rendering thread in
// the busy street. Here the floats come from a table of 4 x 256 words, filled once by calling the recompiled function
// for every byte value after its statics exist: the same words for every input.
//
// Registers: the recompiled code leaves only volatile ones (r5-r11, vector registers) changed, none read by callers.
// LO_NATIVE_COLOR_TABLE=0 keeps the recompiled code; 2 runs it and compares (first 20 differences, a count every 2^20).
#include <stdafx.h>
#include <kernel/memory.h>
#include <os/logger.h>
#include <atomic>
#include <cstdlib>
#include <mutex>

extern "C" PPC_FUNC(__imp__sub_822A42B8);

namespace
{
    int NativeColorTableMode()
    {
        static const int mode = []()
            {
                const char* value = std::getenv("LO_NATIVE_COLOR_TABLE");
                return value ? std::atoi(value) : 1;
            }();
        return mode;
    }

    constexpr uint32_t kStaticFlags = 0x8336D980; // bit 0: scale, bit 1: power vector filled
    // Output word c comes from input byte kByteOf[c] (lbz 1, 2, 3, 0).
    constexpr uint32_t kByteOf[4] = {1, 2, 3, 0};

    uint32_t g_table[4][256]; // guest-order words of each output float
    std::atomic<bool> g_ready{false};
    std::mutex g_buildMutex;

    // Runs the recompiled function on a copy of the context, below the caller's stack, for every byte value.
    void Build(const PPCContext& ctx, uint8_t* base)
    {
        std::lock_guard lock(g_buildMutex);
        if (g_ready.load(std::memory_order_acquire)) return;
        PPCContext scratch = ctx;
        const uint32_t stack = ctx.r1.u32 - 512;
        const uint32_t input = stack - 64, output = stack - 48;
        for (uint32_t value = 0; value < 256; ++value)
        {
            PPC_STORE_U32(input, value * 0x01010101u);
            scratch.r1.u64 = stack;
            scratch.r3.u64 = output;
            scratch.r4.u64 = input;
            __imp__sub_822A42B8(scratch, base);
            for (uint32_t c = 0; c < 4; ++c) g_table[c][value] = PPC_LOAD_U32(output + c * 4);
        }
        g_ready.store(true, std::memory_order_release);
    }

    void Lookup(uint8_t* base, uint32_t out, uint32_t color)
    {
        for (uint32_t c = 0; c < 4; ++c)
            PPC_STORE_U32(out + c * 4, g_table[c][PPC_LOAD_U8(color + kByteOf[c])]);
    }

    void Check(PPCContext& ctx, uint8_t* base)
    {
        static std::atomic<uint32_t> calls, differences;
        const uint32_t out = ctx.r3.u32, color = ctx.r4.u32;
        uint32_t expected[4];
        for (uint32_t c = 0; c < 4; ++c) expected[c] = g_table[c][PPC_LOAD_U8(color + kByteOf[c])];
        __imp__sub_822A42B8(ctx, base);
        const uint32_t n = calls.fetch_add(1) + 1;
        for (uint32_t c = 0; c < 4; ++c)
            if (PPC_LOAD_U32(out + c * 4) != expected[c])
            {
                if (differences.fetch_add(1) < 20)
                    LOG_ERROR("[color table] sub_822A42B8 differs: color {:08X} lane {} {:08X}/{:08X}", PPC_LOAD_U32(color), c,
                        expected[c], PPC_LOAD_U32(out + c * 4));
                break;
            }
        if ((n & 0xFFFFF) == 0)
            LOG_ERROR("[color table] sub_822A42B8 checked {} calls, {} differ", n, differences.load());
    }
}

PPC_FUNC(sub_822A42B8)
{
    const int mode = NativeColorTableMode();
    if (mode == 0)
    {
        __imp__sub_822A42B8(ctx, base);
        return;
    }
    if (!g_ready.load(std::memory_order_acquire))
    {
        if ((PPC_LOAD_U32(kStaticFlags) & 3) != 3)
        {
            __imp__sub_822A42B8(ctx, base);
            return;
        }
        {
            Build(ctx, base);
            // Build ran guest code on a copy of the context: the CPU's flush mode may no longer match ctx's cached one.
            ctx.fpscr.loadFromHost();
        }
    }
    if (mode == 2)
    {
        Check(ctx, base);
        return;
    }
    Lookup(base, ctx.r3.u32, ctx.r4.u32);
}
