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
}

int main()
{
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
    SetRate(8);
    SetKeepAudioPitch(false);
    Sample(0, 0, true);
    Sample(255, 0, true);
    Check(GetStatus().active, "toggle boost starts");
    Release();
    const auto stopped = GetStatus();
    Check(!stopped.active && AudioRate() == 1, "reset releases clock and audio boost");
    Check(stopped.enabled && stopped.mode == Mode::Toggle && stopped.multiplier == 8 && !stopped.keepAudioPitch,
        "reset preserves fast-forward preferences");
    Sample(255, 0, true);
    Check(!GetStatus().active, "held trigger cannot immediately rearm after reset");
    std::puts("soft reset refusal, scene ownership and fast-forward preservation passed");
}
