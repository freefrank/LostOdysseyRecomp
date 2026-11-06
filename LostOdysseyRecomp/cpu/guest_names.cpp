// A cache in front of UE3's FName::Init (sub_82296D30), which game code calls for names it builds from strings every
// frame (the script dispatcher, sub_8229C490 and sub_8229C638): in a busy street it was 3.7% of Guest Main, hashing the
// string and walking a 4,096-bucket chain with a case-insensitive compare each time.
//
// FName::Init(this = r3, const TCHAR* r4, int number = r5, EFindName r6, bool split = r7) sets this->Index and
// this->Number. UE3 never removes a name, so a string that found or added one gets the same Index and Number from then
// on: those results are cached per thread, keyed by the exact string, number and split flag. A miss, a lookup that
// found nothing (FNAME_Find), a replace (FNAME_Replace, r6 = 2) and strings over 63 characters go to the game's code.
// LO_NATIVE_FNAME=0 turns the cache off; 2 runs the game's code on every hit too and logs any difference.
#include <stdafx.h>
#include <kernel/memory.h>
#include <os/logger.h>
#include <cstdlib>
#include <cstring>
#include <memory>

extern "C" PPC_FUNC(__imp__sub_82296D30);

namespace
{
    constexpr uint32_t kMaxChars = 63;
    constexpr uint32_t kEntries = 4096; // per thread that calls it, about 600 KB
    constexpr uint32_t kProbes = 4;

    int NativeNamesMode()
    {
        static const int mode = []()
            {
                const char* value = std::getenv("LO_NATIVE_FNAME");
                return value ? std::atoi(value) : 1;
            }();
        return mode;
    }

    struct Entry
    {
        uint64_t hash = 0;     // 0: empty
        uint32_t number = 0;   // the r5 the call was made with
        uint32_t split = 0;    // the r7
        uint32_t length = 0;
        uint32_t index = 0;    // this->Index, as stored (big endian)
        uint32_t numberOut = 0;
        uint16_t chars[kMaxChars]{};
    };

    struct Cache
    {
        Entry entries[kEntries];
    };

    Cache& ThreadCache()
    {
        thread_local std::unique_ptr<Cache> cache;
        if (!cache) cache = std::make_unique<Cache>();
        return *cache;
    }

    // Reads the guest string as stored (big-endian UTF-16). False when it's longer than kMaxChars.
    bool ReadName(uint8_t* base, uint32_t address, uint16_t* chars, uint32_t& length, uint64_t& hash)
    {
        uint64_t h = 0xcbf29ce484222325ull;
        const uint16_t* source = reinterpret_cast<const uint16_t*>(base + address);
        for (length = 0; length <= kMaxChars; ++length)
        {
            const uint16_t c = source[length];
            if (c == 0)
            {
                hash = h | 1;
                return true;
            }
            if (length == kMaxChars) return false;
            chars[length] = c;
            h = (h ^ c) * 0x100000001b3ull;
        }
        return false;
    }
}

PPC_FUNC(sub_82296D30)
{
    const int mode = NativeNamesMode();
    const uint32_t self = ctx.r3.u32, string = ctx.r4.u32, number = ctx.r5.u32, find = ctx.r6.u32, split = ctx.r7.u32;
    uint16_t chars[kMaxChars];
    uint32_t length = 0;
    uint64_t hash = 0;
    if (mode == 0 || find == 2 || !string || !ReadName(base, string, chars, length, hash))
    {
        __imp__sub_82296D30(ctx, base);
        return;
    }
    hash ^= (uint64_t(number) << 32) ^ (uint64_t(split) << 63);
    hash |= 1;

    Entry* entries = ThreadCache().entries;
    const uint32_t first = uint32_t(hash) & (kEntries - 1);
    for (uint32_t probe = 0; probe < kProbes; ++probe)
    {
        const Entry& entry = entries[(first + probe) & (kEntries - 1)];
        if (entry.hash == 0) break;
        if (entry.hash != hash || entry.number != number || entry.split != split || entry.length != length ||
            std::memcmp(entry.chars, chars, length * sizeof(uint16_t)) != 0)
            continue;
        if (mode == 2)
        {
            __imp__sub_82296D30(ctx, base);
            const uint32_t index = *reinterpret_cast<const uint32_t*>(base + self);
            const uint32_t numberOut = *reinterpret_cast<const uint32_t*>(base + self + 4);
            if (index != entry.index || numberOut != entry.numberOut)
            {
                static uint32_t reported = 0;
                if (reported++ < 32)
                    LOG_ERROR("FName cache: string at {:#x} gives {:#x}/{:#x}, cached {:#x}/{:#x}", string,
                        __builtin_bswap32(index), __builtin_bswap32(numberOut),
                        __builtin_bswap32(entry.index), __builtin_bswap32(entry.numberOut));
            }
            return;
        }
        *reinterpret_cast<uint32_t*>(base + self) = entry.index;
        *reinterpret_cast<uint32_t*>(base + self + 4) = entry.numberOut;
        return;
    }

    __imp__sub_82296D30(ctx, base);
    const uint32_t index = *reinterpret_cast<const uint32_t*>(base + self);
    if (index == 0) return; // FNAME_Find found nothing, or NAME_None: the string may be added later
    Entry& slot = entries[first];
    slot.hash = hash;
    slot.number = number;
    slot.split = split;
    slot.length = length;
    slot.index = index;
    slot.numberOut = *reinterpret_cast<const uint32_t*>(base + self + 4);
    std::memcpy(slot.chars, chars, length * sizeof(uint16_t));
}
