// Switch: run the game's two XAudio worker threads above the game threads.
// The audio driver thread (priority 0x2C) calls the guest audio callback,
// which signals these workers and waits for them to mix the voices. Guest
// thread priorities are not mapped on the Switch (KeSetBasePriorityThread is
// a no-op there), so the workers run at libnx's default 0x3B and share the
// cores with every game thread; when cutscene video decoding keeps the cores
// busy the callback waits too long and the audio driver misses ticks (heard
// as stutter). LO_XAUDIO_WORKER_PRIORITY=<hex> overrides, 0 keeps 0x3B.
#include <stdafx.h>

#if LO_PLATFORM_SWITCH

#include <os/logger.h>
#include <os/switch_platform.h>
#include <cstdlib>
#include <utility>

extern "C" PPC_FUNC(__imp__sub_82CC4D98);
extern "C" PPC_FUNC(__imp__sub_82CC4E60);
extern "C" PPC_FUNC(__imp__sub_82CC3B38);

namespace
{
int WorkerPriority()
{
    static const int priority = [] {
        const char* value = std::getenv("LO_XAUDIO_WORKER_PRIORITY");
        return value && *value ? int(std::strtol(value, nullptr, 16)) : 0x2D;
    }();
    return priority;
}

void Raise(const char* which)
{
    const int priority = WorkerPriority();
    if (priority < 0x2C || priority > 0x3F) return;
    os::switch_platform::SetCurrentThreadPriority(priority);
    LOG_INFO("xaudio: {} worker thread priority {:#x}", which, priority);
}
}

// Thread entry functions (guest threads "82cc4d98" and "82cc4e60"); they loop
// until the audio engine shuts down.
PPC_FUNC(sub_82CC4D98)
{
    Raise("first");
    __imp__sub_82CC4D98(ctx, base);
}

PPC_FUNC(sub_82CC4E60)
{
    Raise("second");
    __imp__sub_82CC4E60(ctx, base);
}

// One XAudio worker instead of one per hardware thread in the game's processor mask (LO_XAUDIO_SINGLE_WORKER=0: one
// per hardware thread, as before).
// sub_82CC3B38 turns that mask into 8 bytes: [0] and [1] the number of processors, [2 + n] the role of hardware
// thread n (2 a helper, 1 the one that leads each mixing pass, sub_82CC4D98). The engine (sub_82CC5128) starts a
// thread per role and counts them, and its memory size (sub_82CC42A8) follows the count. Each pass splits its stages
// between the workers, which take the next voice from a shared list and meet at a spinning barrier (sub_82CC3C50)
// after every stage, about 750 times a second; on the Switch that coordination (barrier spin, lock waits, wakeups)
// was a fifth of the two workers' time. Keeping only the leader's role gives one worker and a count of one: the same
// passes in the list's order, as the game would run with a one-thread mask. Busy street: 26.2% to 22.9% of a core for
// the mixing, 27.08 to 27.48 FPS (two runs each, alternated).
PPC_FUNC(sub_82CC3B38)
{
    static const bool single = [] {
        const char* value = std::getenv("LO_XAUDIO_SINGLE_WORKER");
        return !value || value[0] != '0';
    }();
    const uint32_t out = ctx.r4.u32;
    __imp__sub_82CC3B38(ctx, base);
    if (!single) return;
    bool leader = false;
    for (uint32_t n = 0; n < 6; ++n)
        leader = leader || PPC_LOAD_U8(out + 2 + n) == 1;
    if (!leader) return;
    for (uint32_t n = 0; n < 6; ++n)
        if (PPC_LOAD_U8(out + 2 + n) != 1) PPC_STORE_U8(out + 2 + n, 0);
    PPC_STORE_U8(out + 0, 1);
    PPC_STORE_U8(out + 1, 1);
    static bool logged = false;
    if (!std::exchange(logged, true)) LOG_NOTICE("xaudio: one mixing worker (LO_XAUDIO_SINGLE_WORKER)");
}

#endif
