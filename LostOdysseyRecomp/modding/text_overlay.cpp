#include "text_overlay.h"
#include "fpi_index.h"
#include "mod_api.h"
#include "text_format.h"

#include <gpu/shader/cpx_decode.h>
#include <os/json.h>
#include <os/logger.h>

#include <algorithm>
#include <fstream>
#include <map>
#include <mutex>

namespace modding::text_overlay
{
namespace
{
namespace fs = std::filesystem;
namespace cpx = xenos::resources::cpx;

// The game turns a record's sector into a 32-bit byte offset from its low 21 bits.
constexpr uint64_t kMaxOffset = (uint64_t(1) << 21) * fpi::kSector;

std::string Lower(std::string value)
{
    for (auto &c : value) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return value;
}
std::string Utf8(const fs::path &path)
{
    const auto value = path.u8string();
    return {value.begin(), value.end()};
}

std::vector<uint8_t> ReadAt(const fs::path &file, uint64_t offset, size_t size)
{
    std::ifstream input(file, std::ios::binary);
    std::vector<uint8_t> bytes(size);
    if (!input || !input.seekg(std::streamoff(offset)) || !input.read(reinterpret_cast<char *>(bytes.data()), std::streamsize(size)))
        throw std::runtime_error("cannot read " + Utf8(file));
    return bytes;
}

void PutLe32(std::vector<uint8_t> &out, size_t at, uint32_t value)
{
    for (int i = 0; i < 4; ++i) out[at + i] = uint8_t(value >> (8 * i));
}

// The decoded bytes in CPX stored blocks (FF 00, then the block size - 1 as
// u16 LE, then the bytes), as the discs store incompressible data. Header
// bytes 3-5 (a loader field and the bit width) are the original file's.
std::vector<uint8_t> StoreCpx(std::span<const uint8_t> payload, std::span<const uint8_t> original)
{
    const size_t blocks = (payload.size() + cpx::kBlockSize - 1) / cpx::kBlockSize;
    if (!blocks || blocks > 0xffff) throw std::runtime_error("text file too large for CPX");
    std::vector<uint8_t> out(16 + 4 * blocks, 0);
    std::copy_n("cpx", 3, out.begin());
    std::copy_n(original.begin() + 3, 3, out.begin() + 3);
    out[6] = uint8_t(blocks);
    out[7] = uint8_t(blocks >> 8);
    for (size_t i = 0; i < blocks; ++i)
    {
        const auto chunk = payload.subspan(i * cpx::kBlockSize, std::min(cpx::kBlockSize, payload.size() - i * cpx::kBlockSize));
        PutLe32(out, 16 + 4 * i, uint32_t(out.size()));
        out.insert(out.end(), {0xff, 0x00, uint8_t(chunk.size() - 1), uint8_t((chunk.size() - 1) >> 8)});
        out.insert(out.end(), chunk.begin(), chunk.end());
    }
    PutLe32(out, 8, uint32_t(out.size()));
    PutLe32(out, 12, uint32_t(payload.size()));
    return out;
}

struct Folder
{
    std::map<std::string, std::vector<Range>> ranges; // by lower-case file name
};

std::mutex gMutex;
bool gListed = false;
std::map<std::string, fs::path> gTexts;                     // member path -> translation file
std::map<fs::path, std::optional<text::Replacements>> gFiles; // parsed translations
std::map<fs::path, Folder> gFolders;

const text::Replacements *Translation(const fs::path &file)
{
    auto [it, added] = gFiles.try_emplace(file);
    if (added)
    {
        try
        {
            std::ifstream input(file, std::ios::binary);
            const auto json = nlohmann::json::parse(input);
            if (!json.is_object()) throw std::runtime_error("not a JSON object");
            text::Replacements replacements;
            for (const auto &[key, value] : json.items())
            {
                if (!value.is_string()) throw std::runtime_error("value of \"" + key + "\" is not a string");
                replacements.emplace(key, value.get<std::string>());
            }
            it->second = std::move(replacements);
        }
        catch (const std::exception &error)
        {
            LOG_WARNING("[mods] text {} skipped: {}", Utf8(file), error.what());
        }
    }
    return it->second ? &*it->second : nullptr;
}

struct Counts
{
    size_t files = 0, unknownKeys = 0, failed = 0;
};

// Rebuilds the translated files one index points to. index: the file's first
// IndexSize() bytes, patched in place; tails: per archive file name.
void PatchIndex(const fs::path &folder, const std::map<std::string, fs::path> &names, std::vector<uint8_t> &index,
                std::map<std::string, std::pair<uint64_t, std::vector<uint8_t>>> &tails, Counts &counts)
{
    const auto parsed = fpi::Read(index);
    for (const auto &file : parsed.files)
    {
        const auto text = gTexts.find(file.path);
        const auto format = text::Detect(file.path);
        if (text == gTexts.end() || format == text::Format::None) continue;
        try
        {
            const auto archive = names.find(parsed.archives.at(file.archive));
            if (archive == names.end()) throw std::runtime_error("archive " + parsed.archives[file.archive] + " missing");
            const auto *translation = Translation(text->second);
            if (!translation) continue;
            const auto stored = ReadAt(archive->second, file.offset, file.size);
            const bool packed = stored.size() >= 3 && std::equal(stored.begin(), stored.begin() + 3, "cpx");
            std::vector<uint8_t> member;
            if (packed && !cpx::Decode(stored, member)) throw std::runtime_error("invalid CPX data");
            const auto &original = packed ? member : stored;
            size_t unknown = 0;
            const auto rebuilt = text::Rebuild(format, original, *translation, &unknown);
            counts.unknownKeys += unknown;
            if (rebuilt == original) continue;
            const auto payload = packed ? StoreCpx(rebuilt, stored) : rebuilt;

            auto &[base, tail] = tails.try_emplace(archive->first, 0, std::vector<uint8_t>{}).first->second;
            if (!base)
            {
                std::error_code ec;
                const auto size = fs::file_size(archive->second, ec);
                if (ec) throw std::runtime_error("cannot size " + Utf8(archive->second));
                base = (size + fpi::kSector - 1) / fpi::kSector * fpi::kSector;
            }
            const uint64_t offset = base + tail.size();
            if (offset + payload.size() > kMaxOffset) throw std::runtime_error("archive would pass 4 GiB");
            tail.insert(tail.end(), payload.begin(), payload.end());
            tail.resize((tail.size() + fpi::kSector - 1) / fpi::kSector * fpi::kSector, 0);

            // +8: keep the disc and layer byte, point the sector at the copy.
            uint32_t sector = 0;
            for (int i = 0; i < 4; ++i) sector |= uint32_t(index[file.record + 8 + i]) << (8 * i);
            PutLe32(index, file.record + 8, (sector & 0xff000000u) | uint32_t(offset / fpi::kSector));
            PutLe32(index, file.record + 16, uint32_t(payload.size()));
            if (packed) PutLe32(index, file.record + 20, uint32_t(rebuilt.size()));
            ++counts.files;
        }
        catch (const std::exception &error)
        {
            ++counts.failed;
            LOG_WARNING("[mods] text {} kept original in {}: {}", file.path, Utf8(folder), error.what());
        }
    }
}

Folder Build(const fs::path &folder)
{
    Folder result;
    // File names are matched case-insensitively, as the game does.
    std::map<std::string, fs::path> names;
    std::error_code ec;
    for (const auto &item : fs::directory_iterator(folder, ec))
        names.emplace(Lower(Utf8(item.path().filename())), item.path());
    std::map<std::string, std::pair<uint64_t, std::vector<uint8_t>>> tails;
    Counts counts;
    for (const auto &[name, path] : names)
    {
        if (name.size() < 4 || name.compare(name.size() - 4, 4, ".fpi") != 0) continue;
        try
        {
            auto index = ReadAt(path, 0, 64);
            index = ReadAt(path, 0, size_t(fpi::IndexSize(index)));
            const auto before = counts.files;
            PatchIndex(folder, names, index, tails, counts);
            if (counts.files != before)
                result.ranges[name].push_back({0, std::make_shared<const std::vector<uint8_t>>(std::move(index))});
        }
        catch (const std::exception &error)
        {
            LOG_WARNING("[mods] text: cannot read index {}: {}", Utf8(path), error.what());
        }
    }
    for (auto &[name, tail] : tails)
        if (!tail.second.empty())
            result.ranges[name].push_back({tail.first, std::make_shared<const std::vector<uint8_t>>(std::move(tail.second))});
    if (counts.files || counts.failed)
        LOG_NOTICE("[mods] text: {} translated files in {} ({} failed, {} keys not in their file)",
                   counts.files, Utf8(folder), counts.failed, counts.unknownKeys);
    return result;
}

// Caller holds gMutex.
void List()
{
    if (gListed) return;
    gListed = true;
    for (auto &asset : ListTexts()) gTexts.emplace(asset.id.key, std::move(asset.path));
    if (!gTexts.empty()) LOG_NOTICE("[mods] text: {} translation files", gTexts.size());
}
} // namespace

std::vector<Range> RangesFor(const fs::path &file)
{
    const auto name = Lower(Utf8(file.filename()));
    const bool archive = name.size() > 4 && (name.compare(name.size() - 4, 4, ".fpd") == 0 || name.compare(name.size() - 4, 4, ".fpi") == 0);
    if (!archive) return {};
    std::lock_guard lock(gMutex);
    List();
    if (gTexts.empty()) return {};
    const auto folder = file.parent_path().lexically_normal();
    auto it = gFolders.find(folder);
    if (it == gFolders.end()) it = gFolders.emplace(folder, Build(folder)).first;
    const auto ranges = it->second.ranges.find(name);
    return ranges == it->second.ranges.end() ? std::vector<Range>{} : ranges->second;
}

bool Translates(const std::string &memberPath, const std::string &key)
{
    std::lock_guard lock(gMutex);
    List();
    const auto text = gTexts.find(memberPath);
    if (text == gTexts.end()) return false;
    const auto *translation = Translation(text->second);
    return translation && translation->count(key);
}
}
