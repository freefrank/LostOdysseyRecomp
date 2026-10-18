#include <debug/soft_reset_policy.h>
#include <debug/fast_forward.h>
#include <stdexcept>
#include <cstdio>

namespace
{
void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void CheckFastForwardButtons()
{
    using namespace debug_menu::fast_forward;
    Check(Control{}.button == Button::LT && Status{}.button == Button::LT,
        "existing profiles default to the physical LT button");
    const Buttons distinct{91, 132, 255, 0, 255, 0};
    struct Mapping { Button button; uint8_t value, pair; };
    for (const auto mapping : {Mapping{Button::LT, 91, 132}, Mapping{Button::RT, 132, 91},
            Mapping{Button::LB, 255, 0}, Mapping{Button::RB, 0, 255},
            Mapping{Button::L3, 255, 0}, Mapping{Button::R3, 0, 0}})
    {
        const auto input = SelectButton(mapping.button, distinct);
        Check(input.button == mapping.value && input.pair == mapping.pair,
            "binding selects the physical input and its corresponding pair without changing analog values");
    }
    const auto fallback = SelectButton(static_cast<Button>(99), distinct);
    Check(fallback.button == distinct.lt && fallback.pair == distinct.rt,
        "invalid runtime button falls back to LT and its RT pair");

    struct Binding { Button button; uint8_t Buttons::*source; uint8_t Buttons::*pair; };
    for (const auto binding : {Binding{Button::LT, &Buttons::lt, &Buttons::rt},
            Binding{Button::RT, &Buttons::rt, &Buttons::lt},
            Binding{Button::LB, &Buttons::lb, &Buttons::rb},
            Binding{Button::RB, &Buttons::rb, &Buttons::lb},
            Binding{Button::L3, &Buttons::l3, nullptr},
            Binding{Button::R3, &Buttons::r3, nullptr}})
        for (const auto mode : {Mode::Hold, Mode::Toggle})
        {
            Control control;
            uint64_t now = 1;
            control.Enable(true, now++);
            control.SetButton(binding.button, now++);
            control.SetMode(mode, now++);
            Buttons buttons{255, 255, 255, 255, 255, 255};
            buttons.*binding.source = 0;
            if (binding.pair) buttons.*binding.pair = 0;
            const auto sample = [&] {
                const auto input = SelectButton(control.button, buttons);
                control.Sample(input.button, input.pair, true, now++);
                return control.Get(now).active;
            };
            Check(!sample(), "released selected button does not start fast-forward");
            buttons.*binding.source = 255;
            Check(sample(), "each selected button activates in Hold and Toggle despite unrelated held buttons");
            if (binding.pair)
            {
                buttons.*binding.pair = 255;
                Check(!sample(), "LT+RT and LB+RB suppress an active Hold or Toggle boost");
                buttons.*binding.pair = 0;
                Check(sample(), "releasing the pair restores an existing boost");
                control.Stop(now++);
                buttons.*binding.source = 0;
                Check(!sample(), "stopped boost arms on release");
                buttons.*binding.source = buttons.*binding.pair = 255;
                Check(!sample(), "a fresh paired press cannot activate fast-forward");
                if (mode == Mode::Toggle)
                {
                    buttons.*binding.pair = 0;
                    Check(!sample(), "a paired Toggle press does not latch a boost after releasing the pair");
                }
            }
        }
}

void CheckFastForwardTriggerHysteresis()
{
    using namespace debug_menu::fast_forward;
    for (const auto button : {Button::LT, Button::RT})
    {
        Control control;
        uint64_t now = 1;
        control.Enable(true, now++);
        control.SetButton(button, now++);
        const auto sample = [&](uint8_t value, uint8_t pair = 0) {
            Buttons buttons;
            (button == Button::LT ? buttons.lt : buttons.rt) = value;
            (button == Button::LT ? buttons.rt : buttons.lt) = pair;
            const auto input = SelectButton(button, buttons);
            control.Sample(input.button, input.pair, true, now++);
            return control.Get(now).active;
        };
        Check(!sample(255), "held trigger cannot arm after enabling or rebinding");
        Check(!sample(32) && !sample(63), "release threshold arms without pressing below 64");
        Check(sample(64), "analog trigger activates at the 64 press threshold");
        Check(sample(33), "analog trigger remains held above the 32 release threshold");
        Check(!sample(32), "analog trigger releases at 32");
        Check(sample(64, 32) && !sample(64, 33), "paired trigger blocks above the 32 release threshold");
    }
}

void CheckFastForwardBindingChange()
{
    using namespace debug_menu::fast_forward;
    for (const auto mode : {Mode::Hold, Mode::Toggle})
    {
        Control control;
        uint64_t now = 1;
        control.Enable(true, now++);
        control.SetMode(mode, now++);
        control.Sample(0, 0, true, now++);
        control.Sample(255, 0, true, now++);
        Check(control.Get(now).active, "boost starts before the binding changes");
        control.SetButton(Button::LB, now++);
        Check(!control.Get(now).active && control.Get(now).button == Button::LB,
            "changing the binding stops boost and publishes the selected button");
        control.Sample(255, 0, true, now++);
        Check(!control.Get(now).active, "new binding cannot inherit the previous binding's press");
        control.Sample(0, 0, true, now++);
        control.Sample(255, 0, true, now++);
        Check(control.Get(now).active, "new binding works after release and a fresh press");
        control.Stop(now++);
        control.Sample(0, 0, true, now++);
        control.SetButton(Button::RT, now++);
        control.Sample(255, 0, true, now++);
        Check(!control.Get(now).active, "new binding cannot inherit an armed release from the previous binding");
    }
}
}

