#include "import_image.h"

#include <algorithm>
#include <cstring>
#include <regex>
#include <set>
#include <sstream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace install
{
namespace
{

constexpr const char MAGIC[20] = {
    'M', 'I', 'C', 'R', 'O', 'S', 'O', 'F', 'T', '*',
    'X', 'B', 'O', 'X', '*', 'M', 'E', 'D', 'I', 'A'
};
constexpr uint32_t SECTOR = 2048;
constexpr uint32_t GOD_BLOCK_SIZE = 4096;
constexpr uint64_t GOD_BLOCKS_PER_CHUNK = 0xA1C4;
constexpr uint64_t GOD_CHUNK_DATA_PAYLOAD = GOD_BLOCKS_PER_CHUNK * GOD_BLOCK_SIZE; // 0xA1C4 * 4096
constexpr uint64_t GOD_CHUNK_FULL_SIZE = 0xA290000; // 170459136 bytes

bool IsSymlinkOrReparse(const std::filesystem::path& path)
{
    std::error_code ec;
    if (std::filesystem::is_symlink(path, ec))
        return true;
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data))
    {
        if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
            return true;
    }
#endif
    return false;
}

std::string ToLower(std::string_view str)
{
    std::string result(str);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

std::string ToUpper(std::string_view str)
{
    std::string result(str);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return result;
}

bool ValidateComponentName(std::string_view name)
{
    if (name.empty() || name == "." || name == "..")
        return false;
    char last = name.back();
    if (last == ' ' || last == '.')
        return false;
    for (char c : name)
    {
        if (static_cast<unsigned char>(c) < 32)
            return false;
        if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' ||
            c == '\\' || c == '|' || c == '?' || c == '*')
            return false;
    }

    std::string upper = ToUpper(name);
    size_t dot = upper.find('.');
    std::string stem = (dot != std::string::npos) ? upper.substr(0, dot) : upper;
    if (stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL")
        return false;
    if (stem.size() == 4)
    {
        if ((stem.rfind("COM", 0) == 0 || stem.rfind("LPT", 0) == 0) &&
            stem[3] >= '1' && stem[3] <= '9')
            return false;
    }
    return true;
}

uint16_t ReadLeU16(const uint8_t* p)
{
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

uint16_t ReadBeU16(const uint8_t* p)
{
    return (static_cast<uint16_t>(p[0]) << 8) | static_cast<uint16_t>(p[1]);
}

uint32_t ReadLeU32(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

uint32_t ReadBeU32(const uint8_t* p)
{
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) |
           static_cast<uint32_t>(p[3]);
}

uint64_t ReadBeU64(const uint8_t* p)
{
    return (static_cast<uint64_t>(ReadBeU32(p)) << 32) | ReadBeU32(p + 4);
}

// Traverse XDVDFS Volume Descriptor & Directory Tree
std::vector<Entry> ReadXdvdfsEntries(ImageReader& reader)
{
    std::vector<uint8_t> vd(SECTOR);
    reader.Read(0x10000, vd.data(), SECTOR);

    if (std::memcmp(vd.data(), MAGIC, 20) != 0 ||
        std::memcmp(vd.data() + 0x7ec, MAGIC, 20) != 0)
    {
        throw Error("Unsupported GOD/SVOD/XDVDFS layout");
    }

    uint32_t rootSector = ReadLeU32(vd.data() + 20);
    uint32_t rootSize = ReadLeU32(vd.data() + 24);

    struct DirTask
    {
        std::string prefix;
        uint32_t sector = 0;
        uint32_t size = 0;
        int depth = 0;
    };

    std::vector<DirTask> pending;
    pending.push_back({"", rootSector, rootSize, 0});

    std::set<uint32_t> visitedDirs;
    std::set<std::string> seenNames;
    std::vector<Entry> entries;

    while (!pending.empty())
    {
        auto task = pending.back();
        pending.pop_back();

        if (task.depth > 64 || task.size > 16 * 1024 * 1024 || visitedDirs.count(task.sector))
            throw Error("Invalid or recursive image directory");

        visitedDirs.insert(task.sector);
        if (task.size == 0) continue;

        std::vector<uint8_t> dirData(task.size);
        reader.Read(static_cast<uint64_t>(task.sector) * SECTOR, dirData.data(), task.size);

        std::vector<size_t> nodes{0};
        std::set<size_t> visitedOffsets;

        while (!nodes.empty())
        {
            size_t offset = nodes.back();
            nodes.pop_back();

            if (visitedOffsets.count(offset) || offset + 14 > dirData.size())
                throw Error("Invalid image directory tree");

            visitedOffsets.insert(offset);

            const uint8_t* rec = dirData.data() + offset;
            uint16_t left = ReadLeU16(rec);
            uint16_t right = ReadLeU16(rec + 2);
            uint32_t startSector = ReadLeU32(rec + 4);
            uint32_t fileLength = ReadLeU32(rec + 8);
            uint8_t attr = rec[12];
            uint8_t nameLen = rec[13];

            if (left == 0xffff && right == 0xffff)
                continue;

            if (offset + 14 + nameLen > dirData.size())
                throw Error("Truncated image file name");

            std::string name(reinterpret_cast<const char*>(rec + 14), nameLen);
            if (!ValidateComponentName(name))
                throw Error("Unsafe file name: '" + name + "'");

            std::string relative = task.prefix + name;
            std::string lowerRel = ToLower(relative);

            if (seenNames.count(lowerRel) || seenNames.size() > 100000)
                throw Error("Duplicate or excessive image entries");
            seenNames.insert(lowerRel);

            if (left != 0) nodes.push_back(static_cast<size_t>(left) * 4);
            if (right != 0) nodes.push_back(static_cast<size_t>(right) * 4);

            if (ToLower(name) == "$systemupdate")
                continue;

            if (attr & 0x10) // Subdirectory
            {
                pending.push_back({relative + "/", startSector, fileLength, task.depth + 1});
            }
            else // Regular file
            {
                uint64_t fileStartOffset = static_cast<uint64_t>(startSector) * SECTOR;
                if (fileStartOffset + fileLength > reader.GetLimit())
                    throw Error("File extends beyond image");

                entries.push_back({relative, fileStartOffset, fileLength});
            }
        }
    }

    return entries;
}

} // namespace

// ----------------------------------------------------------------------------
// FolderSource
// ----------------------------------------------------------------------------
std::vector<Entry> FolderSource::GetEntries(const Cancelled& cancelled)
{
    if (cancelled && cancelled())
        throw Error("Source check cancelled", true);

    if (IsSymlinkOrReparse(path_))
        throw Error("Links are not supported as game sources");

    std::vector<Entry> entries;
    std::set<std::string> names;
    std::vector<std::filesystem::path> stack{path_};

    std::error_code ec;
    while (!stack.empty())
    {
        if (cancelled && cancelled())
            throw Error("Source check cancelled", true);

        auto cur = stack.back();
        stack.pop_back();

        for (const auto& dirEntry : std::filesystem::directory_iterator(cur, ec))
        {
            if (ec) continue;
            const auto& p = dirEntry.path();
            if (IsSymlinkOrReparse(p))
                throw Error("Links are not supported: " + p.string());

            std::string filename = p.filename().string();
            if (!ValidateComponentName(filename))
                throw Error("Unsafe file name: '" + filename + "'");

            if (dirEntry.is_directory(ec))
            {
                if (ToLower(filename) == "$systemupdate")
                    continue;

                auto rel = std::filesystem::relative(p, path_, ec).generic_string();
                auto lowerRel = ToLower(rel);
                if (names.count(lowerRel))
                    throw Error("Duplicate file name: " + rel);
                names.insert(lowerRel);
                stack.push_back(p);
            }
            else if (dirEntry.is_regular_file(ec))
            {
                auto rel = std::filesystem::relative(p, path_, ec).generic_string();
                auto lowerRel = ToLower(rel);
                if (names.count(lowerRel))
                    throw Error("Duplicate file name: " + rel);
                names.insert(lowerRel);

                uint64_t sz = dirEntry.file_size(ec);
                entries.push_back({rel, 0, sz});
            }
        }
    }

    return entries;
}

// ----------------------------------------------------------------------------
// IsoImageReader
// ----------------------------------------------------------------------------
IsoImageReader::IsoImageReader(std::filesystem::path path, const Cancelled& cancelled)
    : path_(std::move(path))
{
    if (IsSymlinkOrReparse(path_))
        throw Error("Links are not supported as game sources");

    std::error_code ec;
    limit_ = std::filesystem::file_size(path_, ec);
    if (ec) throw Error("Could not inspect ISO file: " + path_.string());

    stream_.open(path_, std::ios::binary);
    if (!stream_) throw Error("Could not open ISO file: " + path_.string());

    // Search for sector-aligned MAGIC at 0x10000 descriptor offset.
    // Scan in 1MB chunks up to 512MB to find base.
    bool found = false;
    uint64_t searchLimit = std::min<uint64_t>(limit_, 512ULL * 1024 * 1024);
    std::vector<uint8_t> chunk(1024 * 1024 + SECTOR);

    for (uint64_t offset = 0; offset < searchLimit; offset += 1024 * 1024)
    {
        if (cancelled && cancelled())
            throw Error("Source check cancelled", true);

        stream_.clear();
        stream_.seekg(offset);
        size_t toRead = static_cast<size_t>(std::min<uint64_t>(chunk.size(), limit_ - offset));
        stream_.read(reinterpret_cast<char*>(chunk.data()), toRead);
        size_t bytesRead = static_cast<size_t>(stream_.gcount());

        if (bytesRead < 20) continue;

        // XDVDFS descriptors start on 2048-byte sector boundaries. The
        // overlapping sector keeps a descriptor crossing a chunk edge intact.
        for (size_t pos = 0; pos + SECTOR <= bytesRead; pos += SECTOR)
        {
            if (std::memcmp(chunk.data() + pos, MAGIC, 20) == 0)
            {
                uint64_t absolute = offset + pos;
                if (absolute >= 0x10000)
                {
                    if (std::memcmp(chunk.data() + pos + 0x7ec, MAGIC, 20) == 0)
                    {
                        base_ = absolute - 0x10000;
                        limit_ -= base_;
                        found = true;
                        break;
                    }
                }
            }
        }
        if (found) break;
    }

    if (!found)
        throw Error("No XDVDFS game partition found in ISO: " + path_.string());
}

void IsoImageReader::Read(uint64_t offset, void* buffer, size_t size)
{
    if (offset > limit_ || size > limit_ - offset)
        throw Error("Image file range is outside the source");

    stream_.seekg(base_ + offset);
    stream_.read(reinterpret_cast<char*>(buffer), size);
    if (static_cast<size_t>(stream_.gcount()) != size)
        throw Error("Truncated game image read");
}

std::vector<Entry> IsoImageReader::GetEntries()
{
    return ReadXdvdfsEntries(*this);
}

// ----------------------------------------------------------------------------
// GodImageReader
// ----------------------------------------------------------------------------
GodImageReader::GodImageReader(std::filesystem::path path, const Cancelled& cancelled)
    : path_(std::move(path))
{
    if (IsSymlinkOrReparse(path_))
        throw Error("Links are not supported as game sources");

    std::vector<std::filesystem::path> names;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(path_, ec))
    {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;
        std::string filename = entry.path().filename().string();
        static const std::regex dataChunkRegex(R"(Data\d{4})", std::regex::icase);
        if (std::regex_match(filename, dataChunkRegex))
        {
            names.push_back(entry.path());
        }
    }

    if (names.empty())
        throw Error("Missing or non-contiguous GOD data chunks");

    std::sort(names.begin(), names.end(), [](const auto& a, const auto& b) {
        return ToLower(a.filename().string()) < ToLower(b.filename().string());
    });

    for (size_t i = 0; i < names.size(); ++i)
    {
        char expected[16];
        std::snprintf(expected, sizeof(expected), "data%04zu", i);
        if (ToLower(names[i].filename().string()) != expected)
            throw Error("Missing or non-contiguous GOD data chunks");

        if (IsSymlinkOrReparse(names[i]))
            throw Error("Invalid GOD chunk: " + names[i].filename().string());

        uint64_t sz = std::filesystem::file_size(names[i], ec);
        if (i < names.size() - 1 && sz != GOD_CHUNK_FULL_SIZE)
            throw Error("Invalid GOD chunk: " + names[i].filename().string());

        files_.emplace_back(names[i], std::ios::binary);
        if (!files_.back())
            throw Error("Could not open GOD chunk: " + names[i].filename().string());
    }

    limit_ = names.size() * GOD_CHUNK_DATA_PAYLOAD;
}

