#include <stdafx.h>
#include <os/logger.h>
#include "no_encounters.h"
#include "battle_tour.h"
#include <settings/config.h>
#include <atomic>
#include <bit>
#include <cstdlib>
#include <cstring>

extern "C" PPC_FUNC(__imp__sub_829E3048);
extern "C" PPC_FUNC(__imp__sub_829E3268);

namespace
{
    std::atomic<settings::EncounterRate>& Requested()
    {
        static std::atomic<settings::EncounterRate> requested{settings::GetConfig().encounterRate};
        return requested;
    }

    std::atomic<uint32_t> halfDraws{0};

    // Not persisted: a player must not reboot into a battle on every step.
    // LO_DEBUG_ENCOUNTER_EVERY_STEP=1 turns it on at startup for harnesses and
    // leaves the last persisted Normal/Half/None preference untouched.
    std::atomic<bool>& EveryStep()
    {
        static std::atomic<bool> everyStep{[] {
            const char* env = std::getenv("LO_DEBUG_ENCOUNTER_EVERY_STEP");
            const bool on = env && std::strcmp(env, "1") == 0;
            return on;
        }()};
        return everyStep;
    }
}

settings::EncounterRate debug_menu::GetEncounterRate()
{
    return Requested().load(std::memory_order_relaxed);
}

void debug_menu::SetEncounterRate(settings::EncounterRate rate)
{
    const bool wasEveryStep = EveryStep().exchange(false, std::memory_order_relaxed);
    rate = settings::ValidEncounterRate(rate);
    if (Requested().exchange(rate, std::memory_order_relaxed) != rate || wasEveryStep)
        halfDraws.store(0, std::memory_order_relaxed);
    if (!settings::SaveEncounterRate(rate))
        LOG_WARNING("debug menu: failed to persist encounter rate setting");
    LOG_INFO("debug menu: encounter rate {}", uint32_t(rate));
    if (wasEveryStep)
        LOG_INFO("debug menu: encounter every step false");
}

bool debug_menu::EncounterEveryStepEnabled()
{
    return EveryStep().load(std::memory_order_relaxed);
}

void debug_menu::SetEncounterEveryStepEnabled(bool enabled)
{
    EveryStep().store(enabled, std::memory_order_relaxed);
    LOG_INFO("debug menu: encounter every step {}", enabled);
}

// The walking encounter update sub_829E3048 accumulates the distance walked
// and asks this draw (field player vtable +1636) whether to fight; on 1 it
// requests the battle through 0x828278A0. Keep the draw and its counter/
// threshold updates, and only turn its result into "no battle" at that call
// site; an early return from sub_829E3048 would also skip the distance update.
// A battle tour formation replaces the draw's result; the walking update reads
// the formation for RequestBattle from the slot r6 points to.
PPC_FUNC(sub_829E3268)
{
    constexpr uint32_t WalkingEncounterReturn = 0x829E31F8;
    const uint32_t caller = static_cast<uint32_t>(ctx.lr);
    const uint32_t formationSlot = ctx.r6.u32;
    __imp__sub_829E3268(ctx, base);
    if (caller != WalkingEncounterReturn)
        return;
    int32_t formation = -1;
    if (debug_menu::battle_tour::Take(base, formation))
    {
        PPC_STORE_U32(formationSlot, static_cast<uint32_t>(formation));
        ctx.r3.u64 = 1;
        LOG_INFO("battle tour: formation {} drawn", formation);
        return;
    }
    if (ctx.r3.u32 != 1 || EveryStep().load(std::memory_order_relaxed))
        return;
    const auto rate = Requested().load(std::memory_order_relaxed);
    // The original draw has already reset its distance and threshold. Count
    // only successful walking draws: skip the first, allow the second, repeat.
    // A tour draw and Encounter Every Step bypass this counter entirely.
    if (rate == settings::EncounterRate::None ||
        (rate == settings::EncounterRate::Half && (halfDraws.fetch_add(1, std::memory_order_relaxed) & 1) == 0))
        ctx.r3.u64 = 0;
}

// Encounter Every Step: satisfy the distance and step-count conditions before
// each walking update so the game's own draw picks a formation and requests
// the battle; areas without an encounter table (formation -1) never fight.
// Encounter state at 0x832C95C0: +564 step threshold (the draw needs the step
// count +568 above it), +576 minimum and +580 accumulated distance since the
// last battle. Player +1616 is the walked distance compared with the step
// length before each draw; +2252 is the pawn (no pawn: no update).
PPC_FUNC(sub_829E3048)
{
    const bool tour = debug_menu::battle_tour::Pending();
    if (!EveryStep().load(std::memory_order_relaxed) && !tour)
    {
        __imp__sub_829E3048(ctx, base);
        return;
    }

    constexpr uint32_t Encounter = 0x832C95C0;
    const uint32_t player = ctx.r3.u32;
    if (!PPC_LOAD_U32(player + 2252))
    {
        __imp__sub_829E3048(ctx, base);
        return;
    }

    const uint32_t walked = PPC_LOAD_U32(player + 1616);
    const uint32_t threshold = PPC_LOAD_U32(Encounter + 564);
    const uint32_t distance = PPC_LOAD_U32(Encounter + 580);
    PPC_STORE_U32(player + 1616, 0x4E6E6B28); // 1e9f
    PPC_STORE_U32(Encounter + 564, 0xFFFFFFFF);
    PPC_STORE_U32(Encounter + 580, PPC_LOAD_U32(Encounter + 576));
    __imp__sub_829E3048(ctx, base);
    if (tour)
        debug_menu::battle_tour::RestoreStage(base);
    if (ctx.r3.u32 == 1 && !tour)
        return; // the draw already reset the threshold and distance

    PPC_STORE_U32(Encounter + 564, threshold);
    PPC_STORE_U32(Encounter + 580, distance);
    // An early-out before the step-length check leaves the forced distance.
    if (std::bit_cast<float>(PPC_LOAD_U32(player + 1616)) >= 1e8f)
        PPC_STORE_U32(player + 1616, walked);
}
