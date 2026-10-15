#include "text_overlay.h"
#include "cpx_pack.h"
#include "font_patch.h"
#include "fpi_index.h"
#include "fpi_overflow.h"
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
#if __has_include(<lo_mods_builder.h>)
#include <lo_mods_builder.h>
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <tuple>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <pthread.h>
#elif defined(__linux__)
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

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
    std::map<std::string, fs::path> overflow;         // overflow archives (fpi_overflow.h) by lower-case name
    std::shared_ptr<void> cachePin;                   // keeps this set while active or prepared for reload
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

// The mods the rebuilt files are made for. Fixed once listed; a live reload
// replaces it whole, so a build on another thread keeps the set it started with.
struct ModSet
{
    std::map<std::string, ResolvedAsset> texts;               // member path -> translation file or archive entry
    std::map<std::string, std::vector<ResolvedAsset>> models; // package member path -> its model replacements
    std::map<std::string, std::vector<ResolvedAsset>> fonts;  // package member path -> glyphs the selected language pack adds
    uint64_t key = 0;                                          // what the rebuilt bytes depend on (Build adds the folder)
    bool Empty() const { return texts.empty() && models.empty() && fonts.empty(); }
};

std::mutex gMutex;
std::mutex gFilesMutex; // gFiles: a foreground and a background build may both translate
std::mutex gCachePinsMutex;
std::map<fs::path, std::weak_ptr<void>> gCachePins;
std::string gLanguagePack;
std::shared_ptr<const ModSet> gSet; // null until listed
struct ParsedTranslation
{
    std::tuple<uint64_t, uint64_t, fs::file_time_type> version;
    std::shared_ptr<const text::Replacements> replacements;
};
std::map<std::pair<fs::path, uint64_t>, ParsedTranslation> gFiles; // one version per path and archive offset
std::map<fs::path, Folder> gFolders; // made for gSet

std::shared_ptr<void> PinCache(const fs::path &path)
{
    std::lock_guard lock(gCachePinsMutex);
    auto &pin = gCachePins[path];
    if (auto held = pin.lock()) return held;
    auto held = std::make_shared<int>(0);
    pin = held;
    return held;
}

std::shared_ptr<const text::Replacements> Translation(const ResolvedAsset &file)
{
    std::error_code ec;
    const auto size = fs::file_size(file.path, ec);
    const auto diskSize = ec ? uint64_t(0) : size;
    ec.clear();
    const auto modified = fs::last_write_time(file.path, ec);
    const auto version = std::tuple{file.size, diskSize, ec ? fs::file_time_type{} : modified};
    auto [it, added] = gFiles.try_emplace({file.path, file.offset});
    if (added || it->second.version != version)
    {
        it->second.version = version;
        it->second.replacements.reset();
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
            it->second.replacements = std::make_shared<text::Replacements>(std::move(replacements));
        }
        catch (const std::exception &error)
        {
            LOG_WARNING("[mods] text {} skipped: {}", DescribeAsset(file), error.what());
        }
    }
    return it->second.replacements;
}

struct Counts
{
    size_t files = 0, texts = 0, unknownKeys = 0, failed = 0, models = 0, fonts = 0;
};

// Thrown when a background build is stopped; not a std::exception, so the
// per-file and per-index handlers let it through.
struct Cancelled {};

struct BuildOptions
{
    std::atomic<uint64_t> *progress = &rebuild_progress::g_progress;
    size_t threads = 0;                        // 0: up to kMaxThreads
    bool background = false;                   // workers at low priority
    const std::atomic<bool> *cancel = nullptr; // stops the build between files
    bool Cancelled() const { return cancel && cancel->load(std::memory_order_relaxed); }
};

