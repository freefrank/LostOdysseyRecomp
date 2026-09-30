#pragma once

#include <cstdint>

namespace probe {

// SDK-independent state used by the actual probe. A successful Present, a host
// render fence and an FG input-completion fence are different events. Never
// infer a completed input from a mode request or numFramesActuallyPresented.
class InputCompletion {
public:
    enum class Mode { PresentQueue, ExplicitTimeline };
    enum class State { Idle, Unresolved, QueuePending, TimelinePending };
    enum class Observation { QueueProtected, Timeline, Missing, Invalid };
    struct Point {
        uintptr_t fence = 0;
        uint64_t value = 0;
        bool operator==(const Point&) const = default;
    };

private:
    Mode mode_ = Mode::PresentQueue;
    State state_ = State::Idle;
    Point candidate_{}, last_{}, pending_{};
    uint64_t frame_ = 0, epoch_ = 0;
    bool returned_ = false, ready_ = false;

public:
    Mode EffectiveMode() const { return mode_; }
    State Ownership() const { return state_; }
    Point PendingPoint() const { return pending_; }
    uint64_t Frame() const { return frame_; }
    uint64_t Epoch() const { return epoch_; }
    bool Idle() const { return state_ == State::Idle; }
    bool CanEnableExplicit() const {
        return Idle() && ready_ && mode_ == Mode::PresentQueue;
    }

    // Each enable/resize starts in the documented queue-blocking mode. The
    // caller applies SDK options only after old inputs have been retired.
    bool ResetMode() {
        if (!Idle()) return false;
        mode_ = Mode::PresentQueue;
        candidate_ = last_ = pending_ = {};
        returned_ = ready_ = false;
        return true;
    }
    bool EnableExplicit() {
        if (!CanEnableExplicit()) return false;
        mode_ = Mode::ExplicitTimeline;
        return true;
    }
    bool BeginPresent(uint64_t frame, uint64_t epoch) {
        if (!Idle() || !frame || !epoch ||
            (epoch_ == epoch && frame <= frame_) ||
            (epoch_ != 0 && epoch != epoch_ && mode_ != Mode::PresentQueue))
            return false;
        if (epoch != epoch_) { ready_ = false; last_ = {}; }
        frame_ = frame; epoch_ = epoch;
        returned_ = false; candidate_ = pending_ = {};
        state_ = State::Unresolved;
        return true;
    }
    bool PresentReturned(bool success) {
        if (state_ != State::Unresolved || returned_) return false;
        returned_ = success;
        if (success && mode_ == Mode::PresentQueue) state_ = State::QueuePending;
        return success;
    }
    void OrderingSubmitFailed() {
        // A missing post-Present queue marker cannot be replaced by the earlier
        // render submission fence, which predates Streamline's queued waits.
        state_ = State::Unresolved;
        returned_ = false;
        ready_ = false;
        candidate_ = {};
    }
    Observation Observe(uint64_t frame, uint64_t epoch, bool sdkValid, Point point) {
        if (!returned_ || frame != frame_ || epoch != epoch_ ||
            (state_ != State::QueuePending && state_ != State::Unresolved))
            return Observation::Invalid;
        if (state_ == State::QueuePending) {
            // A null or initial-zero signal is not a capability proof. Inputs
            // remain held until a post-Present fence on the protected queue.
            candidate_ = sdkValid && point.fence && point.value ? point : Point{};
            return Observation::QueueProtected;
        }
        if (!sdkValid) return Observation::Invalid;
        if (!point.fence) return Observation::Missing;
        // A changed object or regressed value in an enabled epoch cannot be
        // silently associated with the current frame. Mode/reset clears this
        // baseline only after retirement. An unchanged point is not fresh proof
        // for a newly presented frame, so retry without allowing input reuse.
        if (point.fence != last_.fence || point.value < last_.value)
            return Observation::Invalid;
        if (point.value == last_.value) return Observation::Missing;
        pending_ = point;
        state_ = State::TimelinePending;
        return Observation::Timeline;
    }
    bool QueueWaitFinished(bool success) {
        if (state_ != State::QueuePending || !success) return false;
        ready_ = candidate_.fence && candidate_.value;
        if (ready_) last_ = candidate_;
        candidate_ = {};
        state_ = State::Idle;
        return true;
    }
    bool TimelineWaitFinished(bool success) {
        if (state_ != State::TimelinePending || !success) return false;
        last_ = pending_;
        pending_ = {};
        state_ = State::Idle;
        return true;
    }
};

} // namespace probe