void GodImageReader::Read(uint64_t offset, void* buffer, size_t size)
{
    if (offset > limit_ || size > limit_ - offset)
        throw Error("Image file range is outside the source");

    uint8_t* out = static_cast<uint8_t*>(buffer);
    while (size > 0)
    {
        uint64_t block = offset / GOD_BLOCK_SIZE;
        uint64_t inner = offset % GOD_BLOCK_SIZE;

        uint64_t chunkIndex = block / GOD_BLOCKS_PER_CHUNK;
        uint64_t chunkBlock = block % GOD_BLOCKS_PER_CHUNK;

        uint64_t group = chunkBlock / 204;
        uint64_t within = chunkBlock % 204;

        uint64_t physical = GOD_BLOCK_SIZE + (group + 1) * GOD_BLOCK_SIZE + chunkBlock * GOD_BLOCK_SIZE + inner;
        uint64_t take = std::min<uint64_t>(size, (204 - within) * GOD_BLOCK_SIZE - inner);

        if (chunkIndex >= files_.size())
            throw Error("GOD chunk index out of range");

        auto& f = files_[chunkIndex];
        f.seekg(physical);
        f.read(reinterpret_cast<char*>(out), take);
        if (static_cast<size_t>(f.gcount()) != take)
            throw Error("Truncated game image read");

        out += take;
        offset += take;
        size -= take;
    }
}