// A background build rests after each file as long as the file took: half the
// time idle keeps it from taking the power a handheld's GPU shares with the CPU.
struct Rest
{
    const BuildOptions &options;
    std::chrono::steady_clock::time_point began = std::chrono::steady_clock::now();
    ~Rest()
    {
        if (!options.background) return;
        const auto until = std::chrono::steady_clock::now() + std::min<std::chrono::steady_clock::duration>(
                                                                  std::chrono::steady_clock::now() - began, std::chrono::seconds(5));
        while (!options.Cancelled() && std::chrono::steady_clock::now() < until)
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
};

// CPU (and on Windows I/O) priority below the game's threads.
void LowerThreadPriority()
{
#ifdef _WIN32
    SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
#elif defined(__APPLE__)
    pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
#elif defined(__linux__)
    setpriority(PRIO_PROCESS, id_t(syscall(SYS_gettid)), 19);
#endif
}

// A member path's folder and file name without extension.
std::string FolderOf(const std::string &path) { return path.substr(0, path.rfind('/') + 1); }
std::string StemOf(const std::string &path)
{
    const auto name = path.substr(path.rfind('/') + 1);
    return name.substr(0, name.rfind('.'));
}

// The package with its model replacements, or empty to keep it. Replaced static
// meshes add their vertex maps to maps.
std::vector<uint8_t> RebuildModels(const ModSet &set, const std::string &path, const std::vector<uint8_t> &package, Counts &counts,
                                   model_import::VertexMaps &maps, const std::atomic<bool> *stop)
{
    std::vector<model_import::Replacement> replacements;
    for (const auto &asset : set.models.at(path))
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
    auto result = model_import::RebuildPackage(package, StemOf(path), replacements, stop);
    for (const auto &note : result.notes)
        if (result.replaced) LOG_NOTICE("[mods] model {}: {}", path, note);
        else LOG_WARNING("[mods] model {}: {}", path, note);
    counts.models += result.replaced;
    maps.merge(result.vertexMaps);
    return std::move(result.package);
}

// The package with the language pack's glyphs added to its fonts, or empty to keep it.
std::vector<uint8_t> RebuildFonts(const ModSet &set, const std::string &path, const std::vector<uint8_t> &package, Counts &counts)
{
    std::vector<font_patch::Addition> additions;
    for (const auto &asset : set.fonts.at(path))
    {
        // Key: <package>#<font>.
        font_patch::Addition addition{asset.id.key.substr(asset.id.key.rfind('#') + 1), {}, DescribeAsset(asset)};
        try
        {
            std::ifstream input;
            uint64_t length = 0;
            if (!OpenAsset(asset, input, length) || length > 128 * 1024 * 1024) throw std::runtime_error("cannot read it or larger than 128 MiB");
            std::vector<uint8_t> bytes(static_cast<size_t>(length));
            if (!input.read(reinterpret_cast<char *>(bytes.data()), std::streamsize(bytes.size()))) throw std::runtime_error("truncated");
            addition.glyphs = font_patch::Parse(bytes);
        }
        catch (const std::exception &error)
        {
            LOG_WARNING("[mods] font {} skipped: {}", addition.source, error.what());
            continue;
        }
        additions.push_back(std::move(addition));
    }
    if (additions.empty()) return {};
    auto result = font_patch::RebuildPackage(package, additions);
    for (const auto &note : result.notes)
        if (result.patched) LOG_NOTICE("[mods] font {}: {}", path, note);
        else LOG_WARNING("[mods] font {}: {}", path, note);
    counts.fonts += result.patched;
    return std::move(result.package);
}

// Rebuilds the translated files and model packages one index points to, a
// member folder at a time: its files, then the packages that place its
// replaced static meshes (their baked lighting follows the new vertices),
// then they are stored and freed. A map's meshes are placed from its own
// folder; the few shared ones placed from elsewhere (kPlacedElsewhere) are
// rebuilt first and their vertex maps kept for the folders that place them.
// Files that would end past 4 GiB of their archive go into overflow archives
// (fpi_overflow.h) in the cache.
// index: the file's first IndexSize() bytes, patched in place; tails: per
// archive file name; overflows: the folder's overflow archives by name.
void PatchIndex(const ModSet &set, const fs::path &folder, const std::map<std::string, fs::path> &names, std::vector<uint8_t> &index,
                std::map<std::string, Tail> &tails, std::map<std::string, Tail> &overflows, const fs::path &cache, Counts &counts,
                const BuildOptions &options)
{
    const auto &texts = set.texts;
    const auto &models = set.models;
    const auto &fonts = set.fonts;
    const auto parsed = fpi::Read(index);
    // In archive order, so the reads run forward through each file (slow disks).
    auto byOffset = [](const fpi::File *a, const fpi::File *b) { return std::tie(a->archive, a->offset) < std::tie(b->archive, b->offset); };
    std::map<std::string, const fpi::File *> byPath;
    for (const auto &file : parsed.files) byPath.emplace(file.path, &file);

    // Per folder: members to rebuild, and packages that may place a replaced mesh.
    struct Group { std::vector<const fpi::File *> members, placers; bool shared = false; };
    std::map<std::string, Group> groups;
    for (const auto &file : parsed.files)
        if ((texts.count(file.path) && text::Detect(file.path) != text::Format::None) || models.count(file.path) || fonts.count(file.path))
            groups[FolderOf(file.path)].members.push_back(&file);
    for (const auto &[meshes, placer] : model_import::kPlacedElsewhere)
    {
        const auto mesh = byPath.find(std::string(meshes)), user = byPath.find(std::string(placer));
        if (!models.count(std::string(meshes)) || mesh == byPath.end() || user == byPath.end()) continue;
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
    rebuild_progress::AddTotal(*options.progress, uint32_t(members));
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
    std::mutex storing; // tails, overflows, index
    std::vector<std::string> overflowNames;       // this index's overflow archives
    std::vector<fpi::OverflowFile> overflowFiles; // and the files in them
    std::vector<bool> overflowTexts;
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
        if (offset + payload.size() > kMaxOffset)
        {
            if (cache.empty()) throw std::runtime_error("archive would pass 4 GiB (overflow archives need the mod cache)");
            if (overflowNames.empty() || overflows.at(overflowNames.back()).size + payload.size() > kMaxOffset)
            {
                std::string name;
                for (size_t n = overflows.size(); name.empty() || names.count(name) || overflows.count(name); ++n)
                    name = "xenon_mod" + std::to_string(n) + ".fpd";
                auto &added = overflows[name];
                added.path = cache / name;
                added.file.open(added.path, std::ios::binary | std::ios::trunc);
                if (!added.file) throw std::runtime_error("cannot write " + Utf8(added.path));
                overflowNames.push_back(name);
            }
            auto &overflow = overflows.at(overflowNames.back());
            const uint64_t at = overflow.size;
            overflow.Append(payload);
            overflowFiles.push_back({file.record, file.archive, file.path, uint32_t(overflowNames.size() - 1), at,
                                     uint32_t(payload.size()), uint32_t(item.bytes.size()), item.member.packed});
            overflowTexts.push_back(!item.model);
            ++counts.files;
            counts.texts += !item.model;
            return;
        }
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
            if (options.Cancelled()) return;
            const Rest rest{options};
            // Counted when done with; members that place static meshes once stored below.
            struct Count
            {
                std::atomic<uint64_t> *progress;
                bool later = false;
                ~Count() { if (!later) rebuild_progress::Advance(*progress); }
            } count{options.progress};
            const auto &file = *member;
            const auto text = texts.find(file.path);
            const bool font = fonts.count(file.path) != 0;
            // Packages: model replacements, glyphs or both; anything else is text.
            const bool model = models.count(file.path) != 0 || font;
            try
            {
                std::shared_ptr<const text::Replacements> translation;
                if (!model)
                {
                    std::lock_guard lock(gFilesMutex);
                    translation = Translation(text->second);
                }
                if (!model && !translation) continue;
                auto loaded = load(file);
                std::vector<uint8_t> bytes;
                if (model)
                {
                    if (models.count(file.path))
                    {
                        model_import::VertexMaps own;
                        bytes = RebuildModels(set, file.path, loaded.bytes, counts, own, options.cancel);
                        if (group.shared) shared.insert(own.begin(), own.end());
                        maps.merge(own);
                    }
                    if (font)
                        if (auto patched = RebuildFonts(set, file.path, bytes.empty() ? loaded.bytes : bytes, counts); !patched.empty())
                            bytes = std::move(patched);
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
            if (options.Cancelled()) return;
            const Rest rest{options};
            const auto &file = *placer;
            if (stored.count(placer)) continue; // places no static mesh
            try
            {
                auto it = rebuilt.find(placer);
                Member loaded;
                if (it == rebuilt.end()) loaded = load(file);
                auto result = model_import::RemapLighting(it != rebuilt.end() ? it->second.bytes : loaded.bytes, StemOf(file.path), maps,
                                                          options.cancel);
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
                rebuild_progress::Advance(*options.progress);
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
            if (options.Cancelled()) return;
            put(*file, item);
            rebuild_progress::Advance(*options.progress);
        }
    };

    // Shared mesh folders (sorted first) one by one, as the others need their
    // vertex maps; the rest on worker threads.
    size_t next = 0;
    for (; next < order.size() && order[next]->second.shared && !options.Cancelled(); ++next) work(*order[next], counts);
#ifdef __ANDROID__
    constexpr size_t kMaxThreads = 2; // each holds a few packages; phones share their memory with the game
#else
    constexpr size_t kMaxThreads = 8;
#endif
    const size_t threads = std::min<size_t>(std::clamp<size_t>(std::thread::hardware_concurrency(), 1, options.threads ? options.threads : kMaxThreads),
                                            order.size() - next);
    std::atomic<size_t> claimed = next;
    std::vector<Counts> partial(threads);
    std::vector<std::thread> pool;
    for (size_t t = 0; t < threads; ++t)
        pool.emplace_back([&, t] {
            if (options.background) LowerThreadPriority();
            for (size_t i; !options.Cancelled() && (i = claimed++) < order.size();)
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
        counts.failed += c.failed; counts.models += c.models; counts.fonts += c.fonts;
    }
    if (options.Cancelled()) throw Cancelled{};

    if (overflowFiles.empty()) return;
    try
    {
        index = fpi::AddOverflowArchives(index, overflowNames, overflowFiles);
        std::string list;
        for (const auto &name : overflowNames) list += (list.empty() ? "" : ", ") + name;
        LOG_NOTICE("[mods] {} rebuilt files did not fit in their archives in {}; they are in {}", overflowFiles.size(), Utf8(folder), list);
    }
    catch (const std::exception &error)
    {
        // The original records are untouched: the game reads the original files.
        for (size_t i = 0; i < overflowFiles.size(); ++i)
        {
            LOG_WARNING("[mods] {} kept original in {}: overflow archive: {}", overflowFiles[i].path, Utf8(folder), error.what());
            --counts.files;
            counts.texts -= overflowTexts[i];
            ++counts.failed;
        }
        for (const auto &name : overflowNames)
        {
            auto &overflow = overflows.at(name);
            overflow.file.close();
            std::error_code ec;
            fs::remove(overflow.path, ec);
            overflows.erase(name);
        }
    }
}

// ---- The cache ----------------------------------------------------------------
// cache/mods/<folder hash>/<key>/ holds one set of a folder's rebuilt files: the
// patched indexes (<name>.index), the tails (<archive>.tail), the overflow
// archives (<name>) and manifest.txt, written last: the key, then one line per
// index ("index <name>"), tail ("tail <archive> <base> <size>") and overflow
// archive ("overflow <name> <size>"). The key covers the code that rebuilds the
// files and the cache format, the selected language pack, the mods' files (path,
// entry, size, time), and the folder's indexes and archive sizes; any change
// makes a new set. A game update that leaves the rebuilding code alone keeps the
// cache. Each folder keeps its last kKeptSets sets by use (the manifest's time),
// plus sets still in use, so turning a mod off and on again reads its files
// instead of rebuilding them; a start without mods keeps them too.
constexpr const char *kCacheFormat = "3"; // bump when the rebuilt bytes change for the same inputs
constexpr size_t kKeptSets = 2;           // the set in use and the one before it
#ifdef LO_MODS_BUILDER_HASH
// A hash of modding/ and the readers it uses, made by the build.
constexpr const char *kBuilder = LO_MODS_BUILDER_HASH;
#else
// Tools built without it (LoModelImport) key on the version instead.
constexpr const char *kBuilder = lo_version::Source;
#endif

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

// The cache folder for a game folder, which holds its sets; empty without a cache.
fs::path CacheFolder(const fs::path &folder)
{
    const auto root = CacheRoot();
    return root.empty() ? root : root / Hex(Hash(0xcbf29ce484222325ull, Utf8(fs::absolute(folder).lexically_normal())));
}

bool IsHex16(const std::string &name)
{
    return name.size() == 16 && name.find_first_not_of("0123456789abcdef") == std::string::npos;
}

// Bytes of the files under path (or of path itself).
uint64_t Bytes(const fs::path &path)
{
    std::error_code ec;
    if (!fs::is_directory(path, ec))
    {
        const auto size = fs::file_size(path, ec);
        return ec ? 0 : size;
    }
    uint64_t total = 0;
    for (fs::recursive_directory_iterator it(path, ec), end; !ec && it != end; it.increment(ec))
    {
        std::error_code entryError;
        if (it->is_regular_file(entryError)) total += it->file_size(entryError);
    }
    return total;
}

double MiB(uint64_t bytes) { return double(bytes) / (1024.0 * 1024.0); }

// After a set of folderCache is read or written: it, the most recently used
// other set and any set held by an active folder or open handle stay. Older
// sets, unfinished builds and files of the earlier one-set layout go.
void Prune(const fs::path &folderCache, const fs::path &current)
{
    std::lock_guard pins(gCachePinsMutex); // a new handle cannot acquire a pin during removal
    std::erase_if(gCachePins, [](const auto &entry) { return entry.second.expired(); });
    struct Set { fs::path path; fs::file_time_type used; };
    std::vector<Set> sets;
    std::vector<fs::path> stale;
    std::error_code ec;
    for (fs::directory_iterator it(folderCache, ec), end; !ec && it != end; it.increment(ec))
    {
        const auto &path = it->path();
        if (path.filename() == current.filename()) continue;
        std::error_code entryError;
        const auto manifest = path / "manifest.txt";
        if (it->is_directory(entryError) && IsHex16(Utf8(path.filename())) && fs::is_regular_file(manifest, entryError))
            sets.push_back({path, fs::last_write_time(manifest, entryError)});
        else stale.push_back(path);
    }
    // The earlier layout built a folder's files in <folder hash>.new.
    if (const auto old = fs::path(folderCache).concat(".new"); fs::exists(old, ec)) stale.push_back(old);
    std::sort(sets.begin(), sets.end(), [](const Set &a, const Set &b) { return a.used > b.used; });
    std::vector<Set> keptSets;
    size_t recent = 0;
    for (const auto &set : sets)
    {
        const auto pin = gCachePins.find(set.path);
        if (pin != gCachePins.end() && !pin->second.expired()) keptSets.push_back(set);
        else if (recent++ < kKeptSets - 1) keptSets.push_back(set);
        else stale.push_back(set.path);
    }

    uint64_t kept = Bytes(current), freed = 0;
    size_t keptCount = keptSets.size() + 1;
    for (const auto &set : keptSets) kept += Bytes(set.path);
    for (const auto &path : stale)
    {
        const auto pin = gCachePins.find(path);
        if (pin != gCachePins.end() && !pin->second.expired())
        {
            kept += Bytes(path);
            ++keptCount;
            continue;
        }
        const auto bytes = Bytes(path);
        std::error_code removeError;
        fs::remove_all(path, removeError);
        const auto left = removeError ? Bytes(path) : 0;
        freed += bytes - std::min(bytes, left);
        if (removeError) LOG_WARNING("[mods] cannot remove {} from the mod cache: {}", Utf8(path), removeError.message());
    }
    LOG_NOTICE("[mods] mod cache {}: kept {} set{} ({:.1f} MiB), freed {:.1f} MiB", Utf8(folderCache), keptCount,
               keptCount == 1 ? "" : "s", MiB(kept), MiB(freed));
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
        else if (word == "overflow")
        {
            uint64_t size = 0;
            if (!(manifest >> size)) return false;
            const auto path = cache / value;
            if (fs::file_size(path, ec) != size || ec) return false;
            loaded.overflow[value] = path;
        }
        else return false;
    }
    loaded.cachePin = PinCache(cache);
    for (auto &[name, ranges] : loaded.ranges)
        for (auto &range : ranges) range.cachePin = loaded.cachePin;
    result = std::move(loaded);
    return true;
}

Folder Build(const fs::path &folder, const ModSet &set, const BuildOptions &options = {})
{
    Folder result;
    // File names are matched case-insensitively, as the game does.
    std::map<std::string, fs::path> names;
    std::error_code ec;
    for (const auto &item : fs::directory_iterator(folder, ec))
        names.emplace(Lower(Utf8(item.path().filename())), item.path());
    // The indexes and the archives' sizes are part of the key.
    std::map<std::string, std::vector<uint8_t>> indexes;
    uint64_t key = Hash(set.key, Utf8(fs::absolute(folder).lexically_normal()));
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
    const auto sets = CacheFolder(folder);
    const auto cache = sets.empty() ? sets : sets / Hex(key);
    if (!cache.empty() && LoadCache(cache, key, result))
    {
        LOG_NOTICE("[mods] rebuilt files for {} read from {}", Utf8(folder), Utf8(cache));
        // In use now: the manifest's time orders the sets.
        fs::last_write_time(cache / "manifest.txt", fs::file_time_type::clock::now(), ec);
        Prune(sets, cache);
        return result;
    }
    if (!cache.empty())
    {
        std::error_code missing;
        LOG_NOTICE("[mods] rebuilding the files for {} (mod code {}): {}", Utf8(folder), kBuilder,
                   fs::exists(cache / "manifest.txt", missing) ? "the cached files are incomplete"
                   : fs::is_directory(sets, missing) && !fs::is_empty(sets, missing)
                       ? "the mods, the game files or the mod code changed"
                       : "nothing cached yet");
    }

    if (options.Cancelled()) throw Cancelled{};
    // PatchIndex counts the packages for the window until the files are in place.
    struct Finished
    {
        std::atomic<uint64_t> *progress;
        ~Finished() { rebuild_progress::Finish(*progress); }
    } finished{options.progress};

    // Build into <cache>.new, then put it in place of the old one.
    fs::path building = cache.empty() ? fs::path() : fs::path(cache).concat(".new");
    if (!building.empty())
    {
        {
            std::lock_guard pins(gCachePinsMutex);
            for (size_t suffix = 1;; ++suffix)
            {
                const auto pin = gCachePins.find(building);
                if (pin == gCachePins.end() || pin->second.expired()) break;
                building = fs::path(cache).concat(".new." + std::to_string(suffix));
            }
            fs::remove_all(building, ec);
        }
        fs::create_directories(building, ec);
        if (ec)
        {
            LOG_WARNING("[mods] no cache for rebuilt files ({}): {}; keeping them in memory", Utf8(building), ec.message());
            building.clear();
        }
    }
    std::map<std::string, Tail> tails, overflows;
    std::vector<std::string> patched;
    Counts counts;
    try
    {
        for (auto &[name, index] : indexes)
        {
            try
            {
                const auto before = counts.files;
                PatchIndex(set, folder, names, index, tails, overflows, building, counts, options);
                if (counts.files != before) patched.push_back(name);
            }
            catch (const std::exception &error)
            {
                LOG_WARNING("[mods] cannot patch index {}: {}", Utf8(names.at(name)), error.what());
            }
        }
    }
    catch (const Cancelled &)
    {
        // Nothing half done stays: the folder has no manifest and goes.
        for (auto &[name, tail] : tails) tail.file.close();
        for (auto &[name, overflow] : overflows) overflow.file.close();
        if (!building.empty()) fs::remove_all(building, ec);
        throw;
    }
    for (auto &[name, tail] : tails) tail.file.close();
    for (auto &[name, overflow] : overflows) overflow.file.close();
    if (counts.files || counts.failed)
        // Keys a copy lacks are normal: a pack made from the DLC copy of a file
        // also applies to the disc copy, which has fewer entries.
        LOG_NOTICE("[mods] {} translated and {} model or font packages rebuilt in {} ({} fonts given glyphs; {} failed; {} translation keys this copy does not have)",
                   counts.texts, counts.files - counts.texts, Utf8(folder), counts.fonts, counts.failed, counts.unknownKeys);

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
            for (const auto &[name, overflow] : overflows)
                if (overflow.size) manifest << "overflow " << name << " " << overflow.size << "\n";
            manifest.close();
            if (!manifest) throw std::runtime_error("cannot write the manifest");
            fs::rename(building / "manifest.txt.part", building / "manifest.txt");
            {
                std::lock_guard pins(gCachePinsMutex);
                const auto pin = gCachePins.find(cache);
                if (pin != gCachePins.end() && !pin->second.expired())
                    throw std::runtime_error("cache set is in use");
                fs::remove_all(cache);
                fs::rename(building, cache);
            }
            cached = LoadCache(cache, key, result);
        }
        catch (const std::exception &error)
        {
            LOG_WARNING("[mods] cannot keep the rebuilt files in {}: {}", Utf8(cache), error.what());
        }
        // Not when the files are still served from the build folder.
        if (cached) Prune(sets, cache);
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
        for (const auto &[name, overflow] : overflows)
            if (overflow.size) result.overflow[name] = overflow.path;
        if (!building.empty() && fs::is_directory(building, ec))
        {
            result.cachePin = PinCache(building);
            for (auto &[name, ranges] : result.ranges)
                for (auto &range : ranges) range.cachePin = result.cachePin;
        }
    }
    return result;
}

// The mods as the mod API lists them now. Lists every mod file (it may wait
// for the listing), so a live reload calls it off the game thread.
std::shared_ptr<const ModSet> MakeSet(const std::string &languagePack)
{
    auto set = std::make_shared<ModSet>();
    auto &texts = set->texts;
    for (auto &asset : ListTexts()) texts.emplace(asset.id.key, std::move(asset));
    size_t packFiles = 0;
    for (const auto &pack : LanguagePacks())
        if (pack.id == languagePack)
            for (auto &asset : ListTexts(pack))
            {
                texts.insert_or_assign(asset.id.key, std::move(asset));
                ++packFiles;
            }
    if (!texts.empty())
        LOG_NOTICE("[mods] text: {} translation files ({} from language pack {})", texts.size(), packFiles,
                   languagePack.empty() ? "none" : languagePack);
    size_t fonts = 0;
    for (const auto &pack : LanguagePacks())
        if (pack.id == languagePack)
            for (auto &asset : ListFonts(pack))
            {
                const auto package = asset.id.key.substr(0, asset.id.key.rfind('#'));
                set->fonts[package].push_back(std::move(asset));
                ++fonts;
            }
    if (fonts) LOG_NOTICE("[mods] font: {} glyph files in {} packages from language pack {}", fonts, set->fonts.size(), languagePack);
    size_t models = 0;
    for (auto &asset : ListModels())
    {
        const auto package = asset.id.key.substr(0, asset.id.key.rfind('#'));
        set->models[package].push_back(std::move(asset));
        ++models;
    }
    if (models) LOG_NOTICE("[mods] model: {} replacements in {} packages", models, set->models.size());

    uint64_t key = Hash(Hash(0xcbf29ce484222325ull, kBuilder), kCacheFormat);
    key = Hash(key, languagePack);
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
    for (const auto &[member, asset] : texts) add(asset);
    for (const auto &[member, assets] : set->models)
        for (const auto &asset : assets) add(asset);
    for (const auto &[member, assets] : set->fonts)
        for (const auto &asset : assets) add(asset);
    set->key = key;
    return set;
}

// Caller holds gMutex.
void List()
{
    if (!gSet) gSet = MakeSet(gLanguagePack);
}

// ---- Background preparation of the other discs ------------------------------
// A mod that rebuilds many files makes the first switch to another disc wait
// for that disc's rebuild (a minute or two). After the start, the other discs
// of the game folder are rebuilt into the cache on a few low-priority threads,
// so a later switch only reads the cache. A build the game waits for stops the
// background one (it starts again after) and runs on its own as before.
struct Prebuild
{
    std::mutex mutex;
    std::condition_variable changed;
    bool allowed = false, started = false, stopping = false;
    bool running = false;           // the thread is alive (a live reload restarts the list, not the thread)
    std::shared_ptr<const ModSet> set; // what the discs are prepared for
    std::chrono::seconds delay{30}; // after the first disc: the DLC folders, the first map's shaders
    int foreground = 0;            // builds the game waits for: the background one waits
    std::vector<fs::path> pending; // disc folders still to prepare
    std::set<fs::path> claimed;    // built in the foreground
    fs::path current;              // being prepared
    std::atomic<bool> cancel{false};
    std::atomic<uint64_t> progress{0};
};
Prebuild gPrebuild;

// One thread by default: four made the field drop from 27 to 24 FPS on a Strix
// Halo handheld (p95 frame time 34 -> 98 ms). LO_MODS_PREBUILD_THREADS overrides.
size_t PrebuildThreads()
{
    const char *setting = std::getenv("LO_MODS_PREBUILD_THREADS");
    return std::clamp<size_t>(setting ? size_t(std::atoi(setting)) : 1, 1, 8);
}

void PrebuildThread()
{
    LowerThreadPriority();
    {
        // Let the start finish first.
        std::unique_lock lock(gPrebuild.mutex);
        gPrebuild.changed.wait_for(lock, gPrebuild.delay, [] { return gPrebuild.stopping; });
    }
    for (;;)
    {
        fs::path folder;
        std::shared_ptr<const ModSet> set;
        {
            std::unique_lock lock(gPrebuild.mutex);
            gPrebuild.changed.wait(lock, [] { return gPrebuild.stopping || !gPrebuild.foreground; });
            if (gPrebuild.stopping || gPrebuild.pending.empty())
            {
                if (!gPrebuild.stopping) LOG_NOTICE("[mods] background preparation finished");
                gPrebuild.running = false;
                break;
            }
            folder = gPrebuild.pending.front();
            set = gPrebuild.set;
            gPrebuild.pending.erase(gPrebuild.pending.begin());
            gPrebuild.current = folder;
            gPrebuild.cancel = false;
            gPrebuild.progress = 0;
        }
        bool again = false;
        const auto began = std::chrono::steady_clock::now();
        try
        {
            LOG_NOTICE("[mods] preparing the rebuilt files for {} in the background ({} threads)", Utf8(folder), PrebuildThreads());
            BuildOptions options;
            options.progress = &gPrebuild.progress;
            options.threads = PrebuildThreads();
            options.background = true;
            options.cancel = &gPrebuild.cancel;
            Build(folder, *set, options);
            LOG_NOTICE("[mods] background preparation of {} done in {:.0f} s", Utf8(folder),
                       std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count());
        }
        catch (const Cancelled &)
        {
            again = true;
            LOG_NOTICE("[mods] background preparation of {} stopped", Utf8(folder));
        }
        catch (const std::exception &error)
        {
            LOG_WARNING("[mods] background preparation of {} failed: {}", Utf8(folder), error.what());
        }
        {
            std::lock_guard lock(gPrebuild.mutex);
            gPrebuild.current.clear();
            gPrebuild.progress = 0;
            if (again && !gPrebuild.stopping && !gPrebuild.claimed.count(folder) &&
                std::find(gPrebuild.pending.begin(), gPrebuild.pending.end(), folder) == gPrebuild.pending.end())
                gPrebuild.pending.insert(gPrebuild.pending.begin(), folder);
        }
        gPrebuild.changed.notify_all();
    }
}

// After the first disc folder is in place: the other disc folders beside it.
// Caller holds gMutex.
void StartPrebuild(const fs::path &folder)
{
    std::lock_guard lock(gPrebuild.mutex);
    if (!gPrebuild.allowed || gPrebuild.started || gPrebuild.stopping || CacheRoot().empty()) return; // nothing to keep without a cache
    const auto name = Lower(Utf8(folder.filename()));
    if (name.size() != 5 || !name.starts_with("disc") || name[4] < '1' || name[4] > '4') return;
    gPrebuild.started = true;
    std::error_code ec;
    // The next discs first, as the story goes; then the earlier ones.
    std::vector<char> order;
    for (char disc = char(name[4] + 1); disc <= '4'; ++disc) order.push_back(disc);
    for (char disc = char(name[4] - 1); disc >= '1'; --disc) order.push_back(disc);
    for (const char disc : order)
    {
        const auto other = (folder.parent_path() / (std::string("disc") + disc)).lexically_normal();
        if (other == folder || gPrebuild.claimed.count(other) || gFolders.count(other)) continue;
        bool index = false;
        for (const auto &item : fs::directory_iterator(other, ec))
            index = index || Lower(Utf8(item.path().filename())) == "lo.fpi";
        if (index) gPrebuild.pending.push_back(other);
    }
    if (gPrebuild.pending.empty()) return;
    gPrebuild.set = gSet;
    LOG_NOTICE("[mods] {} other discs will be prepared in the background", gPrebuild.pending.size());
    if (gPrebuild.running)
    {
        gPrebuild.changed.notify_all();
        return;
    }
    gPrebuild.running = true;
    std::thread(PrebuildThread).detach();
}

// A build the game waits for: the background one stops (and waits for the
// foreground to end); for the same folder it stops first, as both would write
// its cache. While waiting, the window shows the stopped build's count.
struct ForegroundBuild
{
    explicit ForegroundBuild(const fs::path &folder)
    {
        std::unique_lock lock(gPrebuild.mutex);
        ++gPrebuild.foreground;
        gPrebuild.claimed.insert(folder);
        std::erase(gPrebuild.pending, folder);
        if (gPrebuild.current.empty()) return;
        gPrebuild.cancel = true;
        if (gPrebuild.current != folder) return;
        LOG_NOTICE("[mods] stopping the background preparation of {}: the game needs it now", Utf8(folder));
        while (!gPrebuild.current.empty())
        {
            rebuild_progress::g_progress.store(gPrebuild.progress.load(std::memory_order_relaxed), std::memory_order_relaxed);
            gPrebuild.changed.wait_for(lock, std::chrono::milliseconds(100));
        }
        rebuild_progress::Finish();
    }
    ~ForegroundBuild()
    {
        {
            std::lock_guard lock(gPrebuild.mutex);
            --gPrebuild.foreground;
        }
        gPrebuild.changed.notify_all();
    }
};

// ---- Live reload ----------------------------------------------------------------
// After the mod list changes in game, the current disc's files are prepared for
// the new mods on a thread while the game runs. The runtime then has the game
// reload that disc's index (modding/hot_reload.h); the files switch when the game
// opens the index again, after it closed every archive, so no handle mixes the
// old index with the new files.
struct Reload
{
    ReloadState state = ReloadState::Idle; // gMutex, like the fields up to error
    uint64_t serial = 0;                   // each request; a newer one drops older results
    fs::path folder;
    std::shared_ptr<const ModSet> set;
    Folder result;
    std::string error;
    std::atomic<uint64_t> progress{0};
    std::atomic<bool> cancel{false};
    std::mutex building; // one preparation at a time: two of the same mods would share a cache folder
};
Reload gReload;

// Disc folders whose files were made for the old mods: folder and its discN siblings.
bool SameDiscs(const fs::path &path, const fs::path &folder)
{
    if (path == folder) return true;
    const auto name = Lower(Utf8(path.filename()));
    return path.parent_path() == folder.parent_path() && name.size() == 5 && name.starts_with("disc") && name[4] >= '1' && name[4] <= '4';
}

// The game opens the index of the reloaded disc. Caller holds gMutex.
void CommitReload(const fs::path &folder)
{
    gSet = std::move(gReload.set);
    std::erase_if(gFolders, [&](const auto &entry) { return SameDiscs(entry.first, folder); });
    if (!gSet->Empty()) gFolders.emplace(folder, std::move(gReload.result));
    gReload.result = {};
    gReload.state = ReloadState::Committed;
    {
        // The other discs are prepared again, for these mods.
        std::lock_guard lock(gPrebuild.mutex);
        gPrebuild.cancel = true;
        gPrebuild.pending.clear();
        gPrebuild.claimed.clear();
        gPrebuild.started = false;
    }
    LOG_NOTICE("[mods] live reload: {} now reads the files for the new mods ({} text, {} model and {} font packages)", Utf8(folder),
               gSet->texts.size(), gSet->models.size(), gSet->fonts.size());
}
} // namespace

