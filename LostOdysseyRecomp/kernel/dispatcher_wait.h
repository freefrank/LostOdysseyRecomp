#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <span>

namespace kernel::wait {
inline constexpr uint32_t Success = 0, Timeout = 0x102, Invalid = 0xC000000D;
inline constexpr uint32_t Infinite = 0xFFFFFFFF;
// Only multiple waits share this notification domain. Single waits sleep on
// their own object, avoiding a global dispatcher lock on the common path.
struct Changes {
    std::mutex mutex;
    std::condition_variable cv;
    uint64_t epoch = 0;
    void Notify() {
        { std::lock_guard lock(mutex); ++epoch; }
        cv.notify_all();
    }
};
inline Changes changes;
class Target {
public:
    std::mutex mutex;
    std::condition_variable cv;
    virtual ~Target() = default;
    // Caller owns mutex. Inspecting readiness never consumes state.
    virtual bool Ready(uint32_t owner) const = 0;
    virtual void Consume(uint32_t owner) = 0;
    void Notify() { cv.notify_all(); changes.Notify(); }
    uint32_t Wait(uint32_t timeout, uint32_t owner = 0) {
        std::unique_lock lock(mutex);
        const auto ready = [&] { return Ready(owner); };
        if (timeout == Infinite) cv.wait(lock, ready);
        else if (!cv.wait_for(lock, std::chrono::milliseconds(timeout), ready)) return Timeout;
        Consume(owner);
        return Success;
    }
};
class Event final : public Target {
    bool manual, signaled;
public:
    Event(bool manualReset, bool initial) : manual(manualReset), signaled(initial) {}
    bool Ready(uint32_t) const override { return signaled; }
    void Consume(uint32_t) override { if (!manual) signaled = false; }
    bool Set() {
        bool previous;
        { std::lock_guard lock(mutex); previous = signaled; signaled = true; }
        Notify();
        return previous;
    }
    bool Reset() { std::lock_guard lock(mutex); bool previous = signaled; signaled = false; return previous; }
    bool IsSignaled() { std::lock_guard lock(mutex); return signaled; }
};
class Semaphore final : public Target {
    uint32_t count, maximum;
public:
    Semaphore(uint32_t initial, uint32_t limit) : count(initial), maximum(limit) {}
    bool Ready(uint32_t) const override { return count != 0; }
    void Consume(uint32_t) override { --count; }
    bool Release(uint32_t amount, uint32_t* previous) {
        {
            std::lock_guard lock(mutex);
            if (previous) *previous = count;
            if (!amount || amount > maximum - count) return false;
            count += amount;
        }
        Notify(); return true;
    }
};
class Mutant final : public Target {
    uint32_t owner = 0, recursion = 0;
public:
    explicit Mutant(uint32_t initialOwner = 0) : owner(initialOwner), recursion(initialOwner ? 1 : 0) {}
    bool Ready(uint32_t self) const override { return !owner || owner == self; }
    void Consume(uint32_t self) override { owner = self; ++recursion; }
    bool Release(uint32_t self, uint32_t* previous = nullptr) {
        bool released;
        {
            std::lock_guard lock(mutex);
            if (owner != self || !recursion) return false;
            if (previous) *previous = recursion;
            released = --recursion == 0;
            if (released) owner = 0;
        }
        if (released) Notify();
        return true;
    }
};
inline uint32_t Multiple(std::span<Target* const> objects, bool all,
    uint32_t timeout, uint32_t owner = 0)
{
    if (objects.empty() || objects.size() > 64) return Invalid;
    std::array<Target*, 64> ordered{};
    for (size_t i = 0; i < objects.size(); ++i) {
        if (!objects[i]) return Invalid;
        ordered[i] = objects[i];
    }
    auto end = ordered.begin() + objects.size();
    std::sort(ordered.begin(), end, std::less<Target*>{});
    const auto uniqueEnd = std::unique(ordered.begin(), end);
    if (all && uniqueEnd != end) return Invalid;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
    for (;;) {
        uint64_t observed;
        { std::lock_guard lock(changes.mutex); observed = changes.epoch; }
        {
            std::array<std::unique_lock<std::mutex>, 64> locks;
            for (auto it = ordered.begin(); it != uniqueEnd; ++it)
                locks[it - ordered.begin()] = std::unique_lock((*it)->mutex);
            if (all) {
                if (std::all_of(objects.begin(), objects.end(), [&](Target* p) { return p->Ready(owner); })) {
                    for (auto* p : objects) p->Consume(owner);
                    return Success;
                }
            } else {
                for (size_t i = 0; i < objects.size(); ++i)
                    if (objects[i]->Ready(owner)) { objects[i]->Consume(owner); return uint32_t(i); }
            }
        }
        // Snapshot before checking readiness; the predicate also covers a
        // signal between releasing the object locks and beginning this wait.
        if (!timeout || (timeout != Infinite && std::chrono::steady_clock::now() >= deadline)) return Timeout;
        std::unique_lock lock(changes.mutex);
        const auto changed = [&] { return changes.epoch != observed; };
        if (timeout == Infinite) changes.cv.wait(lock, changed);
        else if (!changes.cv.wait_until(lock, deadline, changed)) return Timeout;
    }
}
}