std::vector<Entry> GodImageReader::GetEntries()
{
    return ReadXdvdfsEntries(*this);
}

// ----------------------------------------------------------------------------
// StfsPackage
// ----------------------------------------------------------------------------
namespace
{
constexpr uint32_t STFS_BLOCK = 4096;
constexpr uint32_t STFS_FANOUT = 170;
constexpr uint32_t STFS_END = 0xffffff;
} // namespace

StfsPackage::StfsPackage(std::filesystem::path path, const Cancelled& cancelled)
    : path_(std::move(path)), cancelled_(cancelled)
{
    if (IsSymlinkOrReparse(path_))
        throw Error("Links are not supported as DLC sources");

    std::error_code ec;
    length_ = std::filesystem::file_size(path_, ec);
    if (ec) throw Error("Could not inspect DLC source: " + path_.string());

    stream_.open(path_, std::ios::binary);
    if (!stream_) throw Error("Could not open DLC source: " + path_.string());

    std::vector<uint8_t> headerPrefix(0x971a);
    ReadRaw(0, headerPrefix.data(), headerPrefix.size());

    static const uint8_t CON_MAGIC[4] = {'C', 'O', 'N', ' '};
    static const uint8_t LIVE_MAGIC[4] = {'L', 'I', 'V', 'E'};
    static const uint8_t PIRS_MAGIC[4] = {'P', 'I', 'R', 'S'};

    if (std::memcmp(headerPrefix.data(), CON_MAGIC, 4) != 0 &&
        std::memcmp(headerPrefix.data(), LIVE_MAGIC, 4) != 0 &&
        std::memcmp(headerPrefix.data(), PIRS_MAGIC, 4) != 0)
    {
        throw Error("Not an Xbox 360 STFS package (CON, LIVE or PIRS)");
    }

    uint32_t headerSize = ReadBeU32(headerPrefix.data() + 0x340);
    if (headerSize < 0x971a || headerSize > 1024 * 1024)
        throw Error("Invalid STFS header size");

    header_.resize(headerSize);
    ReadRaw(0, header_.data(), headerSize);

    base_ = (headerSize + STFS_BLOCK - 1) & ~(STFS_BLOCK - 1);

    char titleIdBuf[16]{};
    std::snprintf(titleIdBuf, sizeof(titleIdBuf), "%08X", ReadBeU32(header_.data() + 0x360));
    std::string titleId = titleIdBuf;
    if (titleId != "4D5307FA")
        throw Error("Wrong game: Title ID " + titleId + " (expected 4D5307FA)");

    uint32_t contentType = ReadBeU32(header_.data() + 0x344);
    if (contentType != 2)
        throw Error("This package is not Marketplace DLC (content type 2)");

    uint32_t volumeType = ReadBeU32(header_.data() + 0x3a9);
    if (volumeType != 0)
        throw Error("DLC import supports STFS volumes only, not SVOD");

    uint32_t volumeCount = ReadBeU32(header_.data() + 0x39d);
    if (volumeCount > 1)
        throw Error("Multi-file content volumes are not supported");

    const uint8_t* vol = header_.data() + 0x379;
    if (vol[0] != 0x24 || vol[1] != 0)
        throw Error("Unsupported STFS volume descriptor");

    copies_ = (vol[2] & 1) ? 1 : 2;
    rootCopy_ = (copies_ == 2 && (vol[2] & 2)) ? 1 : 0;
    totalBlocks_ = ReadBeU32(vol + 0x1c);
    tableCount_ = ReadLeU16(vol + 3);
    tableStart_ = static_cast<uint32_t>(vol[5]) | (static_cast<uint32_t>(vol[6]) << 8) | (static_cast<uint32_t>(vol[7]) << 16);

    if (totalBlocks_ == 0 || totalBlocks_ > STFS_FANOUT * STFS_FANOUT * STFS_FANOUT ||
        tableCount_ == 0 || tableCount_ > 1563)
    {
        throw Error("Invalid or excessive STFS block/table count");
    }

    if (DataOffset(totalBlocks_ - 1) + STFS_BLOCK > length_)
        throw Error("Truncated STFS data blocks");

    topLevel_ = (totalBlocks_ > STFS_FANOUT * STFS_FANOUT) ? 2 : ((totalBlocks_ > STFS_FANOUT) ? 1 : 0);

    // Content ID: 20 bytes at 0x32c
    std::string contentId = ToUpper(crypto::HexString(header_.data() + 0x32c, 20));
    if (contentId == std::string(40, '0'))
        throw Error("DLC content ID is empty");

    // Display Name: 9 slots of UTF-16 BE strings (256 bytes each) at 0x411
    std::string displayName;
    for (int i = 0; i < 9; ++i)
    {
        const uint8_t* namePtr = header_.data() + 0x411 + i * 256;
        std::string parsed;
        for (int c = 0; c < 256; c += 2)
        {
            uint16_t code = (static_cast<uint16_t>(namePtr[c]) << 8) | namePtr[c + 1];
            if (code == 0) break;
            if (code >= 32 && code < 127) parsed.push_back(static_cast<char>(code));
            else if (code >= 127) {
                // Convert basic UTF-16 code unit to UTF-8
                if (code < 0x800)
                {
                    parsed.push_back(static_cast<char>(0xC0 | (code >> 6)));
                    parsed.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                }
                else
                {
                    parsed.push_back(static_cast<char>(0xE0 | (code >> 12)));
                    parsed.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                    parsed.push_back(static_cast<char>(0x80 | (code & 0x3F)));
                }
            }
        }
        if (!parsed.empty())
        {
            displayName = parsed;
            break;
        }
    }
    if (displayName.empty()) displayName = contentId;

    // License mask: 16 entries of (uint64, uint32 bits, uint32 flags) at 0x22c
    uint32_t licenseMask = 0;
    for (int i = 0; i < 16; ++i)
    {
        uint32_t bits = ReadBeU32(header_.data() + 0x22c + 16 * i + 8);
        uint32_t flags = ReadBeU32(header_.data() + 0x22c + 16 * i + 12);
        if (flags != 0) licenseMask |= bits;
    }

    char formatBuf[5]{};
    std::memcpy(formatBuf, header_.data(), 4);
    std::string format = formatBuf;
    while (!format.empty() && format.back() == ' ') format.pop_back();

    info_.path = path_;
    info_.contentId = contentId;
    info_.displayName = displayName;
    info_.licenseMask = licenseMask;
    info_.format = format;

    ParseEntries();

    uint32_t fileCount = 0;
    uint64_t byteCount = 0;
    for (const auto& e : entries_)
    {
        if (!e.isDirectory)
        {
            fileCount++;
            byteCount += e.size;
        }
    }
    info_.files = fileCount;
    info_.bytes = byteCount;
}

