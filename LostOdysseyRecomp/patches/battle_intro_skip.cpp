// Battle intro skip. The skip shortens phase 0, the UnrealScript state
// bsSMpha_Start.PHASE_ON. The Debug Menu option Skip battle animation (battle_intro_skip
// in settings.ini, Off by default) controls it; Off, the game runs as retail.
// With Press to skip, a random walking battle opens a prompt window at its request (the
// encounter effect starts on the field about 0.3 s later, then the intro actor spawns)
// that stays open until the press or the end of the intro. While the
// window is open, the input latch is armed, and a press of the game's cancel button starts
// the skip; a press before the intro actor spawns is stored and acts at the spawn. With
// Always, the skip starts at the first battle tick of the intro, with no prompt and no
// latch. The option and the random gate are read again when the intro actor spawns
// (sub_82390408).
// After the press (or the Always start), the script still runs each statement and each
// Wait still holds on its own condition:
// - Its Sleep calls end at once (next tick while pawn load tasks remain, because a
//   loop in the script polls them).
// - The resources that its Wait 19-22 hold for load faster. The resource queue
//   (sub_82385300) starts about one package per tick, and the engine's async package
//   tick gets few reads done per frame, so the load is bound to the frame count. The
//   queue ticks more than once per frame, and the package tick is the engine's own flush.
// - Its sounds: the sound manager entry sub_82B1ACA8 records the intro's sounds until
//   the press and blocks new ones after it. At the press the playing ones fade out
//   (0.25 s) and the ones not started end; 0.5 s of guest time later a hard stop ends
//   what is left. Music (a stream) and sounds after phase 0 stay as retail.
#include <stdafx.h>
#include <patches/battle_intro_skip.h>
#include <patches/battle_intro_gate.h>
#include <host_ui/host_ui.h>
#include <debug/battle_intro_skip_setting.h>
#include <debug/battle_tour.h>
#include <hid/hid.h>
#include <kernel/memory.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <string_view>

extern "C" PPC_FUNC(__imp__sub_82470968);
extern "C" PPC_FUNC(__imp__sub_8238F198);
extern "C" PPC_FUNC(__imp__sub_82390408);
extern "C" PPC_FUNC(__imp__sub_82385300);
extern "C" PPC_FUNC(__imp__sub_82322360);
extern "C" PPC_FUNC(__imp__sub_82B1ACA8);
extern "C" PPC_FUNC(__imp__sub_82B1A6E8);

static_assert(uint32_t(settings::BattleIntroSkip::Always) == uint32_t(battle_intro_skip::Option::Always));

