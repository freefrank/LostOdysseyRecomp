#include <stdafx.h>
#include <gpu/cinematic_bars.h>
#include <kernel/memory.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <unordered_map>

namespace
{
std::unordered_map<uint32_t, uint8_t> guest;
bool moviePlaying = false;
using Native = std::function<void(PPCContext&)>;
Native fade, start, taskUpdate, taskCleanup, play, matineeUpdate, matineeCleanup;
constexpr uint32_t taskA = 0x100000, taskB = 0x110000;
constexpr uint32_t matineeA = 0x120000, matineeB = 0x130000;
constexpr uint32_t fadeObject = 0x140000, pauseObject = 0x150000, pauser = 0x160000;
constexpr uint32_t engine = 0x190000, players = 0x1A0000, player = 0x1B0000, controller = 0x1C0000;

void Check(bool result, const char* reason)
{
    if (!result) { std::fprintf(stderr, "FAIL: %s\n", reason); std::exit(1); }
}
void Store8(uint32_t address, uint8_t value) { guest[address] = value; }
void Store32(uint32_t address, uint32_t value)
{
    for (unsigned i = 0; i != 4; ++i) Store8(address + i, uint8_t(value >> (24 - 8 * i)));
}
void ScreenFade(bool active)
{
    Store32(0x832C17EC, fadeObject);
    Store32(fadeObject + 0x4C, active ? 0x80000000u : 0);
}
void GameplayControl(bool enabled)
{
    Store32(0x83315FB4, engine);
    Store32(engine + 0x2B8, players); Store32(engine + 0x2BC, 1);
    Store32(players, player); Store32(player + 0x40, controller);
    Store32(controller + 0x5C8, enabled ? 0x08000000u : 0);
}
bool Active() { return gpu::cinematic_bars::Active(); }
void EngineTick() { gpu::cinematic_bars::UpdatePause(nullptr); }
}

uint8_t fixture::Load8(uint32_t address)
{
    auto found = guest.find(address);
    return found == guest.end() ? 0 : found->second;
}
uint32_t fixture::Load32(uint32_t address)
{
    uint32_t value = 0;
    for (unsigned i = 0; i != 4; ++i) value = (value << 8) | Load8(address + i);
    return value;
}
namespace movie { bool GuestPlaying() { return moviePlaying; } }

// Native functions expose the signal state at the moment the original game
// runs, then perform only the guest state transition selected by each test.
#define NATIVE(name, callback) extern "C" PPC_FUNC(__imp__##name) { if (callback) callback(ctx); }
NATIVE(sub_828F7840, fade)
NATIVE(sub_82B31720, start)
NATIVE(sub_82B322D8, taskUpdate)
NATIVE(sub_82B31538, taskCleanup)
NATIVE(sub_8280DD20, play)
NATIVE(sub_82374E28, matineeUpdate)
NATIVE(sub_82810800, matineeCleanup)
#undef NATIVE

PPC_FUNC(sub_828F7840);
PPC_FUNC(sub_82B31720);
PPC_FUNC(sub_82B322D8);
PPC_FUNC(sub_82B31538);
PPC_FUNC(sub_8280DD20);
PPC_FUNC(sub_82374E28);
PPC_FUNC(sub_82810800);

namespace
{
using Hook = void (*)(PPCContext&, uint8_t*);
uint32_t Call(Hook hook, uint32_t object)
{
    PPCContext ctx;
    ctx.r3.u32 = object;
    hook(ctx, nullptr);
    return ctx.r3.u32;
}
void BeginTask(uint32_t task)
{
    Store8(task + 1, 2);
    start = [](PPCContext& ctx) {
        Check(Active(), "task bars precede the native start");
        ctx.r3.u32 = 1;
    };
    Check(Call(sub_82B31720, task) == 1, "successful native start return retained");
}
void FinishTask(uint32_t task)
{
    taskUpdate = [task](PPCContext&) { Store8(task + 1, 4); Store8(task + 3, 0); };
    Call(sub_82B322D8, task);
}
void BeginMatinee(uint32_t object)
{
    Store8(object + 0x78, 1);
    play = [object](PPCContext&) {
        Check(Active(), "direct Matinee bars precede native playback");
        Store32(object + 0x5C, 1);
        Store8(object + 0x78, 2);
    };
    Call(sub_8280DD20, object);
}
void EndMatinee(uint32_t object)
{
    matineeCleanup = [object](PPCContext&) { Store32(object + 0x5C, 0); };
    Call(sub_82810800, object);
}
}

