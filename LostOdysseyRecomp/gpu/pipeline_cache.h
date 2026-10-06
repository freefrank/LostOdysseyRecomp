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
#include <unordered_map>
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

    // Scenes a recipe was drawn in: up to kSceneSlots tags, 0 = empty slot.
    // A tag is kind << 28 | id; kManyScenes replaces the list once it overflows.
    // kNoScene: drawn while no map or battle was known (title, boot menus).
    inline constexpr size_t kSceneSlots = 8;
    inline constexpr uint32_t kSceneMap = 1, kSceneBattle = 2;
    inline constexpr uint32_t kNoScene = 0xF0000000u, kManyScenes = 0xFFFFFFFFu;
    inline constexpr uint32_t SceneTag(uint32_t kind, uint32_t id) noexcept { return kind << 28 | (id & 0x0FFFFFFFu); }
    struct Scenes
    {
        std::array<uint32_t, kSceneSlots> tags{};
        bool operator==(const Scenes&) const = default;
        bool Contains(uint32_t tag) const noexcept
        {
            return tag && std::find(tags.begin(), tags.end(), tag) != tags.end();
        }
        bool Many() const noexcept { return tags[0] == kManyScenes; }
        size_t Count() const noexcept
        {
            return Many() ? kSceneSlots + 1 : size_t(std::count_if(tags.begin(), tags.end(), [](uint32_t t) { return t != 0; }));
        }
        // Returns whether the list changed.
        bool Add(uint32_t tag) noexcept
        {
            if (!tag || Many() || Contains(tag)) return false;
            for (auto& slot : tags)
                if (!slot) { slot = tag; return true; }
            tags = {};
            tags[0] = kManyScenes;
            return true;
        }
        bool Merge(const Scenes& other) noexcept
        {
            if (other.Many()) { const bool changed = !Many(); tags = other.tags; return changed; }
            bool changed = false;
            for (const uint32_t tag : other.tags) changed |= Add(tag);
            return changed;
        }
    };
    struct Record
    {
        Key key;
        Scenes scenes;
        bool operator==(const Record&) const = default;
    };

    inline constexpr size_t kMaxRecords = 16384;
    inline constexpr size_t kKeyBytes = 64;
    inline constexpr size_t kSceneBytes = kSceneSlots * 4;
    // File version 1 records hold the key and its checksum; version 2 adds the scenes.
    inline constexpr size_t kRecordBytesV1 = kKeyBytes + 8;
    inline constexpr size_t kRecordBytes = kKeyBytes + kSceneBytes + 8;
    inline constexpr size_t kHeaderBytes = 48;
    inline constexpr uint32_t kFileVersion = 2;
    // Load(): accept any translator version (a shipped corpus outlives it).
    inline constexpr uint32_t kAnyShaderVersion = 0;
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

        // Writes bytes to a unique temporary file next to path, then replaces path.
        inline bool WriteAtomically(const std::filesystem::path& path, std::span<const uint8_t> bytes,
                                    std::string& error)
        {
            TemporaryFile temporary;
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
                    error = "could not reserve temporary file: " + ec.message();
                    return false;
                }
            }
            if (temporary.file.empty())
            {
                error = "could not reserve unique temporary file";
                return false;
            }
            {
                std::ofstream output(temporary.file, std::ios::binary | std::ios::trunc);
                output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
                output.flush();
                if (!output)
                {
                    error = "could not write complete file";
                    return false;
                }
                output.close();
                if (!output)
                {
                    error = "could not close file";
                    return false;
                }
            }
            return AtomicReplace(temporary.file, path, error);
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
    // format values are deliberately left to the caller's Validator.
    inline bool IsValid(const Key& key) noexcept
    {
        return !(key.modeCull & ~0x3807U) && !(key.colorMask & ~0xFU) &&
            !(key.stencilRefMask & ~0xFFFFFFU) && !(key.stencilRefMaskBack & ~0xFFFFFFU) &&
            key.flags == 0 && std::isfinite(std::bit_cast<float>(key.slopeBias));
    }

    // RB_BLENDCONTROL for "no blending": ONE, ADD, ZERO for color and alpha.
    inline constexpr uint32_t kNoBlend = 0x00010001u;

    // Clears the key bits the renderer's DescribePipeline does not turn into
    // pipeline state, so draws that differ only there share one pipeline and one
    // recipe. Every rule here must match DescribePipeline (renderer.cpp):
    // - depthControl: bit 3 (early Z) is never read; Z enable and the Z function
    //   only matter with a depth target and Z enabled; stencil state only with a
    //   depth target and stencil enabled, back-face stencil only with bit 7. The
    //   Z write bit is kept: DescribePipeline copies it unconditionally.
    // - Polygon offset enables (modeCull 0x3800) are already folded into the bias
    //   values, which only apply with depth testing on.
    // - Blend state is ignored without a color write mask (DescribePipeline
    //   treats such a draw as unblended), and bits 13-15/29-31 are never read.
    // - Every primitive other than points, lines, line strips, triangle strips
    //   and rectangle lists is drawn as a triangle list.
    inline Key Normalize(Key key) noexcept
    {
        const bool depthTarget = key.depthFormat != 0;
        uint32_t control = key.depthControl & ~0x8u;
        const bool depthOn = depthTarget && (control & 2);
        const bool stencilOn = depthTarget && (control & 1);
        if (!depthOn) {
            control &= ~0x72u;
            key.depthBias = 0;
            key.slopeBias = 0;
        }
        if (!stencilOn) {
            control &= 0x76u;
            key.stencilRefMask = key.stencilRefMaskBack = 0;
        } else if (!(control & 0x80)) {
            control &= 0x000FFFFFu;
            key.stencilRefMaskBack = 0;
        }
        key.depthControl = control;
        key.modeCull &= 0x7u;
        if (key.slopeBias == 0x80000000u) key.slopeBias = 0; // -0.0
        key.blend = (key.colorMask & 0xF) ? key.blend & 0x1FFF1FFFu : kNoBlend;
        if (key.prim != 1 && key.prim != 2 && key.prim != 3 && key.prim != 6 && key.prim != 8) key.prim = 4;
        return key;
    }

    enum class Status { Missing, Loaded, Incompatible, Invalid, IoError };
    struct LoadResult
    {
        Status status = Status::Invalid;
        std::vector<Record> records;
        std::string error;
        size_t duplicates = 0;
        // An older file or recipe version, or keys that normalization changed:
        // the caller should write the file again.
        bool migrated = false;
    };
    struct WriteResult
    {
        bool ok = false;
        std::string error;
        size_t written = 0;
        size_t duplicates = 0;
    };

    // Reads file version 1 (keys only) and 2 (keys and scenes). Recipe versions
    // from oldestRecipeVersion up to recipeVersion are accepted; keys of an older
    // one are normalized (version 1 keys predate Normalize). shaderVersion may be
    // kAnyShaderVersion. Duplicate keys merge their scenes.
    inline LoadResult Load(const std::filesystem::path& path, uint32_t shaderVersion,
                           uint32_t recipeVersion, const Validator& validate = {},
                           uint32_t oldestRecipeVersion = 0)
    {
        auto fail = [](Status status, const std::string& error) {
            return LoadResult{ status, {}, error, 0, false };
        };
        try
        {
            std::error_code ec;
            if (!std::filesystem::exists(path, ec))
                return ec ? fail(Status::IoError, ec.message()) : LoadResult{ Status::Missing, {}, {}, 0, false };
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
            const uint32_t fileVersion = detail::Read32(bytes.data() + 8);
            const uint32_t fileRecipeVersion = detail::Read32(bytes.data() + 16);
            const uint32_t oldest = oldestRecipeVersion ? std::min(oldestRecipeVersion, recipeVersion) : recipeVersion;
            if ((fileVersion != 1 && fileVersion != kFileVersion) ||
                (shaderVersion != kAnyShaderVersion && detail::Read32(bytes.data() + 12) != shaderVersion) ||
                fileRecipeVersion < oldest || fileRecipeVersion > recipeVersion)
                return fail(Status::Incompatible, "recipe cache version mismatch");
            const size_t recordBytes = fileVersion == 1 ? kRecordBytesV1 : kRecordBytes;
            const size_t checkedBytes = recordBytes - 8;
            const uint32_t count = detail::Read32(bytes.data() + 24);
            if (detail::Read32(bytes.data() + 20) != recordBytes ||
                detail::Read32(bytes.data() + 28) != 0 || count > kMaxRecords ||
                bytes.size() != kHeaderBytes + size_t(count) * recordBytes)
                return fail(Status::Invalid, "invalid recipe record framing");
            if (detail::Read64(bytes.data() + 32) != detail::Hash(std::span(bytes).subspan(kHeaderBytes)))
                return fail(Status::Invalid, "recipe payload checksum mismatch");

            const bool normalize = fileRecipeVersion < recipeVersion;
            LoadResult result{ Status::Loaded, {}, {}, 0, fileVersion != kFileVersion || normalize };
            result.records.reserve(count);
            std::unordered_map<Key, size_t, KeyHash> seen;
            seen.reserve(count);
            for (size_t i = 0; i < count; ++i)
            {
                const uint8_t* record = bytes.data() + kHeaderBytes + i * recordBytes;
                if (detail::Read64(record + checkedBytes) != detail::Hash({ record, checkedBytes }))
                    return fail(Status::Invalid, "recipe record checksum mismatch");
                Record entry{ detail::Decode(record), {} };
                if (!IsValid(entry.key) || (validate && !validate(entry.key)))
                    return fail(Status::Invalid, "recipe contains unsupported state");
                if (fileVersion != 1)
                    for (size_t slot = 0; slot < kSceneSlots; ++slot)
                        entry.scenes.tags[slot] = detail::Read32(record + kKeyBytes + 4 * slot);
                if (normalize) entry.key = Normalize(entry.key);
                const auto [found, inserted] = seen.try_emplace(entry.key, result.records.size());
                if (inserted) result.records.push_back(entry);
                else
                {
                    result.records[found->second].scenes.Merge(entry.scenes);
                    ++result.duplicates;
                }
            }
            return result;
        }
        catch (const std::exception& e)
        {
            return fail(Status::IoError, e.what());
        }
    }

    // Writes file version 2. Duplicate keys merge their scenes.
    inline WriteResult Write(const std::filesystem::path& path, std::span<const Record> records,
                             uint32_t shaderVersion, uint32_t recipeVersion,
                             const Validator& validate = {})
    {
        WriteResult result;
        try
        {
            if (records.size() > kMaxRecords)
            {
                result.error = "recipe record limit exceeded";
                return result;
            }
            std::vector<Record> unique;
            unique.reserve(records.size());
            std::unordered_map<Key, size_t, KeyHash> seen;
            seen.reserve(records.size());
            for (const Record& record : records)
            {
                if (!IsValid(record.key) || (validate && !validate(record.key)))
                {
                    result.error = "recipe contains unsupported state";
                    return result;
                }
                const auto [found, inserted] = seen.try_emplace(record.key, unique.size());
                if (inserted) unique.push_back(record);
                else { unique[found->second].scenes.Merge(record.scenes); ++result.duplicates; }
            }
            std::vector<uint8_t> bytes(kHeaderBytes + unique.size() * kRecordBytes, 0);
            for (size_t i = 0; i < unique.size(); ++i)
            {
                uint8_t* out = bytes.data() + kHeaderBytes + i * kRecordBytes;
                const auto encoded = detail::Encode(unique[i].key);
                std::copy(encoded.begin(), encoded.end(), out);
                for (size_t slot = 0; slot < kSceneSlots; ++slot)
                    detail::Put32(out + kKeyBytes + 4 * slot, unique[i].scenes.tags[slot]);
                detail::Put64(out + kKeyBytes + kSceneBytes, detail::Hash({ out, kKeyBytes + kSceneBytes }));
            }
            std::copy(detail::kMagic.begin(), detail::kMagic.end(), bytes.begin());
            detail::Put32(bytes.data() + 8, kFileVersion);
            detail::Put32(bytes.data() + 12, shaderVersion);
            detail::Put32(bytes.data() + 16, recipeVersion);
            detail::Put32(bytes.data() + 20, static_cast<uint32_t>(kRecordBytes));
            detail::Put32(bytes.data() + 24, static_cast<uint32_t>(unique.size()));
            detail::Put64(bytes.data() + 32, detail::Hash(std::span(bytes).subspan(kHeaderBytes)));
            detail::Put64(bytes.data() + 40, detail::Hash(std::span(bytes).first(40)));

            if (!detail::WriteAtomically(path, bytes, result.error))
            {
                result.error = "recipe file: " + result.error;
                return result;
            }
            result.ok = true;
            result.written = unique.size();
            return result;
        }
        catch (const std::exception& e)
        {
            result.error = e.what();
            return result;
        }
    }
}