std::vector<Range> RangesFor(const fs::path &file)
{
    const auto name = Lower(Utf8(file.filename()));
    const bool archive = name.size() > 4 && (name.compare(name.size() - 4, 4, ".fpd") == 0 || name.compare(name.size() - 4, 4, ".fpi") == 0);
    if (!archive) return {};
    std::lock_guard lock(gMutex);
    List();
    const auto folder = file.parent_path().lexically_normal();
    if (gReload.state == ReloadState::Armed && name == "lo.fpi" && folder == gReload.folder) CommitReload(folder);
    if (gSet->Empty())
    {
        // The rebuilt files of earlier starts stay: turning the mods on again reads them.
        static bool reported = false;
        if (const auto root = CacheRoot(); !reported && !root.empty())
        {
            reported = true;
            if (const auto bytes = Bytes(root))
                LOG_NOTICE("[mods] no mods: keeping {:.1f} MiB of rebuilt files in {} for the last mod sets", MiB(bytes), Utf8(root));
        }
        return {};
    }
    auto it = gFolders.find(folder);
    if (it == gFolders.end())
    {
        ForegroundBuild foreground(folder);
        it = gFolders.emplace(folder, Build(folder, *gSet)).first;
    }
    StartPrebuild(folder);
    const auto ranges = it->second.ranges.find(name);
    return ranges == it->second.ranges.end() ? std::vector<Range>{} : ranges->second;
}

