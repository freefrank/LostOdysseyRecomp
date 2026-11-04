#pragma once

#include <cstdint>
#include <string_view>

// Battle intro skip: phase 0 is the script state bsSMpha_Start.PHASE_ON. Its timed
// holds are script Sleep calls, and the resources that its Wait calls need load at a
// rate per frame. The skip lets each statement and Wait run, ends each Sleep early and
// loads faster. Decision only; the hooks are in battle_intro_skip.cpp.
namespace battle_intro_skip
{
enum class StartClass
{
    Other,
    Start,
    StartNoVoice
};

// Formation +72, as the guest spawns it. bsSMpha_Start_OPCrowd waits for a
// matinee from the AI script, so it stays Other with every unknown name.
constexpr StartClass ClassifyStart(std::string_view name)
{
    if (name == "bsSMpha_Start") return StartClass::Start;
    if (name == "bsSMpha_StartNoVoice") return StartClass::StartNoVoice;
    return StartClass::Other;
}

// The guest state at a Sleep call (or at a battle tick for the phase actor).
struct SleepState
{
    bool enabled = false;
    uint32_t mode = 0;           // core +0x15CC
    uint32_t phase = 0;          // core +0x38
    uint32_t sleepingActor = 0;  // the actor that called Sleep
    uint32_t phaseActor = 0;     // *0x832CB560
    uint32_t phaseSlot = 0;      // *(0x832CB550 + 32), 1 is the intro
    uint32_t matinees = 0;       // *(0x832CBFB0 + 24)
    StartClass start = StartClass::Other;
    int32_t battleId = -1;       // manager +5784
};

// Battles that have their own camera or are not a formation stay as they are.
constexpr bool ExcludedBattleId(int32_t id)
{
    return id == -1 || id == 255 || id == 256;
}

constexpr bool CollapseSleep(const SleepState& s)
{
    return s.enabled && s.mode == 2 && s.phase == 0 && s.sleepingActor != 0 && s.sleepingActor == s.phaseActor &&
        s.phaseSlot == 1 && s.matinees == 0 && s.start != StartClass::Other && !ExcludedBattleId(s.battleId);
}

// The script polls the pawn load tasks in a Sleep loop, so that Sleep must give the
// loaders a tick. With no tasks left, a collapsed Sleep needs no tick.
constexpr bool SleepEndsThisTick(const SleepState& s, uint32_t pawnLoadTasks)
{
    return CollapseSleep(s) && pawnLoadTasks == 0;
}

// Resource queue passes per frame while the conditions of CollapseSleep hold for the
// phase actor. Package starts need a queue tick each; retail has one per frame.
constexpr int QueueTicksPerFrame = 8;

// The Sleep native sets this latent op (actor +24 -> +40); Wait sets actor +544 bit 31.
constexpr uint32_t SleepLatentOp = 384;
constexpr uint32_t WaitBit = 0x80000000;

// At the press, the Sleep that already runs must end too: set its time (actor +236)
// to 0. A Wait keeps its own condition.
constexpr bool EndRunningSleep(const SleepState& s, uint32_t latentOp, uint32_t waitBits)
{
    return CollapseSleep(s) && latentOp == SleepLatentOp && !(waitBits & WaitBit);
}

// Random gate. Only a battle that the field walking draw requested is random. Script
// opcodes, the world map, a retry (BATTLEREBOOT), the opening battle and the debug
// battle tour (which uses the walking draw with its own formation) are not.
struct BattleRequest
{
    int32_t id = -1;
    bool random = false;
};

// An accepted RequestBattle (sub_828278A0) replaces the record. tourId: the formation
// that the debug battle tour put into the walking draw, -1 if none.
constexpr BattleRequest RecordRequest(uint32_t id, bool walkingDraw, int32_t tourId)
{
    return {static_cast<int32_t>(id), walkingDraw && tourId != static_cast<int32_t>(id)};
}

// The intro actor spawns: the record is used once, and only for the battle it asked for.
constexpr bool RandomBattle(const BattleRequest& r, int32_t battleId)
{
    return r.random && r.id >= 0 && r.id == battleId;
}

// Skip battle animation, read at the spawn. The values of settings::BattleIntroSkip.
enum class Option : uint32_t
{
    Off,
    Press,
    Always
};

// The option, the random gate and the guest state for the intro actor, each tick.
struct WindowInput
{
    Option option = Option::Off;
    bool random = false;       // RandomBattle at the spawn
    SleepState state;          // the phase actor's state; enabled is not used
};

// The intro can be skipped now: the conditions of CollapseSleep for the phase actor.
constexpr bool WindowEligible(const WindowInput& in)
{
    SleepState s = in.state;
    s.enabled = true;
    return in.option != Option::Off && in.random && CollapseSleep(s);
}

// Visible: the latch is armed and the prompt shows (Press to skip only). A press or the
// end of phase 0 hides it at once.
constexpr bool WindowVisible(const WindowInput& in, bool pressed)
{
    return in.option == Option::Press && WindowEligible(in) && !pressed;
}

// Always: the skip starts at the first tick that the intro can be skipped, as a press
// at that time would. No prompt and no latch.
constexpr bool AutoSkip(const WindowInput& in, bool pressed)
{
    return in.option == Option::Always && WindowEligible(in) && !pressed;
}

// Early window (Press to skip). It opens at the accepted request of a random walking
// battle, before the encounter effect and before the intro actor exists. At the request
// the formation's start class (formation table +72) and the battle ID are known; the
// matinee state is checked again at the spawn (WindowEligible). When the early window
// ends without a spawn (a hold, a timeout, a drop), a random battle that still spawns
// opens the window again at its spawn.
constexpr bool EarlyWindowOpens(Option option, const BattleRequest& r, StartClass start)
{
    return option == Option::Press && r.random && r.id >= 0 && !ExcludedBattleId(r.id) &&
        start != StartClass::Other;
}

// Request to intro spawn, measured in walking battles: 280 to 310 ms at 60 fps. The loader
// picks the request 4 to 19 ms after it and then clears its battle slot. The window
// ends when the slot is empty before that pick (the request was cancelled), or when no
// battle spawns within EarlyWindowMaxMs (a map load, a reset, a lost battle).
constexpr uint64_t EarlyWindowMaxMs = 8000;
enum class EarlyEnd
{
    None,
    Gone,
    Timeout
};
constexpr EarlyEnd EarlyWindowEnds(bool picked, int32_t battleSlotPhase, uint64_t sinceRequestMs)
{
    if (!picked && battleSlotPhase <= 0) return EarlyEnd::Gone;
    if (sinceRequestMs >= EarlyWindowMaxMs) return EarlyEnd::Timeout;
    return EarlyEnd::None;
}

// A press in the early window is stored. It acts only for the battle it was made for
// (at the spawn the random record must match, RandomBattle). Like Always, it acts at the
// first battle tick in phase 0 that can skip; it is dropped when phase 0 ends first or
// when the battle is not this random Press battle.
enum class StoredPress
{
    None,
    Keep,   // phase 0, but the intro cannot be skipped yet
    Apply,  // StartSkip, the same path as Always
    Drop
};
constexpr StoredPress StoredPressAction(const WindowInput& in, bool stored)
{
    if (!stored) return StoredPress::None;
    if (in.option != Option::Press || !in.random || in.state.phase != 0) return StoredPress::Drop;
    return WindowEligible(in) ? StoredPress::Apply : StoredPress::Keep;
}

// The overlay hides the prompt when the game thread has not reported for this long
// (a pause, a load or a stall).
constexpr uint64_t PromptStaleMs = 250;
constexpr bool PromptFresh(uint64_t nowMs, uint64_t tickMs)
{
    return nowMs >= tickMs && nowMs - tickMs <= PromptStaleMs;
}

// The battle tick must run in each engine tick while the latch is armed or the skip is set.
// After this many engine ticks without one (the battle ended or the game left it), both end.
constexpr uint32_t MissedBattleTicks = 2;
constexpr bool BattleTicksLost(uint32_t missedEngineTicks)
{
    return missedEngineTicks >= MissedBattleTicks;
}

// Intro sounds. Every phase 0 sound starts at the battle sound manager entry
// sub_82B1ACA8. In a battle that can be skipped (WindowEligible), the entry records
// the manager handles until the press (Always: until its first tick). After it, the
// entry returns -1, the manager's own "no sound" value.
enum class SoundEntry
{
    Pass,
    Record,
    Block
};

constexpr SoundEntry IntroSoundEntry(const WindowInput& in, bool pressed)
{
    if (!WindowEligible(in)) return SoundEntry::Pass;
    return pressed ? SoundEntry::Block : SoundEntry::Record;
}

// Peak in a skipped intro: about 13 kind 1 sounds.
constexpr uint32_t MaxIntroSounds = 32;

// Low-level handle of the sound system *0x832D268C: slot | kind << 8 | generation << 12.
// Bits 20-27 select other paths. Kind 1 is 2D, 4 is a 3D emitter, 3 is a stream (music).
constexpr uint32_t SoundKind(uint32_t handle) { return (handle >> 8) & 0xF; }
constexpr uint8_t SoundGeneration(uint32_t handle) { return static_cast<uint8_t>(handle >> 12); }
constexpr bool FadableKind(uint32_t handle)
{
    return ((handle >> 20) & 0xFF) == 0 && (SoundKind(handle) == 1 || SoundKind(handle) == 4);
}

// The slot of a low-level handle: +0 state (0 free, 0xFF reserved), +1 generation,
// +3 bit 0x80 release running, +9 status (12 ended).
struct SoundSlot
{
    uint8_t state = 0;
    uint8_t generation = 0;
    uint8_t flags = 0;
    uint8_t status = 0;
};
constexpr uint8_t SlotEnded = 12;
constexpr uint8_t SlotReleasing = 0x80;

// sub_82B62048 does not check the generation: with a stale handle it fades the sound
// that now owns the slot. Read the slot just before each call.
constexpr bool SlotPlays(uint32_t handle, const SoundSlot& s)
{
    return s.state != 0 && s.state != 0xFF && s.generation == SoundGeneration(handle) && s.status != SlotEnded &&
        !(s.flags & SlotReleasing);
}

// A manager object (+4 state: 1 created, 2 playing, 3 ended; +24 low-level handle).
enum class PressAction
{
    None,  // ended, stale, or not a kind 1 or 4 sound: the manager ends it as retail
    End,   // not started: state 3, so the next manager tick deletes it without a start
    Fade   // sub_82B62048(sys, handle, 2, 0, IntroFadeSeconds)
};
constexpr float IntroFadeSeconds = 0.25f;

constexpr PressAction IntroSoundAtPress(uint8_t objectState, uint32_t handle, const SoundSlot& slot)
{
    if (objectState == 1) return PressAction::End;
    if (objectState == 2 && FadableKind(handle) && SlotPlays(handle, slot)) return PressAction::Fade;
    return PressAction::None;
}

// The hard-stop sweep (sub_82B1A518) runs this much guest time after the press. The
// guest clock also drives the fade, so fast-forward and pause move both alike.
constexpr uint64_t SweepAfterNs = 500000000;
constexpr bool SweepDue(uint64_t nowNs, uint64_t pressNs)
{
    return nowNs >= pressNs && nowNs - pressNs >= SweepAfterNs;
}

// The sweep stops every recorded object that still exists, but never a stream.
constexpr bool SweepStops(uint32_t handle)
{
    return SoundKind(handle) != 3;
}

// The records belong to one intro. Without a pending sweep they end with phase 0.
constexpr bool KeepIntroSounds(uint32_t phase, bool sweepPending)
{
    return phase == 0 || sweepPending;
}
}
