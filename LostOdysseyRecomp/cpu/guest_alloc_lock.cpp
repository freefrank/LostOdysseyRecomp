// The lock of the game's allocator proxy (sub_822958F8, an FScopeLock constructor): it busy-spins on
// RtlTryEnterCriticalSection through the import bridge, with no yield, counting each failed try at 0x83315FDC. In a busy
// street that was about 3% of Guest Main. Here the try is the same compare-and-swap without the bridge, a short spin
// follows, and then the address-arbiter wait that RtlEnterCriticalSection uses. LO_NATIVE_ALLOC_LOCK=0: the game's code.
#include <stdafx.h>
#include <cpu/ppc_context.h>
#include <kernel/critical_section.h>
#include <kernel/memory.h>
#include <xbox.h>
#include <cstdlib>

extern "C" PPC_FUNC(__imp__sub_822958F8);

namespace
{
    constexpr uint32_t kFailedTries = 0x83315FDC;
    constexpr uint32_t kSpinTries = 64;

    bool NativeAllocLock()
    {
        static const bool enabled = []()
            {
                const char* value = std::getenv("LO_NATIVE_ALLOC_LOCK");
                return !(value && value[0] == '0');
            }();
        return enabled;
    }
}

// this = r3, section owner = r4 (the critical section is at r4 + 4). Stores r4 at this + 0, returns this.
PPC_FUNC(sub_822958F8)
{
    if (!NativeAllocLock())
    {
        __imp__sub_822958F8(ctx, base);
        return;
    }
    const uint32_t self = ctx.r3.u32, owner = ctx.r4.u32;
    PPC_STORE_U32(self, owner);
    auto* section = reinterpret_cast<XRTL_CRITICAL_SECTION*>(base + owner + 4);
    const uint32_t thread = ctx.r13.u32;
    if (!GuestCriticalSection::TryEnter(section->RecursionCount, section->OwningThread, thread))
    {
        uint32_t failed = 1;
        bool entered = false;
        for (uint32_t spin = 0; spin < kSpinTries && !entered; ++spin, ++failed)
        {
#if defined(__aarch64__)
            __asm__ volatile("yield");
#endif
            entered = GuestCriticalSection::TryEnter(section->RecursionCount, section->OwningThread, thread);
        }
        PPC_STORE_U32(kFailedTries, PPC_LOAD_U32(kFailedTries) + failed);
        if (!entered)
            GuestCriticalSection::Enter(section->RecursionCount, section->OwningThread, thread);
    }
    ctx.r3.u32 = self;
}
