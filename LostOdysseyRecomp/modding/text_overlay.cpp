#include "text_overlay.h"
#include "cpx_pack.h"
#include "fpi_index.h"
#include "mod_api.h"
#include "model_import.h"
#include "rebuild_progress.h"
#include "static_mesh_placements.h"
#include "text_format.h"

#include <gpu/shader/cpx_decode.h>
#include <os/json.h>
#include <os/logger.h>
#include <os/user_paths.h>
#include <version.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <tuple>

namespace modding::text_overlay
{
// A file of the mod cache, opened on the first read.
struct CachedFile
{
    std::filesystem::path path;
    std::mutex mutex;
    std::ifstream stream;
};

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

struct Folder
{
    std::map<std::string, std::vector<Range>> ranges; // by lower-case file name
};

// Rebuilt files after an archive's real end: appended to a cache file, or kept
// in memory when there is no cache.
struct Tail
{
    uint64_t base = 0, size = 0; // base: the archive's size rounded up to a sector
    fs::path path;               // empty: in memory
    std::ofstream file;
    std::vector<uint8_t> memory;
    bool broken = false;         // a write failed: the file no longer matches size
    void Append(std::span<const uint8_t> bytes)
    {
        const auto padded = (bytes.size() + fpi::kSector - 1) / fpi::kSector * fpi::kSector;
        static const std::vector<uint8_t> zeros(fpi::kSector, 0);
        if (path.empty())
        {
            memory.insert(memory.end(), bytes.begin(), bytes.end());
            memory.resize(memory.size() + padded - bytes.size(), 0);
        }
        else if (broken || !file.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size())) ||
                 !file.write(reinterpret_cast<const char *>(zeros.data()), std::streamsize(padded - bytes.size())))
        {
            // Nothing more goes into this file: later members would land past what the index says.
            broken = true;
            throw std::runtime_error("cannot write " + Utf8(path));
        }
        size += padded;
    }
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

