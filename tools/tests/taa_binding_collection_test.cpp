#include "../../LostOdysseyRecomp/gpu/taa_binding_collection.h"
#include <bit>
#include <cstdlib>
#include <fstream>
#include <future>
#include <iostream>
#include <memory>
#include <new>
#include <thread>

using namespace gpu::taa_collection::binding;
static thread_local bool watch = false;
static thread_local unsigned allocations = 0;
static unsigned checks = 0;
void* operator new(size_t bytes) {
    if (watch) ++allocations;
    if (void* p = std::malloc(bytes ? bytes : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](size_t bytes) { return ::operator new(bytes); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
static void Check(bool ok, const char* message) {
    ++checks; if (!ok) { std::cerr << "FAILED: " << message << '\n'; std::exit(1); }
}
static Record Sample(uint32_t salt = 0) {
    Record r;
    r.vs = 0xe810cfacc107fd3c; r.ps = 0x5b11f88a8bb293df;
    r.width = 3840; r.height = 2160; r.candidates = 36; r.flags = 19;
    r.rejection = 2; r.position.kind = 1; r.position.slot = 7; r.position.outputs = 30; r.guards = 31;
    r.consumer.slot = 7;
    for (unsigned i = 0; i < 16; ++i) r.consumer.guestVP[i] = r.consumer.uploadedVP[i] = (i % 5 ? 0 : 0x3f800000);
    r.consumer.viewport = {0, 0, std::bit_cast<uint32_t>(3840.0f), std::bit_cast<uint32_t>(2160.0f)};
    r.psC0 = {0x3f000000, 0xbf000000, 0x3f000000, 0x3f000000};
    auto& t = r.texture;
    t.swapRedBlue = (salt & 1) != 0;
    t.kind = TextureKind::Resolved; t.guestFormat = 6; t.hostFormat = 28; t.dimension = 1; t.swizzle = 1672;
    t.sampler = {2, 2, 2, 1, 1, 0}; t.guestExtent = {1280, 720}; t.hostExtent = t.parentExtent = {3840, 2160};
    t.resolveRect = {0, 0, 3840, 2160}; t.producerFrameAge = 0; t.resolveFrameAge = 0; t.resolveGap = int32_t(salt);
    t.producerState = ProducerState::UniformJittered; t.producerDraws = 145;
    t.producer = r.consumer; t.producer.applied = true; t.producer.phase = 3;
    t.producer.jitterNdc = {std::bit_cast<uint32_t>(0.0001f), std::bit_cast<uint32_t>(-0.0002f)};
    t.producer.uploadedVP[12] = t.producer.jitterNdc[0]; t.producer.uploadedVP[13] = t.producer.jitterNdc[1];
    return r;
}
struct GroupedNumbers : std::numpunct<char> {
    char do_thousands_sep() const override { return ','; }
    std::string do_grouping() const override { return "\3"; }
};
int main(int argc, char** argv) {
    if (argc == 3 && std::string_view(argv[1]) == "--emit-only") {
        Batch batch; batch.size = BatchSize;
        for (uint32_t i = 0; i < BatchSize; ++i) batch.entries[i] = {Sample(i), 1, 0};
        const auto json = Request("vulkan", "Synthetic GPU", "1", batch);
        std::ofstream out(argv[2], std::ios::binary); out << json;
        if (!out) return 1;
        std::cout << "serializer fixture: " << json.size() << " bytes\n";
        return 0;
    }
    auto owned = std::make_unique<Queue>(); auto& q = *owned;
    std::mutex mutex; std::atomic<int> consent{1}; std::atomic<uint64_t> generation{7};
    using Result = Queue::Observation;
    watch = true;
    Check(q.TryObserve(mutex, consent, generation, 7, Sample()) == Result::Queued, "first sample queued");
    Check(q.TryObserve(mutex, consent, generation, 7, Sample()) == Result::Counted, "identical evidence deduplicated");
    Check(q.Pending().entries[0].count == 2, "deduplicated observation count retained");
    for (uint32_t i = 1; i < Capacity; ++i)
        Check(q.TryObserve(mutex, consent, generation, 7, Sample(i)) == Result::Queued, "distinct relative resolve provenance retained");
    Check(!q.Want() && q.Size() == Capacity, "fixed pool bounds all records");
    Check(q.TryObserve(mutex, consent, generation, 7, Sample(100)) == Result::Full, "full pool never spills to heap");
    auto first = q.Pending(); Check(first.size == BatchSize, "background batch limited to eight");
    q.Acknowledge(first); Check(q.Pending().indices[0] == BatchSize, "ack removes only accepted records from pending");
    q.Reset(); q.TryObserve(mutex, consent, generation, 7, Sample()); q.Acknowledge(first);
    Check(q.Pending().size == 1, "stale HTTP ack cannot clear replacement sample");
    consent = 0;
    Check(q.TryObserve(mutex, consent, generation, 7, Sample()) == Result::Disabled, "opt out stops sampling");
    ++generation; consent = 1;
    Check(q.TryObserve(mutex, consent, generation, 7, Sample()) == Result::Stale, "re-enabled consent rejects pre-revocation sample");
    q.Reset(); Check(q.Want() && q.Size() == 0, "consent/device reset clears pool");
    watch = false;
    Check(allocations == 0, "producer insert count full reset and ack allocate nothing");

    std::promise<void> entered, release;
    auto enteredFuture = entered.get_future(); auto releaseFuture = release.get_future();
    std::thread held([&] { std::lock_guard lock(mutex); entered.set_value(); releaseFuture.wait(); });
    enteredFuture.wait();
    const auto start = std::chrono::steady_clock::now();
    watch = true; const auto result = q.TryObserve(mutex, consent, generation, 8, Sample()); watch = false;
    const auto elapsed = std::chrono::steady_clock::now() - start;
    release.set_value(); held.join();
    Check(result == Result::Busy && elapsed < std::chrono::milliseconds(100), "render producer does not wait for worker lock");
    Check(allocations == 0 && q.Size() == 0, "contention allocates nothing and does not mutate pool");
    q.TryObserve(mutex, consent, generation, 8, Sample());
    q.Rotate(std::chrono::steady_clock::now() + std::chrono::minutes(10));
    Check(q.Size() == 1, "failed HTTP never discards pending evidence");
    q.Acknowledge(q.Pending()); q.Rotate(std::chrono::steady_clock::now() + std::chrono::minutes(10));
    Check(q.Size() == 0 && q.Want(), "acknowledged window admits future samples");

    for (uint32_t i = 0; i < BatchSize; ++i) q.TryObserve(mutex, consent, generation, 8, Sample(i));
    const auto locale = std::locale();
    std::locale::global(std::locale(locale, new GroupedNumbers));
    const auto json = Request("vulkan", "Synthetic GPU", "1", q.Pending());
    std::locale::global(locale);
    Check(json.size() < 65536, "actual serializer batch within HTTP budget");
    Check(json.find("3,840") == std::string::npos && json.find("1,065,353,216") == std::string::npos,
        "JSON uses invariant numeric formatting including base schema fields");
    if (argc == 2) { std::ofstream out(argv[1], std::ios::binary); out << json; Check(bool(out), "serializer fixture written"); }
    std::cout << "binding collection: " << checks << " checks passed, " << allocations
        << " producer allocations, " << json.size() << " payload bytes, " << sizeof(Queue) << " queue bytes\n";
}