void StfsPackage::ReadRaw(uint64_t offset, void* buffer, size_t size)
{
    if (cancelled_ && cancelled_())
        throw Error("Source check cancelled", true);

    if (offset > length_ || size > length_ - offset)
        throw Error("Truncated STFS package or out-of-range block");

    stream_.seekg(offset);
    stream_.read(reinterpret_cast<char*>(buffer), size);
    if (static_cast<size_t>(stream_.gcount()) != size)
        throw Error("STFS source changed or was truncated");
}

uint64_t StfsPackage::DataOffset(uint32_t number)
{
    if (number >= totalBlocks_)
        throw Error("STFS block is outside the volume");

    uint64_t overhead = 0;
    for (uint32_t span : {STFS_FANOUT, STFS_FANOUT * STFS_FANOUT, STFS_FANOUT * STFS_FANOUT * STFS_FANOUT})
    {
        overhead += (number / span + 1) * copies_;
        if (number < span) break;
    }
    return base_ + (static_cast<uint64_t>(number) + overhead) * STFS_BLOCK;
}

uint64_t StfsPackage::TableOffset(uint32_t number, int level)
{
    uint64_t firstSpan = STFS_FANOUT + copies_;
    uint64_t secondSpan = STFS_FANOUT * STFS_FANOUT + (STFS_FANOUT + 1) * copies_;
    uint64_t physical = 0;

    if (level == 2)
    {
        physical = secondSpan;
    }
    else if (level == 1)
    {
        physical = (number < STFS_FANOUT * STFS_FANOUT) ? firstSpan : (number / (STFS_FANOUT * STFS_FANOUT)) * secondSpan + copies_;
    }
    else if (number < STFS_FANOUT)
    {
        physical = 0;
    }
    else
    {
        physical = (number / STFS_FANOUT) * firstSpan + (number / (STFS_FANOUT * STFS_FANOUT) + 1) * copies_;
        if (number >= STFS_FANOUT * STFS_FANOUT)
            physical += copies_;
    }

    return base_ + physical * STFS_BLOCK;
}