namespace
{
constexpr uint32_t Core = 0x832CA0E8;
constexpr uint32_t Scene = Core + 0x15CC;
constexpr uint32_t SetMotion = 0x832CB550;
constexpr uint32_t PhaseActor = SetMotion + 16;
constexpr uint32_t ActorSlot = SetMotion + 32;
constexpr uint32_t BattleId = 0x832CB778;
constexpr uint32_t MatineeCount = 0x832CBFB0 + 24;
constexpr uint32_t PawnLoadTasks = SetMotion + 60;  // the list that GetRestPawnLoaderCount counts
constexpr uint32_t ResourceQueue = 0x832CC05C;
constexpr uint32_t FlushBlocked = 0x832C9920;       // sub_8237CC40 returns at once while it is set
// The formation table (debug/battle_tour.h); row +72 is the start class name.
using debug_menu::battle_tour::FormationArray;
using debug_menu::battle_tour::FormationBytes;
// The loader's battle slot phase (encounter_defer.cpp): above 0 while a request waits.
constexpr uint32_t BattleSlotPhase = 0x832631F0 + 3168 + 88;
// Battle sound manager: +24 list of SE objects, +28 count. Object +4 state (byte),
// +8 manager handle, +24 low-level handle in the sound system.
constexpr uint32_t SoundManager = 0x832CC0FC;
constexpr uint32_t SoundSystemPointer = 0x832D268C;

std::atomic<uint32_t> introActor{0};
std::atomic<battle_intro_skip::StartClass> introClass{battle_intro_skip::StartClass::Other};
// Set by the press: the Sleep calls of this battle's intro end and its loads run faster.
std::atomic<bool> introSkipLatched{false};

// Guest game thread only. The last accepted battle request, used once at the next spawn.
battle_intro_skip::BattleRequest lastRequest;
int32_t tourFormation = -1;

// Guest game thread only. The prompt window of the current battle.
struct Intro
{
    battle_intro_skip::Option option = battle_intro_skip::Option::Off;
    bool random = false;
    bool pressed = false;
    bool stored = false;  // a press of the early window, until the first tick that can skip
};
Intro intro;
// Guest game thread only. The early window: from the request to the intro spawn.
struct Early
{
    bool pending = false;
    bool stored = false;  // the press came before the spawn
    bool picked = false;  // the loader started the battle (and cleared its slot)
    int32_t id = -1;
    uint64_t requestMs = 0;
};
Early early;
// The prompt shows and the latch is armed.
bool windowVisible = false;
uint32_t battleTicks = 0;
uint32_t seenBattleTicks = 0;
uint32_t missedBattleTicks = 0;

// Guest game thread only. The sounds of the current intro (manager handles), from
// sub_82B1ACA8 until the press.
struct IntroSounds
{
    uint32_t handles[battle_intro_skip::MaxIntroSounds]{};
    uint32_t count = 0;
    uint64_t pressNs = 0;
    bool sweepPending = false;
};
IntroSounds sounds;

// Read by the host overlay: steady clock milliseconds, promptSince 0 while closed.
std::atomic<uint64_t> promptSince{0};
std::atomic<uint64_t> promptTick{0};
std::atomic<bool> promptCancelA{false};

uint64_t SteadyMs()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

// The game's cancel button for the skip press. The Confirmation button option B / A
// (config +4 bit 0x02000000, the chain of ConfigAddress in settings/menu.cpp) swaps the
// controller's confirm and cancel masks (ctrl +1356/+1360, sub_8282E9E8). At runtime the
// masks follow a change in the game's Settings at once, so the flag is read on each use.
// Called by hid::GetState on the guest thread.
uint16_t GuestCancelBit()
{
    uint8_t* base = g_memory.base;
    const uint32_t object = PPC_LOAD_U32(0x8326A068);
    const uint32_t storage = object ? PPC_LOAD_U32(object + 4) : 0;
    const uint32_t config = storage ? PPC_LOAD_U32(storage + 0x18) : 0;
    return config && (PPC_LOAD_U32(config + 4) & 0x02000000) ? XAMINPUT_GAMEPAD_A : XAMINPUT_GAMEPAD_B;
}
[[maybe_unused]] const bool cancelSourceSet = (hid::SetSkipCancelSource(GuestCancelBit), true);

bool SkipEnabled()
{
    return introSkipLatched.load(std::memory_order_relaxed);
}

battle_intro_skip::SleepState ReadState(uint8_t* base, uint32_t actor, uint32_t phaseActor)
{
    battle_intro_skip::SleepState state;
    state.enabled = SkipEnabled();
    state.mode = PPC_LOAD_U32(Scene);
    state.phase = PPC_LOAD_U32(Core + 0x38);
    state.sleepingActor = actor;
    state.phaseActor = phaseActor;
    state.phaseSlot = PPC_LOAD_U32(ActorSlot);
    state.matinees = PPC_LOAD_U32(MatineeCount);
    state.start = actor == introActor.load() ? introClass.load() : battle_intro_skip::StartClass::Other;
    state.battleId = static_cast<int32_t>(PPC_LOAD_U32(BattleId));
    return state;
}

bool LoadsAccelerated(uint8_t* base)
{
    if (!SkipEnabled()) return false;
    const uint32_t phaseActor = PPC_LOAD_U32(PhaseActor);
    return battle_intro_skip::CollapseSleep(ReadState(base, phaseActor, phaseActor));
}

// The start class of a guest FString (UTF-16 with its terminator), in a stack buffer.
battle_intro_skip::StartClass ClassifyFString(uint8_t* base, uint32_t fstring)
{
    using battle_intro_skip::StartClass;
    if (fstring < 0x100000) return StartClass::Other;
    const uint32_t data = PPC_LOAD_U32(fstring);
    const uint32_t count = PPC_LOAD_U32(fstring + 4);
    char text[64];
    if (data < 0x100000 || count == 0 || count > sizeof(text)) return StartClass::Other;
    for (uint32_t i = 0; i + 1 < count; ++i)
    {
        const uint16_t c = PPC_LOAD_U16(data + i * 2);
        text[i] = c >= 0x20 && c < 0x7F ? char(c) : '?';
    }
    return battle_intro_skip::ClassifyStart(std::string_view(text, count - 1));
}

// The guest clock (mftb). It also drives the sound worker, so the fade and the sweep
// follow fast-forward and pause alike.
uint64_t GuestNs()
{
    return host_ui::GetActiveGameTimeNs();
}

// The slot of a low-level handle: kind 1 at sys +660, kind 4 at +3860, kind 3 at +3580,
// 20 bytes each (sub_82321720). 0 for other kinds.
uint32_t SoundSlotAddress(uint8_t* base, uint32_t handle)
{
    const uint32_t system = PPC_LOAD_U32(SoundSystemPointer);
    const uint32_t index = handle & 0xFF;
    if (!system || ((handle >> 20) & 0xFF) != 0) return 0;
    switch (battle_intro_skip::SoundKind(handle))
    {
    case 1: return index < 128 ? system + 660 + index * 20 : 0;
    case 4: return index < 128 ? system + 3860 + index * 20 : 0;
    case 3: return index < 6 ? system + 3580 + index * 20 : 0;
    default: return 0;
    }
}

battle_intro_skip::SoundSlot ReadSoundSlot(uint8_t* base, uint32_t handle)
{
    const uint32_t slot = SoundSlotAddress(base, handle);
    if (!slot) return {};
    return {PPC_LOAD_U8(slot), PPC_LOAD_U8(slot + 1), PPC_LOAD_U8(slot + 3), PPC_LOAD_U8(slot + 9)};
}

// The live manager object with this handle, or 0.
uint32_t FindSound(uint8_t* base, uint32_t handle)
{
    const uint32_t list = PPC_LOAD_U32(SoundManager + 24);
    const uint32_t count = PPC_LOAD_U32(SoundManager + 28);
    for (uint32_t i = 0; list && i < count && i < 512; ++i)
    {
        const uint32_t object = PPC_LOAD_U32(list + i * 4);
        if (object && PPC_LOAD_U32(object + 8) == handle) return object;
    }
    return 0;
}

void ClearIntroSounds()
{
    sounds = {};
}

// sub_82B1A518(manager, handle): the game's hard stop. It deletes the object; its
// release checks the generation of the low-level handle.
void StopSound(PPCContext& ctx, uint8_t* base, uint32_t handle)
{
    ctx.r3.u64 = SoundManager;
    ctx.r4.u64 = handle;
    sub_82B1A518(ctx, base);
}

// At the press: fade what plays, end what has not started.
void FadeIntroSounds(PPCContext& ctx, uint8_t* base)
{
    using namespace battle_intro_skip;
    const PPCContext saved = ctx;
    for (uint32_t i = 0; i < sounds.count; ++i)
    {
        const uint32_t object = FindSound(base, sounds.handles[i]);
        if (!object) continue;  // it ended on its own
        const uint32_t handle = PPC_LOAD_U32(object + 24);
        const PressAction action = IntroSoundAtPress(PPC_LOAD_U8(object + 4), handle, ReadSoundSlot(base, handle));
        if (action == PressAction::End)
            PPC_STORE_U8(object + 4, 3);
        else if (action == PressAction::Fade)
        {
            ctx.r3.u64 = PPC_LOAD_U32(SoundSystemPointer);
            ctx.r4.u64 = handle;
            ctx.r5.u64 = 2;  // free the slot at the end of the ramp
            ctx.r6.u64 = 0;  // to silence
            ctx.f1.f64 = IntroFadeSeconds;
            sub_82B62048(ctx, base);
            // Message 14 was not queued (full ring): stop it hard.
            if (ctx.r3.u32 == 0)
                StopSound(ctx, base, sounds.handles[i]);
        }
    }
    ctx = saved;
    sounds.pressNs = GuestNs();
    sounds.sweepPending = sounds.count != 0;
}

// 0.5 s of guest time after the press: a hard stop of each recorded object that still
// exists. It ends a 3D emitter whose voice did not start yet and a lost fade.
void SweepIntroSounds(PPCContext& ctx, uint8_t* base)
{
    using namespace battle_intro_skip;
    const PPCContext saved = ctx;
    for (uint32_t i = 0; i < sounds.count; ++i)
    {
        const uint32_t object = FindSound(base, sounds.handles[i]);
        if (object && SweepStops(PPC_LOAD_U32(object + 24)))
            StopSound(ctx, base, sounds.handles[i]);
    }
    ctx = saved;
    sounds.sweepPending = false;
}

// Each battle tick: the sweep, and the end of the records.
void SoundTick(PPCContext& ctx, uint8_t* base)
{
    using namespace battle_intro_skip;
    if (!sounds.count && !sounds.sweepPending)
        return;
    if (sounds.sweepPending && SweepDue(GuestNs(), sounds.pressNs))
    {
        SweepIntroSounds(ctx, base);
        return;
    }
    if (!KeepIntroSounds(PPC_LOAD_U32(Core + 0x38), sounds.sweepPending))
        ClearIntroSounds();
}

// Opens or closes the prompt window. The latch is armed only while it is open.
void SetVisible(bool visible)
{
    if (windowVisible == visible) return;
    windowVisible = visible;
    hid::SetSkipArmed(visible);
    const uint64_t now = SteadyMs();
    promptTick = now;
    promptSince = visible ? std::max<uint64_t>(now, 1) : 0;
}

// The early window ends without a spawn: the stored press goes with it.
void CloseEarly()
{
    if (!early.pending) return;
    early = {};
    SetVisible(false);
}

// Each engine tick from the request to the spawn: the press, and the end without a battle.
void EarlyTick(uint8_t* base)
{
    using namespace battle_intro_skip;
    const uint64_t since = host_ui::GetActiveGameTimeMs() - early.requestMs;
    if (EarlyWindowEnds(early.picked, static_cast<int32_t>(PPC_LOAD_U32(BattleSlotPhase)), since))
    {
        CloseEarly();
        return;
    }
    if (windowVisible && hid::TakeSkipPress())
    {
        early.stored = true;
        SetVisible(false);
    }
    promptCancelA = GuestCancelBit() == XAMINPUT_GAMEPAD_A;
    if (windowVisible)
        promptTick = SteadyMs();
}

// The press, or the Always start: end the Sleep calls, load faster, fade the intro sounds.
void StartSkip(PPCContext& ctx, uint8_t* base, const battle_intro_skip::WindowInput& in, uint32_t phaseActor)
{
    intro.pressed = true;
    introSkipLatched = true;
    // The Sleep that runs now ends on its next poll; the hook ends the later ones.
    battle_intro_skip::SleepState state = in.state;
    state.enabled = true;
    const uint32_t frame = PPC_LOAD_U32(phaseActor + 24);
    const bool sleeping = frame >= 0x100000 &&
        battle_intro_skip::EndRunningSleep(state, PPC_LOAD_U32(frame + 40), PPC_LOAD_U32(phaseActor + 544));
    if (sleeping)
        PPC_STORE_U32(phaseActor + 236, 0);
    FadeIntroSounds(ctx, base);
}

// Each battle tick: window state, and the press.
void WindowTick(PPCContext& ctx, uint8_t* base)
{
    ++battleTicks;
    if (intro.option == battle_intro_skip::Option::Off)
        return;
    const uint32_t phaseActor = PPC_LOAD_U32(PhaseActor);
    const battle_intro_skip::WindowInput in{intro.option, intro.random, ReadState(base, phaseActor, phaseActor)};
    // The press of the early window acts at the first tick that can skip, as Always does.
    const auto stored = battle_intro_skip::StoredPressAction(in, intro.stored);
    if (stored != battle_intro_skip::StoredPress::Keep)
        intro.stored = false;
    if (stored == battle_intro_skip::StoredPress::Apply)
        StartSkip(ctx, base, in, phaseActor);
    if (!battle_intro_skip::WindowEligible(in))
    {
        // The skip belongs to this intro only.
        if (in.state.phase != 0) introSkipLatched = false;
        SetVisible(false);
        return;
    }
    if (battle_intro_skip::AutoSkip(in, intro.pressed))
    {
        StartSkip(ctx, base, in, phaseActor);
        return;
    }
    if (windowVisible && hid::TakeSkipPress())
        StartSkip(ctx, base, in, phaseActor);
    // Before the window opens, so the first prompt raster has the right icon.
    promptCancelA = GuestCancelBit() == XAMINPUT_GAMEPAD_A;
    SetVisible(battle_intro_skip::WindowVisible(in, intro.pressed));
    if (windowVisible)
        promptTick = SteadyMs();
}
}

