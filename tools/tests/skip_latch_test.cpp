// Battle intro skip press latch (hid/skip_latch.h), with a fake clock and no SDL or guest.
#include <hid/skip_latch.h>
#include <cstdio>
#include <cstdlib>

using namespace hid::skip_latch;
using namespace std::chrono_literals;

namespace
{
constexpr uint16_t A = 0x1000, B = 0x2000, X = 0x4000, Up = 0x0001, Start = 0x0010;
int failures = 0;
int checks = 0;

void Expect(bool condition, const char* what)
{
    ++checks;
    if (!condition)
    {
        ++failures;
        std::printf("FAIL: %s\n", what);
    }
}

// One latch and a clock; each Read is one full GetState call.
struct Rig
{
    Latch latch;
    Clock::time_point now{};
    uint16_t cancel = B;
    int fired = 0;

    // raw: every source merged; final: the value after the host filters (raw by default).
    uint16_t Read(uint16_t raw, int final = -1)
    {
        uint16_t buttons = final < 0 ? raw : uint16_t(final);
        if (latch.Active() && latch.Sample(raw, buttons, cancel, now)) ++fired;
        return buttons;
    }
    // One tick: the viewport poll and the pad updater, then time moves on.
    uint16_t Tick(uint16_t raw, std::chrono::milliseconds step = 16ms, int final = -1)
    {
        const uint16_t first = Read(raw, final);
        const uint16_t second = Read(raw, final);
        Expect(first == second, "both reads of a tick agree");
        now += step;
        return first;
    }
    // Holds raw for the given time; returns true when every read hid bit.
    bool HoldHidden(uint16_t raw, std::chrono::milliseconds time, uint16_t bit)
    {
        bool hidden = true;
        for (auto end = now + time; now < end;)
            hidden = !(Tick(raw) & bit) && hidden;
        return hidden;
    }
};

// Armed, with one release sample seen.
void ArmReady(Rig& rig)
{
    rig.latch.SetArmed(true);
    rig.Tick(0);
}

void PressFiresAndHides()
{
    Rig rig;
    ArmReady(rig);
    uint16_t buttons = B | Up;
    Expect(rig.latch.Sample(B | Up, buttons, B, rig.now), "a new press while armed fires");
    Expect(buttons == Up, "the firing read hides the cancel bit and only it");
    Expect(rig.latch.TakePress() && !rig.latch.TakePress(), "the press is taken once");
    Expect(rig.HoldHidden(B, 10s, B), "every read of a 10 s hold is hidden");
    Expect(!rig.latch.TakePress(), "a hold fires once");
}

void ReleasePause()
{
    Rig rig;
    ArmReady(rig);
    rig.Tick(B);
    Expect(!(rig.Tick(0, 100ms) & B), "the release starts the pause");
    Expect(!(rig.Tick(B) & B) && rig.latch.Hidden(), "a press inside the pause is hidden and keeps the latch");
    rig.Tick(0, 249ms);
    rig.Tick(0, 1ms);
    Expect(rig.latch.Hidden(), "249 ms after the new release: still latched");
    rig.Tick(0);
    Expect(!rig.latch.Hidden(), "250 ms up ends the latch");
    Expect(!(rig.Tick(B) & B) && rig.fired == 2, "still armed: the next press after the pause fires again");
}

void ArmedWhileDown()
{
    Rig rig;
    rig.Tick(B);
    rig.latch.SetArmed(true);
    Expect(rig.Tick(B) == B && rig.fired == 0, "a button down at the arm passes and does not fire");
    rig.latch.SetArmed(false);
    rig.Tick(0);
    rig.latch.SetArmed(true);
    Expect(rig.Tick(B) == B && rig.fired == 0, "a release before the arm does not count");
    rig.Tick(0);
    Expect(!(rig.Tick(B) & B) && rig.fired == 1, "after a release, a press fires");
    rig.latch.SetArmed(false);
    rig.latch.SetArmed(true);
    Expect(!rig.latch.TakePress(), "a new arm drops a press that nobody took");
}

void DisarmKeepsHiding()
{
    Rig rig;
    ArmReady(rig);
    rig.Tick(B);
    // Prompt hide, battle end, map load and an option change all disarm.
    rig.latch.SetArmed(false);
    Expect(rig.HoldHidden(B, 2000ms, B), "the hold stays hidden after the disarm");
    rig.now += 60s;
    Expect(!(rig.Tick(B) & B), "time alone never ends the latch");
    rig.Tick(0, 249ms);
    Expect(!(rig.Tick(B) & B), "a press inside the pause stays hidden");
    Expect(rig.latch.Hidden() && rig.fired == 1, "nothing fires while disarmed");
}

void FocusLoss()
{
    Rig rig;
    ArmReady(rig);
    rig.Tick(B);
    // The keyboard is cleared, the key is still down: the pause starts.
    rig.latch.Restart();
    rig.Tick(0, 200ms);
    rig.latch.Restart();
    rig.Tick(0, 200ms);
    Expect(rig.latch.Hidden(), "focus loss restarts the pause and never ends the latch");
    rig.Tick(0, 50ms);
    rig.Tick(0);
    Expect(!rig.latch.Hidden(), "the latch ends 250 ms after the last restart");

    Rig pad;
    ArmReady(pad);
    pad.Tick(B);
    pad.Tick(0, 100ms);  // the pad is gone: a release
    pad.latch.Restart();
    Expect(!(pad.Tick(B) & B), "reconnect with the button down inside the pause: hidden");
    Expect(pad.HoldHidden(B, 1000ms, B) && pad.latch.Hidden(), "a pad held through a restart stays hidden");
}

void HostMenuOpen()
{
    Rig rig;
    ArmReady(rig);
    Expect(rig.Tick(B, 16ms, 0) == 0 && rig.fired == 0, "overlay open: the press does not fire");
    Expect(rig.Tick(B) == B && rig.fired == 0, "overlay closed, still held: no fire without a release");
    rig.Tick(0);
    rig.Tick(B);
    Expect(rig.fired == 1, "a new press fires");
    Expect(rig.Tick(B, 16ms, 0) == 0 && rig.Tick(B) == 0, "latched under a host menu: still hidden after it closes");
}

void OtherInputUntouched()
{
    Rig rig;
    for (uint16_t raw : {uint16_t(0), uint16_t(B), uint16_t(A | Start | Up)})
        Expect(rig.Tick(raw) == raw, "not armed: every bit passes");
    Expect(!rig.latch.Active(), "not armed and idle: inactive");
    ArmReady(rig);
    Expect(rig.Tick(B | A | X | Start | Up) == (A | X | Start | Up), "only the latched bit is hidden");

    // Confirmation button B / A: the game's cancel is A.
    Rig swapped;
    swapped.cancel = A;
    ArmReady(swapped);
    Expect(swapped.Tick(B) == B, "B/A: B passes");
    swapped.Tick(0);
    Expect(swapped.Tick(A | B) == B && swapped.fired == 1, "B/A: A fires, is hidden, B passes");
}
}

int main()
{
    PressFiresAndHides();
    ReleasePause();
    ArmedWhileDown();
    DisarmKeepsHiding();
    FocusLoss();
    HostMenuOpen();
    OtherInputUntouched();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