// Rebuilds the translated files and model packages one index points to, a
// member folder at a time: its files, then the packages that place its
// replaced static meshes (their baked lighting follows the new vertices),
// then they are stored and freed. A map's meshes are placed from its own
// folder; the few shared ones placed from elsewhere (kPlacedElsewhere) are
// rebuilt first and their vertex maps kept for the folders that place them.
// index: the file's first IndexSize() bytes, patched in place; tails: per
// archive file name.
void PatchIndex(const fs::path &folder, const std::map<std::string, fs::path> &names, std::vector<uint8_t> &index,
                std::map<std::string, Tail> &tails, const fs::path &cache, Counts &counts)
{
    const auto parsed = fpi::Read(index);
    // In archive order, so the reads run forward through each file (slow disks).
    auto byOffset = [](const fpi::File *a, const fpi::File *b) { return std::tie(a->archive, a->offset) < std::tie(b->archive, b->offset); };
    std::map<std::string, const fpi::File *> byPath;
    for (const auto &file : parsed.files) byPath.emplace(file.path, &file);

    // Per folder: members to rebuild, and packages that may place a replaced mesh.
    struct Group { std::vector<const fpi::File *> members, placers; bool shared = false; };
    std::map<std::string, Group> groups;
    for (const auto &file : parsed.files)
        if ((gTexts.count(file.path) && text::Detect(file.path) != text::Format::None) || gModels.count(file.path))
            groups[FolderOf(file.path)].members.push_back(&file);
    for (const auto &[meshes, placer] : model_import::kPlacedElsewhere)
    {
        const auto mesh = byPath.find(std::string(meshes)), user = byPath.find(std::string(placer));
        if (!gModels.count(std::string(meshes)) || mesh == byPath.end() || user == byPath.end()) continue;
        groups[FolderOf(mesh->first)].shared = true;
        groups[FolderOf(user->first)].placers.push_back(user->second);
    }
    std::vector<std::pair<const std::string, Group> *> order;
    size_t members = 0;
    for (auto &group : groups)
    {
        std::sort(group.second.members.begin(), group.second.members.end(), byOffset);
        order.push_back(&group);
        members += group.second.members.size();
    }
    rebuild_progress::AddTotal(uint32_t(members));
    std::stable_sort(order.begin(), order.end(), [&](const auto *a, const auto *b) {
        if (a->second.shared != b->second.shared) return a->second.shared;
        if (a->second.members.empty() || b->second.members.empty()) return !a->second.members.empty() && b->second.members.empty();
        return byOffset(a->second.members.front(), b->second.members.front());
    });

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
    std::mutex storing; // tails, index
    auto store = [&](const fpi::File &file, Rebuilt &item, Counts &counts) {
        const auto archive = names.find(parsed.archives.at(file.archive));
        const auto payload = item.member.packed ? cpx_pack::Pack(item.bytes, item.member.stored, fpi::kSector) : item.bytes;
        std::lock_guard lock(storing);
        auto &tail = tails[archive->first];
        if (!tail.base)
        {
            std::error_code ec;
            const auto size = fs::file_size(archive->second, ec);
            if (ec) throw std::runtime_error("cannot size " + Utf8(archive->second));
            tail.base = (size + fpi::kSector - 1) / fpi::kSector * fpi::kSector;
            if (!cache.empty())
            {
                tail.path = cache / (archive->first + ".tail");
                tail.file.open(tail.path, std::ios::binary | std::ios::trunc);
                if (!tail.file) throw std::runtime_error("cannot write " + Utf8(tail.path));
            }
        }
        const uint64_t offset = tail.base + tail.size;
        if (offset + payload.size() > kMaxOffset) throw std::runtime_error("archive would pass 4 GiB");
        tail.Append(payload);

        // +8: keep the disc and layer byte, point the sector at the copy.
        uint32_t sector = 0;
        for (int i = 0; i < 4; ++i) sector |= uint32_t(index[file.record + 8 + i]) << (8 * i);
        PutLe32(index, file.record + 8, (sector & 0xff000000u) | uint32_t(offset / fpi::kSector));
        PutLe32(index, file.record + 16, uint32_t(payload.size()));
        if (item.member.packed) PutLe32(index, file.record + 20, uint32_t(item.bytes.size()));
        ++counts.files;
        counts.texts += !item.model;
    };

    model_import::VertexMaps shared; // of the shared mesh packages, kept for the folders that place them
    std::mutex translating;          // gFiles
    auto work = [&](const std::pair<const std::string, Group> &entry, Counts &counts) {
        const auto &[memberFolder, group] = entry;
        // Packages that may place a replaced static mesh wait for the second
        // pass; every other rebuilt member is stored and freed at once, so a
        // folder of many packages does not stay in memory.
        std::map<const fpi::File *, Rebuilt> rebuilt;
        std::set<const fpi::File *> stored;
        auto put = [&](const fpi::File &file, Rebuilt &item) {
            stored.insert(&file);
            try { store(file, item, counts); }
            catch (const std::exception &error)
            {
                ++counts.failed;
                LOG_WARNING("[mods] {} {} kept original in {}: {}", item.model ? "model" : "text", file.path, Utf8(folder), error.what());
            }
        };
        model_import::VertexMaps maps = shared;
        const auto before = maps.size();
        for (const auto *member : group.members)
        {
            // Counted when done with; members that place static meshes once stored below.
            struct Count { bool later = false; ~Count() { if (!later) rebuild_progress::Advance(); } } count;
            const auto &file = *member;
            const auto text = gTexts.find(file.path);
            const bool model = gModels.count(file.path) != 0;
            try
            {
                const text::Replacements *translation = nullptr;
                if (!model)
                {
                    std::lock_guard lock(translating);
                    translation = Translation(text->second);
                }
                if (!model && !translation) continue;
                auto loaded = load(file);
                std::vector<uint8_t> bytes;
                if (model)
                {
                    model_import::VertexMaps own;
                    bytes = RebuildModels(file.path, loaded.bytes, counts, own);
                    if (group.shared) shared.insert(own.begin(), own.end());
                    maps.merge(own);
                }
                else
                {
                    size_t unknown = 0;
                    bytes = text::Rebuild(text::Detect(file.path), loaded.bytes, *translation, &unknown);
                    counts.unknownKeys += unknown;
                }
                if (bytes.empty() || bytes == loaded.bytes) continue;
                Rebuilt item{std::move(loaded), std::move(bytes), model};
                if (model && model_import::PlacesStaticMeshes(item.bytes))
                {
                    rebuilt[member] = std::move(item);
                    count.later = true;
                }
                else put(file, item);
            }
            catch (const std::exception &error)
            {
                ++counts.failed;
                LOG_WARNING("[mods] {} {} kept original in {}: {}", model ? "model" : "text", file.path, Utf8(folder), error.what());
            }
        }

        // Every package of the folder when its own meshes were replaced, and those placing shared ones.
        std::vector<const fpi::File *> placers = group.placers;
        if (maps.size() != before)
            for (const auto &file : parsed.files)
                if (FolderOf(file.path) == memberFolder && file.path.ends_with(".xxx")) placers.push_back(&file);
        std::sort(placers.begin(), placers.end(), byOffset);
        placers.erase(std::unique(placers.begin(), placers.end()), placers.end());
        if (maps.empty()) placers.clear();
        for (const auto *placer : placers)
        {
            const auto &file = *placer;
            if (stored.count(placer)) continue; // places no static mesh
            try
            {
                auto it = rebuilt.find(placer);
                Member loaded;
                if (it == rebuilt.end()) loaded = load(file);
                auto result = model_import::RemapLighting(it != rebuilt.end() ? it->second.bytes : loaded.bytes, StemOf(file.path), maps);
                for (const auto &note : result.notes) LOG_NOTICE("[mods] model lighting {}: {}", file.path, note);
                if (it == rebuilt.end())
                {
                    if (result.package.empty()) continue;
                    Rebuilt item{std::move(loaded), std::move(result.package), true};
                    put(file, item);
                    continue;
                }
                if (!result.package.empty()) it->second.bytes = std::move(result.package);
                put(file, it->second);
                rebuilt.erase(it);
                rebuild_progress::Advance();
            }
            catch (const std::exception &error)
            {
                ++counts.failed;
                LOG_WARNING("[mods] model lighting {} kept original in {}: {}", file.path, Utf8(folder), error.what());
            }
        }
        // Members that place static meshes but had none of theirs replaced.
        for (auto &[file, item] : rebuilt)
        {
            put(*file, item);
            rebuild_progress::Advance();
        }
    };

    // Shared mesh folders (sorted first) one by one, as the others need their
    // vertex maps; the rest on worker threads.
    size_t next = 0;
    for (; next < order.size() && order[next]->second.shared; ++next) work(*order[next], counts);
#ifdef __ANDROID__
    constexpr size_t kMaxThreads = 2; // each holds a few packages; phones share their memory with the game
#else
    constexpr size_t kMaxThreads = 8;
#endif
    const size_t threads = std::min<size_t>(std::clamp<size_t>(std::thread::hardware_concurrency(), 1, kMaxThreads), order.size() - next);
    std::atomic<size_t> claimed = next;
    std::vector<Counts> partial(threads);
    std::vector<std::thread> pool;
    for (size_t t = 0; t < threads; ++t)
        pool.emplace_back([&, t] {
            for (size_t i; (i = claimed++) < order.size();)
            {
                try { work(*order[i], partial[t]); }
                catch (const std::exception &error)
                {
                    ++partial[t].failed;
                    LOG_WARNING("[mods] {} kept original in {}: {}", order[i]->first, Utf8(folder), error.what());
                }
            }
        });
    for (auto &thread : pool) thread.join();
    for (const auto &c : partial)
    {
        counts.files += c.files; counts.texts += c.texts; counts.unknownKeys += c.unknownKeys;
        counts.failed += c.failed; counts.models += c.models;
    }
}

