#include "text_overlay.h"
#include "fpi_index.h"
#include "mod_api.h"
#include "model_import.h"
#include "text_format.h"

#include <gpu/shader/cpx_decode.h>
#include <os/json.h>
#include <os/logger.h>

#include <algorithm>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <tuple>

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
// bytes 4-5 (bit width) are the original file's. Byte 3 is the loader's
// reserve in 2048-byte units: it decodes in place with the stream placed at
// buffer + decoded + reserve - stored (sub_8284E6B8, sub_828571B8), and a
// stored stream is 16 + 8 per block bytes longer than its output, so the
// reserve has to cover that or block output overwrites the next block header.
std::vector<uint8_t> StoreCpx(std::span<const uint8_t> payload, std::span<const uint8_t> original)
{
    const size_t blocks = (payload.size() + cpx::kBlockSize - 1) / cpx::kBlockSize;
    if (!blocks || blocks > 0xffff) throw std::runtime_error("file too large for CPX");
    std::vector<uint8_t> out(16 + 4 * blocks, 0);
    std::copy_n("cpx", 3, out.begin());
    std::copy_n(original.begin() + 3, 3, out.begin() + 3);
    const size_t reserve = (16 + 8 * blocks + fpi::kSector - 1) / fpi::kSector;
    if (reserve > 0xff) throw std::runtime_error("file too large for CPX");
    out[3] = std::max(out[3], uint8_t(reserve));
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
std::string gLanguagePack;
std::map<std::string, ResolvedAsset> gTexts; // member path -> translation file or archive entry
std::map<std::string, std::vector<ResolvedAsset>> gModels; // package member path -> its model replacements
std::map<std::pair<fs::path, uint64_t>, std::optional<text::Replacements>> gFiles; // parsed, by file and offset
std::map<fs::path, Folder> gFolders;

const text::Replacements *Translation(const ResolvedAsset &file)
{
    auto [it, added] = gFiles.try_emplace({file.path, file.offset});
    if (added)
    {
        try
        {
            std::ifstream input;
            uint64_t length = 0;
            if (!OpenAsset(file, input, length)) throw std::runtime_error("cannot open");
            if (length > 64 * 1024 * 1024) throw std::runtime_error("larger than 64 MiB");
            std::string bytes(size_t(length), '\0');
            if (!input.read(bytes.data(), std::streamsize(bytes.size()))) throw std::runtime_error("truncated");
            const auto json = nlohmann::json::parse(bytes);
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
            LOG_WARNING("[mods] text {} skipped: {}", DescribeAsset(file), error.what());
        }
    }
    return it->second ? &*it->second : nullptr;
}

struct Counts
{
    size_t files = 0, texts = 0, unknownKeys = 0, failed = 0, models = 0;
};

// A member path's folder and file name without extension.
std::string FolderOf(const std::string &path) { return path.substr(0, path.rfind('/') + 1); }
std::string StemOf(const std::string &path)
{
    const auto name = path.substr(path.rfind('/') + 1);
    return name.substr(0, name.rfind('.'));
}

// The package with its model replacements, or empty to keep it. Replaced static
// meshes add their vertex maps to maps.
std::vector<uint8_t> RebuildModels(const std::string &path, const std::vector<uint8_t> &package, Counts &counts,
                                   model_import::VertexMaps &maps)
{
    std::vector<model_import::Replacement> replacements;
    for (const auto &asset : gModels.at(path))
    {
        // Key: <package>#<export index>:<object>.
        const auto hash = asset.id.key.rfind('#'), colon = asset.id.key.find(':', hash);
        model_import::Replacement r;
        r.exportIndex = uint32_t(std::stoul(asset.id.key.substr(hash + 1, colon - hash - 1)));
        r.object = asset.id.key.substr(colon + 1);
        r.source = DescribeAsset(asset);
        std::ifstream input;
        uint64_t length = 0;
        if (!OpenAsset(asset, input, length) || length > 256 * 1024 * 1024)
        {
            LOG_WARNING("[mods] model {} skipped: cannot read it or larger than 256 MiB", r.source);
            continue;
        }
        r.glb.resize(size_t(length));
        if (!input.read(reinterpret_cast<char *>(r.glb.data()), std::streamsize(r.glb.size())))
        {
            LOG_WARNING("[mods] model {} skipped: truncated", r.source);
            continue;
        }
        replacements.push_back(std::move(r));
    }
    auto result = model_import::RebuildPackage(package, StemOf(path), replacements);
    for (const auto &note : result.notes)
        if (result.replaced) LOG_NOTICE("[mods] model {}: {}", path, note);
        else LOG_WARNING("[mods] model {}: {}", path, note);
    counts.models += result.replaced;
    maps.merge(result.vertexMaps);
    return std::move(result.package);
}

// Rebuilds the translated files and model packages one index points to, then
// the packages beside a replaced static mesh's that place it (their baked
// lighting follows its new vertices). index: the file's first IndexSize()
// bytes, patched in place; tails: per archive file name.
void PatchIndex(const fs::path &folder, const std::map<std::string, fs::path> &names, std::vector<uint8_t> &index,
                std::map<std::string, std::pair<uint64_t, std::vector<uint8_t>>> &tails, Counts &counts)
{
    const auto parsed = fpi::Read(index);
    // In archive order, so the reads run forward through each file (slow disks).
    auto byOffset = [](const fpi::File *a, const fpi::File *b) { return std::tie(a->archive, a->offset) < std::tie(b->archive, b->offset); };
    std::vector<const fpi::File *> files;
    for (const auto &file : parsed.files)
        if ((gTexts.count(file.path) && text::Detect(file.path) != text::Format::None) || gModels.count(file.path))
            files.push_back(&file);
    std::sort(files.begin(), files.end(), byOffset);

    struct Member { std::vector<uint8_t> stored, bytes; bool packed = false; };
    auto load = [&](const fpi::File &file) {
        const auto archive = names.find(parsed.archives.at(file.archive));
        if (archive == names.end()) throw std::runtime_error("archive " + parsed.archives[file.archive] + " missing");
        Member member;
        member.stored = ReadAt(archive->second, file.offset, file.size);
        member.packed = member.stored.size() >= 3 && std::equal(member.stored.begin(), member.stored.begin() + 3, "cpx");
        if (member.packed && !cpx::Decode(member.stored, member.bytes)) throw std::runtime_error("invalid CPX data");
        if (!member.packed) member.bytes = member.stored;
        return member;
    };
    struct Rebuilt { Member member; std::vector<uint8_t> bytes; bool model = false; };
    std::map<const fpi::File *, Rebuilt> rebuilt;
    model_import::VertexMaps maps;
    std::set<std::string> relight; // folders of packages whose static meshes were replaced
    for (const auto *entry : files)
    {
        const auto &file = *entry;
        const auto text = gTexts.find(file.path);
        const bool model = gModels.count(file.path) != 0;
        const auto format = text::Detect(file.path);
        try
        {
            const auto *translation = model ? nullptr : Translation(text->second);
            if (!model && !translation) continue;
            auto member = load(file);
            std::vector<uint8_t> bytes;
            if (model)
            {
                const auto before = maps.size();
                bytes = RebuildModels(file.path, member.bytes, counts, maps);
                if (maps.size() != before) relight.insert(FolderOf(file.path));
            }
            else
            {
                size_t unknown = 0;
                bytes = text::Rebuild(format, member.bytes, *translation, &unknown);
                counts.unknownKeys += unknown;
            }
            if (bytes.empty() || bytes == member.bytes) continue;
            rebuilt[&file] = {std::move(member), std::move(bytes), model};
        }
        catch (const std::exception &error)
        {
            ++counts.failed;
            LOG_WARNING("[mods] {} {} kept original in {}: {}", model ? "model" : "text", file.path, Utf8(folder), error.what());
        }
    }
    for (const auto &file : parsed.files)
    {
        if (!relight.count(FolderOf(file.path)) || !file.path.ends_with(".xxx")) continue;
        try
        {
            const auto it = rebuilt.find(&file);
            Member member;
            if (it == rebuilt.end()) member = load(file);
            auto result = model_import::RemapLighting(it != rebuilt.end() ? it->second.bytes : member.bytes, StemOf(file.path), maps);
            for (const auto &note : result.notes) LOG_NOTICE("[mods] model lighting {}: {}", file.path, note);
            if (result.package.empty()) continue;
            if (it != rebuilt.end()) it->second.bytes = std::move(result.package);
            else rebuilt[&file] = {std::move(member), std::move(result.package), true};
        }
        catch (const std::exception &error)
        {
            ++counts.failed;
            LOG_WARNING("[mods] model lighting {} kept original in {}: {}", file.path, Utf8(folder), error.what());
        }
    }

    std::vector<const fpi::File *> order;
    for (const auto &[file, item] : rebuilt) order.push_back(file);
    std::sort(order.begin(), order.end(), byOffset);
    for (const auto *entry : order)
    {
        const auto &file = *entry;
        auto &item = rebuilt.at(entry);
        try
        {
            const auto archive = names.find(parsed.archives.at(file.archive));
            const auto payload = item.member.packed ? StoreCpx(item.bytes, item.member.stored) : item.bytes;

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
            if (item.member.packed) PutLe32(index, file.record + 20, uint32_t(item.bytes.size()));
            ++counts.files;
            counts.texts += !item.model;
        }
        catch (const std::exception &error)
        {
            ++counts.failed;
            LOG_WARNING("[mods] {} {} kept original in {}: {}", item.model ? "model" : "text", file.path, Utf8(folder), error.what());
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
        // Keys a copy lacks are normal: a pack made from the DLC copy of a file
        // also applies to the disc copy, which has fewer entries.
        LOG_NOTICE("[mods] {} translated and {} model packages rebuilt in {} ({} failed; {} translation keys this copy does not have)",
                   counts.texts, counts.files - counts.texts, Utf8(folder), counts.failed, counts.unknownKeys);
    return result;
}

// Caller holds gMutex.
void List()
{
    if (gListed) return;
    gListed = true;
    for (auto &asset : ListTexts()) gTexts.emplace(asset.id.key, std::move(asset));
    size_t packFiles = 0;
    for (const auto &pack : LanguagePacks())
        if (pack.id == gLanguagePack)
            for (auto &asset : ListTexts(pack))
            {
                gTexts.insert_or_assign(asset.id.key, std::move(asset));
                ++packFiles;
            }
    if (!gTexts.empty())
        LOG_NOTICE("[mods] text: {} translation files ({} from language pack {})", gTexts.size(), packFiles,
                   gLanguagePack.empty() ? "none" : gLanguagePack);
    size_t models = 0;
    for (auto &asset : ListModels())
    {
        const auto package = asset.id.key.substr(0, asset.id.key.rfind('#'));
        gModels[package].push_back(std::move(asset));
        ++models;
    }
    if (models) LOG_NOTICE("[mods] model: {} replacements in {} packages", models, gModels.size());
}
} // namespace

std::vector<Range> RangesFor(const fs::path &file)
{
    const auto name = Lower(Utf8(file.filename()));
    const bool archive = name.size() > 4 && (name.compare(name.size() - 4, 4, ".fpd") == 0 || name.compare(name.size() - 4, 4, ".fpi") == 0);
    if (!archive) return {};
    std::lock_guard lock(gMutex);
    List();
    if (gTexts.empty() && gModels.empty()) return {};
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

void SelectLanguagePack(const std::string &id)
{
    std::lock_guard lock(gMutex);
    gLanguagePack = id;
}
}
