// Prefetch for the actor's component tick (sub_8229C100, AActor::TickAuxiliary-like: for each component in
// actor->Components (+72, count +76) it reads the component's flags (+80) and tick group (+84 byte), then calls
// vtable +288 or queues it with sub_825F41E8). In a busy street it was 0.9% of Guest Main with nearly all of it on two
// 16-byte lines: the load of each component's +80/+84, a cache miss per component, taken one after another.
//
// The game's code still does all the work, unchanged. Before it runs, this hook asks the memory system for every
// component's vtable line and +80..+87 at once (up to kMaxComponents), so the misses overlap instead of following one
// another. (sub_8229B788's prefetch, guest_actor_prefetch.cpp, has usually brought the components array in already.)
// Nothing is stored: prefetches change no guest-visible state, so the result is the game's code bit for bit.
//
// LO_NATIVE_COMPONENT_PREFETCH=0 calls the game's code alone; 1 (default) prefetches; 2 prefetches and logs, every
// 65,536 calls, how many components the calls had and how many were over the cap.
#include <stdafx.h>
#include <kernel/memory.h>
#include <os/logger.h>
#include <algorithm>
#include <cstdlib>

extern "C" PPC_FUNC(__imp__sub_8229C100);

namespace
{
    constexpr uint32_t kActorComponents = 72;
    constexpr uint32_t kActorComponentCount = 76;
    constexpr uint32_t kComponentFlags = 80;
    constexpr uint32_t kComponentTickGroup = 84;
    constexpr int32_t kMaxComponents = 32;

    int NativeComponentPrefetchMode()
    {
        static const int mode = []()
            {
                const char* value = std::getenv("LO_NATIVE_COMPONENT_PREFETCH");
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

    // Returns how many components the actor has.
    int32_t PrefetchComponents(uint8_t* base, uint32_t actor)
    {
        const uint32_t components = LoadU32(base, actor + kActorComponents);
        const int32_t count = int32_t(LoadU32(base, actor + kActorComponentCount));
        if (components == 0)
            return 0;
        const int32_t prefetched = std::min(count, kMaxComponents);
        for (int32_t i = 0; i < prefetched; ++i)
        {
            const uint32_t component = LoadU32(base, components + uint32_t(i) * 4);
            if (component == 0)
                continue;
            Prefetch(base, component);
            Prefetch(base, component + kComponentFlags);
            Prefetch(base, component + kComponentTickGroup);
        }
        return count;
    }

    void CountComponents(int32_t count)
    {
        static uint64_t calls, components, overCap;
        ++calls;
        components += uint64_t(std::max(count, 0));
        overCap += count > kMaxComponents;
        if ((calls & 0xFFFF) == 0)
            LOG_ERROR("[component prefetch] {} calls, {} components, {} calls over {} components", calls, components,
                overCap, kMaxComponents);
    }
}

// r3 = the actor, f1 = delta time, r5 = tick type, r6 = the tick lists. The game's code runs unchanged after the
// prefetches.
PPC_FUNC(sub_8229C100)
{
    const int mode = NativeComponentPrefetchMode();
    if (mode != 0)
    {
        const int32_t count = PrefetchComponents(base, ctx.r3.u32);
        if (mode == 2)
            CountComponents(count);
    }
    __imp__sub_8229C100(ctx, base);
}
