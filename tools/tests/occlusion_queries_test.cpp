#include "gpu/occlusion_queries.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace gpu::occlusion;

static void Check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

// END - BEGIN as the guest's GetData computes it from ZPass A and B.
static uint32_t Result(const Record& begin, const Record& end)
{
    return end.zpassA + end.zpassB - begin.zpassA - begin.zpassB;
}

static std::vector<uint64_t> Results(std::initializer_list<uint64_t> values) { return values; }

// One query of one measured draw: returns the BEGIN write and the END write
// the event produced, if any.
static std::optional<Write> Issue(Tracker& tracker, uint32_t slot, uint64_t batch, uint32_t index, Write& begin, double scale = 1.0)
{
    begin = tracker.Begin(slot + kBeginOffset);
    tracker.DrawSeen();
    tracker.DrawMeasured({batch, index, scale});
    return tracker.End(slot);
}

int main()
{
    Check(IsBeginRecord(0x11b020) && !IsBeginRecord(0x11b000), "BEGIN record is the second half of a slot");
    Check(SlotOf(0x11b020) == 0x11b000 && SlotOf(0x11b000) == 0x11b000, "slot of a record");
    Check(SampleScale(1280, 720, 2560, 1440, 0) == 0.25, "2x internal resolution");
    Check(SampleScale(1280, 720, 1280, 720, 1) == 2.0, "2x MSAA counts samples");
    Check(SampleScale(0, 720, 1280, 720, 0) == 1.0, "unknown size keeps host samples");
    Check(GuestSamples(0.0) == 0 && GuestSamples(0.25) == 1 && GuestSamples(250.4) == 250, "rounding keeps any sample visible");
    Check(HostPolicy(nullptr) == Policy::Adaptive && HostPolicy("") == Policy::Adaptive && HostPolicy("host") == Policy::Adaptive,
        "adaptive host queries by default");
    Check(HostPolicy("fast") == Policy::Fast && HostPolicy("strict") == Policy::Strict, "fast and strict pin one mode");
    Check(!HostPolicy("grow") && !HostPolicy("none") && !HostPolicy("xenia"), "fake modes turn host queries off");

    {
        // Fast: answered at once with the record's last count, never zero.
        Tracker tracker(Mode::Fast);
        Write begin;
        auto end = Issue(tracker, 0x11b000, 1, 0, begin, 0.25);
        Check(end && end->apply && Result(begin.record, end->record) == 1, "unknown record reads 1");
        Check(!tracker.HasAwaited() && !tracker.Waiting(1), "fast answers never make the guest wait");
        auto reports = tracker.Complete(1, Results({1000}));
        Check(reports.size() == 1 && !reports[0].apply && reports[0].samples == 250 && reports[0].measured,
            "measured count reported, not written again");
        end = Issue(tracker, 0x11b000, 2, 0, begin);
        Check(end && Result(begin.record, end->record) == 250, "next answer is the last measured count");
        reports = tracker.Complete(2, Results({0}));
        Check(reports.size() == 1 && reports[0].samples == 0, "occluded count measured");
        end = Issue(tracker, 0x11b000, 3, 0, begin);
        Check(end && Result(begin.record, end->record) == 1, "a zero count answers 1: nothing is culled on it");
        Check(tracker.HasPending() && tracker.AbandonAll().size() == 1, "fast queries still collect their counts");
    }
    {
        // Strict: a measured query waits for its batch, then reports the scaled count.
        Tracker tracker(Mode::Strict);
        Write begin;
        Check(!Issue(tracker, 0x11b000, 1, 0, begin, 0.25), "measured END waits for host results");
        Check(begin.address == 0x11b020, "BEGIN written at its record");
        Check(tracker.HasAwaited() && tracker.Waiting(1) && !tracker.Waiting(2), "pending batch is known");
        Check(tracker.Complete(2, Results({7})).empty(), "another batch resolves nothing");
        const auto writes = tracker.Complete(1, Results({1000}));
        Check(writes.size() == 1 && writes[0].apply && writes[0].address == 0x11b000, "END written after completion");
        Check(Result(begin.record, writes[0].record) == 250, "host samples scaled to guest samples");
        Check(!tracker.HasPending() && !tracker.HasAwaited(), "resolved query leaves the queue");
    }
    {
        // Fast with owners (#118): the game's pool hands a record to whichever
        // object allocates first, so the answer follows the owner, not the slot.
        Tracker tracker(Mode::Fast);
        const uint64_t terrain = OwnerKey(0x823CC2D0, 0), sun = OwnerKey(0x823D5688, 0);
        auto issue = [&](uint32_t slot, uint64_t owner, uint64_t batch, Write& begin) {
            begin = tracker.Begin(slot + kBeginOffset, owner);
            tracker.DrawSeen();
            tracker.DrawMeasured({batch, 0, 1.0});
            return tracker.End(slot);
        };
        Write begin;
        issue(0x11b000, terrain, 1, begin);
        tracker.Complete(1, Results({500000}));
        issue(0x11b040, sun, 2, begin);
        tracker.Complete(2, Results({0}));
        // Next frame one fewer object is queried first: the sun takes the
        // terrain's old slot and must not read the terrain's count.
        auto end = issue(0x11b000, sun, 3, begin);
        Check(end && Result(begin.record, end->record) == 1, "sun keeps its own occluded count in another slot");
        tracker.Complete(3, Results({40}));
        end = issue(0x11b040, terrain, 4, begin);
        Check(end && Result(begin.record, end->record) == 500000, "terrain keeps its own count in another slot");
        tracker.Complete(4, Results({500000}));
        end = issue(0x11b080, sun, 5, begin);
        Check(end && Result(begin.record, end->record) == 40, "sun reads its latest count");
        tracker.Complete(5, Results({40}));
        end = issue(0x11b080, 0, 6, begin);
        Check(end && Result(begin.record, end->record) == 1, "a query without an owner keys on its slot");
    }
    {
        // Strict, fully occluded: zero samples make the guest cull, as on hardware.
        Tracker tracker(Mode::Strict);
        Write begin;
        Check(!Issue(tracker, 0x2000, 3, 5, begin), "occluded END waits");
        const auto writes = tracker.Complete(3, Results({0, 0, 0, 0, 0, 0}));
        Check(writes.size() == 1 && Result(begin.record, writes[0].record) == 0, "zero samples reported");
    }
    for (const Mode mode : {Mode::Fast, Mode::Strict}) {
        // A draw the renderer did not measure keeps the old visible answer.
        Tracker tracker(mode);
        Write begin = tracker.Begin(0x3020);
        tracker.DrawSeen();
        tracker.DrawMeasured({1, 0, 1.0});
        tracker.DrawSeen();
        const auto end = tracker.End(0x3000);
        Check(end.has_value() && Result(begin.record, end->record) == kUnmeasuredSamples, "unmeasured draw reads visible at once");
        Check(!tracker.HasPending(), "unmeasured query does not wait");
        // No draws, or an END without its BEGIN, cannot be measured either.
        begin = tracker.Begin(0x4020);
        const auto empty = tracker.End(0x4000);
        Check(empty.has_value() && Result(begin.record, empty->record) == kUnmeasuredSamples, "query without draws reads visible");
        const auto orphan = tracker.End(0x5000);
        Check(orphan.has_value() && orphan->address == 0x5000, "END without BEGIN written at once");
    }
    {
        // Strict: unavailable host results and abandoned batches read visible.
        Tracker tracker(Mode::Strict);
        Write first, second;
        Check(!Issue(tracker, 0x6000, 4, 1, first), "first query waits");
        Check(!Issue(tracker, 0x6040, 5, 0, second), "second query waits");
        auto writes = tracker.Complete(4, Results({12, ~0ull}));
        Check(writes.size() == 1 && !writes[0].measured && Result(first.record, writes[0].record) == kUnmeasuredSamples,
            "unavailable result reads visible");
        writes = tracker.Abandon(5);
        Check(writes.size() == 1 && Result(second.record, writes[0].record) == kUnmeasuredSamples, "abandoned batch reads visible");
    }
    {
        // Strict: a query whose draws span two submissions waits for both.
        Tracker tracker(Mode::Strict);
        const Write begin = tracker.Begin(0x7020);
        tracker.DrawSeen();
        tracker.DrawMeasured({8, 0, 1.0});
        tracker.DrawSeen();
        tracker.DrawMeasured({9, 0, 0.5});
        Check(!tracker.End(0x7000).has_value(), "split query waits");
        Check(tracker.Complete(8, Results({40})).empty(), "first half alone resolves nothing");
        const auto writes = tracker.Complete(9, Results({20}));
        Check(writes.size() == 1 && Result(begin.record, writes[0].record) == 50, "both halves summed");
    }
    {
        // Strict: the guest reissued the slot before the old result arrived.
        // The record belongs to the new query, so the old result is dropped.
        Tracker tracker(Mode::Strict);
        Write old, again;
        Check(!Issue(tracker, 0x8000, 10, 0, old), "old query waits");
        Check(!Issue(tracker, 0x8000, 11, 0, again), "new query waits");
        Check(tracker.Complete(10, Results({99})).empty(), "stale result not written");
        Check(tracker.HasAwaited(), "the new query still waits after the stale one is dropped");
        const auto writes = tracker.Complete(11, Results({33}));
        Check(writes.size() == 1 && Result(again.record, writes[0].record) == 33, "new query result written");
        Check(!tracker.HasAwaited(), "nothing waits once both are gone");
    }
    {
        // Adaptive switches modes between frames: a query left waiting by
        // Strict keeps waiting after the switch to Fast until its batch completes.
        Tracker tracker(Mode::Strict);
        Write strictBegin, fastBegin;
        Check(!Issue(tracker, 0xa000, 13, 0, strictBegin), "strict query waits");
        tracker.SetMode(Mode::Fast);
        Check(tracker.CurrentMode() == Mode::Fast, "mode switched");
        const auto fastEnd = Issue(tracker, 0xa040, 14, 0, fastBegin);
        Check(fastEnd && fastEnd->apply, "the next query is answered at once");
        Check(tracker.HasAwaited() && tracker.Waiting(13) && !tracker.Waiting(14), "only the strict query waits");
        const auto writes = tracker.Complete(13, Results({0}));
        Check(writes.size() == 1 && writes[0].apply && Result(strictBegin.record, writes[0].record) == 0,
            "the strict query gets its exact count");
        Check(!tracker.HasAwaited(), "no guest waits after the strict batch");
    }
    {
        // AdaptiveMode: frames of (measured, zero) completed queries.
        AdaptiveMode adaptive;
        auto run = [&](uint32_t frames, uint32_t measured, uint32_t zero) {
            uint32_t changes = 0;
            for (uint32_t i = 0; i < frames; ++i) {
                adaptive.Add(measured, zero);
                changes += adaptive.EndFrame();
            }
            return changes;
        };
        const uint32_t window = AdaptiveMode::kWindowFrames;
        const uint32_t switchFrames = window * AdaptiveMode::kSwitchWindows;
        Check(adaptive.Current() == Mode::Fast, "starts in fast");
        // A street: 354 queries, 15% zero.
        Check(run(switchFrames * 4, 354, 53) == 0 && adaptive.Current() == Mode::Fast, "few hidden objects stay fast");
        // Half hidden but only a few queries: nothing worth a sync.
        Check(run(switchFrames * 2, 100, 90) == 0 && adaptive.Current() == Mode::Fast, "few queries stay fast");
        // One mostly hidden window (a scene change passing by) is not enough.
        Check(run(window, 949, 760) == 0 && run(window, 354, 53) == 0 && adaptive.Current() == Mode::Fast,
            "a single window does not switch");
        // Loading screens and menus complete no queries: their frames do not count.
        Check(run(switchFrames * 2, 0, 0) == 0 && adaptive.Current() == Mode::Fast, "no queries, no decision");
        // A room seen through a doorway: 949 queries, 80% zero.
        Check(run(switchFrames - 1, 949, 760) == 0 && adaptive.Current() == Mode::Fast, "two full windows are needed");
        Check(run(1, 949, 760) == 1 && adaptive.Current() == Mode::Strict, "mostly hidden switches to strict");
        // Strict tests the hidden objects in batches: 290 queries, 35% zero.
        Check(run(switchFrames * 4, 290, 101) == 0 && adaptive.Current() == Mode::Strict, "strict holds at a lower zero share");
        Check(run(switchFrames * 2, 0, 0) == 0 && adaptive.Current() == Mode::Strict, "strict holds through a loading screen");
        // The view opens up: 560 queries, 1% zero.
        Check(run(switchFrames - 1, 560, 5) == 0 && run(1, 560, 5) == 1 && adaptive.Current() == Mode::Fast,
            "few hidden objects leave strict after two windows");
        const uint32_t firstCooldown = adaptive.Cooldown();
        Check(firstCooldown == AdaptiveMode::kCooldownFrames, "a long strict stretch leaves with the base cooldown");
        Check(run(firstCooldown + switchFrames - 1, 949, 760) == 0 && adaptive.Current() == Mode::Fast,
            "fast holds through the cooldown");
        Check(run(1, 949, 760) == 1 && adaptive.Current() == Mode::Strict, "strict again after the cooldown");
        // An edge case: mostly hidden in fast, too few zeros in strict.
        Check(run(switchFrames, 520, 49) == 1 && adaptive.Current() == Mode::Fast, "brief strict leaves again");
        Check(adaptive.Cooldown() == firstCooldown * 2, "a brief strict stretch doubles the cooldown");
        uint32_t flips = 0;
        for (int i = 0; i < 8; ++i) {
            flips += run(adaptive.Cooldown() + switchFrames, 949, 760);
            flips += run(switchFrames, 520, 49);
        }
        Check(flips == 16 && adaptive.Cooldown() == AdaptiveMode::kMaxCooldownFrames, "the cooldown backs off to its limit");
        // A long strict stretch resets the backoff.
        run(adaptive.Cooldown() + switchFrames, 949, 760);
        run(switchFrames * 4, 290, 101);
        Check(run(switchFrames, 560, 5) == 1 && adaptive.Cooldown() == AdaptiveMode::kCooldownFrames, "backoff reset");
    }
    {
        // Strict: shutdown releases every waiting guest with a visible answer.
        Tracker tracker(Mode::Strict);
        Write begin;
        Check(!Issue(tracker, 0x9000, 12, 0, begin), "query waits");
        const auto writes = tracker.AbandonAll();
        Check(writes.size() == 1 && writes[0].apply && Result(begin.record, writes[0].record) == kUnmeasuredSamples,
            "abandoned on shutdown");
        Check(!tracker.HasPending() && !tracker.Active(), "nothing left after shutdown");
    }
    std::cout << "PASS: occlusion query bookkeeping\n";
    return 0;
}