int main()
{
    CheckFastForwardButtons();
    CheckFastForwardTriggerHysteresis();
    CheckFastForwardBindingChange();
    using namespace debug_menu::soft_reset;
    Snapshot ready;
    ready.engine = 0x100000;
    ready.world = 0x200000;
    ready.worldMode = ready.activeWorldMode = 2;
    ready.storageStage = 1;
    Check(Availability({}) == State::Unavailable, "startup cannot reset");
    Check(Availability(ready) == State::Ready, "loaded scene can reset");
    auto battle = ready;
    battle.battleQueued = true;
    battle.loading = true;
    Check(Availability(battle) == State::Battle, "queued encounter must refuse before scene commit");
    battle = ready;
    battle.worldMode = 3;
    Check(Availability(battle) == State::Battle, "battle start transition must refuse");
    battle.activeWorldMode = 3;
    Check(Availability(battle) == State::Battle, "battle and victory/results remain unavailable regardless of core phase");
    battle.worldMode = 2;
    Check(Availability(battle) == State::Battle, "return transition must refuse until active battle mode clears");
    battle.activeWorldMode = 2;
    Check(Availability(battle) == State::Ready, "returned field control can reset again");
    battle.worldMode = battle.activeWorldMode = 4;
    Check(Availability(battle) == State::Ready, "world map can reset");
    auto current = ready;
    current.loading = true;
    Check(Availability(current) == State::Loading, "queued/load/activation window must refuse");
    current = ready;
    current.pendingMap = 1;
    Check(Availability(current) == State::Loading, "pending travel must refuse");
    current = ready;
    current.storageStage = 47;
    Check(Availability(current) == State::StorageMenu, "open native save list must refuse");
    current.loading = true;
    Check(Availability(current) == State::StorageMenu, "storage screen notice takes precedence over load notice");
    current = ready;
    current.storageStage = 0;
    Check(Availability(current) == State::Ready, "closed storage stage zero can reset");
    current.storageIoBusy = true;
    Check(Availability(current) == State::Saving, "asynchronous storage I/O must refuse after UI closes");
    current = ready;
    current.reset = 16;
    Check(Availability(current) == State::Returning, "existing native reset cannot be overwritten");
    current = ready;
    current.title = 0x300000;
    Check(Availability(current) == State::Title, "title menu cannot start another reset");
    current = ready;
    current.settingsOpen = true;
    Check(Availability(current) == State::Unavailable, "host Settings must finish closing");
    current = ready;
    current.debugPending = true;
    Check(Availability(current) == State::Unavailable, "queued teleport or memory edit must finish first");
    current = ready;
    Check(SameScene(ready, current), "unchanged scene keeps its request");
    ++current.world;
    Check(!SameScene(ready, current), "world changed after confirmation must cancel");
    current = ready;
    ++current.engine;
    Check(!SameScene(ready, current), "engine changed after confirmation must cancel");

    // Reset stops the active boost without changing the player's preferences.
    using namespace debug_menu::fast_forward;
    Enable(true);
    SetMode(Mode::Toggle);
    SetButton(Button::RB);
    SetRate(8);
    SetKeepAudioPitch(false);
    Sample(0, 0, true);
    Sample(255, 0, true);
    Check(GetStatus().active, "toggle boost starts");
    Release();
    const auto stopped = GetStatus();
    Check(!stopped.active && AudioRate() == 1, "reset releases clock and audio boost");
    Check(stopped.enabled && stopped.mode == Mode::Toggle && stopped.button == Button::RB && stopped.multiplier == 8 && !stopped.keepAudioPitch,
        "reset preserves fast-forward preferences");
    Sample(255, 0, true);
    Check(!GetStatus().active, "held trigger cannot immediately rearm after reset");
    std::puts("soft reset refusal, scene ownership, fast-forward button mapping, hysteresis and preservation passed");
}