fs::path OverflowArchive(const fs::path &file, std::shared_ptr<void> *cachePin)
{
    const auto name = Lower(Utf8(file.filename()));
    if (!name.ends_with(".fpd")) return {};
    std::lock_guard lock(gMutex);
    const auto folder = gFolders.find(file.parent_path().lexically_normal());
    if (folder == gFolders.end()) return {};
    const auto it = folder->second.overflow.find(name);
    if (it != folder->second.overflow.end() && cachePin) *cachePin = folder->second.cachePin;
    return it == folder->second.overflow.end() ? fs::path() : it->second;
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
    const auto text = gSet->texts.find(memberPath);
    if (text == gSet->texts.end()) return false;
    std::lock_guard files(gFilesMutex);
    const auto translation = Translation(text->second);
    return translation && translation->count(key);
}

void EnablePrebuild(std::chrono::seconds delay)
{
    const char *setting = std::getenv("LO_MODS_PREBUILD");
#ifdef __ANDROID__
    // Phones share memory and heat with the game: only when asked for.
    const bool allowed = setting && std::string_view(setting) == "1";
#else
    const bool allowed = !setting || std::string_view(setting) != "0";
#endif
    std::lock_guard lock(gPrebuild.mutex);
    gPrebuild.allowed = allowed;
    gPrebuild.delay = delay;
}

