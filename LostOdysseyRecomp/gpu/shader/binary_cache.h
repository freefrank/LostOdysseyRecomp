#pragma once

#include "cache.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace xenos::cache {
inline constexpr size_t BinaryHeaderSize = 144;
inline constexpr size_t MaxBinaryBytes = 32u << 20;

// The identity is checked inside the file as well as in its name: copying or
// renaming a valid cache from another backend/compiler cannot bypass isolation.
inline std::vector<uint8_t> ReadBinary(const std::filesystem::path& path, bool pixel,
    uint64_t hash, const Identity& identity, bool* present = nullptr) {
    if (present) *present = false;
    if (!ValidIdentity(identity)) return {};
    try {
        std::ifstream in(path, std::ios::binary | std::ios::ate);
        if (present) *present = in.is_open();
        const auto size = in.tellg();
        if (size < std::streamoff(BinaryHeaderSize) || size > std::streamoff(BinaryHeaderSize + MaxBinaryBytes)) return {};
        std::array<uint8_t, BinaryHeaderSize> header{};
        in.seekg(0);
        if (!in.read(reinterpret_cast<char*>(header.data()), header.size())) return {};
        if (std::memcmp(header.data(), "LOSHDR1\n", 8)) return {};
        const auto key = ArtifactKey(pixel, hash, identity);
        if (std::memcmp(header.data()+8, key.data(), key.size())) return {};
        uint64_t length=0;
        for (unsigned i=0;i<8;++i) length |= uint64_t(header[136+i]) << (i*8);
        if (length != uint64_t(size)-BinaryHeaderSize) return {};
        std::vector<uint8_t> binary(static_cast<size_t>(length));
        if (!in.read(reinterpret_cast<char*>(binary.data()), binary.size()) ||
            in.peek()!=std::char_traits<char>::eof()) return {};
        const auto digest=resources::Sha256Hex(resources::Sha256(binary));
        if (std::memcmp(header.data()+72, digest.data(), digest.size())) return {};
        if (!CompleteBinary(binary, identity.format)) return {};
        return binary;
    } catch (...) { return {}; }
}

inline bool WriteBinary(const std::filesystem::path& path, bool pixel, uint64_t hash,
    const Identity& identity, std::span<const uint8_t> binary, std::string* error = nullptr) {
    if (!ValidIdentity(identity) || binary.size()>MaxBinaryBytes || !CompleteBinary(binary, identity.format)) {
        if (error) *error="invalid shader cache identity or binary";
        return false;
    }
    static std::atomic<uint64_t> sequence{0};
    const auto temp=std::filesystem::path(path.wstring()+L".tmp-"+
        std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count())+L"-"+std::to_wstring(sequence++));
    try {
        std::ofstream out(temp, std::ios::binary);
        out << "LOSHDR1\n" << ArtifactKey(pixel, hash, identity) << resources::Sha256Hex(resources::Sha256(binary));
        for (unsigned i=0;i<8;++i) out.put(char(uint64_t(binary.size())>>(i*8)));
        out.write(reinterpret_cast<const char*>(binary.data()), binary.size());
        out.close();
        if (!out) throw std::runtime_error("shader cache write failed");
#ifdef _WIN32
        if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("shader cache publish failed: "+std::to_string(GetLastError()));
#else
        std::filesystem::rename(temp,path);
#endif
        return true;
    } catch (const std::exception& e) {
        std::error_code ignored; std::filesystem::remove(temp,ignored);
        if (error) *error=e.what();
        return false;
    }
}
}