// ---- The cache ----------------------------------------------------------------
// cache/mods/<folder hash>/ holds a folder's patched indexes (<name>.index), its
// tails (<archive>.tail) and manifest.txt, written last: the key, then one line
// per index ("index <name>") and tail ("tail <archive> <base> <size>"). The key
// covers this build's version and cache format, the selected language pack, the
// mods' files (path, entry, size, time), and the folder's indexes and archive
// sizes; any change rebuilds.
constexpr const char *kCacheFormat = "2"; // bump when the rebuilt bytes change for the same inputs
uint64_t gModsKey = 0;

uint64_t Hash(uint64_t hash, std::string_view bytes)
{
    for (const unsigned char c : bytes) hash = (hash ^ c) * 0x100000001b3ull;
    return (hash ^ 0xff) * 0x100000001b3ull; // field separator
}
uint64_t Hash(uint64_t hash, uint64_t value) { return Hash(hash, std::string_view(reinterpret_cast<const char *>(&value), 8)); }

std::string Hex(uint64_t value)
{
    static const char digits[] = "0123456789abcdef";
    std::string out(16, '0');
    for (int i = 15; i >= 0; --i, value >>= 4) out[size_t(i)] = digits[value & 15];
    return out;
}

// Where the cache is; empty without one.
fs::path CacheRoot()
{
    if (const char *configured = std::getenv("LO_MODS_CACHE_DIR"))
        return std::string_view(configured) == "none" ? fs::path() : fs::path(configured);
    return os::user_paths::UsePortableLayout() ? fs::path("cache/mods") : os::user_paths::DataDir() / "cache/mods";
}

