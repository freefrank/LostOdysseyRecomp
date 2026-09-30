#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>

namespace xenos::retry {
// A failed module/temporary DXC error must not poison the success cache, nor
// launch DXC on every draw. Retry with capped exponential backoff; only a
// deterministic compiler failure is permanent for this process/cache identity.
class State {
public:
    using Clock = std::chrono::steady_clock;
private:
    unsigned failures = 0;
    bool permanent = false;
    Clock::time_point retryAt{};
public:
    bool Ready(Clock::time_point now = Clock::now()) const {
        return !permanent && now >= retryAt;
    }
    void Failed(bool deterministic, Clock::time_point now = Clock::now()) {
        permanent = deterministic;
        failures = std::min(failures + 1, 7u);
        retryAt = now + std::chrono::milliseconds(100u << (failures - 1));
    }
    void Succeeded() { failures = 0; permanent = false; retryAt = {}; }
};
class Attempt {
    State& state;
    bool success = false, deterministic = false;
public:
    explicit Attempt(State& value) : state(value) {}
    Attempt(const Attempt&) = delete;
    Attempt& operator=(const Attempt&) = delete;
    ~Attempt() { if (!success) state.Failed(deterministic); }
    void PermanentFailure(bool value = true) { deterministic = value; }
    void Succeeded() { state.Succeeded(); success = true; }
};
}
