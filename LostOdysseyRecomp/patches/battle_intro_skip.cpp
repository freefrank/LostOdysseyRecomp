// Battle intro skip and observation. The skip shortens phase 0, the UnrealScript state
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
// LO_TRACE_BATTLE_INTRO=1 logs the phase 0 script's natives and only reads guest
// memory. Unset, each hook costs one cached bool.
#include <stdafx.h>
#include <patches/battle_intro_skip.h>
#include <patches/battle_intro_gate.h>
#include <host_ui/host_ui.h>
#include <os/logger.h>
#include <debug/battle_intro_skip_setting.h>
#include <debug/battle_tour.h>
#include <hid/hid.h>
#include <kernel/memory.h>
#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdlib>
#include <string>
#include <string_view>

extern std::atomic<uint32_t> g_presentedSwaps;

extern "C" PPC_FUNC(__imp__sub_82470968);
extern "C" PPC_FUNC(__imp__sub_8238F198);
extern "C" PPC_FUNC(__imp__sub_82390408);
extern "C" PPC_FUNC(__imp__sub_82AE5590);
extern "C" PPC_FUNC(__imp__sub_82AE2AF0);
extern "C" PPC_FUNC(__imp__sub_82AE2C28);
extern "C" PPC_FUNC(__imp__sub_82AE1020);
extern "C" PPC_FUNC(__imp__sub_82AE10E8);
extern "C" PPC_FUNC(__imp__sub_82AE1E90);
extern "C" PPC_FUNC(__imp__sub_82AAD838);
extern "C" PPC_FUNC(__imp__sub_82AAA7C8);
extern "C" PPC_FUNC(__imp__sub_82AF9B88);
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
constexpr uint32_t OtherSleepLines = 40;
constexpr uint32_t Manager = 0x832CA0E0;
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
constexpr uint32_t BgmPlaying = SoundManager - 4;

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
    uint64_t spawnMs = 0;
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
// sub_82B1ACA8 until the press, and what the press did to them.
struct IntroSounds
{
    uint32_t handles[battle_intro_skip::MaxIntroSounds]{};
    uint32_t count = 0;
    uint64_t pressNs = 0;
    bool sweepPending = false;
    // Trace counts. At the press each recorded sound is faded, fadeFailed, ended,
    // untouched or goneAtPress.
    uint32_t recorded = 0, overflow = 0, blocked = 0, faded = 0, fadeFailed = 0, ended = 0, untouched = 0,
             goneAtPress = 0, swept = 0, goneAtSweep = 0;
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

bool Tracing()
{
    static const bool enabled = std::getenv("LO_TRACE_BATTLE_INTRO") != nullptr;
    return enabled;
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

float LoadFloat(uint8_t* base, uint32_t address)
{
    return std::bit_cast<float>(PPC_LOAD_U32(address));
}

std::string ReadFString(uint8_t* base, uint32_t fstring)
{
    if (fstring < 0x100000) return "?";
    const uint32_t data = PPC_LOAD_U32(fstring);
    const uint32_t count = PPC_LOAD_U32(fstring + 4);
    if (data < 0x100000 || count == 0 || count > 64) return "?";
    std::string text;
    for (uint32_t i = 0; i + 1 < count; ++i)
    {
        const uint16_t c = PPC_LOAD_U16(data + i * 2);
        text += c >= 0x20 && c < 0x7F ? char(c) : '?';
    }
    return text;
}

// One line per event, with the swap number and the active game time.
template <typename... Args>
void Trace(uint8_t* base, fmt::format_string<Args...> format, Args&&... args)
{
    LOG_NOTICE("battle intro: swap={} t={} phase={} {}", g_presentedSwaps.load(),
        host_ui::GetActiveGameTimeMs(), PPC_LOAD_U32(Core + 0x38),
        fmt::format(format, std::forward<Args>(args)...));
}

void TraceChanges(uint8_t* base)
{
    struct Fields
    {
        uint32_t phase, request, hold, turn, scene, actor, slot, stage, matinees, exitFlag;
        int32_t battleId;
        bool operator==(const Fields&) const = default;
    };
    static Fields previous{};
    // Core +148 bit 0x800 makes sub_8238E3D8 return before its phase switch.
    const Fields now{PPC_LOAD_U32(Core + 0x38), PPC_LOAD_U32(Core + 144), PPC_LOAD_U32(Core + 148), PPC_LOAD_U32(Core + 52),
        PPC_LOAD_U32(Scene), PPC_LOAD_U32(PhaseActor), PPC_LOAD_U32(ActorSlot), PPC_LOAD_U32(SetMotion + 244),
        PPC_LOAD_U32(MatineeCount), PPC_LOAD_U8(Core + 60 + (PPC_LOAD_U32(Core + 0x38) & 15)),
        static_cast<int32_t>(PPC_LOAD_U32(BattleId))};
    if (now == previous) return;
    previous = now;
    Trace(base, "state request={} hold={:#x} exit_flag={} turn={} scene={} actor={:#x} slot={} motion_state={} matinees={} battle_id={}",
        now.request, now.hold, now.exitFlag, now.turn, now.scene, now.actor, now.slot, now.stage, now.matinees, now.battleId);
}

// One line per battle character and one for the camera. Pawns are in the SetMotion
// manager's array (+4 data, +8 count) and their +552 is the character's +64.
void Snapshot(uint8_t* base, const char* label)
{
    const uint32_t list = PPC_LOAD_U32(Core + 20);
    const uint32_t chars = list ? PPC_LOAD_U32(list) : 0;
    const uint32_t count = list ? PPC_LOAD_U32(list + 4) : 0;
    const uint32_t pawns = PPC_LOAD_U32(SetMotion + 4);
    const uint32_t pawnCount = PPC_LOAD_U32(SetMotion + 8);
    for (uint32_t i = 0; chars && i < count && i < 64; ++i)
    {
        const uint32_t character = PPC_LOAD_U32(chars + i * 4);
        const uint32_t id = PPC_LOAD_U32(character + 64);
        uint32_t pawn = 0;
        for (uint32_t k = 0; pawns && k < pawnCount && k < 64 && !pawn; ++k)
        {
            const uint32_t candidate = PPC_LOAD_U32(pawns + k * 4);
            if (PPC_LOAD_U32(candidate + 552) == id) pawn = candidate;
        }
        // Actor Location is +0xF8 (docs/notes/debug-teleport.md); the rotator follows it.
        std::string where = "loc=none rot=none";
        if (pawn)
            where = fmt::format("loc={:.1f},{:.1f},{:.1f} rot={},{},{}", LoadFloat(base, pawn + 248), LoadFloat(base, pawn + 252),
                LoadFloat(base, pawn + 256), static_cast<int32_t>(PPC_LOAD_U32(pawn + 260)),
                static_cast<int32_t>(PPC_LOAD_U32(pawn + 264)), static_cast<int32_t>(PPC_LOAD_U32(pawn + 268)));
        // The motion stack of the pawn's skeletal component (see sub_82ADAE90): 64-byte
        // entries, the last one plays, and entry +8 is a sequence with its name at +220.
        std::string motions;
        const uint32_t component = pawn ? PPC_LOAD_U32(pawn + 516) : 0;
        const uint32_t node = component ? PPC_LOAD_U32(component + 648) : 0;
        if (node)
        {
            const uint32_t stack = PPC_LOAD_U32(node + 180);
            const uint32_t depth = PPC_LOAD_U32(node + 184);
            for (uint32_t k = 0; stack && k < depth && k < 8; ++k)
            {
                const uint32_t sequence = PPC_LOAD_U32(stack + k * 64 + 8);
                motions += (k ? "|" : "") + (sequence ? ReadFString(base, sequence + 220) : std::string("?"));
            }
        }
        Trace(base, "snapshot label={} index={} char={:#x} id={} c60={:#x} c124={:#x} pawn={:#x} p604={:#x} hidden={} p608={:#x} {} motions={}",
            label, i, character, id, PPC_LOAD_U32(character + 60), PPC_LOAD_U32(character + 124), pawn,
            pawn ? PPC_LOAD_U32(pawn + 604) : 0, pawn ? (PPC_LOAD_U32(pawn + 604) & 0x08000000) != 0 : true,
            pawn ? PPC_LOAD_U32(pawn + 608) : 0, where, motions);
    }
    // The camera object sits inside the manager; its view point starts at +168.
    const uint32_t camera = Manager + 232;
    std::string pov;
    for (uint32_t offset = 168; offset < 180; offset += 4)
        pov += fmt::format("{:.1f},", LoadFloat(base, camera + offset));
    Trace(base, "snapshot label={} camera={:#x} pov={} rot={},{},{} fov={:.1f}", label, camera, pov,
        static_cast<int32_t>(PPC_LOAD_U32(camera + 180)), static_cast<int32_t>(PPC_LOAD_U32(camera + 184)),
        static_cast<int32_t>(PPC_LOAD_U32(camera + 188)), LoadFloat(base, camera + 192));
}

// Snapshots at the phase changes of the intro and when the first command has settled.
void SnapshotChanges(uint8_t* base)
{
    static uint32_t previous = ~0u;
    static uint64_t enteredCommand = 0;
    static int settled = 0;
    const uint32_t phase = PPC_LOAD_U32(Core + 0x38);
    const uint64_t now = host_ui::GetActiveGameTimeMs();
    if (phase != previous)
    {
        if (previous == 0 && phase == 1) Snapshot(base, "p0_to_p1");
        if (previous == 1 && phase == 2 && !enteredCommand)
        {
            Snapshot(base, "p1_to_p2");
            enteredCommand = now;
            settled = 0;
        }
        if (phase == 0 || phase > 10) enteredCommand = 0;
        previous = phase;
    }
    if (phase == 2 && enteredCommand)
    {
        if (settled == 0 && now - enteredCommand >= 1000) { Snapshot(base, "p2_plus_1s"); settled = 1; }
        if (settled == 1 && now - enteredCommand >= 4000) { Snapshot(base, "p2_plus_4s"); settled = 2; }
    }
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

// Trace: manager objects, kind 1 and 4 slots in use, the streams (music) and the BGM byte.
std::string SoundUse(uint8_t* base)
{
    const uint32_t system = PPC_LOAD_U32(SoundSystemPointer);
    uint32_t k1 = 0, k4 = 0;
    std::string streams;
    for (uint32_t i = 0; system && i < 128; ++i)
    {
        k1 += PPC_LOAD_U8(system + 660 + i * 20) != 0;
        k4 += PPC_LOAD_U8(system + 3860 + i * 20) != 0;
    }
    for (uint32_t i = 0; system && i < 6; ++i)
    {
        const uint32_t slot = system + 3580 + i * 20;
        if (PPC_LOAD_U8(slot))
            streams += fmt::format("{}{}:{:#x}:{}:{}", streams.empty() ? "" : ",", i, PPC_LOAD_U8(slot + 1),
                PPC_LOAD_U8(slot), PPC_LOAD_U8(slot + 9));
    }
    return fmt::format("list={} k1={} k4={} streams={} bgm={}", PPC_LOAD_U32(SoundManager + 28), k1, k4,
        streams.empty() ? "none" : streams, PPC_LOAD_U8(BgmPlaying));
}

void ClearIntroSounds(uint8_t* base, const char* reason)
{
    if (Tracing() && (sounds.recorded || sounds.blocked || sounds.overflow))
        Trace(base, "skip_sound_summary reason={} recorded={} overflow={} blocked={} {}", reason, sounds.recorded,
            sounds.overflow, sounds.blocked, SoundUse(base));
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
        if (!object)
        {
            ++sounds.goneAtPress;  // it ended on its own
            continue;
        }
        const uint32_t handle = PPC_LOAD_U32(object + 24);
        const PressAction action = IntroSoundAtPress(PPC_LOAD_U8(object + 4), handle, ReadSoundSlot(base, handle));
        if (action == PressAction::End)
        {
            PPC_STORE_U8(object + 4, 3);
            ++sounds.ended;
        }
        else if (action == PressAction::Fade)
        {
            ctx.r3.u64 = PPC_LOAD_U32(SoundSystemPointer);
            ctx.r4.u64 = handle;
            ctx.r5.u64 = 2;  // free the slot at the end of the ramp
            ctx.r6.u64 = 0;  // to silence
            ctx.f1.f64 = IntroFadeSeconds;
            sub_82B62048(ctx, base);
            const bool hardStop = ctx.r3.u32 == 0;
            if (hardStop)
            {
                // Message 14 was not queued (full ring): stop it hard.
                StopSound(ctx, base, sounds.handles[i]);
                ++sounds.fadeFailed;
            }
            else
            {
                ++sounds.faded;
            }
        }
        else
        {
            ++sounds.untouched;
        }
    }
    ctx = saved;
    sounds.pressNs = GuestNs();
    sounds.sweepPending = sounds.count != 0;
    if (Tracing())
        Trace(base, "skip_sound_press recorded={} overflow={} faded={} fade_failed={} ended={} untouched={} gone={} {}",
            sounds.count, sounds.overflow, sounds.faded, sounds.fadeFailed, sounds.ended, sounds.untouched,
            sounds.goneAtPress, SoundUse(base));
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
        if (!object)
        {
            ++sounds.goneAtSweep;
            continue;
        }
        if (!SweepStops(PPC_LOAD_U32(object + 24))) continue;
        StopSound(ctx, base, sounds.handles[i]);
        ++sounds.swept;
    }
    ctx = saved;
    sounds.sweepPending = false;
    if (Tracing())
        Trace(base, "skip_sound_sweep since_press_guest_ms={} swept={} gone={} {}",
            (GuestNs() - sounds.pressNs) / 1000000, sounds.swept, sounds.goneAtSweep, SoundUse(base));
}

// Each battle tick: the sweep, and the end of the records.
void SoundTick(PPCContext& ctx, uint8_t* base)
{
    using namespace battle_intro_skip;
    if (!sounds.count && !sounds.sweepPending && !sounds.recorded && !sounds.blocked)
        return;
    if (sounds.sweepPending && SweepDue(GuestNs(), sounds.pressNs))
    {
        SweepIntroSounds(ctx, base);
        return;
    }
    if (!KeepIntroSounds(PPC_LOAD_U32(Core + 0x38), sounds.sweepPending))
        ClearIntroSounds(base, "phase");
}

// Opens or closes the prompt window. The latch is armed only while it is open.
void SetVisible(uint8_t* base, bool visible, const char* reason)
{
    if (windowVisible == visible) return;
    windowVisible = visible;
    hid::SetSkipArmed(visible);
    const uint64_t now = SteadyMs();
    promptTick = now;
    promptSince = visible ? std::max<uint64_t>(now, 1) : 0;
    if (Tracing())
        Trace(base, "skip_window visible={} reason={} early={}", visible, reason, early.pending);
}

// The early window ends without a spawn: the stored press goes with it.
void CloseEarly(uint8_t* base, const char* reason)
{
    if (!early.pending) return;
    if (Tracing())
        Trace(base, "skip_early_end reason={} stored={} since_request_ms={} battle_slot={}", reason, early.stored,
            host_ui::GetActiveGameTimeMs() - early.requestMs, static_cast<int32_t>(PPC_LOAD_U32(BattleSlotPhase)));
    early = {};
    SetVisible(base, false, reason);
}

// Each engine tick from the request to the spawn: the press, and the end without a battle.
void EarlyTick(uint8_t* base)
{
    using namespace battle_intro_skip;
    const uint64_t since = host_ui::GetActiveGameTimeMs() - early.requestMs;
    const EarlyEnd end = EarlyWindowEnds(early.picked, static_cast<int32_t>(PPC_LOAD_U32(BattleSlotPhase)), since);
    if (end != EarlyEnd::None)
    {
        CloseEarly(base, end == EarlyEnd::Gone ? "request_gone" : "timeout");
        return;
    }
    if (windowVisible && hid::TakeSkipPress())
    {
        early.stored = true;
        if (Tracing())
            Trace(base, "skip_press_stored since_request_ms={}", since);
        SetVisible(base, false, "press_early");
    }
    promptCancelA = GuestCancelBit() == XAMINPUT_GAMEPAD_A;
    if (windowVisible)
        promptTick = SteadyMs();
}

// The press, or the Always start: end the Sleep calls, load faster, fade the intro sounds.
void StartSkip(PPCContext& ctx, uint8_t* base, const battle_intro_skip::WindowInput& in, uint32_t phaseActor,
    uint64_t sinceSpawn, const char* source)
{
    intro.pressed = true;
    introSkipLatched = true;
    // The Sleep that runs now ends on its next poll; the hook ends the later ones.
    battle_intro_skip::SleepState state = in.state;
    state.enabled = true;
    const uint32_t frame = PPC_LOAD_U32(phaseActor + 24);
    const bool sleeping = frame >= 0x100000 &&
        battle_intro_skip::EndRunningSleep(state, PPC_LOAD_U32(frame + 40), PPC_LOAD_U32(phaseActor + 544));
    if (Tracing())
        Trace(base, "skip_press source={} since_spawn_ms={} running_sleep={} seconds_left={:.3f}", source,
            sinceSpawn, sleeping,
            sleeping ? LoadFloat(base, phaseActor + 236) : 0.0f);
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
    const uint64_t sinceSpawn = host_ui::GetActiveGameTimeMs() - intro.spawnMs;
    // The press of the early window acts at the first tick that can skip, as Always does.
    const auto stored = battle_intro_skip::StoredPressAction(in, intro.stored);
    if (stored != battle_intro_skip::StoredPress::Keep)
        intro.stored = false;
    if (stored == battle_intro_skip::StoredPress::Apply)
        StartSkip(ctx, base, in, phaseActor, sinceSpawn, "stored");
    else if (stored == battle_intro_skip::StoredPress::Drop && Tracing())
        Trace(base, "skip_press_dropped reason=not_eligible");
    if (!battle_intro_skip::WindowEligible(in))
    {
        // The skip belongs to this intro only.
        if (in.state.phase != 0) introSkipLatched = false;
        SetVisible(base, false, in.state.phase != 0 ? "phase" : "not_eligible");
        return;
    }
    if (battle_intro_skip::AutoSkip(in, intro.pressed))
    {
        StartSkip(ctx, base, in, phaseActor, sinceSpawn, "always");
        return;
    }
    if (windowVisible && hid::TakeSkipPress())
        StartSkip(ctx, base, in, phaseActor, sinceSpawn, "press");
    // Before the window opens, so the first prompt raster has the right icon.
    promptCancelA = GuestCancelBit() == XAMINPUT_GAMEPAD_A;
    SetVisible(base, battle_intro_skip::WindowVisible(in, intro.pressed), intro.pressed ? "press" : "open");
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
    return battle_intro_skip::ClassifyStart(ReadFString(base, data + id * FormationBytes + 72));
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
    if (Tracing())
        Trace(base, "request accepted formation={} walking={} random={} class={} early={}", id, walkingDraw,
            lastRequest.random, static_cast<int>(start), opens);
    // The walking update can request the same battle again: the window stays as it is.
    if (opens && early.pending && early.id == lastRequest.id)
        return;
    CloseEarly(base, "new_request");
    if (!opens)
        return;
    // A new battle: the last one's window state is gone.
    intro = {};
    early = {true, false, false, lastRequest.id, host_ui::GetActiveGameTimeMs()};
    promptCancelA = GuestCancelBit() == XAMINPUT_GAMEPAD_A;
    SetVisible(base, true, "request");
}

void battle_intro_skip::NoteSlotRewritten()
{
    lastRequest = {};
    CloseEarly(g_memory.base, "slot_rewritten");
}

void battle_intro_skip::NoteRequestHeld()
{
    CloseEarly(g_memory.base, "held");
}

void battle_intro_skip::NoteBattlePicked(uint32_t id)
{
    if (!early.pending || early.id != static_cast<int32_t>(id)) return;
    early.picked = true;
    if (Tracing())
        Trace(g_memory.base, "skip_early_picked since_request_ms={}", host_ui::GetActiveGameTimeMs() - early.requestMs);
}

void battle_intro_skip::NoteRequestDropped()
{
    lastRequest = {};
    CloseEarly(g_memory.base, "dropped");
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
        SetVisible(base, false, "no_battle_tick");
        ClearIntroSounds(base, "no_battle_tick");
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

// Script Sleep. No guest code calls it directly, so a line here proves that the
// script's native table reaches the override.
PPC_FUNC(sub_82470968)
{
    if (!Tracing() && !SkipEnabled())
    {
        __imp__sub_82470968(ctx, base);
        return;
    }
    const uint32_t actor = ctx.r3.u32;
    const uint32_t caller = static_cast<uint32_t>(ctx.lr);
    const uint32_t phaseActor = PPC_LOAD_U32(PhaseActor);
    __imp__sub_82470968(ctx, base);
    if (SkipEnabled())
    {
        const auto state = ReadState(base, actor, phaseActor);
        if (battle_intro_skip::CollapseSleep(state))
        {
            if (Tracing())
                Trace(base, "skip_sleep actor={:#x} seconds={} class={}", actor, LoadFloat(base, actor + 236),
                    static_cast<int>(state.start));
            PPC_STORE_U32(actor + 236, 0);
            // No latent action: the script goes on in this tick.
            if (battle_intro_skip::SleepEndsThisTick(state, PPC_LOAD_U32(PawnLoadTasks)))
                PPC_STORE_U32(PPC_LOAD_U32(actor + 24) + 40, 0);
        }
    }
    if (!Tracing()) return;
    static std::atomic<uint32_t> others{0};
    if (actor == phaseActor || others.fetch_add(1, std::memory_order_relaxed) < OtherSleepLines)
    {
        const uint32_t frame = PPC_LOAD_U32(actor + 24);
        Trace(base, "sleep actor={:#x} phase_actor={} slot={} seconds={} latent_op={} caller={:#x} matinees={}",
            actor, actor == phaseActor, PPC_LOAD_U32(ActorSlot), LoadFloat(base, actor + 236),
            PPC_LOAD_U32(frame + 40), caller, PPC_LOAD_U32(MatineeCount));
    }
}

// SetMotion tick: it runs in every battle state update, so state changes are
// logged here, on the guest game thread.
PPC_FUNC(sub_8238F198)
{
    if (Tracing())
    {
        TraceChanges(base);
        SnapshotChanges(base);
    }
    WindowTick(ctx, base);
    SoundTick(ctx, base);
    __imp__sub_8238F198(ctx, base);
}

// Spawns the phase script actor by class name (formation +72 for the intro).
PPC_FUNC(sub_82390408)
{
    const uint32_t manager = ctx.r3.u32;
    const uint32_t slot = ctx.r4.u32;
    const std::string name = Tracing() || slot == 1 ? ReadFString(base, ctx.r5.u32) : std::string();
    __imp__sub_82390408(ctx, base);
    // The guest calls this on each update; only the first call spawns (result 1).
    if ((ctx.r3.u32 & 0xFF) && slot == 1)
    {
        introClass = battle_intro_skip::ClassifyStart(name);
        introActor = PPC_LOAD_U32(manager + 16);
        introSkipLatched = false;
        ClearIntroSounds(base, "spawn");
        // The setting and the request for this battle, read again at the spawn.
        const int32_t battleId = static_cast<int32_t>(PPC_LOAD_U32(BattleId));
        const Early before = early;
        early = {};
        intro = {};
        intro.option = battle_intro_skip::Option(debug_menu::GetBattleIntroSkip());
        intro.random = battle_intro_skip::RandomBattle(lastRequest, battleId);
        intro.spawnMs = host_ui::GetActiveGameTimeMs();
        const bool press = intro.option == battle_intro_skip::Option::Press && intro.random;
        // A stored press is for this battle only; an open early window stays open into the
        // intro, so the latch stays armed and the fade-in does not start again.
        intro.stored = press && before.pending && before.stored && before.id == battleId;
        if (!(press && before.pending && before.id == battleId && windowVisible))
            SetVisible(base, false, "spawn");
        if (Tracing())
            Trace(base, "skip_gate option={} random={} request={} request_random={} battle_id={} early={} stored={} "
                "since_request_ms={} {}", uint32_t(intro.option), intro.random, lastRequest.id, lastRequest.random,
                battleId, before.pending, intro.stored,
                before.pending ? intro.spawnMs - before.requestMs : 0, SoundUse(base));
        if (before.stored && !intro.stored && Tracing())
            Trace(base, "skip_press_dropped reason=spawn");
        lastRequest = {};
        tourFormation = -1;
    }
    if (Tracing() && (ctx.r3.u32 & 0xFF))
        Trace(base, "spawn manager={:#x} slot={} class={} result={} actor={:#x} stored_slot={} battle_id={}",
            manager, slot, name, ctx.r3.u32 & 0xFF, PPC_LOAD_U32(manager + 16), PPC_LOAD_U32(manager + 32),
            static_cast<int32_t>(PPC_LOAD_U32(BattleId)));
}

// ChangePhase: the name of the state that the actor enters.
PPC_FUNC(sub_82AE5590)
{
    if (Tracing() && ctx.r3.u32 == PPC_LOAD_U32(PhaseActor))
        Trace(base, "change_phase actor={:#x} state={}", ctx.r3.u32, ReadFString(base, ctx.r4.u32));
    __imp__sub_82AE5590(ctx, base);
}

// Wait: r5 is the wait type.
PPC_FUNC(sub_82AE2AF0)
{
    if (Tracing())
        Trace(base, "wait actor={:#x} target={:#x} type={}", ctx.r3.u32, ctx.r4.u32, ctx.r5.u32 & 0xFF);
    __imp__sub_82AE2AF0(ctx, base);
}

PPC_FUNC(sub_82AE2C28)
{
    if (Tracing())
        Trace(base, "continue actor={:#x} wait_type={}", ctx.r3.u32, PPC_LOAD_U8(ctx.r3.u32 + 548));
    __imp__sub_82AE2C28(ctx, base);
}

PPC_FUNC(sub_82AE1020)
{
    if (Tracing())
        Trace(base, "change_battle_phase actor={:#x} request={}", ctx.r3.u32, ctx.r4.u32 & 0xFF);
    __imp__sub_82AE1020(ctx, base);
}

PPC_FUNC(sub_82AE10E8)
{
    if (Tracing())
        Trace(base, "switch_battle_ui value={}", ctx.r4.u32);
    __imp__sub_82AE10E8(ctx, base);
}

PPC_FUNC(sub_82AE1E90)
{
    if (Tracing())
        Trace(base, "hide actor={:#x} target={:#x} hide={} r6={}", ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32);
    __imp__sub_82AE1E90(ctx, base);
}

PPC_FUNC(sub_82AAD838)
{
    if (Tracing())
        Trace(base, "call_battle_in manager={:#x} arg={}", ctx.r3.u32, ctx.r4.u32);
    __imp__sub_82AAD838(ctx, base);
}

PPC_FUNC(sub_82AAA7C8)
{
    if (Tracing())
        Trace(base, "set_phase core={:#x} next={} force={} caller={:#x} before={}", ctx.r3.u32, ctx.r4.u32,
            ctx.r5.u32 & 0xFF, static_cast<uint32_t>(ctx.lr), PPC_LOAD_U32(ctx.r3.u32 + 56));
    __imp__sub_82AAA7C8(ctx, base);
}

// AI script instruction 131 (its only writer): core +148 bit 0x800 = the operand.
PPC_FUNC(sub_82AF9B88)
{
    if (Tracing())
    {
        const uint32_t thread = PPC_LOAD_U32(ctx.r3.u32 + 24);
        const uint32_t pc = PPC_LOAD_U32(thread + 52);
        const uint32_t code = PPC_LOAD_U32(thread + 36);
        const bool wide = (PPC_LOAD_U32(PPC_LOAD_U32(ctx.r3.u32 + 44) + 28) & 0x4000000) != 0;
        Trace(base, "ai_hold vm={:#x} thread={:#x} pc={:#x} value={} wide={} hold_before={:#x}", ctx.r3.u32, thread, pc,
            PPC_LOAD_U8(code + pc + (wide ? 2 : 1)), wide, PPC_LOAD_U32(Core + 148));
    }
    __imp__sub_82AF9B88(ctx, base);
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
    const auto start = std::chrono::steady_clock::now();
    sub_8237CC40(ctx, base);
    if (Tracing())
        Trace(base, "async_flush ms={:.1f}",
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
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
        ++sounds.blocked;
        ctx.r3.s64 = -1;
        return;
    }
    __imp__sub_82B1ACA8(ctx, base);
    const int32_t handle = ctx.r3.s32;
    if (handle >= 0)
    {
        if (sounds.count < MaxIntroSounds)
            sounds.handles[sounds.count++] = static_cast<uint32_t>(handle);
        else
            ++sounds.overflow;
        ++sounds.recorded;
    }
}

// Battle teardown: stops every SE object and unloads the banks.
PPC_FUNC(sub_82B1A6E8)
{
    if (ctx.r3.u32 == SoundManager)
        ClearIntroSounds(base, "battle_end");
    __imp__sub_82B1A6E8(ctx, base);
}
