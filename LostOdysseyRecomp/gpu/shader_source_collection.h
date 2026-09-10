#pragma once

#include <cstddef>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gpu::taa_collection::shader_sources {

inline constexpr size_t MaxProgramBytes = 64 * 1024;
inline constexpr size_t MaxRequestBytes = 256 * 1024;
inline constexpr size_t MaxBatchPrograms = 32;
inline constexpr size_t MaxTrackedPrograms = 8192;
inline constexpr size_t MaxPendingBytes = 4 * 1024 * 1024;
inline constexpr const char* Build = "0.5.0-shader-sources-1";

using Key = std::pair<bool, uint64_t>; // Vertex/pixel stage and renderer byte FNV-1a.
using Bytes = std::vector<uint8_t>;
struct Program {
    Key key;
    std::shared_ptr<const Bytes> bytes;
    uint64_t token = 0;
};
struct Batch {
    uint64_t epoch = 0;
    std::vector<Program> programs;
};

inline std::string Base64(const Bytes& bytes) {
    static constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve(4 * ((bytes.size() + 2) / 3));
    for (size_t i = 0; i < bytes.size(); i += 3) {
        const uint32_t value = (uint32_t(bytes[i]) << 16) |
            (i + 1 < bytes.size() ? uint32_t(bytes[i + 1]) << 8 : 0) |
            (i + 2 < bytes.size() ? uint32_t(bytes[i + 2]) : 0);
        result += alphabet[(value >> 18) & 63];
        result += alphabet[(value >> 12) & 63];
        result += i + 1 < bytes.size() ? alphabet[(value >> 6) & 63] : '=';
        result += i + 2 < bytes.size() ? alphabet[value & 63] : '=';
    }
    return result;
}

// Device strings have already passed the collection allowlist. Called by the
// uploader, never the render thread. No filesystem paths or compiled GPU blobs.
inline std::string RequestStart(std::string_view backend, std::string_view gpu,
    std::string_view driver) {
    return "{\"schema\":1,\"build\":\"" + std::string(Build) + "\",\"backend\":\"" +
        std::string(backend) + "\",\"gpu\":\"" + std::string(gpu) + "\",\"driver\":\"" +
        std::string(driver) + "\",\"programs\":[";
}

inline std::string Request(std::string start, const Batch& batch) {
    if (batch.programs.empty() || batch.programs.size() > MaxBatchPrograms) return {};
    for (const auto& program : batch.programs) {
        if (!program.bytes || program.bytes->empty() ||
            program.bytes->size() > MaxProgramBytes || program.bytes->size() % 4) return {};
        std::ostringstream record;
        record << "{\"stage\":\"" << (program.key.first ? "vs" : "ps") <<
            "\",\"hash\":\"" << std::hex << std::setfill('0') << std::setw(16) <<
            program.key.second << "\",\"data\":\"" << Base64(*program.bytes) << "\"}";
        if (&program != &batch.programs.front()) start += ',';
        start += record.str();
        if (start.size() + 2 > MaxRequestBytes) return {};
    }
    start += "]}";
    return start;
}

// Synchronization belongs to taa_collection's mutex. Observe is called only
// after try_lock. Initialization reserves the payload slab before rendering;
// Observe performs no allocation, deallocation, hashing or I/O.
// Pending source data is held until HTTP 200; acknowledged programs keep only
// their small stage/hash key for this process. No disk acknowledgement cache.
class Queue {
public:
    enum class Observation { Queued, Known, Invalid, Full, Disabled, Busy };
    explicit Queue(size_t trackedLimit = MaxTrackedPrograms,
        size_t byteLimit = MaxPendingBytes) : trackedLimit_(trackedLimit), byteLimit_(byteLimit) {}

    void Initialize() {
        if (!storage_) storage_ = std::make_unique<uint8_t[]>(MaxPendingBytes);
    }

    Observation TryObserve(std::mutex& mutex, const std::atomic<int>& consent,
        bool vertex, uint64_t hash, const uint32_t* words, size_t count) noexcept {
        if (consent.load(std::memory_order_relaxed) != 1) return Observation::Disabled;
        try {
            std::unique_lock lock(mutex, std::try_to_lock);
            if (!lock) return Observation::Busy;
            if (consent.load(std::memory_order_relaxed) != 1) return Observation::Disabled;
            return Observe(vertex, hash, words, count);
        } catch (...) { return Observation::Busy; }
    }