namespace
{
// The start class of a formation at its request, as the spawn will read it.
battle_intro_skip::StartClass RequestStartClass(uint8_t* base, uint32_t id)
{
    const uint32_t data = PPC_LOAD_U32(FormationArray);
    const uint32_t count = PPC_LOAD_U32(FormationArray + 4);
    if (data < 0x100000 || count >= 4096 || id >= count) return battle_intro_skip::StartClass::Other;
    return ClassifyFString(base, data + id * FormationBytes + 72);
}
}

void battle_intro_skip::NoteBattleRequest(uint32_t id, bool walkingDraw)
{
    uint8_t* base = g_memory.base;
    // The tour formation stays until the spawn: the walking update can request it twice.
    lastRequest = RecordRequest(id, walkingDraw, tourFormation);
    const Option option = Option(debug_menu::GetBattleIntroSkip());
    const StartClass start = option == Option::Press ? RequestStartClass(base, id) : StartClass::Other;
    const bool opens = EarlyWindowOpens(option, lastRequest, start);
    // The walking update can request the same battle again: the window stays as it is.
    if (opens && early.pending && early.id == lastRequest.id)
        return;
    CloseEarly();
    if (!opens)
        return;
    // A new battle: the last one's window state is gone.
    intro = {};
    early = {true, false, false, lastRequest.id, host_ui::GetActiveGameTimeMs()};
    promptCancelA = GuestCancelBit() == XAMINPUT_GAMEPAD_A;
    SetVisible(true);
}

