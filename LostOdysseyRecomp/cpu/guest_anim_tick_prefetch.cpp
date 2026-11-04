// Prefetch for the animation tree tick (sub_822B34A0, USkeletalMeshComponent::TickAnimNodes-like: for each node of the
// component's AnimTickArray (+652, count +656) it updates the node's relevance (+132, +144, +148), calls the
// OnBecomeRelevant/OnCeaseRelevant virtuals (+300/+304) when that changes, and TickAnim (+280)). In a busy street it was
// 2.2% of Guest Main, three quarters of it on one 16-byte line: the load of each node's vtable pointer (+0), a cache
// miss per node. The node's +132..+148 line is usually warm (sub_822B3600 writes +148 of every child), +0 is not.
//
// The game's code still does all the work, unchanged. Before it runs, this hook asks the memory system for every node's
// +0 line and +132..+148 line at once (up to kMaxNodes), so the misses overlap each other and the TickAnim calls
// instead of each one waiting for its own. Nothing is stored: prefetches change no guest-visible state, so the result
// is the game's code bit for bit.
//
// LO_NATIVE_ANIM_TICK_PREFETCH=0 calls the game's code alone; 1 (default) prefetches; 2 prefetches and logs, every
// 65,536 calls, how many nodes the calls had and how many were over the cap.
#include <stdafx.h>
#include <kernel/memory.h>
#include <os/logger.h>
#include <algorithm>
#include <cstdlib>

extern "C" PPC_FUNC(__imp__sub_822B34A0);

namespace
{
    constexpr uint32_t kTickArray = 652;
    constexpr uint32_t kTickCount = 656;
    constexpr uint32_t kNodeFlags = 132;   // +132 flags, +136 tick tag, +144 weight, +148 node total weight
    constexpr uint32_t kNodeTotalWeight = 148;
    constexpr int32_t kMaxNodes = 48;

    int NativeAnimTickPrefetchMode()
    {
        static const int mode = []()
            {
                const char* value = std::getenv("LO_NATIVE_ANIM_TICK_PREFETCH");
                return value ? std::atoi(value) : 1;
            }();
        return mode;
    }

    uint32_t LoadU32(uint8_t* base, uint32_t address)
    {
        return ByteSwap(*reinterpret_cast<uint32_t*>(base + address));
    }

    // Returns how many nodes the component ticks.
    int32_t PrefetchNodes(uint8_t* base, uint32_t component)
    {
        const uint32_t nodes = LoadU32(base, component + kTickArray);
        const int32_t count = int32_t(LoadU32(base, component + kTickCount));
        if (nodes == 0)
            return 0;
        const int32_t prefetched = std::min(count, kMaxNodes);
        for (int32_t i = 0; i < prefetched; ++i)
        {
            const uint32_t node = LoadU32(base, nodes + uint32_t(i) * 4);
            if (node == 0)
                continue;
            __builtin_prefetch(base + node, 0, 3);
            __builtin_prefetch(base + node + kNodeFlags, 1, 3);
            __builtin_prefetch(base + node + kNodeTotalWeight, 1, 3);
        }
        return count;
    }

    void CountNodes(int32_t count)
    {
        static uint64_t calls, nodes, overCap;
        ++calls;
        nodes += uint64_t(std::max(count, 0));
        overCap += count > kMaxNodes;
        if ((calls & 0xFFFF) == 0)
            LOG_ERROR("[anim tick prefetch] {} calls, {} nodes, {} calls over {} nodes", calls, nodes, overCap, kMaxNodes);
    }
}

// r3 = the skeletal mesh component, f1 = delta time. The game's code runs unchanged after the prefetches.
PPC_FUNC(sub_822B34A0)
{
    const int mode = NativeAnimTickPrefetchMode();
    if (mode != 0)
    {
        const int32_t count = PrefetchNodes(base, ctx.r3.u32);
        if (mode == 2)
            CountNodes(count);
    }
    __imp__sub_822B34A0(ctx, base);
}
