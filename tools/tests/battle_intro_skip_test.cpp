// Decision table for the battle intro skip (patches/battle_intro_skip.h).
#include <patches/battle_intro_skip.h>
#include <cstdio>
#include <cstdlib>

using namespace battle_intro_skip;

namespace
{
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

// A Sleep of the intro actor in a normal formation, with the skip on.
SleepState Intro()
{
    SleepState s;
    s.enabled = true;
    s.mode = 2;
    s.phase = 0;
    s.sleepingActor = 0x3c2aa00;
    s.phaseActor = 0x3c2aa00;
    s.phaseSlot = 1;
    s.matinees = 0;
    s.start = StartClass::Start;
    s.battleId = 3;
    return s;
}
}

int main()
{
    Expect(ClassifyStart("bsSMpha_Start") == StartClass::Start, "Start class");
    Expect(ClassifyStart("bsSMpha_StartNoVoice") == StartClass::StartNoVoice, "NoVoice class");
    Expect(ClassifyStart("bsSMpha_Start_OPCrowd") == StartClass::Other, "OPCrowd is excluded");
    Expect(ClassifyStart("") == StartClass::Other, "empty name is excluded");
    Expect(ClassifyStart("?") == StartClass::Other, "unreadable name is excluded");
    Expect(ClassifyStart("bsSMpha_Start2") == StartClass::Other, "similar name is excluded");

    Expect(CollapseSleep(Intro()), "intro sleep collapses");
    SleepState s = Intro();
    s.start = StartClass::StartNoVoice;
    Expect(CollapseSleep(s), "NoVoice intro sleep collapses");

    s = Intro();
    s.enabled = false;
    Expect(!CollapseSleep(s), "off keeps retail");
    s = Intro();
    s.mode = 1;
    Expect(!CollapseSleep(s), "mode 1 keeps retail");
    s = Intro();
    s.mode = 4;
    Expect(!CollapseSleep(s), "mode 4 keeps retail");
    for (uint32_t phase = 1; phase <= 13; ++phase)
    {
        s = Intro();
        s.phase = phase;
        Expect(!CollapseSleep(s), "a later phase keeps retail");
    }
    s = Intro();
    s.sleepingActor = 0x1000;
    Expect(!CollapseSleep(s), "another actor keeps retail");
    s = Intro();
    s.sleepingActor = 0;
    s.phaseActor = 0;
    Expect(!CollapseSleep(s), "no phase actor keeps retail");
    s = Intro();
    s.phaseSlot = 2;
    Expect(!CollapseSleep(s), "another slot keeps retail");
    s = Intro();
    s.matinees = 1;
    Expect(!CollapseSleep(s), "an active matinee keeps retail");
    s = Intro();
    s.start = StartClass::Other;
    Expect(!CollapseSleep(s), "OPCrowd keeps retail");
    for (int32_t id : {-1, 255, 256})
    {
        s = Intro();
        s.battleId = id;
        Expect(!CollapseSleep(s), "an excluded battle id keeps retail");
    }
    for (int32_t id : {0, 254, 257, 398})
    {
        s = Intro();
        s.battleId = id;
        Expect(CollapseSleep(s), "other battle ids collapse");
    }

    Expect(SleepEndsThisTick(Intro(), 0), "no load tasks: the sleep needs no tick");
    Expect(!SleepEndsThisTick(Intro(), 1), "load tasks left: the polling loop gets a tick");
    s = Intro();
    s.enabled = false;
    Expect(!SleepEndsThisTick(s, 0), "off keeps the retail tick");
    s = Intro();
    s.phase = 1;
    Expect(!SleepEndsThisTick(s, 0), "a later phase keeps the retail tick");
    s = Intro();
    s.start = StartClass::Other;
    Expect(!SleepEndsThisTick(s, 0), "OPCrowd keeps the retail tick");
    Expect(QueueTicksPerFrame > 1, "the queue ticks more than once per frame");

    // The running Sleep at the press.
    Expect(EndRunningSleep(Intro(), SleepLatentOp, 0), "a running intro Sleep ends at the press");
    Expect(!EndRunningSleep(Intro(), 0, 0), "no latent Sleep: nothing to end");
    Expect(!EndRunningSleep(Intro(), SleepLatentOp, WaitBit), "a Wait keeps its condition");
    Expect(EndRunningSleep(Intro(), SleepLatentOp, 0x1), "other actor +544 bits do not matter");
    s = Intro();
    s.enabled = false;
    Expect(!EndRunningSleep(s, SleepLatentOp, 0), "no press: the Sleep keeps its time");
    s = Intro();
    s.phase = 1;
    Expect(!EndRunningSleep(s, SleepLatentOp, 0), "a later phase keeps its Sleep");
    s = Intro();
    s.sleepingActor = 0x1000;
    Expect(!EndRunningSleep(s, SleepLatentOp, 0), "another actor keeps its Sleep");

    // Random gate.
    Expect(RecordRequest(43, true, -1).random, "the walking draw is random");
    Expect(RecordRequest(43, true, -1).id == 43, "the record keeps the formation");
    Expect(!RecordRequest(43, false, -1).random, "a script, world map, retry or opening request is not random");
    Expect(!RecordRequest(43, true, 43).random, "a battle tour formation in the walking draw is not random");
    Expect(RecordRequest(44, true, 43).random, "an old tour formation does not change another draw");
    Expect(RandomBattle(RecordRequest(43, true, -1), 43), "the requested battle is random");
    Expect(!RandomBattle(RecordRequest(43, true, -1), 44), "a record for another battle does not count");
    Expect(!RandomBattle(BattleRequest{}, -1), "no record: not random");
    Expect(!RandomBattle(BattleRequest{}, 43), "a used record: not random");
    Expect(!RandomBattle(RecordRequest(43, false, -1), 43), "a scripted record: not random");

    // Window eligibility and visibility.
    WindowInput in{Option::Press, true, Intro()};
    in.state.enabled = false;
    Expect(WindowEligible(in), "random battle, Press to skip, normal intro: eligible");
    Expect(WindowVisible(in, false), "from the spawn to the press: visible (no minimum time, no capture)");
    Expect(!WindowVisible(in, true), "pressed: hidden at once");
    WindowInput w = in;
    w.option = Option::Off;
    Expect(!WindowEligible(w), "option Off: never armed");
    w = in;
    w.random = false;
    Expect(!WindowEligible(w), "not random: never armed");
    w = in;
    w.state.phase = 1;
    Expect(!WindowVisible(w, false), "phase 1 hides it");
    w = in;
    w.state.start = StartClass::Other;
    Expect(!WindowEligible(w), "OPCrowd: never armed");
    w = in;
    w.state.matinees = 1;
    Expect(!WindowEligible(w), "an active matinee: never armed");
    for (int32_t id : {-1, 255, 256})
    {
        w = in;
        w.state.battleId = id;
        Expect(!WindowEligible(w), "an excluded battle id: never armed");
    }
    w = in;
    w.state.sleepingActor = w.state.phaseActor = 0;
    Expect(!WindowEligible(w), "no phase actor: never armed");
    w = in;
    w.state.mode = 1;
    Expect(!WindowEligible(w), "not in battle mode: never armed");
    Expect(!AutoSkip(in, false), "Press to skip never starts the skip by itself");

    // Always: the same gate and exclusions, no prompt, the skip starts at once.
    WindowInput always = in;
    always.option = Option::Always;
    Expect(WindowEligible(always), "Always, random battle, normal intro: eligible");
    Expect(AutoSkip(always, false), "Always starts the skip at the first eligible tick");
    Expect(!AutoSkip(always, true), "Always starts the skip once");
    Expect(!WindowVisible(always, false), "Always shows no prompt and arms no latch");
    w = always;
    w.option = Option::Off;
    Expect(!AutoSkip(w, false), "Off: no skip");
    w = always;
    w.random = false;
    Expect(!AutoSkip(w, false), "Always, not random (tour, boss, story): no skip");
    w = always;
    w.state.start = StartClass::Other;
    Expect(!AutoSkip(w, false), "Always, OPCrowd: no skip");
    w = always;
    w.state.start = StartClass::StartNoVoice;
    Expect(AutoSkip(w, false), "Always, NoVoice intro: skip");
    w = always;
    w.state.matinees = 1;
    Expect(!AutoSkip(w, false), "Always, an active matinee: no skip");
    for (int32_t id : {-1, 255, 256})
    {
        w = always;
        w.state.battleId = id;
        Expect(!AutoSkip(w, false), "Always, an excluded battle id: no skip");
    }
    w = always;
    w.state.phase = 1;
    Expect(!AutoSkip(w, false), "Always after phase 0: no skip");
    w = always;
    w.state.mode = 1;
    Expect(!AutoSkip(w, false), "Always, not in battle mode: no skip");
    w = always;
    w.state.sleepingActor = w.state.phaseActor = 0;
    Expect(!AutoSkip(w, false), "Always, no phase actor: no skip");

    // Prompt heartbeat.
    Expect(PromptFresh(1000, 1000), "a report this tick is fresh");
    Expect(PromptFresh(1000 + PromptStaleMs, 1000), "a report PromptStaleMs old is fresh");
    Expect(!PromptFresh(1001 + PromptStaleMs, 1000), "an older report is stale");
    Expect(!PromptFresh(999, 1000), "a report from the future is not used");
    Expect(!BattleTicksLost(0), "a battle tick in this engine tick keeps the latch");
    Expect(!BattleTicksLost(1), "one engine tick without a battle tick does not disarm");
    Expect(BattleTicksLost(MissedBattleTicks), "MissedBattleTicks engine ticks without one disarm");
    Expect(BattleTicksLost(MissedBattleTicks + 5), "more missed ticks still disarm");
    Expect(MissedBattleTicks >= 2 && MissedBattleTicks <= 4, "the watchdog acts within a few frames");

    // Intro sounds: record, block, pass.
    WindowInput snd{Option::Press, true, Intro()};
    Expect(IntroSoundEntry(snd, false) == SoundEntry::Record, "phase 0 of a skippable battle: record");
    Expect(IntroSoundEntry(snd, true) == SoundEntry::Block, "phase 0 after the press: block");
    w = snd;
    w.state.phase = 1;
    Expect(IntroSoundEntry(w, true) == SoundEntry::Pass, "phase 1 after the press: boss voices play");
    w.state.phase = 2;
    Expect(IntroSoundEntry(w, false) == SoundEntry::Pass, "battle sounds play");
    w = snd;
    w.option = Option::Off;
    Expect(IntroSoundEntry(w, false) == SoundEntry::Pass, "option Off: retail, no record");
    w = snd;
    w.random = false;
    Expect(IntroSoundEntry(w, true) == SoundEntry::Pass, "not random (boss, tour): retail");
    w = snd;
    w.state.start = StartClass::Other;
    Expect(IntroSoundEntry(w, false) == SoundEntry::Pass, "OPCrowd: retail");
    w = snd;
    w.state.matinees = 1;
    Expect(IntroSoundEntry(w, true) == SoundEntry::Pass, "an active matinee: retail");
    w = snd;
    w.state.battleId = 255;
    Expect(IntroSoundEntry(w, true) == SoundEntry::Pass, "an excluded battle id: retail");
    w = snd;
    w.state.start = StartClass::StartNoVoice;
    Expect(IntroSoundEntry(w, true) == SoundEntry::Block, "NoVoice intro of a random battle: block");
    w = snd;
    w.option = Option::Always;
    Expect(IntroSoundEntry(w, false) == SoundEntry::Record, "Always before its start: record");
    Expect(IntroSoundEntry(w, true) == SoundEntry::Block, "Always after its start: block");
    w.random = false;
    Expect(IntroSoundEntry(w, true) == SoundEntry::Pass, "Always, not random: retail");
    Expect(MaxIntroSounds >= 16, "room above the measured peak of 13");

    // Low-level handles.
    const uint32_t k1 = 0x3101;   // slot 1, kind 1, generation 3
    const uint32_t k4 = 0x7405;   // slot 5, kind 4, generation 7
    const uint32_t k3 = 0x2300;   // stream
    Expect(SoundKind(k1) == 1 && SoundGeneration(k1) == 3, "handle fields");
    Expect(FadableKind(k1) && FadableKind(k4), "kind 1 and 4 fade");
    Expect(!FadableKind(k3), "music (kind 3) never fades");
    Expect(!FadableKind(0x1205) && !FadableKind(0x1505) && !FadableKind(0), "other kinds and no handle do not fade");
    Expect(!FadableKind(0x00104101), "bits 20-27 select other paths: no fade");

    const SoundSlot live{2, 3, 0, 10};
    Expect(SlotPlays(k1, live), "a live slot with the generation plays");
    SoundSlot slot = live;
    slot.generation = 4;
    Expect(!SlotPlays(k1, slot), "stale generation: another sound owns the slot");
    slot = live;
    slot.state = 0;
    Expect(!SlotPlays(k1, slot), "free slot");
    slot = live;
    slot.state = 0xFF;
    Expect(!SlotPlays(k1, slot), "reserved slot");
    slot = live;
    slot.status = SlotEnded;
    Expect(!SlotPlays(k1, slot), "ended sound");
    slot = live;
    slot.flags = SlotReleasing;
    Expect(!SlotPlays(k1, slot), "release already runs");

    Expect(IntroSoundAtPress(1, 0, SoundSlot{}) == PressAction::End, "not started: state 3");
    Expect(IntroSoundAtPress(2, k1, live) == PressAction::Fade, "playing kind 1: fade");
    Expect(IntroSoundAtPress(2, k4, SoundSlot{2, 7, 0, 10}) == PressAction::Fade, "playing kind 4: fade");
    Expect(IntroSoundAtPress(2, k3, SoundSlot{6, 2, 0, 10}) == PressAction::None, "music: never");
    Expect(IntroSoundAtPress(2, k1, slot) == PressAction::None, "releasing: no call");
    slot = live;
    slot.generation = 9;
    Expect(IntroSoundAtPress(2, k1, slot) == PressAction::None, "stale handle: no call");
    Expect(IntroSoundAtPress(3, k1, live) == PressAction::None, "ended object: the manager deletes it");
    Expect(IntroFadeSeconds == 0.25f, "fade time is 0.25 s");

    Expect(!SweepDue(1000, 1000), "no sweep at the press");
    Expect(!SweepDue(1000 + SweepAfterNs - 1, 1000), "no sweep before 0.5 s");
    Expect(SweepDue(1000 + SweepAfterNs, 1000), "sweep at 0.5 s of guest time");
    Expect(!SweepDue(500, 1000), "a clock before the press is not due");
    Expect(SweepAfterNs == 500000000, "sweep after 0.5 s");
    Expect(SweepStops(k1) && SweepStops(k4) && SweepStops(0), "the sweep stops SE and unstarted objects");
    Expect(!SweepStops(k3), "the sweep never stops a stream");

    Expect(KeepIntroSounds(0, false), "phase 0 keeps the records");
    Expect(!KeepIntroSounds(1, false), "phase 1 without a press clears them");
    Expect(KeepIntroSounds(1, true), "a pending sweep keeps them into phase 1");
    Expect(KeepIntroSounds(2, true), "and into phase 2");
    Expect(!KeepIntroSounds(11, false), "the battle end clears them");

    // Early window: it opens at the request of a random walking battle (Press to skip).
    const BattleRequest walk = RecordRequest(43, true, -1);
    Expect(EarlyWindowOpens(Option::Press, walk, StartClass::Start), "Press, walking draw: opens at the request");
    Expect(EarlyWindowOpens(Option::Press, walk, StartClass::StartNoVoice), "a NoVoice formation: opens");
    Expect(!EarlyWindowOpens(Option::Off, walk, StartClass::Start), "Off: no early window");
    Expect(!EarlyWindowOpens(Option::Always, walk, StartClass::Start), "Always: no prompt, no early window");
    Expect(!EarlyWindowOpens(Option::Press, walk, StartClass::Other), "OPCrowd or unknown class: no early window");
    Expect(!EarlyWindowOpens(Option::Press, RecordRequest(43, false, -1), StartClass::Start),
        "a script, retry or world map request: no early window");
    Expect(!EarlyWindowOpens(Option::Press, RecordRequest(43, true, 43), StartClass::Start),
        "a battle tour formation: no early window");
    Expect(!EarlyWindowOpens(Option::Press, BattleRequest{}, StartClass::Start), "no record: no early window");
    for (uint32_t id : {255u, 256u, 0xFFFFFFFFu})
        Expect(!EarlyWindowOpens(Option::Press, RecordRequest(id, true, -1), StartClass::Start),
            "an excluded battle id: no early window");

    // It ends without a spawn when the request is cancelled before the loader picks it,
    // or after the limit.
    Expect(EarlyWindowEnds(false, 1, 0) == EarlyEnd::None, "a queued request keeps it open");
    Expect(EarlyWindowEnds(true, 0, 300) == EarlyEnd::None, "the loader picked it and cleared the slot: open");
    Expect(EarlyWindowEnds(true, 2, EarlyWindowMaxMs - 1) == EarlyEnd::None, "a battle that loads keeps it open");
    Expect(EarlyWindowEnds(false, 0, 10) == EarlyEnd::Gone, "the request was cancelled: it ends");
    Expect(EarlyWindowEnds(false, -1, 10) == EarlyEnd::Gone, "an empty slot before the pick: it ends");
    Expect(EarlyWindowEnds(true, 0, EarlyWindowMaxMs) == EarlyEnd::Timeout, "no spawn in time: it ends");
    Expect(EarlyWindowEnds(false, 1, EarlyWindowMaxMs) == EarlyEnd::Timeout, "a request never picked: it ends");
    Expect(EarlyWindowMaxMs >= 2000 && EarlyWindowMaxMs <= 10000, "the limit has a margin over the load time");

    // A press before the spawn acts at the first tick in phase 0 that can skip.
    WindowInput first{Option::Press, true, Intro()};
    Expect(StoredPressAction(first, false) == StoredPress::None, "no stored press: nothing to do");
    Expect(StoredPressAction(first, true) == StoredPress::Apply, "eligible intro: the press acts at once");
    w = first;
    w.state.matinees = 1;
    Expect(StoredPressAction(w, true) == StoredPress::Keep, "a matinee runs: the press waits");
    w = first;
    w.state.mode = 1;
    Expect(StoredPressAction(w, true) == StoredPress::Keep, "not in battle mode yet: the press waits");
    w = first;
    w.state.sleepingActor = w.state.phaseActor = 0;
    Expect(StoredPressAction(w, true) == StoredPress::Keep, "no phase actor yet: the press waits");
    w = first;
    w.state.start = StartClass::Other;
    Expect(StoredPressAction(w, true) == StoredPress::Keep, "OPCrowd: never eligible, kept until phase 1");
    w.state.phase = 1;
    Expect(StoredPressAction(w, true) == StoredPress::Drop, "OPCrowd at phase 1: dropped");
    w = first;
    w.state.phase = 1;
    Expect(StoredPressAction(w, true) == StoredPress::Drop, "phase 0 is over: dropped");
    w = first;
    w.random = false;
    Expect(StoredPressAction(w, true) == StoredPress::Drop, "not this random battle: dropped");
    w = first;
    w.option = Option::Off;
    Expect(StoredPressAction(w, true) == StoredPress::Drop, "the option changed to Off: dropped");
    w = first;
    w.option = Option::Always;
    Expect(StoredPressAction(w, true) == StoredPress::Drop, "Always starts by itself: the press is not used");

    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