void battle_intro_skip::NoteSlotRewritten()
{
    lastRequest = {};
    CloseEarly();
}

void battle_intro_skip::NoteRequestHeld()
{
    CloseEarly();
}

void battle_intro_skip::NoteBattlePicked(uint32_t id)
{
    if (!early.pending || early.id != static_cast<int32_t>(id)) return;
    early.picked = true;
}

void battle_intro_skip::NoteRequestDropped()
{
    lastRequest = {};
    CloseEarly();
}

void battle_intro_skip::NoteTourDraw(int32_t formation)
{
    tourFormation = formation;
}

// A window or a skip that no battle tick keeps (the battle ended, a soft reset) ends here.
void battle_intro_skip::EngineTick(uint8_t* base)
{
    // The Debug Menu pause blocks the guest threads at their next host import
    // (kernel/function.h), so neither tick runs and the guest clock stops: a pause
    // cannot drop a pending sweep.
    if (early.pending)
    {
        EarlyTick(base);
        seenBattleTicks = battleTicks;
        missedBattleTicks = 0;
        return;
    }
    const bool soundsHeld = sounds.count || sounds.sweepPending;
    if ((!windowVisible && !introSkipLatched.load(std::memory_order_relaxed) && !soundsHeld) ||
        battleTicks != seenBattleTicks)
    {
        seenBattleTicks = battleTicks;
        missedBattleTicks = 0;
        return;
    }
    if (BattleTicksLost(++missedBattleTicks))
    {
        // A soft reset in phase 0 after the press also ends the skip.
        introSkipLatched = false;
        SetVisible(false);
        ClearIntroSounds();
    }
}

