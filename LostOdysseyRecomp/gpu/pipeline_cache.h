#pragma once

// Device-independent PSO recipes. This file never stores driver cache blobs or
// native structure layouts. The caller must verify shader sources and validate
// renderer-specific primitive/format enums before creating a device object.
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <span>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace gpu::pipeline_cache
{
    struct Key
    {
        uint64_t vs = 0, ps = 0;
        uint32_t blend = 0, depthControl = 0, modeCull = 0, colorMask = 0;
        uint32_t prim = 0, rtFormat = 0, depthFormat = 0, flags = 0;
        uint32_t stencilRefMask = 0, stencilRefMaskBack = 0;
        int32_t depthBias = 0;
        uint32_t slopeBias = 0;
        bool operator==(const Key&) const = default;
    };

    inline constexpr size_t kMaxRecords = 16384;
    inline constexpr size_t kKeyBytes = 64;
    inline constexpr size_t kRecordBytes = kKeyBytes + 8;
    inline constexpr size_t kHeaderBytes = 48;
    inline constexpr uint32_t kFileVersion = 1;
    using Validator = std::function<bool(const Key&)>;

    namespace detail
    {
        inline uint64_t Hash(std::span<const uint8_t> bytes) noexcept
        {
            uint64_t hash = 0xCBF29CE484222325ULL;
            for (const uint8_t byte : bytes)
                hash = (hash ^ byte) * 0x100000001B3ULL;
            return hash;
        }

        inline void Put32(uint8_t* out, uint32_t value) noexcept
        {
            for (unsigned i = 0; i < 4; ++i)
                out[i] = static_cast<uint8_t>(value >> (i * 8));
        }

        inline void Put64(uint8_t* out, uint64_t value) noexcept
        {
            for (unsigned i = 0; i < 8; ++i)
                out[i] = static_cast<uint8_t>(value >> (i * 8));
        }

        inline uint32_t Read32(const uint8_t* in) noexcept
        {
            uint32_t value = 0;
            for (unsigned i = 0; i < 4; ++i)
                value |= uint32_t(in[i]) << (i * 8);
            return value;
        }

        inline uint64_t Read64(const uint8_t* in) noexcept
        {
            uint64_t value = 0;
            for (unsigned i = 0; i < 8; ++i)
                value |= uint64_t(in[i]) << (i * 8);
            return value;
        }

        inline std::array<uint8_t, kKeyBytes> Encode(const Key& key) noexcept
        {
            std::array<uint8_t, kKeyBytes> bytes{};
            Put64(bytes.data(), key.vs);
            Put64(bytes.data() + 8, key.ps);
            const std::array<uint32_t, 12> words = { key.blend, key.depthControl,
                key.modeCull, key.colorMask, key.prim, key.rtFormat, key.depthFormat,
                key.flags, key.stencilRefMask, key.stencilRefMaskBack,
                std::bit_cast<uint32_t>(key.depthBias), key.slopeBias };
            for (size_t i = 0; i < words.size(); ++i)
                Put32(bytes.data() + 16 + 4 * i, words[i]);
            return bytes;
        }

        inline Key Decode(const uint8_t* bytes) noexcept
        {
            Key key{};
            key.vs = Read64(bytes);
            key.ps = Read64(bytes + 8);
            key.blend = Read32(bytes + 16);
            key.depthControl = Read32(bytes + 20);
            key.modeCull = Read32(bytes + 24);
            key.colorMask = Read32(bytes + 28);
            key.prim = Read32(bytes + 32);
            key.rtFormat = Read32(bytes + 36);
            key.depthFormat = Read32(bytes + 40);
            key.flags = Read32(bytes + 44);
            key.stencilRefMask = Read32(bytes + 48);
            key.stencilRefMaskBack = Read32(bytes + 52);
            key.depthBias = std::bit_cast<int32_t>(Read32(bytes + 56));
            key.slopeBias = Read32(bytes + 60);
            return key;
        }

        inline constexpr std::array<uint8_t, 8> kMagic = { 'L', 'O', 'P', 'S', 'O', '0', '0', '1' };

        // A unique directory reserves a temporary pathname using an atomic
        // create operation. Only these two exact paths are ever cleaned up.
        struct TemporaryFile
        {
            std::filesystem::path directory, file;
            ~TemporaryFile()
            {
                std::error_code ignored;
                if (!file.empty()) std::filesystem::remove(file, ignored);
                if (!directory.empty()) std::filesystem::remove(directory, ignored);
            }
        };

        inline bool AtomicReplace(const std::filesystem::path& from,
                                  const std::filesystem::path& to, std::string& error)
        {
#ifdef _WIN32
            if (MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                return true;
            error = "atomic replacement failed (Windows error " + std::to_string(GetLastError()) + ")";
            return false;
#else
            std::error_code ec;
            std::filesystem::rename(from, to, ec);
            if (!ec) return true;
            error = "atomic replacement failed: " + ec.message();
            return false;
#endif
        }
    }

    struct KeyHash
    {
        size_t operator()(const Key& key) const noexcept
        {
            return static_cast<size_t>(detail::Hash(detail::Encode(key)));
        }
    };

    // These masks are the values emitted by the current renderer. Primitive and
    // format values are deliberately left to the caller's Validator. No key bits
    // are canonicalized: a warmed entry must equal the later draw's exact key.
    inline bool IsValid(const Key& key) noexcept
    {
        return !(key.modeCull & ~0x3807U) && !(key.colorMask & ~0xFU) &&
            !(key.stencilRefMask & ~0xFFFFFFU) && !(key.stencilRefMaskBack & ~0xFFFFFFU) &&
            key.flags == 0 && std::isfinite(std::bit_cast<float>(key.slopeBias));
    }

    enum class Status { Missing, Loaded, Incompatible, Invalid, IoError };
    struct LoadResult
    {
        Status status = Status::Invalid;
        std::vector<Key> keys;
        std::string error;
        size_t duplicates = 0;
    };
    struct WriteResult
    {
        bool ok = false;
        std::string error;
        size_t written = 0;
        size_t duplicates = 0;
    };

    inline LoadResult Load(const std::filesystem::path& path, uint32_t shaderVersion,
                           uint32_t recipeVersion, const Validator& validate = {})
    {
        auto fail = [](Status status, const std::string& error) {
            return LoadResult{ status, {}, error, 0 };
        };
        try
        {
            std::error_code ec;
            if (!std::filesystem::exists(path, ec))
                return ec ? fail(Status::IoError, ec.message()) : LoadResult{ Status::Missing, {}, {}, 0 };
            const auto size = std::filesystem::file_size(path, ec);
            if (ec) return fail(Status::IoError, ec.message());
            if (size < kHeaderBytes || size > kHeaderBytes + kMaxRecords * kRecordBytes)
                return fail(Status::Invalid, "invalid recipe file length");
            std::vector<uint8_t> bytes(static_cast<size_t>(size));
            std::ifstream input(path, std::ios::binary);
            if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
                return fail(Status::IoError, "could not read complete recipe file");
            if (input.peek() != std::char_traits<char>::eof())
                return fail(Status::Invalid, "recipe file changed while being read");
            for (size_t i = 0; i < detail::kMagic.size(); ++i)
                if (bytes[i] != detail::kMagic[i]) return fail(Status::Invalid, "invalid recipe signature");
            if (detail::Read64(bytes.data() + 40) != detail::Hash(std::span(bytes).first(40)))
                return fail(Status::Invalid, "recipe header checksum mismatch");
            if (detail::Read32(bytes.data() + 8) != kFileVersion ||
                detail::Read32(bytes.data() + 12) != shaderVersion ||
                detail::Read32(bytes.data() + 16) != recipeVersion)
                return fail(Status::Incompatible, "recipe cache version mismatch");
            const uint32_t count = detail::Read32(bytes.data() + 24);
            if (detail::Read32(bytes.data() + 20) != kRecordBytes ||
                detail::Read32(bytes.data() + 28) != 0 || count > kMaxRecords ||
                bytes.size() != kHeaderBytes + size_t(count) * kRecordBytes)
                return fail(Status::Invalid, "invalid recipe record framing");
            if (detail::Read64(bytes.data() + 32) != detail::Hash(std::span(bytes).subspan(kHeaderBytes)))
                return fail(Status::Invalid, "recipe payload checksum mismatch");

            LoadResult result{ Status::Loaded, {}, {}, 0 };
            result.keys.reserve(count);
            std::unordered_set<Key, KeyHash> seen;
            seen.reserve(count);
            for (size_t i = 0; i < count; ++i)
            {
                const uint8_t* record = bytes.data() + kHeaderBytes + i * kRecordBytes;
                if (detail::Read64(record + kKeyBytes) != detail::Hash({ record, kKeyBytes }))
                    return fail(Status::Invalid, "recipe record checksum mismatch");
                const Key key = detail::Decode(record);
                if (!IsValid(key) || (validate && !validate(key)))
                    return fail(Status::Invalid, "recipe contains unsupported state");
                if (seen.insert(key).second) result.keys.push_back(key);
                else ++result.duplicates;
            }
            return result;
        }
        catch (const std::exception& e)
        {
            return fail(Status::IoError, e.what());
        }
    }

    inline WriteResult Write(const std::filesystem::path& path, std::span<const Key> keys,
                             uint32_t shaderVersion, uint32_t recipeVersion,
                             const Validator& validate = {})
    {
        WriteResult result;
        try
        {
            if (keys.size() > kMaxRecords)
            {
                result.error = "recipe record limit exceeded";
                return result;
            }
            std::vector<uint8_t> bytes(kHeaderBytes, 0);
            bytes.reserve(kHeaderBytes + keys.size() * kRecordBytes);
            std::unordered_set<Key, KeyHash> seen;
            seen.reserve(keys.size());
            for (const Key& key : keys)
            {
                if (!IsValid(key) || (validate && !validate(key)))
                {
                    result.error = "recipe contains unsupported state";
                    return result;
                }
                if (!seen.insert(key).second) { ++result.duplicates; continue; }
                const auto encoded = detail::Encode(key);
                const size_t offset = bytes.size();
                bytes.resize(offset + kRecordBytes);
                std::copy(encoded.begin(), encoded.end(), bytes.begin() + static_cast<ptrdiff_t>(offset));
                detail::Put64(bytes.data() + offset + kKeyBytes, detail::Hash(encoded));
            }
            std::copy(detail::kMagic.begin(), detail::kMagic.end(), bytes.begin());
            detail::Put32(bytes.data() + 8, kFileVersion);
            detail::Put32(bytes.data() + 12, shaderVersion);
            detail::Put32(bytes.data() + 16, recipeVersion);
            detail::Put32(bytes.data() + 20, static_cast<uint32_t>(kRecordBytes));
            detail::Put32(bytes.data() + 24, static_cast<uint32_t>(seen.size()));
            detail::Put64(bytes.data() + 32, detail::Hash(std::span(bytes).subspan(kHeaderBytes)));
            detail::Put64(bytes.data() + 40, detail::Hash(std::span(bytes).first(40)));

            detail::TemporaryFile temporary;
            static std::atomic<uint64_t> serial{ 0 };
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            for (unsigned attempt = 0; attempt < 64; ++attempt)
            {
                auto candidate = path;
                candidate += ".tmp-" + std::to_string(stamp) + "-" + std::to_string(serial.fetch_add(1));
                std::error_code ec;
                if (std::filesystem::create_directory(candidate, ec))
                {
                    temporary.directory = std::move(candidate);
                    temporary.file = temporary.directory / "data";
                    break;
                }
                if (ec)
                {
                    result.error = "could not reserve recipe temporary file: " + ec.message();
                    return result;
                }
            }
            if (temporary.file.empty())
            {
                result.error = "could not reserve unique recipe temporary file";
                return result;
            }
            {
                std::ofstream output(temporary.file, std::ios::binary | std::ios::trunc);
                output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
                output.flush();
                if (!output)
                {
                    result.error = "could not write complete recipe file";
                    return result;
                }
                output.close();
                if (!output)
                {
                    result.error = "could not close recipe file";
                    return result;
                }
            }
            if (!detail::AtomicReplace(temporary.file, path, result.error)) return result;
            result.ok = true;
            result.written = seen.size();
            return result;
        }
        catch (const std::exception& e)
        {
            result.error = e.what();
            return result;
        }
    }
}