// The cache folder for a game folder; empty without a cache.
fs::path CacheFolder(const fs::path &folder)
{
    const auto root = CacheRoot();
    return root.empty() ? root : root / Hex(Hash(0xcbf29ce484222325ull, Utf8(fs::absolute(folder).lexically_normal())));
}

bool LoadCache(const fs::path &cache, uint64_t key, Folder &result)
{
    std::ifstream manifest(cache / "manifest.txt");
    std::string word, value;
    if (!(manifest >> word >> value) || word != "key" || value != Hex(key)) return false;
    Folder loaded;
    while (manifest >> word >> value)
    {
        std::error_code ec;
        if (word == "index")
        {
            const auto path = cache / (value + ".index");
            const auto size = fs::file_size(path, ec);
            if (ec) return false;
            auto bytes = std::make_shared<std::vector<uint8_t>>(ReadAt(path, 0, size_t(size)));
            loaded.ranges[value].push_back({0, size, std::move(bytes), nullptr});
        }
        else if (word == "tail")
        {
            uint64_t base = 0, size = 0;
            if (!(manifest >> base >> size)) return false;
            auto file = std::make_shared<CachedFile>();
            file->path = cache / (value + ".tail");
            if (fs::file_size(file->path, ec) != size || ec) return false;
            loaded.ranges[value].push_back({base, size, nullptr, std::move(file)});
        }
        else return false;
    }
    result = std::move(loaded);
    return true;
}

