// Native fast paths of UE3's pool allocator, about 6% of Guest Main in a battle. FMallocPool::Malloc (sub_82295B08) and
// Free (sub_82298990) pop and push a block of a pool's free list, under a lock the callers hold, so a shorter path also
// holds that lock less. These do the same loads and stores on the same guest structures, and leave everything else to the
// recompiled functions before touching memory: a new pool, an exhausted or emptied one, and large allocations.
// LO_NATIVE_MALLOC=0 keeps the recompiled code, for A/B runs.
#include <stdafx.h>
#include <kernel/memory.h>
#include <cstdlib>

namespace
{
    // FMallocPool, by the offsets the guest code uses.
    constexpr uint32_t kMaxPooledSize = 32768;
    constexpr uint32_t kOsTable = 3364;        // the table of allocations that are not pooled
    constexpr uint32_t kSizeTable = 878 * 4;   // pool index by size
    constexpr uint32_t kPageTable = 846 * 4;   // pool headers by the address's top bits
    constexpr uint32_t kTables = 2524;
    constexpr uint32_t kAlternateTables = 4;
    constexpr uint32_t kUseAlternate = 0x48DE4;
    constexpr uint32_t kAlternateSet = 0x48DE8;
    constexpr uint32_t kTableSize = 20;
    constexpr uint32_t kTablesPerSet = 42;
    // FPoolTable
    constexpr uint32_t kTableFirstPool = 8;
    constexpr uint32_t kTableBlockSize = 16;
    // FPoolInfo
    constexpr uint32_t kPoolTaken = 8;
    constexpr uint32_t kPoolTable = 12;
    constexpr uint32_t kPoolFirstMem = 16;
    // FFreeMem
    constexpr uint32_t kFreeNext = 0;
    constexpr uint32_t kFreeBlocks = 4;

    bool Enabled()
    {
        static const bool enabled = !(std::getenv("LO_NATIVE_MALLOC") && std::getenv("LO_NATIVE_MALLOC")[0] == '0');
        return enabled;
    }

    uint32_t Load(uint8_t* base, uint32_t address)
    {
        return ByteSwap(*reinterpret_cast<uint32_t*>(base + address));
    }

    void Store(uint8_t* base, uint32_t address, uint32_t value)
    {
        *reinterpret_cast<uint32_t*>(base + address) = ByteSwap(value);
    }
}

extern "C" PPC_FUNC(__imp__sub_82295B08);
extern "C" PPC_FUNC(__imp__sub_82298990);

PPC_FUNC(sub_82295B08)
{
    const uint32_t self = ctx.r3.u32, size = ctx.r4.u32;
    if (Enabled() && size <= kMaxPooledSize)
    {
        uint32_t index = Load(base, self + kSizeTable + size * 4);
        uint32_t table;
        if (Load(base, self + kUseAlternate))
            table = self + (index + Load(base, self + kAlternateSet) * kTablesPerSet) * kTableSize + kAlternateTables;
        else
            table = self + index * kTableSize + kTables;
        const uint32_t pool = Load(base, table + kTableFirstPool);
        if (pool)
        {
            const uint32_t free = Load(base, pool + kPoolFirstMem);
            const uint32_t blocks = Load(base, free + kFreeBlocks) - 1;
            const uint32_t next = blocks ? 0 : Load(base, free + kFreeNext);
            // The last block of the pool's last run empties the pool: the recompiled code moves it between lists.
            if (blocks || next)
            {
                Store(base, pool + kPoolTaken, Load(base, pool + kPoolTaken) + 1);
                Store(base, free + kFreeBlocks, blocks);
                if (!blocks)
                    Store(base, pool + kPoolFirstMem, next);
                ctx.r3.u64 = free + Load(base, table + kTableBlockSize) * blocks;
                return;
            }
        }
    }
    __imp__sub_82295B08(ctx, base);
}

PPC_FUNC(sub_82298990)
{
    const uint32_t self = ctx.r3.u32, address = ctx.r4.u32;
    if (Enabled() && address)
    {
        const uint32_t pool = Load(base, self + kPageTable + (address >> 27) * 4) + ((address >> 11) & 0xFFE0);
        const uint32_t firstMem = Load(base, pool + kPoolFirstMem);
        const uint32_t taken = Load(base, pool + kPoolTaken) - 1;
        // Not pooled, or the pool changes list (it was full, or this empties it): the recompiled code.
        if (Load(base, pool + kPoolTable) != self + kOsTable && firstMem && taken)
        {
            Store(base, address + kFreeBlocks, 1);
            Store(base, address + kFreeNext, firstMem);
            Store(base, pool + kPoolFirstMem, address);
            Store(base, pool + kPoolTaken, taken);
            return;
        }
    }
    __imp__sub_82298990(ctx, base);
}
