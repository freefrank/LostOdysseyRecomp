// Battle intro skip press latch (hid/skip_latch.h): rules R1-R10 and the cases of
// .claude research unknown_06 section 7, with a fake clock and no SDL or guest.
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
    void Wait(std::chrono::milliseconds time) { now += time; }
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
    Expect(rig.latch.Sample(B | Up, buttons, B, rig.now), "R3: a rising edge while armed fires");
    Expect(buttons == Up, "R3: the firing read hides the bit and only it");
    buttons = B | Up;
    rig.latch.Sample(B | Up, buttons, B, rig.now);
    Expect(buttons == Up, "R4: the second read of the tick is hidden");
    Expect(rig.latch.TakePress(), "R10: the press is taken once");
    Expect(!rig.latch.TakePress(), "R10: a second take is empty");
    Expect(rig.HoldHidden(B, 1000ms, B), "R4: every read of a 1 s hold is hidden");
    Expect(!rig.latch.TakePress(), "R10: a hold fires once");
}

void ReleasePause()
{
    Rig rig;
    ArmReady(rig);
    rig.Tick(B);
    Expect(rig.fired == 1, "fire");
    rig.latch.SetArmed(false);
    Expect(!(rig.Tick(0, 100ms) & B), "R5: the release starts the pause");
    Expect(!(rig.Tick(B, 16ms) & B), "R5: a press 100 ms after the release is hidden");
    Expect(rig.latch.Hidden(), "R5: the press keeps the latch");
    // The pause starts again at the next release.
    rig.Tick(0, 200ms);
    Expect(!(rig.Tick(0, 49ms) & B) && rig.latch.Hidden(), "R5: 249 ms after the new release: still latched");
    Expect(!(rig.Tick(B) & B), "R5: a press at 249 ms is hidden");
    rig.Tick(0, 250ms);
    Expect(rig.latch.Hidden(), "R5: the first up read only starts the pause");
    rig.Tick(0);
    Expect(!rig.latch.Hidden(), "R5: 250 ms up ends the latch");
    Expect(rig.Tick(B) & B, "after the pause, disarmed: the next press passes");
    Expect(rig.fired == 1, "a disarmed latch never fires");
}

void PauseEndThenArmedFires()
{
    Rig rig;
    ArmReady(rig);
    rig.Tick(B);
    rig.Tick(0, 250ms);
    rig.Tick(0);
    Expect(!rig.latch.Hidden(), "pause over");
    Expect(!(rig.Tick(B) & B) && rig.fired == 2, "armed: the next press after the pause fires again");
}

void SwappedButtons()
{
    Rig rig;
    rig.cancel = A;
    ArmReady(rig);
    Expect(rig.Tick(B) == B, "B/A: B passes");
    rig.Tick(0);
    Expect(rig.Tick(A | B) == B && rig.fired == 1, "B/A: A fires, is hidden, B passes");
    Expect(rig.Tick(A | B) == B, "B/A: A stays hidden, B still passes");
}

void OptionChangeMidHold()
{
    Rig rig;
    ArmReady(rig);
    rig.Tick(B);
    rig.cancel = A;
    Expect(rig.Tick(A | B) == A, "R4: the bit chosen at the edge stays hidden after an option change");
    Expect(rig.latch.Bit() == B, "R4: the bit is not read again while latched");
    rig.Tick(A, 250ms);
    rig.Tick(A);
    Expect(!rig.latch.Hidden(), "the old bit's release ends the latch");
    Expect(rig.Tick(A) == A && rig.fired == 1, "the new bit, held: no fire without a release first");
}

void ArmedWhileDown()
{
    Rig rig;
    rig.Tick(B);
    rig.latch.SetArmed(true);
    Expect(rig.Tick(B) == B && rig.fired == 0, "R2: a button down at the arm never fires");
    Expect(rig.HoldHidden(B, 500ms, B) == false && rig.fired == 0, "R2: the held button passes");
    rig.Tick(0);
    Expect(!(rig.Tick(B) & B) && rig.fired == 1, "R2: after a release, a press fires");
}

