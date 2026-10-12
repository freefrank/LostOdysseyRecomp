#include <stdafx.h>

// #375: BGM stops changing after hours of play until a battle or a restart.
// The sound system has six stream slots (sound system + 3580, see
// sub_82321720 kind 3). A slot is freed when its stream ends only if the
// stream carries the auto-release flag (slot + 5 bit 1, sub_82B62610); stop
// mode 2 sets it (sub_82B62048). The menu plays the item jingle
// (s04_item.xwv) through sub_82B61320 with flags 0 and forgets the handle as
// soon as IsPlaying (sub_8236C728) is <= 0 (sub_822E0E10, 0x822E0F48). It
// never stops the jingle, so the slot stays taken until the next battle frees
// every stream; area changes do not. With five slots held this way, the next
// area's BGM cannot start and the old track keeps playing.
//
// Start the jingle with the auto-release flag, as if stop mode 2 had been
// requested: it still plays to its end, then the sound thread frees the slot.
// The flag bit is otherwise unused by the stream manager (channel + 92 tests
// bits 0, 8 and 9-19 only).

extern "C" PPC_FUNC(__imp__sub_82B61320);

namespace
{
constexpr uint32_t ItemJingleReturn = 0x822E0EE8; // after bl sub_82B61320 in sub_822E0E10
constexpr uint32_t AutoRelease = 2;
}

// sub_82B61320(sound system, path, flags, params): plays a stream of type 0.
PPC_FUNC(sub_82B61320)
{
    if (uint32_t(ctx.lr) == ItemJingleReturn)
        ctx.r5.u64 |= AutoRelease;
    __imp__sub_82B61320(ctx, base);
}