    Observation Observe(bool vertex, uint64_t hash, const uint32_t* words, size_t count) {
        if (!words || !count || count > MaxProgramBytes / sizeof(uint32_t)) return Observation::Invalid;
        const Key key{vertex, hash};
        const size_t slot = Find(key);
        if (slot < MaxTrackedPrograms && programs_[slot].occupied) return Observation::Known;
        const size_t size = count * sizeof(uint32_t);
        if (!storage_ || slot == MaxTrackedPrograms || tracked_ >= trackedLimit_ ||
            size > byteLimit_ - pendingBytes_) return Observation::Full;
        const size_t pages = (size + PageBytes - 1) / PageBytes;
        size_t run = 0, begin = PageCount;
        for (size_t page = 0; page < PageCount; ++page) {
            run = usedPages_[page] ? 0 : run + 1;
            if (run == pages) { begin = page + 1 - pages; break; }
        }
        if (begin == PageCount) return Observation::Full;
        for (size_t page = begin; page < begin + pages; ++page) usedPages_[page] = true;
        std::memcpy(storage_.get() + begin * PageBytes, words, size);
        programs_[slot] = {key, ++sequence_, size, begin, pages, true};
        ++tracked_;
        pendingBytes_ += size;
        return Observation::Queued;
    }

    Batch Pending(size_t headerBytes) const {
        Batch batch{epoch_, {}};
        if (headerBytes > MaxRequestBytes - 2) return batch;
        size_t wireBytes = headerBytes + 2;
        for (const auto& program : programs_) {
            if (!program.size) continue;
            // 64 covers the fixed JSON keys, 16 hash digits and separator.
            const size_t recordBytes = 64 + 4 * ((program.size + 2) / 3);
            if (recordBytes > MaxRequestBytes - wireBytes) continue;
            // Allocation and serialization occur only on the uploader thread.
            const auto* first = storage_.get() + program.firstPage * PageBytes;
            batch.programs.push_back({program.key,
                std::make_shared<Bytes>(first, first + program.size), program.token});
            wireBytes += recordBytes;
            if (batch.programs.size() == MaxBatchPrograms) break;
        }
        return batch;
    }

    void Acknowledge(const Batch& batch) {
        if (batch.epoch != epoch_) return;
        for (const auto& program : batch.programs) {
            const size_t slot = Find(program.key);
            if (slot == MaxTrackedPrograms) continue;
            auto& entry = programs_[slot];
            if (entry.occupied && entry.size && entry.token == program.token) {
                pendingBytes_ -= entry.size;
                for (size_t page = entry.firstPage; page < entry.firstPage + entry.pages; ++page)
                    usedPages_[page] = false;
                entry.size = entry.pages = 0;
            }
        }
    }

    void Reset() {
        ++epoch_;
        programs_.fill({});
        usedPages_.fill(false);
        tracked_ = 0;
        pendingBytes_ = 0;
    }
    size_t PendingBytes() const { return pendingBytes_; }
    size_t Tracked() const { return tracked_; }
    size_t PendingCount() const noexcept {
        size_t count=0;for(const auto& program:programs_)if(program.size)++count;return count;
    }
    uint64_t Epoch() const { return epoch_; }

private:
    const size_t trackedLimit_, byteLimit_;
    uint64_t epoch_ = 0;
    uint64_t sequence_ = 0;
    size_t pendingBytes_ = 0;
    size_t tracked_ = 0;
    static constexpr size_t PageBytes = 4096, PageCount = MaxPendingBytes / PageBytes;
    struct Entry {
        Key key{};
        uint64_t token = 0;
        size_t size = 0, firstPage = 0, pages = 0;
        bool occupied = false;
    };
    std::array<Entry, MaxTrackedPrograms> programs_{};
    std::array<bool, PageCount> usedPages_{};
    std::unique_ptr<uint8_t[]> storage_;

    size_t Find(Key key) const {
        const auto mix = key.second ^ (key.second >> 32) ^ (key.first ? 0x9e3779b9u : 0u);
        size_t slot = size_t(mix) % MaxTrackedPrograms;
        for (size_t probe = 0; probe < MaxTrackedPrograms; ++probe) {
            if (!programs_[slot].occupied || programs_[slot].key == key) return slot;
            slot = (slot + 1) % MaxTrackedPrograms;
        }
        return MaxTrackedPrograms;
    }
};
}
