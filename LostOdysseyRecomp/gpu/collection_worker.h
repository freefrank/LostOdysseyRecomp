#pragma once

#include <atomic>
#include <os/detach_thread.h>
#include <chrono>
#include <cstdint>
#include <memory>
#include <semaphore>
#include <thread>
#include <os/thread_name.h>
#include <utility>

namespace gpu::taa_collection {

// Background analysis is expendable. Stop/destruction must never make the game
// wait for a stalled callback. The detached thread owns its control and captured
// state until its callback exits; callbacks must not refer to application globals.
class CollectionWorker {
public:
    class Control {
    public:
        bool Stopped() const { return stopped_.load(std::memory_order_relaxed); }
        bool Wait(std::chrono::milliseconds duration) {
            if (Stopped()) return true;
            // A semaphore retains notifications sent before the wait starts.
            // Only a consumed token clears signaled_; clearing it on timeout
            // could race a release and overflow the binary semaphore.
            if (wake_.try_acquire_for(duration)) signaled_.store(false);
            return Stopped();
        }
        void Notify() noexcept {
            if (!signaled_.exchange(true)) wake_.release();
        }
        void Stop() noexcept {
            stopped_.store(true, std::memory_order_relaxed);
            Notify();
        }
    private:
        std::atomic<bool> stopped_{false};
        std::atomic<bool> signaled_{false};
        std::binary_semaphore wake_{0};
    };

    template<class Function> explicit CollectionWorker(Function function)
        : control_(std::make_shared<Control>()) {
        os::DetachThread(std::thread([control = control_, function = std::move(function)]() mutable {
            os::SetCurrentThreadName("Collect Worker");
            try { function(*control); } catch (...) { /* Optional collection cannot crash the game. */ }
        }));
    }
    ~CollectionWorker() { Stop(); }
    CollectionWorker(const CollectionWorker&) = delete;
    CollectionWorker& operator=(const CollectionWorker&) = delete;
    void Stop() noexcept { control_->Stop(); }
    void Notify() noexcept { control_->Notify(); }
private:
    std::shared_ptr<Control> control_;
};
}
