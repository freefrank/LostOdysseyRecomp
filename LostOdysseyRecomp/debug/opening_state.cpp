#include <stdafx.h>
#include <os/logger.h>
#include "opening_state.h"

namespace
{
    constexpr uint32_t BattleCore = 0x832ca0e8;
    constexpr uint32_t BattleId = 0x832cb778;

    bool RestoreNormalKaimResource(uint8_t* base, uint32_t core)
    {
        if (core != BattleCore) return false;
        const uint32_t playData = PPC_LOAD_U32(core + 0x20);
        if (!playData || PPC_LOAD_U32(playData + 0x80) != 11) return false;
        // The ordinary opening flow writes this in 82B003EC (82B00398).
        // Preset 11 lacks the later encounter's entry and attack sequences.
        PPC_STORE_U32(playData + 0x80, 0);
        return true;
    }
}

void debug_menu::CompleteOpeningResourceTransition(uint8_t* base, uint32_t core)
{
    // Direct victory bypasses the opening's intermediate AFI scene commands.
    // Preserve their verified Kaim resource transition before result handling.
    if (PPC_LOAD_U32(BattleId) <= 2 && RestoreNormalKaimResource(base, core))
        LOG_INFO("debug victory: completed opening Kaim resource transition 11 -> 0");
}

extern "C" PPC_FUNC(__imp__sub_82AF6290);
PPC_FUNC(sub_82AF6290)
{
    // Compatibility with an opening resource left in a post-opening save.
    // Restrict recovery to the reproduced Highlands of Wohl encounter; do not rewrite
    // intentionally selected resources in other scripted battles.
    if (PPC_LOAD_U32(BattleId) == 0x139 && RestoreNormalKaimResource(base, ctx.r3.u32))
        LOG_INFO("legacy opening state: restored Kaim resource 11 -> 0 for encounter 0x139");
    __imp__sub_82AF6290(ctx, base);
}
