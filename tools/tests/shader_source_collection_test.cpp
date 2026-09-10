#include "../../LostOdysseyRecomp/gpu/shader_source_collection.h"
#include "../../LostOdysseyRecomp/gpu/collection_worker.h"

#include <cstdlib>
#include <fstream>
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

int main(int argc, char** argv) {
    using Result = Queue::Observation;
    const auto start = RequestStart("d3d12", "AMD Radeon RX 9060 XT", "123456");
    std::vector<uint32_t> words{0x03020100, 0xfffefdfc, 0x34333231};
    const auto hash = Hash(words);

    Check(Base64({}) == "", "empty base64");
    Check(Base64({'f'}) == "Zg==", "base64 two padding characters");
    Check(Base64({'f', 'o'}) == "Zm8=", "base64 one padding character");
    Check(Base64({'f', 'o', 'o'}) == "Zm9v", "base64 full triplet");
    Check(Base64({0, 1, 2, 3, 252, 253, 254, 255, 49, 50, 51, 52}) ==
        "AAECA/z9/v8xMjM0", "binary byte order and canonical base64");

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
    const auto first = queue.Pending(start.size());
    Check(first.programs.size() == 2, "VS and PS both selected");
    const auto request = Request(start, first);
    Check(request.find("\"schema\":1,\"build\":\"0.5.0-shader-sources-1\"") != std::string::npos, "frozen protocol version");
    Check(request.find("\"gpu\":\"AMD Radeon RX 9060 XT\"") != std::string::npos, "GPU model supplied");
    Check(request.find("AAECA/z9/v8xMjM0") != std::string::npos, "original memory bytes encoded");
    Check(request.find("\"stage\":\"vs\"") != std::string::npos &&
        request.find("\"stage\":\"ps\"") != std::string::npos, "both stages serialized");
    Check(request.ends_with("]}") && request.size() <= MaxRequestBytes, "bounded complete request");
    if (argc == 2) {
        std::ofstream output(argv[1], std::ios::binary);
        output << request;
        Check(bool(output), "serialized request fixture saved");
    }

    // A timeout/non-200 does not acknowledge anything. The next request still
    // contains the same immutable bytes even if guest memory has been reused.
    words[0] ^= 0xffffffffu;
    Check(Request(start, queue.Pending(start.size())) == request, "failure retains exact retry data");
    queue.Acknowledge(first);
    Check(queue.PendingBytes() == 0 && queue.Tracked() == 2, "ack releases raw and retains keys");
    Check(queue.Pending(start.size()).programs.empty(), "acknowledged programs no longer uploaded");
    Check(queue.Observe(true, hash, words.data(), words.size()) == Result::Known, "acknowledged ID avoids recopy");
    queue.Acknowledge(first);
    Check(queue.PendingBytes() == 0, "duplicate acknowledgement is idempotent");

    // Disabling collection or changing GPU metadata resets the source epoch.
    // A response to the old batch may arrive after the same shader is admitted.
    queue.Reset();
    Check(queue.Tracked() == 0 && queue.PendingBytes() == 0, "reset clears pending and acknowledged keys");
    Check(queue.Epoch() != first.epoch, "reset advances epoch");
    words[0] ^= 0xffffffffu;
    Check(queue.Observe(true, hash, words.data(), words.size()) == Result::Queued, "same ID after re-enable admitted");
    queue.Acknowledge(first);
    Check(queue.PendingBytes() == 12, "old response cannot acknowledge current epoch");
    queue.Acknowledge(queue.Pending(start.size()));
    Check(queue.PendingBytes() == 0, "current epoch response acknowledged");

    const auto boundedOwner = std::make_unique<Queue>(2, 16);
    auto& bounded = *boundedOwner;
    bounded.Initialize();
    Check(bounded.Observe(true, 1, nullptr, 1) == Result::Invalid, "null source rejected");
    Check(bounded.Observe(true, 1, words.data(), 0) == Result::Invalid, "empty source rejected");
    Check(bounded.Observe(true, 1, words.data(), MaxProgramBytes / 4 + 1) == Result::Invalid, "oversized source rejected before read");
    Check(bounded.Observe(true, 1, words.data(), words.size()) == Result::Queued, "within raw budget admitted");
    Check(bounded.Observe(true, 2, words.data(), words.size()) == Result::Full, "raw budget enforced");
    Check(bounded.Tracked() == 1 && bounded.PendingBytes() == 12, "full admission does not mutate queue");
    bounded.Acknowledge(bounded.Pending(start.size()));
    Check(bounded.Observe(true, 2, words.data(), words.size()) == Result::Queued, "later draw retries after room reclaimed");
    bounded.Acknowledge(bounded.Pending(start.size()));
    Check(bounded.Observe(true, 3, words.data(), words.size()) == Result::Full, "acknowledged key index remains bounded");
    Check(bounded.Observe(true, 2, words.data(), words.size()) == Result::Known, "known ID works at index capacity");
    Check(bounded.Pending(MaxRequestBytes).programs.empty(), "oversized metadata cannot produce a batch");
    Check(Request(start, {}) == "", "empty batches do not become network requests");

    const auto manyOwner = std::make_unique<Queue>();
    auto& many = *manyOwner;
    many.Initialize();
    for (uint64_t i = 0; i < 40; ++i)
        Check(many.Observe(true, i, words.data(), 1) == Result::Queued, "small source admitted");
    const auto first32 = many.Pending(start.size());
    Check(first32.programs.size() == 32, "program count batch bound");
    Check(Request(start, first32).size() <= MaxRequestBytes, "32-program wire limit");
    many.Acknowledge(first32);
    Check(many.Pending(start.size()).programs.size() == 8, "incremental remainder preserved");

    const auto largeOwner = std::make_unique<Queue>();
    auto& large = *largeOwner;
    large.Initialize();
    std::vector<uint32_t> maximum(MaxProgramBytes / 4, 0xdeadbeef);
    for (uint64_t i = 0; i < 3; ++i)
        Check(large.Observe(false, i, maximum.data(), maximum.size()) == Result::Queued, "maximum size source admitted");
    const auto firstLarge = large.Pending(start.size());
    Check(firstLarge.programs.size() == 2, "wire bound splits 64 KiB sources");
    const auto largeRequest = Request(start, firstLarge);
    Check(!largeRequest.empty() && largeRequest.size() <= MaxRequestBytes, "large batch respects encoded wire budget");
    large.Acknowledge(firstLarge);
    Check(large.PendingBytes() == MaxProgramBytes && large.Pending(start.size()).programs.size() == 1, "large remainder retained");
    Batch overWire{0, {firstLarge.programs[0], firstLarge.programs[0], firstLarge.programs[0]}};
    Check(Request(start, overWire).empty(), "serializer rejects oversized caller batch");
    Batch misaligned{0, {{{true, 1}, std::make_shared<Bytes>(3)}}};
    Check(Request(start, misaligned).empty(), "unaligned microcode rejected");

    large.Reset();
    watchAllocations = true;
    for (uint64_t i = 0; i < MaxPendingBytes / MaxProgramBytes; ++i)
        Check(large.Observe(true, i, maximum.data(), maximum.size()) == Result::Queued, "slab page allocation");
    Check(large.PendingBytes() == MaxPendingBytes, "full slab uses exact bounded raw budget");
    Check(large.Observe(true, 100, maximum.data(), maximum.size()) == Result::Full, "full slab drops new observations");
    watchAllocations = false;
    Check(allocations == 0, "slab fill and full queue perform no allocations");
    large.Acknowledge(large.Pending(start.size()));
    Check(large.PendingBytes() == MaxPendingBytes - 2 * MaxProgramBytes, "ack returns all occupied pages");
    Check(large.Observe(true, 100, maximum.data(), maximum.size()) == Result::Queued &&
        large.Observe(true, 101, maximum.data(), maximum.size()) == Result::Queued, "returned page runs are reusable");

    std::mutex sourceMutex;
    std::atomic<int> consent{1};
    std::promise<void> locked, unlock;
    auto unlocked = unlock.get_future();
    auto lockReady = locked.get_future();
    std::thread holder([&] {
        std::lock_guard guard(sourceMutex);
        locked.set_value();
        unlocked.wait();
    });
    Check(lockReady.wait_for(std::chrono::seconds(2)) == std::future_status::ready, "contention fixture acquired lock");
    const auto busyStart = std::chrono::steady_clock::now();
    watchAllocations = true;
    const auto busy = queue.TryObserve(sourceMutex, consent, true, hash, words.data(), words.size());
    watchAllocations = false;
    const auto busyTime = std::chrono::steady_clock::now() - busyStart;
    Check(busy == Result::Busy && busyTime < std::chrono::milliseconds(100), "contended producer returns without waiting");
    consent = 0;
    Check(queue.TryObserve(sourceMutex, consent, true, hash, words.data(), words.size()) == Result::Disabled,
        "opt-out returns before even trying a contended lock");
    Check(allocations == 0, "contended producer allocates no memory");
    unlock.set_value();
    holder.join();

    // Simulate an HTTP implementation that remains stuck even after stop.
    // Destroying the actual production worker helper must not wait for it, and
    // its captured state must remain alive until the callback releases it.
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
    Check(stopTime < std::chrono::milliseconds(100), "worker destruction does not wait for stalled HTTP");
    Check(!lifetime.expired(), "stalled worker retains its state after frontend destruction");
    release.set_value();
    Check(doneFuture.wait_for(std::chrono::seconds(2)) == std::future_status::ready, "stalled callback can finish after destruction");
    Check(stoppedObserved && lifetime.expired(), "stop observed and owned state released safely");

    std::cout << "shader source collection: " << checks << " checks passed\n";
    std::cout << "contended draw " << std::chrono::duration<double, std::micro>(busyTime).count()
        << " us; stopped worker destruction " << std::chrono::duration<double, std::micro>(stopTime).count()
        << " us; hot-path allocations " << allocations << '\n';
}