const std::vector<uint8_t>& StfsPackage::GetTable(uint32_t number, int level)
{
    uint64_t offset = TableOffset(number, level);
    uint64_t key = (static_cast<uint64_t>(level) << 32) | (offset / STFS_BLOCK);

    auto it = tables_.find(key);
    if (it != tables_.end())
        return it->second;

    uint32_t active = 0;

    if (level == topLevel_)
    {
        active = rootCopy_;
    }
    else
    {
        const auto& parent = GetTable(number, level + 1);
        uint32_t span = (level == 0) ? STFS_FANOUT : (STFS_FANOUT * STFS_FANOUT);
        uint32_t index = (number / span) % STFS_FANOUT;
        const uint8_t* record = parent.data() + index * 24;
        active = (copies_ == 2 && (record[20] & 0x40)) ? 1 : 0;
    }

    std::vector<uint8_t> data(STFS_BLOCK);
    ReadRaw(offset + static_cast<uint64_t>(active) * STFS_BLOCK, data.data(), STFS_BLOCK);

    tables_[key] = std::move(data);
    return tables_[key];
}

void StfsPackage::ReadBlock(uint32_t number, void* outBuffer)
{
    DataOffset(number);
    const auto& table = GetTable(number, 0);
    uint32_t index = number % STFS_FANOUT;
    const uint8_t* record = table.data() + index * 24;

    if (!(record[20] & 0x80))
        throw Error("STFS chain references an unallocated block");

    ReadRaw(DataOffset(number), outBuffer, STFS_BLOCK);

}

