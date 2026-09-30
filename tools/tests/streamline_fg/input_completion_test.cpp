#include "input_completion.h"

#include <cstdio>
#include <stdexcept>

namespace {
using Sync = probe::InputCompletion;
using State = Sync::State;
using Mode = Sync::Mode;
using Observation = Sync::Observation;
unsigned checks = 0;
void Check(bool ok, const char* why) {
    ++checks;
    if (!ok) throw std::runtime_error(why);
}
void BeginProtected(Sync& sync, uint64_t frame = 1, uint64_t epoch = 1) {
    Check(sync.BeginPresent(frame, epoch), "begin protected input ownership");
    Check(sync.Ownership() == State::Unresolved, "before present is unresolved");
    Check(sync.PresentReturned(true), "successful hooked present");
    Check(sync.Ownership() == State::QueuePending, "present is not GPU completion");
}
void Bootstrap(Sync& sync) {
    BeginProtected(sync);
    Check(sync.Observe(1, 1, true, {0x100, 7}) == Observation::QueueProtected, "observe real signal");
    Check(!sync.CanEnableExplicit(), "state observation does not retire input");
    Check(sync.QueueWaitFinished(true), "post-present queue fence completed");
    Check(sync.CanEnableExplicit(), "capability only ready after ordered retirement");
    Check(sync.EnableExplicit(), "opt-in explicit mode admitted");
    Check(sync.EffectiveMode() == Mode::ExplicitTimeline, "effective mode recorded");
}
void MissingBootstrap() {
    // Includes background/first-on/zero-signal observations. All reuse is
    // justified by the protected queue fence, never by a missing SDK signal.
    for (auto p : {Sync::Point{}, Sync::Point{0, 9}, Sync::Point{0x100, 0}}) {
        Sync sync;
        Check(!sync.EnableExplicit(), "cannot start in unprotected mode");
        BeginProtected(sync);
        Check(sync.Observe(1, 1, true, p) == Observation::QueueProtected, "missing bootstrap signal allowed only while protected");
        Check(!sync.TimelineWaitFinished(true), "host completion cannot impersonate provider completion");
        Check(!sync.QueueWaitFinished(false), "failed queue wait preserves input");
        Check(sync.Ownership() == State::QueuePending, "queue pending retained");
        Check(!sync.ResetMode(), "cannot reset pending input");
        Check(!sync.BeginPresent(2, 1), "cannot overwrite pending input");
        Check(sync.QueueWaitFinished(true), "queue wait retry retires");
        Check(sync.Idle() && !sync.CanEnableExplicit(), "no fabricated explicit capability");
        Check(sync.BeginPresent(2, 1), "ordinary protected path can continue");
    }
    Sync invalid;
    BeginProtected(invalid);
    Check(invalid.Observe(1, 1, false, {0x100, 8}) == Observation::QueueProtected, "invalid state cannot arm capability");
    Check(invalid.QueueWaitFinished(true) && !invalid.CanEnableExplicit(), "queue cleanup remains possible after SDK error");
}
void PendingTimeline() {
    Sync sync; Bootstrap(sync);
    Check(sync.BeginPresent(2, 1) && sync.PresentReturned(true), "explicit present");
    Check(sync.Observe(2, 1, true, {}) == Observation::Missing, "missing does not mean complete");
    Check(sync.Ownership() == State::Unresolved, "missing signal remains unresolved");
    Check(!sync.ResetMode() && !sync.EnableExplicit() && !sync.QueueWaitFinished(true), "cannot escape unresolved work using fallback");
    Check(sync.Observe(2, 1, true, {0x100, 9}) == Observation::Timeline, "same-frame delayed signal can resolve");
    Check(sync.Ownership() == State::TimelinePending, "SDK point is not a completed wait");
    Check(!sync.TimelineWaitFinished(false), "failed timeline wait retains pending state");
    Check(sync.PendingPoint() == Sync::Point{0x100, 9}, "failed wait retains exact semaphore/value");
    Check(!sync.BeginPresent(3, 1) && !sync.ResetMode(), "failed wait cannot allow reuse or mode change");
    Check(sync.TimelineWaitFinished(true) && sync.Idle(), "successful retry can retire");
    Check(!sync.TimelineWaitFinished(true), "duplicate wait cannot retire another frame");
    Check(sync.BeginPresent(3, 1) && sync.PresentReturned(true), "next explicit frame");
    Check(sync.Observe(3, 1, true, {0x100, 9}) == Observation::Missing, "unchanged point cannot prove the new frame complete");
    Check(!sync.TimelineWaitFinished(true), "stale point cannot permit input reuse");
    Check(sync.Observe(3, 1, true, {0x100, 10}) == Observation::Timeline, "fresh point after deferred query");
    Check(sync.TimelineWaitFinished(true), "fresh point uses actual wait");
    Check(sync.ResetMode() && !sync.CanEnableExplicit(), "Off invalidates fence capability");
    Check(sync.EffectiveMode() == Mode::PresentQueue, "next enable reboots safely");
    BeginProtected(sync, 4, 2);
    Check(sync.Observe(4, 2, true, {0x200, 1}) == Observation::QueueProtected, "recreated SDK fence only in fresh protected epoch");
    Check(sync.QueueWaitFinished(true) && sync.CanEnableExplicit(), "fresh epoch point validated independently");
}
void RejectBadIdentity() {
    for (unsigned test = 0; test < 6; ++test) {
        Sync sync; Bootstrap(sync);
        Check(sync.BeginPresent(2, 1) && sync.PresentReturned(true), "identity test explicit present");
        Observation result;
        switch (test) {
        case 0: result = sync.Observe(1, 1, true, {0x100, 9}); break;
        case 1: result = sync.Observe(2, 2, true, {0x100, 9}); break;
        case 2: result = sync.Observe(2, 1, false, {0x100, 9}); break;
        case 3: result = sync.Observe(2, 1, true, {0x200, 9}); break;
        case 4: result = sync.Observe(2, 1, true, {0x100, 6}); break;
        default: result = sync.Observe(2, 1, true, {0x100, 0}); break;
        }
        Check(result == Observation::Invalid, "stale/invalid/regressed state rejected");
        Check(sync.Ownership() == State::Unresolved, "bad identity cannot free input");
        Check(!sync.TimelineWaitFinished(true) && !sync.QueueWaitFinished(true), "wrong wait cannot resolve bad point");
    }
}
void FailureOrdering() {
    Sync sync;
    Check(sync.BeginPresent(1, 1), "start failed present");
    Check(!sync.PresentReturned(false), "failed present reported");
    Check(sync.Observe(1, 1, true, {0x100, 9}) == Observation::Invalid, "query after failed Present proves nothing");
    Check(!sync.ResetMode(), "failed Present cannot discard ownership");
    Sync marker;
    BeginProtected(marker);
    marker.OrderingSubmitFailed();
    Check(!marker.QueueWaitFinished(true), "earlier render fence cannot replace missing queue marker");
    Check(!marker.Idle() && !marker.CanEnableExplicit(), "failed marker remains fail-closed");
    Sync epochs; Bootstrap(epochs);
    Check(!epochs.BeginPresent(2, 2), "explicit mode cannot migrate to unbootstrapped swapchain");
    Check(epochs.ResetMode(), "drained reset allowed");
    BeginProtected(epochs, 2, 2);
    Check(epochs.Observe(2, 2, true, {}) == Observation::QueueProtected, "missing signal after resize remains protected");
    Check(epochs.QueueWaitFinished(true), "resize protected cleanup");
    Check(!epochs.BeginPresent(2, 2), "duplicate real frame rejected");
    Check(!epochs.BeginPresent(0, 2) && !epochs.BeginPresent(3, 0), "unknown frame/epoch rejected");
}
}
int main() {
    try { MissingBootstrap(); PendingTimeline(); RejectBadIdentity(); FailureOrdering(); }
    catch (const std::exception& e) { std::fprintf(stderr, "FAIL: %s (%u checks)\n", e.what(), checks); return 1; }
    std::printf("PASS: %u input-completion ownership checks (CPU, no SDK/GPU execution)\n", checks);
}