Folder Build(const fs::path &folder)
{
    Folder result;
    // File names are matched case-insensitively, as the game does.
    std::map<std::string, fs::path> names;
    std::error_code ec;
    for (const auto &item : fs::directory_iterator(folder, ec))
        names.emplace(Lower(Utf8(item.path().filename())), item.path());
    // The indexes and the archives' sizes are part of the key.
    std::map<std::string, std::vector<uint8_t>> indexes;
    uint64_t key = Hash(gModsKey, Utf8(fs::absolute(folder).lexically_normal()));
    for (const auto &[name, path] : names)
    {
        const bool index = name.ends_with(".fpi");
        if (!index && !name.ends_with(".fpd")) continue;
        key = Hash(Hash(key, name), uint64_t(fs::file_size(path, ec)));
        if (!index) continue;
        try
        {
            auto bytes = ReadAt(path, 0, 64);
            bytes = ReadAt(path, 0, size_t(fpi::IndexSize(bytes)));
            key = Hash(key, std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
            indexes.emplace(name, std::move(bytes));
        }
        catch (const std::exception &error)
        {
            LOG_WARNING("[mods] cannot read index {}: {}", Utf8(path), error.what());
        }
    }
    const auto cache = CacheFolder(folder);
    if (!cache.empty() && LoadCache(cache, key, result))
    {
        LOG_NOTICE("[mods] rebuilt files for {} read from {}", Utf8(folder), Utf8(cache));
        return result;
    }

    // PatchIndex counts the packages for the window until the files are in place.
    struct Finished { ~Finished() { rebuild_progress::Finish(); } } finished;

    // Build into <cache>.new, then put it in place of the old one.
    fs::path building = cache.empty() ? fs::path() : fs::path(cache).concat(".new");
    if (!building.empty())
    {
        fs::remove_all(building, ec);
        fs::create_directories(building, ec);
        if (ec)
        {
            LOG_WARNING("[mods] no cache for rebuilt files ({}): {}; keeping them in memory", Utf8(building), ec.message());
            building.clear();
        }
    }
    std::map<std::string, Tail> tails;
    std::vector<std::string> patched;
    Counts counts;
    for (auto &[name, index] : indexes)
    {
        try
        {
            const auto before = counts.files;
            PatchIndex(folder, names, index, tails, building, counts);
            if (counts.files != before) patched.push_back(name);
        }
        catch (const std::exception &error)
        {
            LOG_WARNING("[mods] cannot patch index {}: {}", Utf8(names.at(name)), error.what());
        }
    }
    for (auto &[name, tail] : tails) tail.file.close();
    if (counts.files || counts.failed)
        // Keys a copy lacks are normal: a pack made from the DLC copy of a file
        // also applies to the disc copy, which has fewer entries.
        LOG_NOTICE("[mods] {} translated and {} model packages rebuilt in {} ({} failed; {} translation keys this copy does not have)",
                   counts.texts, counts.files - counts.texts, Utf8(folder), counts.failed, counts.unknownKeys);

    bool cached = false;
    if (!building.empty())
    {
        try
        {
            std::ofstream manifest(building / "manifest.txt.part");
            manifest << "key " << Hex(key) << "\n";
            for (const auto &name : patched)
            {
                std::ofstream out(building / (name + ".index"), std::ios::binary | std::ios::trunc);
                const auto &bytes = indexes.at(name);
                if (!out.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size())))
                    throw std::runtime_error("cannot write the index");
                manifest << "index " << name << "\n";
            }
            for (const auto &[name, tail] : tails)
                if (tail.size) manifest << "tail " << name << " " << tail.base << " " << tail.size << "\n";
            manifest.close();
            if (!manifest) throw std::runtime_error("cannot write the manifest");
            fs::rename(building / "manifest.txt.part", building / "manifest.txt");
            fs::remove_all(cache);
            fs::rename(building, cache);
            cached = LoadCache(cache, key, result);
        }
        catch (const std::exception &error)
        {
            LOG_WARNING("[mods] cannot keep the rebuilt files in {}: {}", Utf8(cache), error.what());
        }
    }
    if (!cached)
    {
        // In memory: the cache could not be written, or there is none.
        result = {};
        for (const auto &name : patched)
        {
            auto bytes = std::make_shared<const std::vector<uint8_t>>(indexes.at(name));
            result.ranges[name].push_back({0, bytes->size(), bytes, nullptr});
        }
        for (auto &[name, tail] : tails)
        {
            if (!tail.size) continue;
            if (tail.path.empty())
            {
                auto bytes = std::make_shared<const std::vector<uint8_t>>(std::move(tail.memory));
                result.ranges[name].push_back({tail.base, bytes->size(), bytes, nullptr});
            }
            else
            {
                auto file = std::make_shared<CachedFile>();
                file->path = tail.path;
                result.ranges[name].push_back({tail.base, tail.size, nullptr, std::move(file)});
            }
        }
    }
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

    uint64_t key = Hash(Hash(0xcbf29ce484222325ull, lo_version::Source), kCacheFormat);
    key = Hash(key, gLanguagePack);
    // Sizes and times from one listing per folder: a stat per file takes
    // seconds for a pack of tens of thousands of files on Windows.
    using Stat = std::pair<uint64_t, int64_t>;
    std::map<fs::path, std::map<fs::path, Stat>> folders;
    auto stat = [&](const fs::path &path) {
        auto [it, added] = folders.try_emplace(path.parent_path());
        std::error_code ec;
        if (added)
            for (const auto &entry : fs::directory_iterator(path.parent_path(), ec))
                it->second[entry.path().filename()] = {uint64_t(entry.file_size(ec)),
                                                       int64_t(entry.last_write_time(ec).time_since_epoch().count())};
        const auto found = it->second.find(path.filename());
        return found == it->second.end() ? Stat{} : found->second;
    };
    auto add = [&](const ResolvedAsset &asset) {
        const auto [size, time] = stat(asset.path);
        key = Hash(Hash(Hash(key, asset.id.key), Utf8(asset.path)), asset.offset);
        key = Hash(Hash(Hash(key, asset.size), size), uint64_t(time));
    };
    for (const auto &[member, asset] : gTexts) add(asset);
    for (const auto &[member, assets] : gModels)
        for (const auto &asset : assets) add(asset);
    gModsKey = key;
}
} // namespace

