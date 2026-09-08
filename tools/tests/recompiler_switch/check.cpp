#include <cstdio>
#include <initializer_list>
#include <windows.h>
#include "generated.cpp"

int main(int argc, char** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    alignas(32) uint8_t memory[0x500]{};
    // Each guest case address is stored big-endian, as lwzx expects.
    for (unsigned i = 0; i < 8; ++i)
    {
        const uint32_t label = __builtin_bswap32(0x1020 + i * 8);
        memcpy(memory + 0x400 + i * 4, &label, 4);
    }
    constexpr uint32_t values[]{13, 57, 22, 96, 5, 84, 41, 70};
    unsigned checks = 0;
    auto check = [&](bool increment, uint64_t input) {
        PPCContext ctx{};
        ctx.r3.u64 = input;
        const uint32_t index = uint32_t(input) + (increment ? 1u : 0u);
        const uint32_t expected = index < 8 ? values[index] : 255;
        if (increment)
            __imp__switch_increment(ctx, memory);
        else
            __imp__switch_direct(ctx, memory);
        ++checks;
        if (ctx.r3.u32 != expected ||
            (index < 8 && ctx.ctr.u32 != 0x1020 + index * 8))
        {
            printf("FAIL increment=%d input=%016llX expected=%u actual=%u ctr=%08X\n",
                increment, input, expected, ctx.r3.u32, ctx.ctr.u32);
            return false;
        }
        return true;
    };

    // Separate mode also validates the old lowering on ordinary inputs before
    // asking its child process to demonstrate the high-word failure.
    const bool normalOnly = argc == 2 && strcmp(argv[1], "normal") == 0;
    for (unsigned i = 0; i < 10; ++i)
        if (!check(false, i) || !check(true, i))
            return 1;
    if (!normalOnly)
    {
        // Exact failure: lwz produces zero-extended -1; 64-bit addi retains the
        // carry, while cmplwi and rlwinm select case zero using the low word.
        if (!check(true, 0x00000000FFFFFFFFull))
            return 1;
        constexpr uint64_t highs[]{0, 1, 0x12345678, 0xFFFFFFFF};
        constexpr uint32_t lows[]{0, 1, 2, 3, 4, 5, 6, 7, 8, 0xFFFFFFFE, 0xFFFFFFFF};
        for (auto high : highs)
            for (auto low : lows)
                for (bool increment : {false, true})
                    if (!check(increment, (high << 32) | low))
                        return 1;
    }
    printf("PASS %u generated PPC switch cases\n", checks);
    return 0;
}
