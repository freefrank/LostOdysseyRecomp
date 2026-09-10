#pragma once
#include "shader/resource_cpx_index_sha256.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace gpu::shader_source_capture {

// Created once at capture start. Observe/MarkHlsl only touch bounded CPU memory;
// Write is called after ownership has moved to the background archive worker.
class Capture {
public:
    static constexpr size_t MaxRecords = 4096, MaxBytes = 8u << 20, MaxProgramBytes = 64u << 10;
    enum class Status { Included, Missing, Oversize, ByteCapacity, HashConflict };
    struct Record {
        bool vertex = false, hlsl = false;
        uint64_t hash = 0, firstFrame = 0, lastFrame = 0;
        size_t byteCount = 0, offset = 0;
        Status status = Status::Missing;
    };

    explicit Capture(size_t records = MaxRecords, size_t bytes = MaxBytes)
        : records_(std::min(records, MaxRecords)), bytes_(std::min(bytes, MaxBytes)), index_(MaxRecords * 2) {}

    void Reset() noexcept {
        std::fill(index_.begin(), index_.end(), 0);
        usedRecords_ = usedBytes_ = droppedRecords_ = psNotBound_ = 0;
    }

    void Observe(bool vertex, uint64_t hash, const uint32_t* words, size_t count, uint64_t frame) noexcept {
        const auto slot = Find(vertex, hash);
        if (index_[slot]) {
            auto& record = records_[index_[slot] - 1];
            record.lastFrame = frame;
            if (record.status == Status::Included && (!words || count > MaxProgramBytes / 4 ||
                count * 4 != record.byteCount || std::memcmp(bytes_.data() + record.offset, words, record.byteCount)))
                record.status = Status::HashConflict;
            return;
        }
        if (usedRecords_ == records_.size()) { ++droppedRecords_; return; }
        auto& record = records_[usedRecords_];
        record = {vertex, false, hash, frame, frame, count <= MaxProgramBytes / 4 ? count * 4 : 0, usedBytes_, Status::Missing};
        index_[slot] = static_cast<uint32_t>(++usedRecords_);
        if (!words || !count) return;
        if (count > MaxProgramBytes / 4) { record.status = Status::Oversize; return; }
        if (record.byteCount > bytes_.size() - usedBytes_) { record.status = Status::ByteCapacity; return; }
        std::memcpy(bytes_.data() + usedBytes_, words, record.byteCount);
        usedBytes_ += record.byteCount;
        record.status = Status::Included;
    }

    void MarkHlsl(bool vertex, uint64_t hash, bool available) noexcept {
        const auto entry = index_[Find(vertex, hash)];
        if (entry) records_[entry - 1].hlsl |= available;
    }
    void NotePixelNotBound() noexcept { ++psNotBound_; }
    size_t Count() const noexcept { return usedRecords_; }
    size_t DroppedRecords() const noexcept { return droppedRecords_; }
    const Record& At(size_t index) const noexcept { return records_[index]; }
    std::span<const uint8_t> Bytes(size_t index) const noexcept {
        const auto& r = records_[index];
        return r.status == Status::Included ? std::span<const uint8_t>(bytes_.data() + r.offset, r.byteCount) : std::span<const uint8_t>();
    }
    static uint64_t Hash(std::span<const uint8_t> bytes) noexcept {
        uint64_t result = 0xcbf29ce484222325ull;
        for (auto byte : bytes) { result ^= byte; result *= 0x100000001b3ull; }
        return result;
    }

    void Write(const std::filesystem::path& captureRoot) const {
        const auto directory = captureRoot / "shaders" / "source";
        std::filesystem::create_directories(directory);
        std::ofstream manifest(directory / "manifest.json", std::ios::binary | std::ios::trunc);
        if (!manifest) throw std::system_error(std::make_error_code(std::errc::io_error));
        manifest << "{\n  \"schema\":1,\n  \"namespace\":\"renderer-byte-fnv1a64\",\n"
            << "  \"byte_order\":\"original-renderer-memory-no-swap\",\n"
            << "  \"record_limit\":" << records_.size() << ",\n  \"byte_limit\":" << bytes_.size()
            << ",\n  \"dropped_record_capacity\":" << droppedRecords_ << ",\n  \"pixel_not_bound_draws\":" << psNotBound_
            << ",\n  \"programs\":[\n";
        bool complete = !droppedRecords_;
        for (size_t i = 0; i < usedRecords_; ++i) {
            const auto& r = records_[i];
            const auto bytes = Bytes(i);
            const auto hash = Hex(r.hash);
            const bool included = r.status == Status::Included && Hash(bytes) == r.hash;
            const char* status = r.status == Status::Included ? (included ? "included" : "hash_mismatch") : StatusText(r.status);
            complete &= included;
            std::string file, sha;
            if (included) {
                file = std::string(r.vertex ? "vs_" : "ps_") + hash + ".bin";
                std::ofstream output(directory / file, std::ios::binary | std::ios::trunc);
                output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
                output.close();
                if (output.fail()) throw std::system_error(std::make_error_code(std::errc::io_error));
                sha = xenos::resources::Sha256Hex(xenos::resources::Sha256(bytes));
            }
            manifest << (i ? ",\n" : "") << "    {\"stage\":\"" << (r.vertex ? "vs" : "ps")
                << "\",\"hash\":\"" << hash << "\",\"bytes\":" << r.byteCount
                << ",\"first_frame\":" << r.firstFrame << ",\"last_frame\":" << r.lastFrame
                << ",\"status\":\"" << status << "\",\"file\":\"" << file << "\",\"sha256\":\"" << sha
                << "\",\"hlsl_status\":\"" << (r.hlsl ? "included" : "hlsl_unavailable") << "\"}";
        }
        manifest << "\n  ],\n  \"complete\":" << (complete ? "true" : "false") << "\n}\n";
        manifest.close();
        if (manifest.fail()) throw std::system_error(std::make_error_code(std::errc::io_error));
    }

private:
    size_t Find(bool vertex, uint64_t hash) const noexcept {
        size_t slot = static_cast<size_t>((hash ^ (hash >> 32) ^ (vertex ? 0x9e3779b9u : 0u)) & (index_.size() - 1));
        while (index_[slot]) {
            const auto& record = records_[index_[slot] - 1];
            if (record.vertex == vertex && record.hash == hash) break;
            slot = (slot + 1) & (index_.size() - 1);
        }
        return slot;
    }
    static std::string Hex(uint64_t value) {
        constexpr char digits[] = "0123456789abcdef";
        std::string result(16, '0');
        for (size_t i = 0; i < 16; ++i) result[15 - i] = digits[(value >> (i * 4)) & 15];
        return result;
    }
    static const char* StatusText(Status status) noexcept {
        switch (status) {
        case Status::Missing: return "missing_microcode";
        case Status::Oversize: return "program_exceeds_64kib";
        case Status::ByteCapacity: return "byte_capacity_exhausted";
        case Status::HashConflict: return "hash_conflict";
        default: return "included";
        }
    }
    std::vector<Record> records_;
    std::vector<uint8_t> bytes_;
    std::vector<uint32_t> index_;
    size_t usedRecords_ = 0, usedBytes_ = 0, droppedRecords_ = 0, psNotBound_ = 0;
};
}
