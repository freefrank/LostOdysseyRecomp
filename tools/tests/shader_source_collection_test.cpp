#include "../../LostOdysseyRecomp/gpu/shader_source_collection.h"
#include "../../LostOdysseyRecomp/gpu/collection_worker.h"

#include <cstdlib>
#include <iostream>
#include <future>
#include <new>

static thread_local bool watchAllocations = false;
static thread_local size_t allocations = 0;
void* operator new(size_t size) {
    if (watchAllocations) ++allocations;
    if (void* pointer = std::malloc(size ? size : 1)) return pointer;
    throw std::bad_alloc();
}
void* operator new[](size_t size) { return ::operator new(size); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
// Match malloc-backed new when the native ABI selects sized deallocation.
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }

using namespace gpu::taa_collection::shader_sources;
static unsigned checks = 0;
static void Check(bool condition, const char* message) {
    ++checks;
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}
static uint64_t Hash(const std::vector<uint32_t>& words) {
    uint64_t value = 14695981039346656037ull;
    for (uint32_t word : words)
        for (unsigned shift = 0; shift < 32; shift += 8)
            value = (value ^ ((word >> shift) & 255)) * 1099511628211ull;
    return value;
}

int main() {
    using Result = Queue::Observation;
    std::vector<uint32_t> words{0x03020100, 0xfffefdfc, 0x34333231};
    const auto hash = Hash(words);

    const auto queueOwner = std::make_unique<Queue>();
    auto& queue = *queueOwner;
    Check(queue.Observe(true, hash, words.data(), words.size()) == Result::Full, "uninitialized queue never allocates on draw");
    queue.Initialize();
    watchAllocations = true;
    Check(queue.Observe(true, hash, words.data(), words.size()) == Result::Queued, "first VS admitted");
    Check(queue.Observe(true, hash, words.data(), words.size()) == Result::Known, "repeated VS deduplicated");
    Check(queue.Observe(false, hash, words.data(), words.size()) == Result::Queued, "stage is part of identity");
    Check(queue.Tracked() == 2 && queue.PendingBytes() == 24, "pending memory includes unique stage identities");
    watchAllocations = false;
    Check(allocations == 0, "first and known observations allocate no memory");
    const auto first = queue.Pending();
    Check(first.programs.size() == 2, "VS and PS both selected");
    Check(first.programs[0].bytes && first.programs[0].bytes->size() == 12 &&
        std::memcmp(first.programs[0].bytes->data(), words.data(), 12) == 0, "original memory bytes copied");

    // Pending copies are immutable even if guest memory is reused before the
    // consumer acknowledges them.
    words[0] ^= 0xffffffffu;
    Check(*queue.Pending().programs[0].bytes == *first.programs[0].bytes, "unacknowledged data is retained exactly");
    queue.Acknowledge(first);
    Check(queue.PendingBytes() == 0 && queue.Tracked() == 2, "ack releases raw and retains keys");
    Check(queue.Pending().programs.empty(), "acknowledged programs are not handed out again");
    Check(queue.Observe(true, hash, words.data(), words.size()) == Result::Known, "acknowledged ID avoids recopy");
    queue.Acknowledge(first);
    Check(queue.PendingBytes() == 0, "duplicate acknowledgement is idempotent");

    queue.Reset();
    Check(queue.Tracked() == 0 && queue.PendingBytes() == 0, "reset clears pending and acknowledged keys");
    Check(queue.Epoch() != first.epoch, "reset advances epoch");
    words[0] ^= 0xffffffffu;
    Check(queue.Observe(true, hash, words.data(), words.size()) == Result::Queued, "same ID after reset admitted");
    queue.Acknowledge(first);
    Check(queue.PendingBytes() == 12, "old batch cannot acknowledge current epoch");
    queue.Acknowledge(queue.Pending());
    Check(queue.PendingBytes() == 0, "current epoch batch acknowledged");

    const auto boundedOwner = std::make_unique<Queue>(2, 16);
    auto& bounded = *boundedOwner;
    bounded.Initialize();
    Check(bounded.Observe(true, 1, nullptr, 1) == Result::Invalid, "null source rejected");
    Check(bounded.Observe(true, 1, words.data(), 0) == Result::Invalid, "empty source rejected");
    Check(bounded.Observe(true, 1, words.data(), MaxProgramBytes / 4 + 1) == Result::Invalid, "oversized source rejected before read");
    Check(bounded.Observe(true, 1, words.data(), words.size()) == Result::Queued, "within raw budget admitted");
    Check(bounded.Observe(true, 2, words.data(), words.size()) == Result::Full, "raw budget enforced");
    Check(bounded.Tracked() == 1 && bounded.PendingBytes() == 12, "full admission does not mutate queue");
    bounded.Acknowledge(bounded.Pending());
    Check(bounded.Observe(true, 2, words.data(), words.size()) == Result::Queued, "later draw retries after room reclaimed");
    bounded.Acknowledge(bounded.Pending());
    Check(bounded.Observe(true, 3, words.data(), words.size()) == Result::Full, "acknowledged key index remains bounded");
    Check(bounded.Observe(true, 2, words.data(), words.size()) == Result::Known, "known ID works at index capacity");

    const auto manyOwner = std::make_unique<Queue>();
    auto& many = *manyOwner;
    many.Initialize();
    for (uint64_t i = 0; i < 40; ++i)
        Check(many.Observe(true, i, words.data(), 1) == Result::Queued, "small source admitted");
    const auto first32 = many.Pending();
    Check(first32.programs.size() == MaxBatchPrograms, "program count batch bound");
    many.Acknowledge(first32);
    Check(many.Pending().programs.size() == 8, "incremental remainder preserved");

    const auto largeOwner = std::make_unique<Queue>();
    auto& large = *largeOwner;
    large.Initialize();
    std::vector<uint32_t> maximum(MaxProgramBytes / 4, 0xdeadbeef);
    watchAllocations = true;
    for (uint64_t i = 0; i < MaxPendingBytes / MaxProgramBytes; ++i)
        Check(large.Observe(true, i, maximum.data(), maximum.size()) == Result::Queued, "slab page allocation");
    Check(large.PendingBytes() == MaxPendingBytes, "full slab uses exact bounded raw budget");
    Check(large.Observe(true, 100, maximum.data(), maximum.size()) == Result::Full, "full slab drops new observations");
    watchAllocations = false;
    Check(allocations == 0, "slab fill and full queue perform no allocations");
    large.Acknowledge(large.Pending());
    Check(large.PendingBytes() == MaxPendingBytes - MaxBatchPrograms * MaxProgramBytes, "ack returns all occupied pages");
    Check(large.Observe(true, 100, maximum.data(), maximum.size()) == Result::Queued &&
        large.Observe(true, 101, maximum.data(), maximum.size()) == Result::Queued, "returned page runs are reusable");

    // A stalled callback must not delay destruction of the production worker
    // helper, and its captured state must stay alive until the callback releases it.
    std::promise<void> entered, release, done;
    auto enteredFuture = entered.get_future();
    auto releaseFuture = release.get_future();
    auto doneFuture = done.get_future();
    auto payload = std::make_shared<int>(42);
    std::weak_ptr<int> lifetime = payload;
    std::atomic<bool> stoppedObserved{false};
    auto worker = std::make_unique<gpu::taa_collection::CollectionWorker>(
        [owned = payload, &entered, &releaseFuture, &done, &stoppedObserved](auto& control) mutable {
            entered.set_value();
            releaseFuture.wait();
            stoppedObserved = control.Stopped() && *owned == 42;
            owned.reset();
            done.set_value();
        });
    Check(enteredFuture.wait_for(std::chrono::seconds(2)) == std::future_status::ready, "worker stall fixture entered");
    payload.reset();
    const auto stopStart = std::chrono::steady_clock::now();
    worker.reset();
    const auto stopTime = std::chrono::steady_clock::now() - stopStart;
    Check(stopTime < std::chrono::milliseconds(100), "worker destruction does not wait for a stalled callback");
    Check(!lifetime.expired(), "stalled worker retains its state after frontend destruction");
    release.set_value();
    Check(doneFuture.wait_for(std::chrono::seconds(2)) == std::future_status::ready, "stalled callback can finish after destruction");
    Check(stoppedObserved && lifetime.expired(), "stop observed and owned state released safely");

    std::cout << "shader source queue: " << checks << " checks passed; stopped worker destruction "
        << std::chrono::duration<double, std::micro>(stopTime).count() << " us; hot-path allocations " << allocations << '\n';
}
