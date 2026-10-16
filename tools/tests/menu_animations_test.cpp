// Exercise the production guest-call adapter without game data or a clock.
#include <debug/menu_animations.h>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <thread>

namespace
{
union Register { uint64_t u64 = 0; uint32_t u32; double f64; };
struct Context { Register r3, r4, f1; uint64_t lr = 0; };
void Check(bool ok, const char* message)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
}

int main()
{
    using namespace debug_menu::menu_animations;
    Context ctx;
    ctx.r4.u64 = 0xFEEDFACE12345678;
    ctx.lr = 0x822E0F98;
    const auto checkCall = [&](uint32_t address, Update kind, unsigned rate, double delta, double expected,
                               bool restore) {
        ctx.r3.u32 = address;
        ctx.f1.f64 = delta;
        const auto bits = ctx.f1.u64;
        int calls = 0;
        Dispatch(ctx, rate, kind, [&](Context& call) {
            ++calls;
            Check(std::bit_cast<uint64_t>(call.f1.f64) == std::bit_cast<uint64_t>(expected), "forwarded animation delta");
            Check(call.r3.u32 == address && call.r4.u64 == 0xFEEDFACE12345678 && call.lr == 0x822E0F98,
                  "adapter preserves the object, other arguments and return address");
            call.f1.f64 = 123; // Volatile output: Normal must forward it untouched.
        });
        Check(calls == 1, "one update per frame, no replay of input or sound events");
        Check(restore ? ctx.f1.u64 == bits : ctx.f1.f64 == 123, "delta restored only for accelerated calls");
    };
    const uint32_t title = CampMenu + 15292, panel = CampMenu + 14348;
    const uint32_t heapCell = 0x40012300;
    // The same visual updater is used by title, battle and world UI. Outside
    // the camp task it must forward their calls at every configured rate.
    for (unsigned rate : {1u, 2u, 4u})
        for (auto address : {title, panel, heapCell, FocusCursor, 0x832AD2B8u, 0x832CA0E0u})
            for (auto kind : {Update::Tween, Update::Panel})
                checkCall(address, kind, rate, 0.015625, 0.015625, false);

    {
        const ScopedCampUpdate camp(CampMenu);
        Check(campUpdate, "camp task enables local visual timing");
        for (unsigned rate : {1u, 2u, 4u})
        {
            checkCall(title, Update::Tween, rate, 0.015625, 0.015625 * rate, rate != 1);
            checkCall(panel, Update::Panel, rate, 0.015625, 0.015625 * rate, rate != 1);
            checkCall(heapCell, Update::Tween, rate, 0.015625, 0.015625 * rate, rate != 1);
            for (auto cursor : {FocusCursor, FormationCursor, SaveCursor, DreamsCursor})
                checkCall(cursor, Update::Tween, rate, 0.015625, 0.015625, false);
        }
        for (unsigned invalidRate : {0u, 3u, 8u, 0xFFFFFFFFu})
            checkCall(title, Update::Tween, invalidRate, 0.015625, 0.015625, false);
        for (double delta : {0.0, -0.0, -1.0, std::numeric_limits<double>::infinity(),
                             std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::max()})
            checkCall(title, Update::Tween, 4, delta, delta, false);

        ctx.r3.u32 = panel;
        ctx.f1.f64 = 0.015625;
        int panelCalls = 0, baseCalls = 0;
        Dispatch(ctx, 4, Update::Panel, [&](Context& call) {
            ++panelCalls;
            Check(call.f1.f64 == 0.0625, "panel timeline receives Faster delta");
            Dispatch(call, 4, Update::Tween, [&](Context& base) {
                ++baseCalls;
                Check(base.f1.f64 == 0.0625, "nested panel base is accelerated exactly once");
            });
            // The tile child is a different heap object, also updated by a
            // panel's already scaled f31. It must not get another multiplier.
            call.r3.u32 = heapCell;
            Dispatch(call, 4, Update::Panel, [&](Context& child) {
                Check(child.f1.f64 == 0.0625, "nested child tile timeline is accelerated once");
            });
        });
        Check(panelCalls == 1 && baseCalls == 1 && ctx.f1.f64 == 0.015625 && !scaledPanel,
              "panel restores delta and suppression before the next subsystem");
        {
            const ScopedCampUpdate unrelated(0x832AD2B8);
            checkCall(heapCell, Update::Tween, 4, 0.015625, 0.015625, false);
        }
        Check(campUpdate, "nested unrelated task restores its caller's scope");
        bool leaked = true;
        std::thread other([&] { leaked = campUpdate || scaledPanel; });
        other.join();
        Check(!leaked, "camp timing cannot leak to another guest thread");
    }
    Check(!campUpdate && !scaledPanel, "camp return restores all thread-local timing state");
    checkCall(heapCell, Update::Tween, 4, 0.015625, 0.015625, false);
    for (auto task : {FormationMenu, SaveLoadMenu})
    for (unsigned parentState : {0u, 6u, 7u, 8u, 15u, 16u, 17u})
    {
        const ScopedCampUpdate child(task, parentState);
        const bool belongs = task == FormationMenu ? parentState == 7 : parentState == 16;
        checkCall(heapCell, Update::Tween, 4, 0.015625, belongs ? 0.0625 : 0.015625, belongs);
    }
    Check(!campUpdate, "separate child task restores the frame's original timing scope");
    for (unsigned parentState : {0u, 7u, 14u, 16u})
    for (unsigned childState : {0u, 1u, 2u, 3u, 4u, 5u})
    {
        const ScopedCampUpdate dreams(DreamsMenu, parentState, childState);
        const bool transition = parentState == 14 && (childState == 2 || childState == 4);
        checkCall(heapCell, Update::Tween, 4, 0.015625, transition ? 0.0625 : 0.015625, transition);
    }
    Check(!campUpdate, "Dreams selector does not retain its scope for the story reader");
    std::puts("PASS camp call scope, isolated delta forwarding, Normal and nested panel timeline");
}