bool battle_intro_skip::prompt::Shown()
{
    return promptSince.load() != 0 && PromptFresh(SteadyMs(), promptTick.load());
}

bool battle_intro_skip::prompt::CancelIsA()
{
    return promptCancelA.load();
}

uint64_t battle_intro_skip::prompt::ShownForMs()
{
    const uint64_t since = promptSince.load();
    const uint64_t now = SteadyMs();
    return since && PromptFresh(now, promptTick.load()) && now >= since ? now - since : 0;
}

// Script Sleep (a native of the script; no guest code calls it directly).
PPC_FUNC(sub_82470968)
{
    if (!SkipEnabled())
    {
        __imp__sub_82470968(ctx, base);
        return;
    }
    const uint32_t actor = ctx.r3.u32;
    const uint32_t phaseActor = PPC_LOAD_U32(PhaseActor);
    __imp__sub_82470968(ctx, base);
    if (!SkipEnabled()) return;
    const auto state = ReadState(base, actor, phaseActor);
    if (battle_intro_skip::CollapseSleep(state))
    {
        PPC_STORE_U32(actor + 236, 0);
        // No latent action: the script goes on in this tick.
        if (battle_intro_skip::SleepEndsThisTick(state, PPC_LOAD_U32(PawnLoadTasks)))
            PPC_STORE_U32(PPC_LOAD_U32(actor + 24) + 40, 0);
    }
}

// SetMotion tick: it runs in every battle state update, on the guest game thread.
PPC_FUNC(sub_8238F198)
{
    WindowTick(ctx, base);
    SoundTick(ctx, base);
    __imp__sub_8238F198(ctx, base);
}

