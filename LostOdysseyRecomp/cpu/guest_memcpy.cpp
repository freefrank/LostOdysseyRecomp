// The game's memcpy (sub_82B7A0B0) with the host's. Copying bytes needs no byte swapping, and the recompiled
// PowerPC routine took 3.7% of the game's main thread in the first battle (and 1.2% of its render thread).
// memmove, as a guest copy that overlaps forwards would otherwise behave differently from the guest's routine.
#include <stdafx.h>
#include <cstring>

PPC_FUNC(sub_82B7A0B0)
{
    const uint32_t size = ctx.r5.u32;
    if (size)
        std::memmove(base + ctx.r3.u32, base + ctx.r4.u32, size);
    // r3 stays the destination, as the guest routine returns it.
}
