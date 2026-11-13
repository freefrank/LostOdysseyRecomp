#include <stdafx.h>
#include <os/logger.h>
#include "encounter_defer.h"
#include "battle_intro_gate.h"
#include <chrono>

// Issue #114. The walking encounter (sub_829E3048) queues a battle in the
// controller tick through sub_828278A0 (loader slot 1 at 0x83263EA8, phase 1).
// In the same frame the box script's C1 touch query can still accept (its
// battle predicate sub_829E54F0 only sees committed battles), and DD clears
// the controller's control bit, sets the destroyed flag and opens the item
// message. The next loader pick (sub_8231F5E0) starts the battle and the field
// is unloaded. The message that would clear VM field 0x1010 is gone after the
// battle, so the box script waits at 0xCB forever with control still taken:
// Kaim cannot move and the menu does not open.
//
// Hold the pick instead, until the event gives control back (at most 15 s);
// the battle then starts right after the item message. C1 and the scripts are
// not touched.

extern "C" PPC_FUNC(__imp__sub_828278A0);
extern "C" PPC_FUNC(__imp__sub_82828698);
extern "C" PPC_FUNC(__imp__sub_8231F5E0);
PPC_FUNC(sub_82826E80);

namespace
{
constexpr uint32_t Loader = 0x832631F0;
constexpr uint32_t BattleSlot = Loader + 3256;
// Return address of `bl 0x828278A0` in the walking encounter sub_829E3048.
constexpr uint32_t WalkingEncounterReturn = 0x829E3220;
// Controller +0x5C8 bit set by sub_82A15868 (script DD) and cleared while a
// field event holds the player; also the bit the post-battle softlock leaves clear.
constexpr uint32_t PlayerControlBit = 0x08000000;

// Guest game thread only (controller tick, VM and loader run there).
encounter_defer::RandomRequest heldRequest;
std::chrono::steady_clock::time_point holdStart;

bool Address(uint32_t p) { return p >= 0x100000 && p < 0x7BFF0000 && !(p & 3); }

// Same path as debug/teleport.cpp: GEngine -> GamePlayers[0] -> controller.
// Unknown state counts as "has control" so a request is never held on a guess.
bool PlayerHasControl(uint8_t* base)
{
    const uint32_t engine = PPC_LOAD_U32(0x83315FB4);
    if (!Address(engine)) return true;
    const uint32_t players = PPC_LOAD_U32(engine + 0x2B8);
    if (!Address(players) || PPC_LOAD_U32(engine + 0x2BC) == 0) return true;
    const uint32_t player = PPC_LOAD_U32(players);
    if (!Address(player)) return true;
    const uint32_t controller = PPC_LOAD_U32(player + 0x40);
    if (!Address(controller)) return true;
    return (PPC_LOAD_U32(controller + 0x5C8) & PlayerControlBit) != 0;
}

encounter_defer::LoaderState ReadLoader(uint8_t* base)
{
    encounter_defer::LoaderState s;
    s.stage = PPC_LOAD_U8(Loader);
    s.current = PPC_LOAD_U8(Loader + 1);
    for (int slot = 0; slot < 6; ++slot)
        s.phase[slot] = static_cast<int32_t>(PPC_LOAD_U32(Loader + 3168 + 88 * slot));
    s.battleId = PPC_LOAD_U32(BattleSlot + 16);
    return s;
}
}

PPC_FUNC(sub_828278A0)
{
    const uint32_t caller = static_cast<uint32_t>(ctx.lr);
    const uint32_t id = ctx.r4.u32;
    static const bool traceRequests = std::getenv("LO_TRACE_BATTLE_INTRO") != nullptr;
    if (traceRequests)
        LOG_NOTICE("battle request: formation={} caller={:#x} walking={}", id, caller, caller == WalkingEncounterReturn);
    if (caller != WalkingEncounterReturn &&
        encounter_defer::DropForScriptedRequest(static_cast<int32_t>(PPC_LOAD_U32(BattleSlot)),
            PPC_LOAD_U32(BattleSlot + 16), heldRequest))
    {
        // Clear the held slot the way the game's own cancel (sub_82826F40) does.
        LOG_INFO("encounter defer: dropped held encounter {} for battle request {} from {:#x}", heldRequest.id, id, caller);
        const PPCContext saved = ctx;
        ctx.r3.u64 = BattleSlot;
        sub_82826E80(ctx, base);
        ctx = saved;
        heldRequest = {};
        battle_intro_skip::NoteRequestDropped();
    }
    __imp__sub_828278A0(ctx, base);
    if (ctx.r3.s32 == 0)
    {
        heldRequest = {caller == WalkingEncounterReturn, id, 0};
        // The battle intro skip prompt is only for the walking draw.
        battle_intro_skip::NoteBattleRequest(id, caller == WalkingEncounterReturn);
    }
}

// The other writer of slot 1 (stores phase 1 without the busy check). If it
// rewrote the slot, the request there is no longer the walking encounter.
PPC_FUNC(sub_82828698)
{
    uint32_t before[6];
    for (int i = 0; i < 6; ++i) before[i] = PPC_LOAD_U32(BattleSlot + 4 * i);
    __imp__sub_82828698(ctx, base);
    for (int i = 0; i < 6; ++i)
        if (PPC_LOAD_U32(BattleSlot + 4 * i) != before[i])
        {
            heldRequest = {};
            battle_intro_skip::NoteSlotRewritten();
            break;
        }
}

PPC_FUNC(sub_8231F5E0)
{
    if (ctx.r3.u32 == Loader && heldRequest.active)
    {
        const auto state = ReadLoader(base);
        const auto now = std::chrono::steady_clock::now();
        if (heldRequest.heldPicks == 0) holdStart = now;
        const auto heldMs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now - holdStart).count());
        if (encounter_defer::HoldPick(state, heldRequest, PlayerHasControl(base), heldMs))
        {
            // The first pick is always skipped; a later one is a hold for a field event.
            if (heldRequest.heldPicks > 0)
                battle_intro_skip::NoteRequestHeld();
            if (heldRequest.heldPicks++ == 1)
                LOG_INFO("encounter defer: holding encounter {} while a field event has player control", heldRequest.id);
            ctx.r3.u64 = 0; // the original's result when it starts nothing
            return;
        }
        // Stop tracking once the loader takes the request or it is gone.
        if (encounter_defer::PicksBattle(state) || state.phase[1] != 1 || state.battleId != heldRequest.id)
        {
            if (heldRequest.heldPicks > 1 && encounter_defer::PicksBattle(state))
                LOG_INFO("encounter defer: starting encounter {} after {} held picks ({} ms)", heldRequest.id, heldRequest.heldPicks, heldMs);
            if (encounter_defer::PicksBattle(state) && state.battleId == heldRequest.id)
                battle_intro_skip::NoteBattlePicked(heldRequest.id);
            heldRequest.active = false;
        }
    }
    __imp__sub_8231F5E0(ctx, base);
}
