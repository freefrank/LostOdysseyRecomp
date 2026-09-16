#include <hid/button_quarantine.h>
#include <debug/menu_overlay.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <thread>

static void Require(bool condition, const char* message)
{
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

// XInput bits; the fixture needs no SDL controller or guest runtime.
constexpr uint16_t XAMINPUT_GAMEPAD_DPAD_UP = 1, XAMINPUT_GAMEPAD_DPAD_DOWN = 2;
constexpr uint16_t XAMINPUT_GAMEPAD_DPAD_LEFT = 4, XAMINPUT_GAMEPAD_DPAD_RIGHT = 8;
constexpr uint16_t XAMINPUT_GAMEPAD_LEFT_SHOULDER = 0x100, XAMINPUT_GAMEPAD_RIGHT_SHOULDER = 0x200;
constexpr uint16_t XAMINPUT_GAMEPAD_A = 0x1000, XAMINPUT_GAMEPAD_B = 0x2000;
static bool visible = false;
static std::function<void()> onResume;
namespace debug_menu
{
bool IsOverlayVisible() { return visible; }
void ToggleOverlay()
{
    visible = !visible;
    if (!visible && onResume) onResume();
}
void HandleInput(InputAction action)
{
    if (action == InputAction::Cancel) { visible = false; if (onResume) onResume(); }
}
}
#include "input_dispatch.inc"

int main()
{
    constexpr uint16_t B = XAMINPUT_GAMEPAD_B, A = XAMINPUT_GAMEPAD_A;
    constexpr uint16_t chord = XAMINPUT_GAMEPAD_LEFT_SHOULDER | XAMINPUT_GAMEPAD_RIGHT_SHOULDER;
    // The real production dispatcher invokes this callback while it closes the
    // menu. A reader runs immediately, before the dispatcher can do any more work.
    for (uint16_t close : {B, chord})
    {
        for (int iteration = 0; iteration < 128; ++iteration)
        {
            ProcessHostInput(0);
            visible = true;
            bool checked = false;
            onResume = [&] {
                std::thread guest([&] {
                    const auto before = s_buttonQuarantine.Capture();
                    Require(s_buttonQuarantine.Filter(close, before) == 0,
                            "close button escaped while the resume callback was still running");
                    checked = true;
                });
                guest.join();
            };
            ProcessHostInput(close);
            Require(checked && !visible, "close callback was not exercised");
            onResume = {};
            auto before = s_buttonQuarantine.Capture();
            Require(s_buttonQuarantine.Filter(close | A, before) == A,
                    "held close button leaked or unrelated A was suppressed");
            ProcessHostInput(0);
            before = s_buttonQuarantine.Capture();
            Require(s_buttonQuarantine.Filter(close, before) == close, "fresh press remained blocked after release");
        }
    }
    hid::ButtonQuarantine q;
    q.Consume(B);
    const auto heldSample = q.Capture();
    q.ObserveRelease(0);
    Require(q.Filter(B, heldSample) == 0, "release unmasked an old held sample");
    const auto beforeClose = q.Capture();
    q.Consume(chord);
    q.ObserveRelease(0);
    Require(q.Filter(chord, beforeClose) == 0, "sample spanning a whole close/release cycle leaked");

    // Guest filters cannot clear bits, even with stale or neutral samples.
    std::atomic<bool> done{false};
    std::thread reader([&] {
        while (!done.load()) { (void)q.Filter(0, beforeClose); (void)q.Filter(B, beforeClose); }
    });
    for (int i = 0; i < 10000; ++i)
    {
        q.ObserveRelease(0);
        q.Consume(B);
        const auto s = q.Capture();
        Require(q.Filter(B, s) == 0, "guest reader cleared a newly consumed button");
    }
    done = true;
    reader.join();
    std::puts("PASS: actual dispatcher close-before-resume (256 schedules), hold/release, stale samples, read-only concurrent filtering");
}