void RearmNeedsRelease()
{
    Rig rig;
    ArmReady(rig);
    rig.latch.SetArmed(false);
    rig.Tick(B);
    rig.latch.SetArmed(true);
    Expect(rig.Tick(B) == B && rig.fired == 0, "R1: a new arm needs a release sample");
    rig.latch.SetArmed(false);
    rig.Tick(0);
    rig.latch.SetArmed(true);
    Expect(rig.Tick(B) == B && rig.fired == 0, "R1: a release before the arm does not count");
}

void RearmDropsPress()
{
    Rig rig;
    ArmReady(rig);
    rig.Tick(B);
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
    Expect(rig.HoldHidden(B, 2000ms, B), "R6: the hold stays hidden after the disarm");
    rig.Tick(0, 249ms);
    Expect(!(rig.Tick(B) & B), "R6: a press inside the pause stays hidden");
    Expect(rig.latch.Hidden() && rig.fired == 1, "R6: nothing fires while disarmed");
}

void NoTimerAlone()
{
    Rig rig;
    ArmReady(rig);
    rig.Tick(B);
    rig.Wait(60s);
    Expect(!(rig.Tick(B) & B), "R8: time alone never ends the latch");
    rig.latch.Restart();
    Expect(!(rig.Tick(B) & B), "R7: a source reset with the button down changes nothing");
}

void TwoSources()
{
    Rig rig;
    ArmReady(rig);
    uint16_t pad = B, keyboard = B;
    rig.Tick(pad | keyboard);
    pad = 0;
    rig.Tick(pad | keyboard, 300ms);
    Expect(!(rig.Tick(pad | keyboard) & B) && rig.latch.Hidden(), "one source still down: hidden, no pause");
    keyboard = 0;
    rig.Tick(pad | keyboard, 250ms);
    rig.Tick(pad | keyboard);
    Expect(!rig.latch.Hidden(), "every source up for 250 ms: the latch ends");
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
    Expect(rig.latch.Hidden(), "R7: focus loss restarts the pause and never ends the latch");
    rig.Tick(0, 50ms);
    rig.Tick(0);
    Expect(!rig.latch.Hidden(), "R7: the latch ends 250 ms after the last restart");

    Rig pad;
    ArmReady(pad);
    pad.Tick(B);
    pad.latch.Restart();
    Expect(pad.HoldHidden(B, 1000ms, B) && pad.latch.Hidden(), "R7: a pad held through focus loss stays hidden");
}

void Disconnect()
{
    Rig rig;
    ArmReady(rig);
    rig.Tick(B);
    rig.Tick(0, 100ms);  // the pad is gone: a release
    rig.latch.Restart();
    Expect(!(rig.Tick(B) & B), "reconnect with the button down inside the pause: hidden");
    rig.Tick(0, 249ms);
    Expect(rig.latch.Hidden(), "the reconnect press restarted the pause");
}

void Remap()
{
    Rig rig;
    ArmReady(rig);
    // The cancel bound to another key: the merged bit is still B, from that key.
    Expect(!(rig.Tick(B | X) & B) && rig.Tick(B | X) == X, "the final bit is hidden, the other bits pass");
    Rig unbound;
    ArmReady(unbound);
    Expect(unbound.Tick(X) == X && unbound.fired == 0, "no source gives the bit: no fire");
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
        Expect(rig.Tick(raw) == raw, "R9: not armed: every bit passes");
    Expect(!rig.latch.Active(), "R9: not armed and idle: inactive");
    ArmReady(rig);
    Expect(rig.Tick(B | A | X | Start | Up) == (A | X | Start | Up), "only the latched bit is hidden");
}

void LongHoldOneRequest()
{
    Rig rig;
    ArmReady(rig);
    for (int tick = 0; tick < 600; ++tick)
        Expect(!(rig.Tick(B) & B), "every read of a 10 s hold is hidden");
    Expect(rig.fired == 1, "R10: a 10 s hold gives one fire");
    Expect(rig.latch.TakePress() && !rig.latch.TakePress(), "R10: one request");
}
}

int main()
{
    PressFiresAndHides();
    ReleasePause();
    PauseEndThenArmedFires();
    SwappedButtons();
    OptionChangeMidHold();
    ArmedWhileDown();
    RearmNeedsRelease();
    RearmDropsPress();
    DisarmKeepsHiding();
    NoTimerAlone();
    TwoSources();
    FocusLoss();
    Disconnect();
    Remap();
    HostMenuOpen();
    OtherInputUntouched();
    LongHoldOneRequest();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
