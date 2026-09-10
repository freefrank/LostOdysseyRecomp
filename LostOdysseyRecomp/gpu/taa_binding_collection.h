#pragma once
#include "taa_binding_evidence.h"
#include "taa_collection_format.h"
#include <atomic>
#include <chrono>
#include <locale>
#include <mutex>
#include <type_traits>

namespace gpu::taa_collection::binding {
inline constexpr size_t Capacity = 64, BatchSize = 8;
inline constexpr const char* Build = "0.5.2-taa-bindings-1";
static_assert(std::is_trivially_copyable_v<Record>);
struct Entry { Record record{}; uint32_t count = 0, sent = 0; };
struct Batch {
    uint64_t epoch = 0;
    size_t size = 0;
    std::array<Entry, BatchSize> entries{};
    std::array<size_t, BatchSize> indices{};
};

// Fixed storage is initialized with collection startup. Producers only try the
// snapshot lock, copy POD, and do bounded comparisons. No allocation or I/O.
class Queue {
public:
    enum class Observation { Queued, Counted, Full, Busy, Disabled, Stale };
    bool Want() const noexcept { return available_.load(std::memory_order_relaxed); }
    Observation TryObserve(std::mutex& mutex, const std::atomic<int>& consent,
        const std::atomic<uint64_t>& generation, uint64_t epoch, const Record& record) noexcept try {
        if (consent.load(std::memory_order_relaxed) != 1) return Observation::Disabled;
        if (epoch != generation.load(std::memory_order_relaxed)) return Observation::Stale;
        std::unique_lock lock(mutex, std::try_to_lock);
        if (!lock) return Observation::Busy;
        if (consent.load(std::memory_order_relaxed) != 1) return Observation::Disabled;
        if (epoch != generation.load(std::memory_order_relaxed)) return Observation::Stale;
        for (size_t i = 0; i < size_; ++i) {
            auto& entry = entries_[i];
            if (entry.record.vs == record.vs && entry.record.ps == record.ps && entry.record == record) {
                if (entry.count < 1000000000) ++entry.count;
                return Observation::Counted;
            }
        }
        if (size_ == Capacity) return Observation::Full;
        entries_[size_++] = {record, 1, 0};
        available_.store(size_ < Capacity, std::memory_order_relaxed);
        return Observation::Queued;
    } catch (...) {
        return Observation::Busy; // Optional collection must not terminate rendering on a lock error.
    }
    // Worker/UI only, under the snapshot mutex. Epoch blocks stale HTTP acks.
    void Reset() noexcept {
        ++epoch_; size_ = 0; available_.store(true, std::memory_order_relaxed);
        nextWindow_ = std::chrono::steady_clock::now() + std::chrono::minutes(3);
    }
    Batch Pending() const noexcept {
        Batch batch; batch.epoch = epoch_;
        for (size_t i = 0; i < size_ && batch.size < BatchSize; ++i) if (!entries_[i].sent) {
            batch.indices[batch.size] = i; batch.entries[batch.size++] = entries_[i];
        }
        return batch;
    }
    void Acknowledge(const Batch& batch) noexcept {
        if (batch.epoch != epoch_) return;
        for (size_t i = 0; i < batch.size; ++i) {
            const auto index = batch.indices[i];
            if (index < size_ && entries_[index].record == batch.entries[i].record)
                entries_[index].sent = batch.entries[i].count;
        }
    }
    void Rotate(std::chrono::steady_clock::time_point now) noexcept {
        if (!size_ || now < nextWindow_) return;
        for (size_t i = 0; i < size_; ++i) if (!entries_[i].sent) return;
        Reset();
    }
    size_t Size() const noexcept { return size_; }
    size_t PendingCount() const noexcept {
        size_t count=0;for(size_t i=0;i<size_;++i)if(!entries_[i].sent)++count;return count;
    }
private:
    std::array<Entry, Capacity> entries_{};
    size_t size_ = 0;
    uint64_t epoch_ = 0;
    std::atomic<bool> available_{true};
    std::chrono::steady_clock::time_point nextWindow_ = std::chrono::steady_clock::now() + std::chrono::minutes(3);
};

// Serialization is called exclusively on the detached upload worker.
template<size_t N> inline void ArrayJson(std::ostream& out, const std::array<uint32_t, N>& values) {
    out << '[';
    for (size_t i = 0; i < N; ++i) { if (i) out << ','; out << values[i]; }
    out << ']';
}
inline void TransformJson(std::ostream& out, const Transform& t) {
    out << "{\"slot\":" << t.slot << ",\"phase\":" << t.phase << ",\"applied\":" << (t.applied ? "true" : "false");
    out << ",\"guestVP\":"; ArrayJson(out, t.guestVP);
    out << ",\"uploadedVP\":"; ArrayJson(out, t.uploadedVP);
    out << ",\"viewport\":"; ArrayJson(out, t.viewport);
    out << ",\"jitterNdc\":"; ArrayJson(out, t.jitterNdc); out << '}';
}
inline std::string RecordJson(const Record& r, uint32_t draws) {
    auto base = taa_collection::RecordJson(r.vs, r.ps, r.width, r.height, r.slot,
        r.candidates, r.flags, r.rejection, draws, r.position, r.guards);
    base.pop_back();
    std::ostringstream out; out.imbue(std::locale::classic()); out << base << ",\"consumer\":";
    TransformJson(out, r.consumer); out << ",\"psC0\":"; ArrayJson(out, r.psC0);
    const auto& t = r.texture;
    out << ",\"texture\":{\"slot\":" << t.slot << ",\"bank\":" << t.bank << ",\"kind\":" << uint32_t(t.kind)
        << ",\"guestFormat\":" << t.guestFormat << ",\"hostFormat\":" << t.hostFormat
        << ",\"dimension\":" << t.dimension << ",\"swizzle\":" << t.swizzle
        << ",\"sourceMip\":" << t.sourceMip << ",\"sign\":" << t.sign
        << ",\"swapRedBlue\":" << (t.swapRedBlue ? "true" : "false");
    out << ",\"sampler\":"; ArrayJson(out, t.sampler);
    out << ",\"guestExtent\":"; ArrayJson(out, t.guestExtent);
    out << ",\"hostExtent\":"; ArrayJson(out, t.hostExtent);
    out << ",\"parentExtent\":"; ArrayJson(out, t.parentExtent);
    out << ",\"resolveRect\":"; ArrayJson(out, t.resolveRect);
    out << ",\"producerFrameAge\":" << t.producerFrameAge << ",\"resolveFrameAge\":" << t.resolveFrameAge
        << ",\"resolveGap\":" << t.resolveGap << ",\"producerState\":" << uint32_t(t.producerState)
        << ",\"producerDraws\":" << t.producerDraws << ",\"producer\":";
    TransformJson(out, t.producer); out << "}}";
    return out.str();
}
inline std::string Request(std::string_view backend, std::string_view gpu, std::string_view driver, const Batch& batch) {
    std::string result = "{\"schema\":3,\"build\":\"" + std::string(Build) + "\",\"backend\":\"" + std::string(backend)
        + "\",\"gpu\":\"" + std::string(gpu) + "\",\"driver\":\"" + std::string(driver) + "\",\"records\":[";
    for (size_t i = 0; i < batch.size; ++i) {
        if (i) result += ',';
        result += RecordJson(batch.entries[i].record, batch.entries[i].count);
    }
    result += "]}";
    return result;
}
}
