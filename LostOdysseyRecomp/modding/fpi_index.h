#pragma once
#include <cstddef>
#include <cstdint>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

// The archive index of a disc (LO.fpi) or a DLC (LODLC00x.fpi, whose one
// archive is the .fpi file itself). Little-endian on disc; the game swaps it
// after loading. Shared by the asset exporter and language packs.
namespace modding::fpi
{
// A file in an archive. record: offset of its 24-byte record in the index
// (+8 sector | disc and layer << 24, +14 flags with 0x40 = CPX, +16 stored
// size, +20 CPX decoded size).
struct File
{
    std::string path; // lower case, '/' separators
    uint32_t archive = 0;
    size_t record = 0;
    uint64_t offset = 0;
    uint32_t size = 0;
};

struct Index
{
    uint32_t disc = 0;
    std::vector<std::string> archives; // lower-case file names, in archive order
    std::vector<File> files;
};

constexpr uint32_t kFlagCpx = 0x40;
constexpr uint64_t kSector = 2048;

namespace detail
{
inline void Require(bool ok, const char *message)
{
    if (!ok) throw std::runtime_error(message);
}
inline uint32_t Le32(std::span<const uint8_t> b, size_t p)
{
    Require(p <= b.size() && 4 <= b.size() - p, "FPI read out of bounds");
    return uint32_t(b[p]) | uint32_t(b[p + 1]) << 8 | uint32_t(b[p + 2]) << 16 | uint32_t(b[p + 3]) << 24;
}
inline uint16_t Le16(std::span<const uint8_t> b, size_t p)
{
    Require(p <= b.size() && 2 <= b.size() - p, "FPI read out of bounds");
    return uint16_t(b[p] | uint16_t(b[p + 1]) << 8);
}

struct Names
{
    std::span<const uint8_t> bytes;
    uint32_t dictionary = 0, extensions = 0;
    std::string Unpack(size_t offset) const
    {
        // Base-40 filename alphabet of the FPI dictionary (see settings/menu_assets.cpp).
        constexpr char alphabet[] = "\0" "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_.\\";
        auto word = Le16(bytes, offset);
        const auto count = word % 40;
        Require(word / 1600 < 40, "invalid FPI name");
        std::string value;
        value += alphabet[word / 40 % 40]; value += alphabet[word / 1600];
        for (unsigned i = 0; i < count; ++i)
        {
            word = Le16(bytes, offset + 2 + 2 * i); Require(word / 1600 < 40, "invalid FPI name");
            value += alphabet[word % 40]; value += alphabet[word / 40 % 40]; value += alphabet[word / 1600];
        }
        if (const auto end = value.find('\0'); end != std::string::npos) value.resize(end);
        for (auto &c : value) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        return value;
    }
    std::string Name(uint32_t bits) const
    {
        if (!(bits & 0x3ffff)) return {};
        constexpr const char *suffix[] = {"", "_sndw", "_scrw", "_mapw", "_lvdw", "_navw", "_colw", "_camw",
            "_map", "_cam", "_bx", "_mw", "_a", "_d", "_f", "_m", "_p", "_u", "_w", "_0", "_1", "_2", "_00", "_01",
            "_0mw", "_nav", "_elgt", "_000a0", "_010a0", "_020a0", "_030a0", "_040a0"};
        auto name = Unpack(size_t(dictionary) + (bits & 0x3ffff) * 2) + suffix[(bits >> 18) & 31];
        if (const auto ext = (bits >> 23) & 31)
            name += "." + Unpack(size_t(dictionary) + Le16(bytes, size_t(extensions) + (ext - 1) * 2) * 2);
        return name;
    }
};
}

// Bytes of the index at the start of the file: the first `used` sectors.
inline uint64_t IndexSize(std::span<const uint8_t> header)
{
    const uint64_t used = uint64_t(detail::Le16(header, 12)) * kSector;
    detail::Require(used >= 64 && used <= 4 * 1024 * 1024, "invalid FPI index size");
    return used;
}

// index: IndexSize() bytes from the start of the file. Throws std::runtime_error.
inline Index Read(std::span<const uint8_t> index)
{
    using detail::Le16, detail::Le32, detail::Require;
    Require(Le32(index, 8) == 0x10000, "unsupported FPI version");
    Index result;
    result.disc = index[20];
    const detail::Names names{index, Le32(index, 40), Le32(index, 44)};
    const uint32_t count = Le16(index, 26), begin = Le32(index, 32);
    Require(count && count <= 64 && begin >= 64 && begin <= index.size() && size_t(count) * 48 <= index.size() - begin,
            "invalid FPI archive table");
    for (uint32_t i = 0; i < count; ++i)
    {
        const auto ar = size_t(begin) + i * 48;
        result.archives.push_back(names.Name(Le32(index, ar + 24)));
        const auto base = ar + Le32(index, ar + 4);
        Require(base <= index.size(), "invalid FPI archive record");
        struct Folder { uint32_t index, count, depth; std::string path; };
        const auto prefix = names.Name(Le32(index, ar + 20));
        std::vector<Folder> pending{{0, Le16(index, ar + 2), 0, prefix.empty() ? "" : prefix + "/"}};
        std::set<uint32_t> visited;
        while (!pending.empty())
        {
            const auto folder = std::move(pending.back());
            pending.pop_back();
            Require(folder.depth < 64 && uint64_t(folder.index) + folder.count <= 65536, "invalid FPI folder");
            for (uint32_t j = 0; j < folder.count; ++j)
            {
                const auto entry = folder.index + j;
                Require(visited.insert(entry).second, "FPI folder cycle");
                const auto p = base + size_t(entry) * 24;
                Require(p <= index.size() && 24 <= index.size() - p, "FPI record out of bounds");
                const auto bits = Le32(index, p);
                auto path = folder.path + names.Name(bits);
                for (auto &c : path) if (c == '\\') c = '/';
                if (bits & 0x10000000)
                    pending.push_back({Le32(index, p + 20), Le16(index, p + 14), folder.depth + 1, path + "/"});
                else
                    result.files.push_back({std::move(path), i, p, uint64_t(Le32(index, p + 8) & 0xffffff) * kSector,
                                            Le32(index, p + 16)});
            }
        }
    }
    return result;
}
}
