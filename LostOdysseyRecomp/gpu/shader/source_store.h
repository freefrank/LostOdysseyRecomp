#pragma once

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace xenos::resources {
// A bounded, immutable-after-discovery input set. Prebuild workers only read it.
// Source bytes are small; translated HLSL and DXC working sets are much larger.
// Keep compiler checkpoints on disk, not these disposable extraction products.
class SourceStore {
public:
    struct Source {
        bool pixel;
        uint64_t hash;
        std::vector<uint8_t> code;
        uint8_t priority = 0; // 0 = primary (XEX, game packages, learned), 1 = generated variants
    };
    using Key = std::pair<bool, uint64_t>;
    static constexpr size_t MaxBytes = 128u << 20;
    static constexpr size_t MaxSources = 100000;
    static constexpr size_t MaxSourceBytes = 262144;

    explicit SourceStore(size_t bytes = MaxBytes, size_t records = MaxSources)
        : byteLimit(std::min(bytes, MaxBytes)), recordLimit(std::min(records, MaxSources)) {}

    static uint64_t CodeHash(std::span<const uint8_t> code) {
        uint64_t hash = 0xcbf29ce484222325ULL;
        for (const auto byte : code) hash = (hash ^ byte) * 0x100000001b3ULL;
        return hash;
    }
    bool Contains(bool pixel, uint64_t hash) const { return sources.contains({pixel, hash}); }
    void Add(bool pixel, std::span<const uint8_t> code, uint8_t priority = 0) {
        if (code.size() < 12 || code.size() > MaxSourceBytes || code.size() % 4)
            throw std::runtime_error("invalid in-memory shader source size");
        const auto hash = CodeHash(code);
        const Key key{pixel, hash};
        if (const auto found = sources.find(key); found != sources.end()) {
            if (found->second.code.size() != code.size() ||
                !std::equal(code.begin(), code.end(), found->second.code.begin()))
                throw std::runtime_error("conflicting shader source identity");
            found->second.priority = std::min(found->second.priority, priority);
            return;
        }
        if (sources.size() >= recordLimit || code.size() > byteLimit - byteCount)
            throw std::runtime_error("shader source memory budget exceeded");
        sources.emplace(key, Source{pixel, hash, {code.begin(), code.end()}, priority});
        byteCount += code.size();
    }
    int Read(uint64_t hash, uint32_t size, bool pixel, std::vector<uint8_t>& code) const {
        const auto found = sources.find({pixel, hash});
        if (found == sources.end()) return 0;
        if (found->second.code.size() != size) return -1;
        code = found->second.code;
        return 1;
    }
    std::vector<const Source*> Jobs() const {
        std::vector<const Source*> jobs;
        jobs.reserve(sources.size());
        for (const auto& [key, source] : sources) jobs.push_back(&source);
        std::stable_sort(jobs.begin(), jobs.end(), [](const Source* a, const Source* b) {
            if (a->priority != b->priority) return a->priority < b->priority;
            if (a->pixel != b->pixel) return !a->pixel && b->pixel;
            return a->hash < b->hash;
        });
        return jobs;
    }
    size_t Size() const { return sources.size(); }
    size_t Bytes() const { return byteCount; }

    // Migrate previous exports and preserve shaders learned during gameplay.
    // Known imported sources are skipped by name before opening them. These are
    // generated cache files (not verified game assets), so unknown files still
    // require bounds and content-identity checks. No files are written here.
    size_t ImportLearned(const std::filesystem::path& directory) {
        std::error_code ec;
        if (!std::filesystem::exists(directory, ec)) {
            if (ec) throw std::filesystem::filesystem_error("shader source directory", directory, ec);
            return 0;
        }
        size_t imported = 0;
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            const auto name = entry.path().filename().string();
            if (name.size() != 23 || (!name.starts_with("ps_") && !name.starts_with("vs_")) ||
                !name.ends_with(".bin")) continue;
            uint64_t hash = 0;
            const auto parsed = std::from_chars(name.data()+3, name.data()+19, hash, 16);
            if (parsed.ec != std::errc{} || parsed.ptr != name.data()+19) continue;
            const bool pixel = name.starts_with("ps_");
            if (Contains(pixel, hash)) continue;
            if (entry.is_symlink() || !entry.is_regular_file()) continue;
            std::ifstream in(entry.path(), std::ios::binary | std::ios::ate);
            const auto size = in.tellg();
            if (size < 12 || size > MaxSourceBytes || size % 4) continue;
            std::vector<uint8_t> bytes(static_cast<size_t>(size)); in.seekg(0);
            if (!in.read(reinterpret_cast<char*>(bytes.data()), size) || CodeHash(bytes) != hash) continue;
            Add(pixel, bytes);
            ++imported;
        }
        return imported;
    }
private:
    std::map<Key, Source> sources;
    size_t byteCount = 0;
    const size_t byteLimit, recordLimit;
};
}
