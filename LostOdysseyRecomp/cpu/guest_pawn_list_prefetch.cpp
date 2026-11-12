// Prefetch for the pawn-list scan in sub_829FCCC8 (a per-NPC query: walk the list at r3 (head +0, count +8, next pawn
// at +608), skip pawns with +516 bit 31 set, test each one's distance from +248..+256 against +540, and call
// sub_829FCB98 for the near ones). Every NPC walks the whole list each frame, so a crowd makes it O(N^2); in a busy
// street it was 1.9% of Guest Main, three quarters of that on the loads of each pawn's +516 and +608: a chain of cache
// misses, each next pointer known only when the previous pawn has arrived.
//
// The game's code still does all the work, unchanged. This hook remembers which pawns the last walk of each list went
// through and, before the next call, asks the memory system for all of them at once (their +248..+259, +516..+543 and
// +608 lines), so the walk that follows finds them in cache instead of waiting for each in turn. After every 8th call of
// a list it walks the list again (now in cache) to refresh what it remembers; pawns come and go much more slowly than
// NPCs query. A pawn that left the list since only costs a useless prefetch, one that joined is not prefetched. Nothing is stored in guest memory: prefetches change no guest-visible state, so the result is the game's
// code bit for bit.
//
// LO_NATIVE_PAWN_PREFETCH=0 calls the game's code alone; 1 (default) prefetches; 2 prefetches, walks the list after
// every call and logs, every 65,536 calls, how many pawns the walks went through and how many had been prefetched.
#include <stdafx.h>
#include <kernel/memory.h>
#include <os/logger.h>
#include <algorithm>
#include <cstdlib>

extern "C" PPC_FUNC(__imp__sub_829FCCC8);

namespace
{
    constexpr uint32_t kListHead = 0;
    constexpr uint32_t kListCount = 8;
    constexpr uint32_t kPawnLocation = 248;     // X, Y, Z
    constexpr uint32_t kPawnLocationEnd = 259;
    constexpr uint32_t kPawnFlags = 516;
    constexpr uint32_t kPawnRadiusEnd = 543;    // +540, the radius it tests against
    constexpr uint32_t kPawnNext = 608;
    constexpr uint32_t kMaxPawns = 256;
    constexpr uint32_t kLists = 4;
    constexpr uint32_t kRefreshCalls = 8;

    int NativePawnPrefetchMode()
    {
        static const int mode = []()
            {
                const char* value = std::getenv("LO_NATIVE_PAWN_PREFETCH");
                return value ? std::atoi(value) : 1;
            }();
        return mode;
    }

    uint32_t LoadU32(uint8_t* base, uint32_t address)
    {
        return ByteSwap(*reinterpret_cast<uint32_t*>(base + address));
    }

    void Prefetch(uint8_t* base, uint32_t address)
    {
        __builtin_prefetch(base + address, 0, 3);
    }

    // The pawns the last walk of one list went through, in order.
    struct History
    {
        uint32_t list = 0;
        uint32_t count = 0;
        uint32_t calls = 0;
        uint64_t used = 0;
        uint32_t pawns[kMaxPawns];
    };

    struct Histories
    {
        History lists[kLists];
        uint64_t clock = 0;

        History& Find(uint32_t list)
        {
            ++clock;
            History* oldest = &lists[0];
            for (History& history : lists)
            {
                if (history.list == list)
                {
                    history.used = clock;
                    return history;
                }
                if (history.used < oldest->used)
                    oldest = &history;
            }
            oldest->list = list;
            oldest->count = 0;
            oldest->calls = 0;
            oldest->used = clock;
            return *oldest;
        }
    };

    Histories& ThreadHistories()
    {
        thread_local Histories histories;
        return histories;
    }

    void PrefetchPawns(uint8_t* base, const History& history)
    {
        for (uint32_t i = 0; i < history.count; ++i)
        {
            const uint32_t pawn = history.pawns[i];
            Prefetch(base, pawn + kPawnLocation);
            Prefetch(base, pawn + kPawnLocationEnd);
            Prefetch(base, pawn + kPawnFlags);
            Prefetch(base, pawn + kPawnRadiusEnd);
            Prefetch(base, pawn + kPawnNext);
        }
    }

    // The same walk as the game's loop (stops at a null pawn or after `count` pawns), recording each pawn.
    uint32_t Walk(uint8_t* base, uint32_t list, uint32_t* pawns)
    {
        const int32_t count = int32_t(LoadU32(base, list + kListCount));
        uint32_t pawn = LoadU32(base, list + kListHead);
        uint32_t recorded = 0;
        for (int32_t i = 0; pawn != 0 && i < count && recorded < kMaxPawns; ++i)
        {
            pawns[recorded++] = pawn;
            pawn = LoadU32(base, pawn + kPawnNext);
        }
        return recorded;
    }

    // Mode 2: how many of the pawns the game's walk went through had been prefetched.
    void CountPrefetched(uint8_t* base, const History& history)
    {
        static uint64_t calls, walked, prefetched;
        static uint32_t pawns[kMaxPawns];
        const uint32_t count = Walk(base, history.list, pawns);
        ++calls;
        walked += count;
        for (uint32_t i = 0; i < count; ++i)
            prefetched += std::find(history.pawns, history.pawns + history.count, pawns[i]) != history.pawns + history.count;
        if ((calls & 0xFFFF) == 0)
            LOG_ERROR("[pawn prefetch] {} calls, {} pawns walked, {} of them prefetched", calls, walked, prefetched);
    }
}

// r3 = the pawn list, r4 = the querying pawn, r5 = the result. The game's code runs unchanged between the prefetches
// and the refresh.
PPC_FUNC(sub_829FCCC8)
{
    const int mode = NativePawnPrefetchMode();
    if (mode == 0)
    {
        __imp__sub_829FCCC8(ctx, base);
        return;
    }

    const uint32_t list = ctx.r3.u32;
    History& history = ThreadHistories().Find(list);
    PrefetchPawns(base, history);
    __imp__sub_829FCCC8(ctx, base);
    if (mode == 2)
        CountPrefetched(base, history);
    // A call made from inside the game's code may have taken this slot for another list: then leave it to that one.
    if (history.list == list && (history.count == 0 || ++history.calls % kRefreshCalls == 0))
        history.count = Walk(base, list, history.pawns);
}