std::vector<uint32_t> StfsPackage::ReadChain(uint32_t start, uint32_t count)
{
    if (count > totalBlocks_)
        throw Error("Invalid STFS chain length");

    std::vector<uint32_t> result;
    uint32_t number = start;

    for (uint32_t i = 0; i < count; ++i)
    {
        DataOffset(number);
        if (cancelled_ && cancelled_())
            throw Error("Source check cancelled", true);

        if (claimedBlocks_.count(number))
            throw Error("Cyclic or overlapping STFS block chain");

        claimedBlocks_.insert(number);
        result.push_back(number);

        const auto& table = GetTable(number, 0);
        uint32_t idx = number % STFS_FANOUT;
        const uint8_t* record = table.data() + idx * 24;

        if (!(record[20] & 0x80))
            throw Error("STFS chain references an unallocated block");

        number = (static_cast<uint32_t>(record[21]) << 16) |
                 (static_cast<uint32_t>(record[22]) << 8) |
                 static_cast<uint32_t>(record[23]);
    }

    if (count > 0 && number != STFS_END)
        throw Error("STFS chain exceeds declared file length");

    return result;
}

void StfsPackage::ParseEntries()
{
    auto dirBlocks = ReadChain(tableStart_, tableCount_);

    struct RawEntry
    {
        std::string name;
        bool isDirectory = false;
        uint16_t parentIndex = 0;
        uint32_t size = 0;
        uint32_t startBlock = 0;
        uint32_t validBlocks = 0;
        uint32_t allocatedBlocks = 0;
    };

    std::map<uint32_t, RawEntry> rawEntries;
    std::vector<uint8_t> blockBuf(STFS_BLOCK);

    for (size_t tableIndex = 0; tableIndex < dirBlocks.size(); ++tableIndex)
    {
        ReadBlock(dirBlocks[tableIndex], blockBuf.data());
        for (uint32_t slot = 0; slot < 64; ++slot)
        {
            const uint8_t* rec = blockBuf.data() + slot * 64;
            uint8_t nameLen = rec[40] & 63;
            if (rec[0] == 0 && nameLen == 0) continue;

            if (nameLen == 0 || nameLen > 40)
                throw Error("Invalid STFS file name length");

            std::string name(reinterpret_cast<const char*>(rec), nameLen);
            if (!ValidateComponentName(name))
                throw Error("Unsafe file name: '" + name + "'");

            if (ToLower(name).rfind(".lo-", 0) == 0)
                throw Error("DLC payload uses a reserved metadata name");

            RawEntry entry;
            entry.name = name;
            entry.isDirectory = (rec[40] & 128) != 0;
            entry.parentIndex = ReadBeU16(rec + 50);
            entry.size = ReadBeU32(rec + 52);
            entry.startBlock = static_cast<uint32_t>(rec[47]) |
                               (static_cast<uint32_t>(rec[48]) << 8) |
                               (static_cast<uint32_t>(rec[49]) << 16);
            entry.validBlocks = static_cast<uint32_t>(rec[41]) |
                                (static_cast<uint32_t>(rec[42]) << 8) |
                                (static_cast<uint32_t>(rec[43]) << 16);
            entry.allocatedBlocks = static_cast<uint32_t>(rec[44]) |
                                    (static_cast<uint32_t>(rec[45]) << 8) |
                                    (static_cast<uint32_t>(rec[46]) << 16);

            rawEntries[static_cast<uint32_t>(tableIndex * 64 + slot)] = entry;
        }
    }

    if (rawEntries.empty() || rawEntries.size() > 100000)
        throw Error("DLC package is empty or contains too many entries");

    std::map<uint32_t, std::string> fullPaths;
    auto getPath = [&](auto& self, uint32_t idx, std::vector<uint32_t> active) -> std::string {
        if (fullPaths.count(idx)) return fullPaths[idx];
        if (std::find(active.begin(), active.end(), idx) != active.end() || active.size() >= 64)
            throw Error("Cyclic or excessively deep STFS directory");

        const auto& item = rawEntries.at(idx);
        if (item.parentIndex == 0xffff)
        {
            fullPaths[idx] = item.name;
            return item.name;
        }

        if (!rawEntries.count(item.parentIndex) || !rawEntries.at(item.parentIndex).isDirectory)
            throw Error("STFS file has an invalid parent directory");

        active.push_back(idx);
        std::string res = self(self, item.parentIndex, active) + "/" + item.name;
        fullPaths[idx] = res;
        return res;
    };

    std::set<std::string> seenPaths;
    for (const auto& [idx, item] : rawEntries)
    {
        std::string p = getPath(getPath, idx, {});
        std::string lowerP = ToLower(p);
        if (seenPaths.count(lowerP))
            throw Error("Duplicate STFS file path: " + p);
        seenPaths.insert(lowerP);

        uint32_t expectedBlocks = static_cast<uint32_t>((uint64_t(item.size) + STFS_BLOCK - 1) / STFS_BLOCK);
        std::vector<uint32_t> blocks;

        if (item.isDirectory)
        {
            if (item.size != 0)
                throw Error("STFS directory has a nonzero file length");
        }
        else
        {
            if (item.validBlocks != expectedBlocks ||
                item.allocatedBlocks < expectedBlocks ||
                item.allocatedBlocks > totalBlocks_)
            {
                throw Error("STFS file length and block counts disagree");
            }

            if (item.allocatedBlocks > 0)
            {
                blocks = ReadChain(item.startBlock, item.allocatedBlocks);
            }
        }

        entries_.push_back({p, item.size, std::move(blocks), item.isDirectory});
    }

    bool hasFiles = false;
    for (const auto& e : entries_)
    {
        if (!e.isDirectory) { hasFiles = true; break; }
    }
    if (!hasFiles)
        throw Error("DLC package contains no files");
}

} // namespace install