void WaitForPrebuild()
{
    std::unique_lock lock(gPrebuild.mutex);
    gPrebuild.changed.wait(lock, [] { return !gPrebuild.started || gPrebuild.stopping || (gPrebuild.pending.empty() && gPrebuild.current.empty()); });
}

void StopPrebuild()
{
    {
        std::lock_guard lock(gPrebuild.mutex);
        gPrebuild.stopping = true;
        gPrebuild.cancel = true;
    }
    gPrebuild.changed.notify_all();
}

void SelectLanguagePack(const std::string &id)
{
    std::lock_guard lock(gMutex);
    gLanguagePack = id;
}

void PrepareReload(const fs::path &discFolder)
{
    // As RangesFor names it: the parent of the index's path.
    const auto folder = (discFolder / "LO.fpi").parent_path().lexically_normal();
    uint64_t serial = 0;
    std::string pack;
    {
        std::lock_guard lock(gMutex);
        gReload.cancel = true; // an earlier preparation stops; its result would be dropped anyway
        serial = ++gReload.serial;
        gReload.state = ReloadState::Preparing;
        gReload.folder = folder;
        gReload.set.reset();
        gReload.result = {};
        gReload.error.clear();
        pack = gLanguagePack;
    }
    LOG_NOTICE("[mods] live reload: preparing the files of {} for the changed mods", Utf8(folder));
    std::thread([serial, folder, pack] {
        auto current = [&] { return gReload.serial == serial; }; // caller holds gMutex
        const auto began = std::chrono::steady_clock::now();
        try
        {
            std::lock_guard building(gReload.building);
            {
                std::lock_guard lock(gMutex);
                if (!current()) return;
                gReload.cancel = false;
                gReload.progress = 0;
            }
            auto set = MakeSet(pack); // waits for the mod listing
            {
                std::lock_guard lock(gMutex);
                if (!current()) return;
                if (gSet && gSet->key == set->key)
                {
                    gReload.state = ReloadState::Unchanged;
                    LOG_NOTICE("[mods] live reload: the mods give the same files; nothing to reload");
                    return;
                }
            }
            Folder result;
            if (!set->Empty())
            {
                ForegroundBuild pause(folder); // the background preparation of other discs waits
                BuildOptions options;
                options.progress = &gReload.progress;
                options.cancel = &gReload.cancel;
                result = Build(folder, *set, options);
            }
            std::lock_guard lock(gMutex);
            if (!current()) return;
            gReload.set = std::move(set);
            gReload.result = std::move(result);
            gReload.state = ReloadState::Ready;
            LOG_NOTICE("[mods] live reload: files of {} ready in {:.1f} s", Utf8(folder),
                       std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count());
        }
        catch (const Cancelled &)
        {
            LOG_NOTICE("[mods] live reload: preparation of {} stopped", Utf8(folder));
        }
        catch (const std::exception &error)
        {
            std::lock_guard lock(gMutex);
            if (!current()) return;
            gReload.state = ReloadState::Failed;
            gReload.error = error.what();
            LOG_WARNING("[mods] live reload: preparing {} failed: {}", Utf8(folder), error.what());
        }
    }).detach();
}

ReloadStatus GetReloadStatus()
{
    std::lock_guard lock(gMutex);
    const auto progress = gReload.progress.load(std::memory_order_relaxed);
    return {gReload.state, rebuild_progress::Done(progress), rebuild_progress::Total(progress), gReload.error};
}

bool ArmReload()
{
    std::lock_guard lock(gMutex);
    if (gReload.state != ReloadState::Ready) return false;
    gReload.state = ReloadState::Armed;
    return true;
}

void DisarmReload()
{
    std::lock_guard lock(gMutex);
    if (gReload.state == ReloadState::Armed) gReload.state = ReloadState::Ready;
}

void EndReload()
{
    std::lock_guard lock(gMutex);
    gReload.cancel = true;
    ++gReload.serial;
    gReload.state = ReloadState::Idle;
    gReload.set.reset();
    gReload.result = {};
}
}
