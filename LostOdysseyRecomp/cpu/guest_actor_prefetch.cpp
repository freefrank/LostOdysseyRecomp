// Prefetch for the game's actor iterator (sub_8229B788, FActorIterator::operator++ over GWorld->Levels[]->Actors). The
// actor tick (sub_8229B590) and UWorld::Tick (sub_82299818) walk every actor of every loaded level through it, and in a
// busy street most of its time (1.4% of Guest Main, 80% of it on one 16-byte line) is the cache miss on actor+112, the
// first field it reads of each actor; the tick loop then misses again on actor+8 and +87 (sub_8229B590 0.6%).
//
// The game's code still does all the work, unchanged. Before each call this hook asks the memory system for the actors
// that come next in the same level's array, `ahead` entries in advance: the lines holding +0..+15 (vtable, object
// flags), +72..+87 (components array and count, tick group) and +112 (actor flags). Halfway there, when those lines
// should have arrived, it also asks for the components array of each actor the tick loop won't skip (actor+112 bit
// 0x2000000 clear), which sub_8229C100 walks next. Nothing is stored: prefetches change no guest-visible state, so the
// result is the game's code bit for bit, whatever the prefetches pick.
//
// LO_NATIVE_ACTOR_PREFETCH=0 calls the game's code alone; 1 (default) prefetches; 2 prefetches and logs, every 65,536
// calls, how many of the actors the iterator returned had been prefetched. LO_NATIVE_ACTOR_PREFETCH_AHEAD sets the
// distance in actors (default 8, 2..64).
#include <stdafx.h>
#include <kernel/memory.h>
#include <os/logger.h>
#include <algorithm>
#include <cstdlib>

extern "C" PPC_FUNC(__imp__sub_8229B788);

namespace
{
    constexpr uint32_t kWorld = 0x83318744;       // GWorld
    constexpr uint32_t kWorldLevels = 68;         // TArray<ULevel*>: data, count
    constexpr uint32_t kWorldLevelCount = 72;
    constexpr uint32_t kLevelActors = 60;         // TArray<AActor*>: data, count
    constexpr uint32_t kLevelActorCount = 64;
    constexpr uint32_t kIteratorActor = 0;        // index of the current actor
    constexpr uint32_t kIteratorLevel = 4;        // index of the current level
    constexpr uint32_t kIteratorEnd = 8;
    constexpr uint32_t kIteratorCurrent = 16;     // the actor it returns
    constexpr uint32_t kActorComponents = 72;
    constexpr uint32_t kActorComponentCount = 76;
    constexpr uint32_t kActorTickGroup = 87;
    constexpr uint32_t kActorFlags = 112;
    constexpr uint32_t kActorStatic = 0x2000000;  // sub_8229B590 skips these
    constexpr int32_t kMaxAhead = 64;

    int NativeActorPrefetchMode()
    {
        static const int mode = []()
            {
                const char* value = std::getenv("LO_NATIVE_ACTOR_PREFETCH");
                return value ? std::atoi(value) : 1;
            }();
        return mode;
    }

    int32_t AheadDistance()
    {
        static const int32_t ahead = []()
            {
                const char* value = std::getenv("LO_NATIVE_ACTOR_PREFETCH_AHEAD");
                return value ? std::clamp(std::atoi(value), 2, int(kMaxAhead)) : 8;
            }();
        return ahead;
    }

    uint32_t LoadU32(uint8_t* base, uint32_t address)
    {
        return ByteSwap(*reinterpret_cast<uint32_t*>(base + address));
    }

    void Prefetch(uint8_t* base, uint32_t address)
    {
        __builtin_prefetch(base + address, 0, 3);
    }

    // What has been asked for already in the array the iterator is in, so each actor is prefetched once.
    struct Window
    {
        uint32_t iterator = 0;
        uint32_t actors = 0;
        int32_t last = 0;      // the iterator's actor index at the previous call
        int32_t lines = 0;     // actors below this index had their lines prefetched
        int32_t components = 0; // and below this one their components array
        int32_t start = 0;     // first index prefetched since the window was reset
    };

    thread_local Window t_window;

    struct Stats
    {
        uint64_t calls = 0, returned = 0, prefetched = 0;
    };

