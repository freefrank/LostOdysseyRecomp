#include "../../LostOdysseyRecomp/gpu/summary_collection.h"
#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
#include <memory>
#include <thread>

using namespace gpu::taa_collection::summary_collection;
static thread_local bool watch = false;
static thread_local size_t allocations = 0, frees = 0;
void* operator new(size_t size) {
    if (watch) ++allocations;
    if (void* pointer = std::malloc(size ? size : 1)) return pointer;
    throw std::bad_alloc();
}
void* operator new[](size_t size) { return ::operator new(size); }
void operator delete(void* pointer) noexcept { if (watch && pointer) ++frees; std::free(pointer); }
void operator delete[](void* pointer) noexcept { ::operator delete(pointer); }
static unsigned checks = 0;
static void Check(bool ok, const char* message) {
    ++checks;
    if (!ok) { std::cerr << "FAILED: " << message << '\n'; std::exit(1); }
}
static Key Record(uint64_t id, int priority = 4, uint32_t width = 1280) {
    gpu::position_evidence::Summary position;
    if (priority == -1) position.kind = 1;
    const int slot = priority <= 0 ? -1 : 4;
    const uint32_t candidates = priority == 0 ? 1 : 0;
    const uint32_t flags = priority <= 2 ? 3 : priority == 3 ? 1 : 0;
    const uint32_t rejection = priority == 1 ? 1 : 0;
    return {id, 5, width, 720, slot, candidates, flags, rejection, position, 31};
}

int main() {
    using Result = Queue::Observation;
    auto owner = std::make_unique<Queue>();
    auto& queue = *owner;
    std::mutex mutex;
    std::atomic<int> consent{1};
    const auto initialFree = queue.AvailableNodes();
    Check(initialFree >= NodePool::Nodes - 2, "only implementation sentinels occupy fresh pool");
    for (int priority = -1; priority <= 4; ++priority)
        Check(Priority(Record(0, priority)) == priority, "original priority classification retained");

    watch = true;
    for (uint32_t width = 1; width <= 4; ++width)
        Check(queue.TryObserve(mutex, consent, Record(1, 4, width)) == Result::Queued, "first four dimension variants retained");
    Check(queue.TryObserve(mutex, consent, Record(1, 4, 5)) == Result::Dropped, "fifth dimension variant dropped");
    Check(queue.TryObserve(mutex, consent, Record(1, 4, 1)) == Result::Counted, "existing variant increments without admission");
    Check(queue.Entries().at(Record(1, 4, 1)).count == 2 && queue.Families() == 1, "duplicate count and family identity retained");
    queue.Entries().at(Record(1, 4, 1)).count = 1000000000;
    Check(queue.TryObserve(mutex, consent, Record(1, 4, 1)) == Result::Counted &&
        queue.Entries().at(Record(1, 4, 1)).count == 1000000000, "draw count saturates");
    queue.Reset();
    Check(queue.AvailableNodes() == initialFree, "reset returns every data node");

    for (uint64_t id = 0; id < Capacity / 2; ++id)
        Check(queue.TryObserve(mutex, consent, Record(id)) == Result::Queued, "low-priority half capacity admitted");
    Check(queue.TryObserve(mutex, consent, Record(10000)) == Result::Dropped, "normal reports stop at half capacity");
    Check(queue.Families() == Capacity / 2, "family map stays bounded");
    for (uint64_t id = Capacity / 2; id < Capacity; ++id)
        Check(queue.TryObserve(mutex, consent, Record(id, 0)) == Result::Queued, "anomaly reports use reserved capacity");
    Check(queue.Entries().size() == Capacity, "total capacity enforced");
    Check(queue.TryObserve(mutex, consent, Record(50000, -1)) == Result::Queued, "strongest evidence replaces lower priority");
    Check(!queue.Entries().contains(Record(0)) && queue.Entries().contains(Record(50000, -1)), "old map-order victim preserved");
    for (uint64_t id = 1; id < Capacity / 2; ++id)
        Check(queue.TryObserve(mutex, consent, Record(50000 + id, 0)) == Result::Queued, "remaining normal entries replaced");
    Check(queue.TryObserve(mutex, consent, Record(90000, 0)) == Result::Dropped, "full equal-priority queue drops new key");
    Check(queue.TryObserve(mutex, consent, Record(90000, 1)) == Result::Dropped, "lower-priority report cannot evict anomaly");
    Check(queue.TryObserve(mutex, consent, Record(50000, -1)) == Result::Counted, "known reports still count when full");
    Check(queue.AvailableNodes() >= NodePool::Nodes - Capacity - Capacity / 2 - 2, "bounded node pool retains safety margin");
    queue.Reset();
    Check(queue.Entries().empty() && queue.Families() == 0 && queue.AvailableNodes() == initialFree, "reset recycles nodes after eviction");
    watch = false;
    Check(allocations == 0 && frees == 0, "all summary insert/update/eviction/reset paths avoid heap allocation and free");

    std::promise<void> entered, release;
    auto enteredFuture = entered.get_future();
    auto releaseFuture = release.get_future();
    std::thread holder([&] { std::lock_guard lock(mutex); entered.set_value(); releaseFuture.wait(); });
    Check(enteredFuture.wait_for(std::chrono::seconds(2)) == std::future_status::ready, "held-lock fixture ready");
    const auto begin = std::chrono::steady_clock::now();
    watch = true;
    const auto blocked = queue.TryObserve(mutex, consent, Record(1));
    consent = 0;
    const auto disabled = queue.TryObserve(mutex, consent, Record(1));
    watch = false;
    const auto elapsed = std::chrono::steady_clock::now() - begin;
    Check(blocked == Result::Busy && disabled == Result::Disabled, "contention and revocation skip sampling");
    Check(elapsed < std::chrono::milliseconds(100), "producer never waits for held snapshot lock");
    Check(queue.Entries().empty(), "dropped/disabled samples do not mutate queue");
    Check(allocations == 0 && frees == 0, "contended/disabled path performs no heap work");
    release.set_value();
    holder.join();
    consent = 1;
    Check(queue.TryObserve(mutex, consent, Record(1)) == Result::Queued, "later draw retries successfully");
    std::cout << "summary collection: " << checks << " checks passed; heap allocations " << allocations
        << ", heap frees " << frees << "; contention + opt-out "
        << std::chrono::duration<double, std::micro>(elapsed).count() << " us\n";
}