// Spawns the phase script actor by class name (formation +72 for the intro).
PPC_FUNC(sub_82390408)
{
    const uint32_t manager = ctx.r3.u32;
    const uint32_t slot = ctx.r4.u32;
    // The guest frees the class name (r5) inside the call, so it is read before it. The
    // guest calls slot 1 on each update, but it returns 0 at once while the slot already
    // has its actor (manager +32 == slot, +16 != 0): the name is read only on a call that
    // can spawn, and only while the option is on. The same option value is used for this
    // battle after the spawn.
    const bool canSpawn = !(PPC_LOAD_U32(manager + 32) == slot && PPC_LOAD_U32(manager + 16) != 0);
    const auto option = slot == 1 && canSpawn ? battle_intro_skip::Option(debug_menu::GetBattleIntroSkip())
                                              : battle_intro_skip::Option::Off;
    const auto start = option != battle_intro_skip::Option::Off ? ClassifyFString(base, ctx.r5.u32)
                                                                : battle_intro_skip::StartClass::Other;
    __imp__sub_82390408(ctx, base);
    // The guest calls this on each update; only the first call spawns (result 1).
    if ((ctx.r3.u32 & 0xFF) && slot == 1)
    {
        introClass = start;
        introActor = PPC_LOAD_U32(manager + 16);
        introSkipLatched = false;
        ClearIntroSounds();
        // The setting and the request for this battle, read again at the spawn.
        const int32_t battleId = static_cast<int32_t>(PPC_LOAD_U32(BattleId));
        const Early before = early;
        early = {};
        intro = {};
        intro.option = option;
        intro.random = battle_intro_skip::RandomBattle(lastRequest, battleId);
        const bool press = intro.option == battle_intro_skip::Option::Press && intro.random;
        // A stored press is for this battle only; an open early window stays open into the
        // intro, so the latch stays armed and the fade-in does not start again.
        intro.stored = press && before.pending && before.stored && before.id == battleId;
        if (!(press && before.pending && before.id == battleId && windowVisible))
            SetVisible(false);
        lastRequest = {};
        tourFormation = -1;
    }
}

// Resource queue tick. Each pass handles the state changes that a frame would handle.
PPC_FUNC(sub_82385300)
{
    if (ctx.r3.u32 != ResourceQueue || !LoadsAccelerated(base))
    {
        __imp__sub_82385300(ctx, base);
        return;
    }
    const uint64_t queue = ctx.r3.u64;
    const double delta = ctx.f1.f64;
    for (int i = 0; i < battle_intro_skip::QueueTicksPerFrame; ++i)
    {
        ctx.r3.u64 = queue;
        ctx.f1.f64 = delta;
        __imp__sub_82385300(ctx, base);
    }
}

// The engine loop's per-frame async package tick. sub_8237CC40 is the engine's flush,
// and the guest also calls it from this engine loop.
PPC_FUNC(sub_82322360)
{
    if (PPC_LOAD_U32(FlushBlocked) != 0 || !LoadsAccelerated(base))
    {
        __imp__sub_82322360(ctx, base);
        return;
    }
    sub_8237CC40(ctx, base);
}

// Battle sound manager entry for actor sounds (r3 manager, r4 id, r5 actor, r6 flag).
// All phase 0 sounds start here. It returns the manager handle, or -1 for no sound;
// every caller handles -1, and the script natives drop the value.
PPC_FUNC(sub_82B1ACA8)
{
    using namespace battle_intro_skip;
    SoundEntry entry = SoundEntry::Pass;
    if (intro.option != Option::Off && intro.random)
    {
        const uint32_t phaseActor = PPC_LOAD_U32(PhaseActor);
        entry = IntroSoundEntry({intro.option, intro.random, ReadState(base, phaseActor, phaseActor)}, intro.pressed);
    }
    if (entry == SoundEntry::Pass)
    {
        __imp__sub_82B1ACA8(ctx, base);
        return;
    }
    if (entry == SoundEntry::Block)
    {
        ctx.r3.s64 = -1;
        return;
    }
    __imp__sub_82B1ACA8(ctx, base);
    const int32_t handle = ctx.r3.s32;
    if (handle >= 0 && sounds.count < MaxIntroSounds)
        sounds.handles[sounds.count++] = static_cast<uint32_t>(handle);
}

// Battle teardown: stops every SE object and unloads the banks.
PPC_FUNC(sub_82B1A6E8)
{
    if (ctx.r3.u32 == SoundManager)
        ClearIntroSounds();
    __imp__sub_82B1A6E8(ctx, base);
}