    // Returns the actor array the iterator walks and the current index, or false when it's at the end.
    bool CurrentArray(uint8_t* base, uint32_t iterator, uint32_t& actors, int32_t& index, int32_t& count)
    {
        if (LoadU32(base, iterator + kIteratorEnd) != 0)
            return false;
        const uint32_t world = LoadU32(base, kWorld);
        if (world == 0)
            return false;
        const int32_t level = int32_t(LoadU32(base, iterator + kIteratorLevel));
        if (level < 0 || level >= int32_t(LoadU32(base, world + kWorldLevelCount)))
            return false;
        const uint32_t levelObject = LoadU32(base, LoadU32(base, world + kWorldLevels) + uint32_t(level) * 4);
        if (levelObject == 0)
            return false;
        actors = LoadU32(base, levelObject + kLevelActors);
        count = int32_t(LoadU32(base, levelObject + kLevelActorCount));
        index = int32_t(LoadU32(base, iterator + kIteratorActor));
        return actors != 0;
    }

    void PrefetchAhead(uint8_t* base, uint32_t iterator)
    {
        Window& window = t_window;
        uint32_t actors;
        int32_t index, count;
        if (!CurrentArray(base, iterator, actors, index, count))
            return;

        const int32_t first = std::max(index + 1, 0);
        if (window.iterator != iterator || window.actors != actors || index < window.last)
        {
            window.iterator = iterator;
            window.actors = actors;
            window.lines = window.components = window.start = first;
        }
        window.last = index;

        // The actors' own lines, `ahead` entries in advance.
        const int32_t ahead = AheadDistance();
        const int32_t linesEnd = std::min(first + ahead, count);
        for (int32_t i = std::max(window.lines, first); i < linesEnd; ++i)
        {
            const uint32_t actor = LoadU32(base, actors + uint32_t(i) * 4);
            if (actor == 0)
                continue;
            Prefetch(base, actor);
            Prefetch(base, actor + kActorComponents);
            Prefetch(base, actor + kActorTickGroup);
            Prefetch(base, actor + kActorFlags);
        }
        window.lines = std::max(window.lines, linesEnd);

        // Half as far ahead, the actor's lines should be in: its components array, if the tick loop will walk it.
        const int32_t componentsEnd = std::min(first + ahead / 2, count);
        for (int32_t i = std::max(window.components, first); i < componentsEnd; ++i)
        {
            const uint32_t actor = LoadU32(base, actors + uint32_t(i) * 4);
            if (actor == 0 || (LoadU32(base, actor + kActorFlags) & kActorStatic) != 0)
                continue;
            const uint32_t components = LoadU32(base, actor + kActorComponents);
            const int32_t componentCount = int32_t(LoadU32(base, actor + kActorComponentCount));
            if (components == 0 || componentCount <= 0)
                continue;
            Prefetch(base, components);
            if (componentCount > 16)
                Prefetch(base, components + 64);
        }
        window.components = std::max(window.components, componentsEnd);
    }

    // Mode 2: was the actor the game's code returned prefetched by an earlier call (`before` is the window then)?
    void CountReturned(uint8_t* base, uint32_t iterator, const Window& before)
    {
        static Stats stats;
        ++stats.calls;
        uint32_t actors;
        int32_t index, count;
        if (LoadU32(base, iterator + kIteratorCurrent) != 0 && CurrentArray(base, iterator, actors, index, count))
        {
            ++stats.returned;
            if (before.iterator == iterator && before.actors == actors && index >= before.start && index < before.lines)
                ++stats.prefetched;
        }
        if ((stats.calls & 0xFFFF) == 0)
            LOG_ERROR("[actor prefetch] {} calls, {} actors returned, {} of them prefetched by an earlier call", stats.calls,
                stats.returned, stats.prefetched);
    }
}

// r3 = the iterator. The game's code runs unchanged after the prefetches.
PPC_FUNC(sub_8229B788)
{
    const int mode = NativeActorPrefetchMode();
    if (mode == 0)
    {
        __imp__sub_8229B788(ctx, base);
        return;
    }

    const uint32_t iterator = ctx.r3.u32;
    const Window before = t_window;
    PrefetchAhead(base, iterator);
    __imp__sub_8229B788(ctx, base);
    if (mode == 2)
        CountReturned(base, iterator, before);
}
