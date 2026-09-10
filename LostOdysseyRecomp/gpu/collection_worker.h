#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <semaphore>
#include <thread>
#include <utility>

namespace gpu::taa_collection {

// One pending F1 flush per consent generation. A revoke/re-enable cannot turn a
// previously requested flush into a request under the new consent epoch.
class UploadRequestGate {
public:
    bool Request(const std::atomic<int>& consent, const std::atomic<uint64_t>& generation) noexcept {
        const auto epoch = generation.load();
        if (consent.load(std::memory_order_relaxed) != 1) return false;
        auto queued = requested_.load();
        for (;;) {
            // A delayed producer from an older epoch must not overwrite a
            // newer F1 request that was queued after revoke/re-enable.
            if (queued != None && queued >= epoch) return queued == epoch;
            if (requested_.compare_exchange_weak(queued, epoch)) return true;
        }
    }
    std::optional<uint64_t> Consume(const std::atomic<int>& consent, const std::atomic<uint64_t>& generation) noexcept {
        const auto requested = requested_.exchange(None);
        if (requested != None && requested == generation.load() &&
            consent.load(std::memory_order_relaxed) == 1) return requested;
        return {};
    }
private:
    static constexpr uint64_t None = ~uint64_t{};
    std::atomic<uint64_t> requested_{None};
};

// Collection is expendable. Stop/destruction must never make the game wait for
// a stalled network request. The detached thread owns its control and captured
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
        std::thread([control = control_, function = std::move(function)]() mutable {
            try { function(*control); } catch (...) { /* Optional collection cannot crash the game. */ }
        }).detach();
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
