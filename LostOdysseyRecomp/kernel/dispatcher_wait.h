#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <span>
#include <vector>

#include <os/platform.h>

namespace kernel::wait {
inline constexpr uint32_t Success = 0, Timeout = 0x102, Invalid = 0xC000000D;
inline constexpr uint32_t Infinite = 0xFFFFFFFF;
// A thread in Multiple. It registers on the objects it waits for, so a signal
// wakes the threads waiting for that object. A domain shared by all multiple
// waits woke every such thread on every signal in the game: on the Switch that
// was thousands of context switches a second, taken from the busy threads.
struct MultipleWaiter {
    std::mutex mutex;
    std::condition_variable cv;
    bool signaled = false;
    void Wake() {
        { std::lock_guard lock(mutex); signaled = true; }
        cv.notify_one();
    }
};
class Target {
public:
    std::mutex mutex;
    std::condition_variable cv;
    virtual ~Target() = default;
    // Caller owns mutex. Inspecting readiness never consumes state.
    virtual bool Ready(uint32_t owner) const = 0;
    virtual void Consume(uint32_t owner) = 0;
    // Threads in Wait. Same rule as Changes: a waiter counts itself under the
    // mutex before it checks Ready, and a signaller changes the object under
    // the mutex before reading the count, so a zero count means nobody can be
    // asleep on cv. On the Switch every notify is a kernel call
    // (svcSignalProcessWideKey) even with no waiter, so skip it there (#14);
    // elsewhere a notify with no waiter is cheap and stays unconditional.
    std::atomic<uint32_t> waiters{0};
    void Notify() {
        if (!LO_PLATFORM_SWITCH || waiters.load()) cv.notify_all();
        if (multipleCount.load()) {
            std::lock_guard lock(mutex);
            for (MultipleWaiter* waiter : multipleWaiters) waiter->Wake();
        }
    }
    // Threads in Multiple, under mutex. The same rule as waiters: a thread
    // registers under the mutex that guards the state it just found not ready.
    std::vector<MultipleWaiter*> multipleWaiters;
    std::atomic<uint32_t> multipleCount{0};
    uint32_t Wait(uint32_t timeout, uint32_t owner = 0) {
        std::unique_lock lock(mutex);
        const auto ready = [&] { return Ready(owner); };
        if (!ready()) {
            waiters.fetch_add(1);
            struct Counted { std::atomic<uint32_t>& count; ~Counted() { count.fetch_sub(1); } } counted{waiters};
            if (timeout == Infinite) cv.wait(lock, ready);
            else if (!cv.wait_for(lock, std::chrono::milliseconds(timeout), ready)) return Timeout;
        }
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
    void Reset(uint32_t initial, uint32_t limit) {
        {
            std::lock_guard lock(mutex);
            count = initial;
            maximum = limit;
        }
        Notify();
    }
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
    MultipleWaiter self;
    bool registered = false;
    const auto unregister = [&] {
        if (!registered) return;
        for (auto it = ordered.begin(); it != uniqueEnd; ++it) {
            auto& list = (*it)->multipleWaiters;
            list.erase(std::find(list.begin(), list.end(), &self));
            (*it)->multipleCount.fetch_sub(1);
        }
    };
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
    for (;;) {
        // Cleared before the check: a wake after this leaves one more pass at most.
        { std::lock_guard lock(self.mutex); self.signaled = false; }
        {
            std::array<std::unique_lock<std::mutex>, 64> locks;
            for (auto it = ordered.begin(); it != uniqueEnd; ++it)
                locks[it - ordered.begin()] = std::unique_lock((*it)->mutex);
            if (all) {
                if (std::all_of(objects.begin(), objects.end(), [&](Target* p) { return p->Ready(owner); })) {
                    for (auto* p : objects) p->Consume(owner);
                    unregister();
                    return Success;
                }
            } else {
                for (size_t i = 0; i < objects.size(); ++i)
                    if (objects[i]->Ready(owner)) {
                        objects[i]->Consume(owner);
                        unregister();
                        return uint32_t(i);
                    }
            }
            if (!timeout || (timeout != Infinite && std::chrono::steady_clock::now() >= deadline)) {
                unregister();
                return Timeout;
            }
            if (!registered) {
                for (auto it = ordered.begin(); it != uniqueEnd; ++it) {
                    (*it)->multipleWaiters.push_back(&self);
                    (*it)->multipleCount.fetch_add(1);
                }
                registered = true;
            }
        }
        std::unique_lock lock(self.mutex);
        const auto woken = [&] { return self.signaled; };
        if (timeout == Infinite) self.cv.wait(lock, woken);
        else self.cv.wait_until(lock, deadline, woken);
    }
}
}
