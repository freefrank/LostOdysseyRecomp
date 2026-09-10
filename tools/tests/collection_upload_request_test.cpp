#include "../../LostOdysseyRecomp/gpu/collection_worker.h"
#include <cstdlib>
#include <future>
#include <iostream>
#include <new>

using namespace gpu::taa_collection;
static unsigned checks = 0;
static thread_local bool watch = false;
static thread_local size_t allocations = 0;
void* operator new(size_t size) {
    if (watch) ++allocations;
    if (void* pointer = std::malloc(size ? size : 1)) return pointer;
    throw std::bad_alloc();
}
void* operator new[](size_t size) { return ::operator new(size); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
static void Check(bool ok, const char* message) {
    ++checks;
    if (!ok) { std::cerr << "FAILED: " << message << '\n'; std::exit(1); }
}

int main() {
    UploadRequestGate gate;
    std::atomic<int> consent{-1};
    std::atomic<uint64_t> generation{1};
    watch = true;
    Check(!gate.Request(consent, generation), "undecided consent never requests upload");
    consent = 0;
    Check(!gate.Request(consent, generation), "disabled consent never requests upload");
    consent = 1;
    Check(gate.Request(consent, generation) && gate.Request(consent, generation), "repeated F1 accepted without allocations");
    const auto requested = gate.Consume(consent, generation);
    Check(requested && *requested == 1 && !gate.Consume(consent, generation), "repeated F1 coalesces into one flush");
    Check(gate.Request(consent, generation), "another F1 while uploader busy leaves one pending flush");
    consent = 0;
    ++generation;
    Check(!gate.Consume(consent, generation), "revocation drops pending flush");
    consent = 1;
    Check(!gate.Consume(consent, generation), "re-enable does not revive old request");
    Check(gate.Request(consent, generation), "new epoch can request upload");
    std::atomic<uint64_t> oldGeneration{1};
    Check(!gate.Request(consent, oldGeneration), "delayed older producer cannot overwrite new request");
    const auto newRequest = gate.Consume(consent, generation);
    Check(newRequest && *newRequest == 2, "new epoch request survives stale producer");
    ++generation;
    Check(*newRequest != generation, "snapshot can reject an epoch changed after consumption");
    watch = false;
    Check(allocations == 0, "force-upload gate performs no heap allocation");

    CollectionWorker::Control control;
    watch = true;
    control.Notify();
    control.Notify();
    control.Notify();
    watch = false;
    const auto beforeWait = std::chrono::steady_clock::now();
    Check(!control.Wait(std::chrono::seconds(2)), "pre-wait notification wakes a running worker");
    const auto wakeTime = std::chrono::steady_clock::now() - beforeWait;
    Check(wakeTime < std::chrono::milliseconds(100), "notification survives before wait starts");
    const auto emptyWait = std::chrono::steady_clock::now();
    Check(!control.Wait(std::chrono::milliseconds(40)), "duplicate notifications are coalesced");
    Check(std::chrono::steady_clock::now() - emptyWait >= std::chrono::milliseconds(25), "coalescing does not leave extra wake tokens");
    Check(allocations == 0, "notify performs no heap allocation");

    // Exercise the wait/notify transition repeatedly. Each handshake starts
    // from either a waiting consumer or a retained token, with no polling loop.
    CollectionWorker::Control race;
    std::binary_semaphore consumed{0};
    std::thread consumer([&] {
        for (unsigned i = 0; i < 100; ++i) {
            race.Wait(std::chrono::seconds(2));
            consumed.release();
        }
    });
    for (unsigned i = 0; i < 100; ++i) {
        race.Notify();
        Check(consumed.try_acquire_for(std::chrono::seconds(1)), "wake notification is never lost at wait transition");
    }
    consumer.join();

    std::promise<void> entered, done;
    auto enteredFuture = entered.get_future();
    auto doneFuture = done.get_future();
    auto sleeping = std::make_unique<CollectionWorker>([&](auto& state) {
        entered.set_value();
        if (state.Wait(std::chrono::hours(1))) done.set_value();
    });
    Check(enteredFuture.wait_for(std::chrono::seconds(2)) == std::future_status::ready, "stop fixture entered");
    const auto stopStart = std::chrono::steady_clock::now();
    watch = true;
    sleeping.reset();
    watch = false;
    const auto stopTime = std::chrono::steady_clock::now() - stopStart;
    Check(stopTime < std::chrono::milliseconds(100), "destructor still never joins worker");
    Check(doneFuture.wait_for(std::chrono::seconds(2)) == std::future_status::ready, "stop wakes long wait immediately");

    // A pending notification must not suppress a later stop, even if the
    // backend callback is stuck rather than waiting on the semaphore.
    std::promise<void> stalled, release, finished;
    auto stalledFuture = stalled.get_future();
    auto releaseFuture = release.get_future();
    auto finishedFuture = finished.get_future();
    std::atomic<bool> stopped{false};
    auto blocked = std::make_unique<CollectionWorker>([&](auto& state) {
        stalled.set_value();
        releaseFuture.wait();
        stopped = state.Stopped();
        finished.set_value();
    });
    Check(stalledFuture.wait_for(std::chrono::seconds(2)) == std::future_status::ready, "blocked callback fixture entered");
    blocked->Notify();
    const auto blockedStopStart = std::chrono::steady_clock::now();
    watch = true;
    blocked.reset();
    watch = false;
    Check(std::chrono::steady_clock::now() - blockedStopStart < std::chrono::milliseconds(100), "pending wake plus blocked HTTP cannot delay destruction");
    release.set_value();
    Check(finishedFuture.wait_for(std::chrono::seconds(2)) == std::future_status::ready && stopped,
        "callback observes stop despite an already-pending wake");
    Check(allocations == 0, "request/notify/stop paths perform no heap allocation");
    std::cout << "collection upload request: " << checks << " checks passed; notify wake "
        << std::chrono::duration<double, std::micro>(wakeTime).count() << " us; destructor stop "
        << std::chrono::duration<double, std::micro>(stopTime).count() << " us; allocations " << allocations << '\n';
}