std::vector<Range> RangesFor(const fs::path &file)
{
    const auto name = Lower(Utf8(file.filename()));
    const bool archive = name.size() > 4 && (name.compare(name.size() - 4, 4, ".fpd") == 0 || name.compare(name.size() - 4, 4, ".fpi") == 0);
    if (!archive) return {};
    std::lock_guard lock(gMutex);
    List();
    if (gTexts.empty() && gModels.empty())
    {
        // Without mods the rebuilt files of earlier starts are not needed (a
        // pack's can take gigabytes). Only the cache's own folders go.
        static bool cleared = false;
        if (const auto root = CacheRoot(); !cleared && !root.empty())
        {
            cleared = true;
            std::error_code ec;
            size_t removed = 0;
            for (const auto &entry : fs::directory_iterator(root, ec))
            {
                auto name = Utf8(entry.path().filename());
                if (name.ends_with(".new")) name.resize(name.size() - 4);
                if (name.size() == 16 && name.find_first_not_of("0123456789abcdef") == std::string::npos && entry.is_directory(ec))
                    removed += fs::remove_all(entry.path(), ec) > 0;
            }
            if (removed) LOG_NOTICE("[mods] no mods: removed {} folders of rebuilt files in {}", removed, Utf8(root));
        }
        return {};
    }
    const auto folder = file.parent_path().lexically_normal();
    auto it = gFolders.find(folder);
    if (it == gFolders.end()) it = gFolders.emplace(folder, Build(folder)).first;
    const auto ranges = it->second.ranges.find(name);
    return ranges == it->second.ranges.end() ? std::vector<Range>{} : ranges->second;
}

bool Range::Read(uint64_t at, uint8_t *out, size_t count) const
{
    if (at > size || count > size - at) return false;
    if (bytes)
    {
        std::memcpy(out, bytes->data() + at, count);
        return true;
    }
    if (!file) return false;
    std::lock_guard lock(file->mutex);
    if (!file->stream.is_open()) file->stream.open(file->path, std::ios::binary);
    file->stream.clear();
    if (!file->stream.seekg(std::streamoff(at)) || !file->stream.read(reinterpret_cast<char *>(out), std::streamsize(count)))
    {
        LOG_WARNING("[mods] cannot read {} at {}", Utf8(file->path), at);
        return false;
    }
    return true;
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
