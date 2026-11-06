// The game's wcslen (sub_82296830, 547 call sites: FName, FString, script and UI code). The recompiled loop reads one
// big-endian character per iteration; in a busy street it was 0.6% of Guest Main, three quarters of that in the loop.
// Here the string is read eight bytes (four characters) at a time from aligned words, which never cross a page, so
// nothing past the word that holds the terminator is touched; a zero character is found the same whichever byte order
// it's read in. Strings at an odd address take the game's character-by-character path.
//
// Registers as the recompiled code leaves them: r3 = the length (from the same 32-bit arithmetic), r10 = 0 (the
// terminator it loaded last); it doesn't write back r11 or XER.
// LO_NATIVE_WCSLEN=0 keeps the recompiled code; 2 runs both and logs differences (first 20, and a count every 2^20).
#include <stdafx.h>
#include <kernel/memory.h>
#include <os/logger.h>
#include <cstdlib>
#include <cstring>

extern "C" PPC_FUNC(__imp__sub_82296830);

namespace
{
    int NativeWcslenMode()
    {
        static const int mode = []()
            {
                const char* value = std::getenv("LO_NATIVE_WCSLEN");
                return value ? std::atoi(value) : 1;
            }();
        return mode;
    }

    // Number of characters before the terminator of the string at `address` (even), with guest address wraparound.
    uint32_t StringBytes(uint8_t* base, uint32_t address)
    {
        constexpr uint64_t kLow = 0x0001000100010001ull;
        constexpr uint64_t kHigh = 0x8000800080008000ull;

        uint32_t word = address & ~7u;
        uint64_t value;
        std::memcpy(&value, base + word, sizeof(value));
        // The characters before the string's first read as non-zero.
        const uint32_t skipped = (address & 7u) * 8;
        if (skipped != 0)
            value |= (uint64_t(1) << skipped) - 1;
        for (;;)
        {
            // The lowest flagged lane is the first zero character (a false flag can only follow a real one).
            const uint64_t zero = (value - kLow) & ~value & kHigh;
            if (zero != 0)
                return word + uint32_t(__builtin_ctzll(zero) / 16) * 2 - address;
            word += 8;
            std::memcpy(&value, base + word, sizeof(value));
        }
    }

    // r3 as the guest leaves it: ((start + 2 * (length + 1)) - start) as a 64-bit difference, srawi 1, minus 1.
    uint64_t GuestResult(uint64_t start, uint32_t bytes)
    {
        const uint64_t end = start + uint64_t(bytes) + 2;
        const int64_t difference = int64_t(end - start);
        const int64_t halved = int64_t(int32_t(uint32_t(difference)) >> 1);
        return uint64_t(halved - 1);
    }

    void CheckLength(PPCContext& ctx, uint8_t* base, uint64_t native)
    {
        static uint32_t calls, differences;
        const uint64_t start = ctx.r3.u64;
        __imp__sub_82296830(ctx, base);
        ++calls;
        if ((ctx.r3.u64 != native || ctx.r10.u64 != 0) && ++differences <= 20)
            LOG_ERROR("[wcslen] sub_82296830 differs: string {:08X} r3 {:016X}/{:016X} r10 {:016X}", uint32_t(start), native,
                ctx.r3.u64, ctx.r10.u64);
        if ((calls & 0xFFFFF) == 0)
            LOG_ERROR("[wcslen] sub_82296830 checked {} calls, {} differ", calls, differences);
    }
}

PPC_FUNC(sub_82296830)
{
    const int mode = NativeWcslenMode();
    const uint32_t address = ctx.r3.u32;
    if (mode == 0 || (address & 1) != 0)
    {
        __imp__sub_82296830(ctx, base);
        return;
    }

    const uint64_t result = GuestResult(ctx.r3.u64, StringBytes(base, address));
    if (mode == 2)
    {
        CheckLength(ctx, base, result);
        return;
    }
    ctx.r3.u64 = result;
    ctx.r10.u64 = 0;
}