int main()
{
    GameplayControl(true);
    EngineTick();
    Check(!Active(), "field gameplay has no bars");

    // Merely preparing a timeline must not crop the field view.
    for (uint8_t state : std::array<uint8_t, 3>{10, 4, 8})
    {
        Store32(matineeA + 0x5C, 1);
        Store8(matineeA + 0x78, state);
        Call(sub_82374E28, matineeA);
        Check(!Active(), "passive Matinee preparation leaves gameplay uncropped");
    }

    constexpr uint32_t manager = 0x170000, array = 0x180000;
    Store32(manager + 20, array); Store32(manager + 24, 1);
    Store32(array, taskA); Store32(taskA + 4, 42); Store8(taskA + 1, 2);
    PPCContext request;
    request.r31.u32 = manager; request.r30.u32 = 42;
    request.lr = 0x82AD6100;
    fade = [](PPCContext&) { Check(!Active(), "other fade callers do not activate cutscene bars"); };
    sub_828F7840(request, nullptr);
    Check(!Active(), "unrelated fade preserves field view");
    request.lr = 0x82AD616C;
    fade = [](PPCContext&) { Check(Active(), "scenario bars precede the native fade call"); };
    sub_828F7840(request, nullptr);
    Check(request.lr == 0x82AD616C && Active(), "scenario request retains caller and bars");
    FinishTask(taskA);
    Check(Active(), "completed scenario waits for engine control confirmation");
    EngineTick();
    Check(!Active(), "completed scenario releases bars when field control is restored");

    Store8(taskA + 1, 2);
    start = [](PPCContext& ctx) {
        Check(Active(), "failed start is still covered while native code runs");
        ctx.r3.u32 = 0;
    };
    Check(Call(sub_82B31720, taskA) == 0, "failed native start return retained");
    EngineTick();
    Check(!Active(), "failed task start rolls back bars after field control confirmation");

    BeginTask(taskA); BeginTask(taskB);
    FinishTask(taskA);
    Check(Active(), "one completed RT task cannot release an overlapping RT task");
    FinishTask(taskB);
    EngineTick();
    Check(!Active(), "last completed RT task returns to full gameplay");

    BeginMatinee(matineeA); BeginMatinee(matineeB);
    EndMatinee(matineeA);
    Check(Active(), "one Matinee cleanup preserves overlapping playback");
    EndMatinee(matineeB);
    EngineTick();
    Check(!Active(), "last Matinee cleanup releases bars");

    BeginTask(taskA); BeginMatinee(matineeA);
    FinishTask(taskA);
    Check(Active(), "outer task completion preserves a running native Matinee");
    EndMatinee(matineeA);
    EngineTick();
    Check(!Active(), "all RT owners must finish before gameplay expands");

    // The native Matinee may end before its central black transition. A clear
    // screen-fade bit alone cannot authorize expanding back to the field view.
    BeginMatinee(matineeA); GameplayControl(false); EndMatinee(matineeA);
    EngineTick();
    Check(Active(), "Matinee completion retains bars while field control is disabled without ScreenFade");
    Store32(player + 0x40, 0);
    EngineTick();
    Check(Active(), "missing field controller cannot release the Matinee transition tail");
    GameplayControl(false);
    EngineTick();
    Check(Active(), "restoring a disabled controller still retains the transition tail");
    GameplayControl(true);
    Check(Active(), "presentation does not follow restored guest control directly");
    EngineTick();
    Check(!Active(), "engine tick releases transition bars once field control returns");

    // Scripted RT events can enter battle while the field controller stays
    // disabled. Both native world state slots can report that handoff.
    constexpr uint32_t world = 0x1D0000;
    Store32(0x83318744, world);
    for (uint32_t battleState : {472u, 476u})
    {
        Store32(world + 472, 0); Store32(world + 476, 0);
        GameplayControl(false);
        BeginMatinee(matineeA); EndMatinee(matineeA);
        EngineTick();
        Check(Active(), "scripted field transition retains bars before battle begins");
        Store32(world + battleState, 3);
        Check(Active(), "presentation waits for the engine to confirm battle gameplay");
        EngineTick();
        Check(!Active(), "battle gameplay releases completed RT tail despite disabled field control");
        BeginMatinee(matineeA);
        EngineTick();
        Check(Active(), "battle gameplay cannot release an independently active Matinee");
        EndMatinee(matineeA);
        EngineTick();
        Check(!Active(), "ending the battle Matinee restores full battle gameplay");
    }
    Store32(0x83318744, 0);
    GameplayControl(true);

    BeginTask(taskA); ScreenFade(true); FinishTask(taskA);
    Check(Active(), "finished task retains bars through its final fade");
    EngineTick();
    Check(Active(), "engine tick retains a running final fade");
    ScreenFade(false);
    Check(Active(), "presentation does not follow guest fade memory");
    EngineTick();
    Check(!Active(), "engine tick releases the completed final fade");

    BeginTask(taskA);
    taskCleanup = [](PPCContext& ctx) { ctx.r3.u32 = 0; };
    Call(sub_82B31538, taskA);
    Check(Active(), "incomplete task cleanup cannot release bars");
    taskCleanup = [](PPCContext& ctx) { ctx.r3.u32 = 1; };
    Call(sub_82B31538, taskA);
    EngineTick();
    Check(!Active(), "successful task cleanup releases bars");

    Store32(0x832C0C40, pauseObject); Store32(pauseObject + 12, pauser);
    Store8(pauseObject + 0x5D, 1); EngineTick();
    Check(Active(), "native pause draw byte enables bars");
    Store8(pauseObject + 0x5E, 1); EngineTick();
    Check(!Active(), "native pause suppression byte disables bars");
    Store8(pauseObject + 0x5E, 0); Store8(pauser + 6, 1); EngineTick();
    Check(!Active(), "native pauser suppression disables bars");
    Store8(pauser + 6, 0); EngineTick();
    Check(Active(), "visible pause restores bars");
    Store8(pauseObject + 0x5D, 0); EngineTick();
    Check(!Active(), "leaving pause restores full gameplay");

    // No RT object, pause, or mod/overlay setting participates in FMV playback.
    moviePlaying = true;
    Check(Active(), "guest FMV playback enables bars independently");
    moviePlaying = false;
    Check(!Active(), "FMV completion restores full gameplay");
    std::puts("cinematic bars: production signal wrappers passed");
}
