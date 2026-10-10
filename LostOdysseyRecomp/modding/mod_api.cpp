#include "mod_api.h"
#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <system_error>
#include <tuple>
#include <unordered_map>

namespace modding {
namespace {
// One replacement file. The map key is its overlay-relative name, so explicit
// mod.ini lines and files found under a mod's overlay/ meet on the same key.
struct Entry {
    AssetId id; // key empty for hashed kinds found by file name
    std::filesystem::path file;
    uint32_t mod = 0; // index into Snapshot::mods
};
struct Snapshot {
    std::filesystem::path root, defaultRoot, modList;
    std::unordered_map<std::string, Entry> entries; // winners in mod order
    std::vector<ModInfo> mods;                      // every mod folder, effective order
    std::vector<std::string> modIds;
    std::vector<LanguagePack> languagePacks;
    std::vector<Diagnostic> diagnostics;
    ResolutionMode mode = ResolutionMode::Combined;
    bool enabled = false;
    bool textureEntries = false;
    uint32_t overlayKinds = 0; // bit per AssetKind: top-level overlay/<folder> existed
};
// mod.ini as read, before the effective order decides whether its files count.
struct Loaded {
    ModInfo info;
    std::string dirName;
    std::vector<std::pair<size_t, std::string>> resources;
    size_t rank = SIZE_MAX; // mod-list.ini position
    bool duplicate = false;
};
std::mutex gMutex, gSaveMutex;
std::shared_ptr<const Snapshot> gSnapshot = std::make_shared<Snapshot>();
std::map<AssetKind, std::shared_ptr<AssetProvider>> gProviders;
uint64_t gGeneration = 0;
thread_local bool gInProvider = false;
constexpr size_t kMaxManifestBytes = 1024 * 1024;
constexpr const char* kKindFolders[] = {"", "images", "fonts", "models", "movies", "textures", "text"};

std::string Utf8(const std::filesystem::path& path) {
    const auto text = path.generic_u8string();
    return {text.begin(), text.end()};
}
std::filesystem::path FromUtf8(std::string_view text) {
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}
std::string Lower(std::string s) {
    for (char& c : s) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return s;
}
std::string_view Trim(std::string_view s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == s.npos) return {};
    return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}
bool KindValid(AssetKind kind) {
    return kind >= AssetKind::Image && kind <= AssetKind::Text;
}
uint32_t KindBit(AssetKind kind) { return 1u << static_cast<uint32_t>(kind); }
std::optional<AssetKind> ParseKind(std::string_view s) {
    if (s == "image") return AssetKind::Image;
    if (s == "font") return AssetKind::Font;
    if (s == "model") return AssetKind::Model;
    if (s == "movie") return AssetKind::Movie;
    if (s == "texture") return AssetKind::Texture;
    if (s == "text") return AssetKind::Text;
    return {};
}
std::optional<AssetKind> FolderKind(std::string_view folder) {
    for (uint32_t k = 1; k != std::size(kKindFolders); ++k)
        if (folder == kKindFolders[k]) return static_cast<AssetKind>(k);
    return {};
}
template<class T> bool Number(std::string_view s, T& value) {
    if (s.empty()) return false;
    const auto result = std::from_chars(s.data(), s.data() + s.size(), value);
    return result.ec == std::errc{} && result.ptr == s.data() + s.size();
}
bool Hex16(std::string_view s) {
    return s.size() == 16 && std::all_of(s.begin(), s.end(),
        [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}
bool SafeText(std::string_view s) {
    return !s.empty() && s.size() <= 4096 &&
        std::none_of(s.begin(), s.end(), [](unsigned char c) { return c < 32 || c == 127; });
}
// api_version=2 display metadata: valid UTF-8 without control characters.
bool DisplayText(std::string_view s, size_t limit) {
    if (s.empty() || s.size() > limit) return false;
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = s[i];
        if (c < 0x80) {
            if (c < 32 || c == 127) return false;
            ++i;
            continue;
        }
        size_t n = 0;
        uint32_t cp = 0;
        if (c >= 0xC2 && c <= 0xDF) { n = 1; cp = c & 0x1F; }
        else if (c >= 0xE0 && c <= 0xEF) { n = 2; cp = c & 0x0F; }
        else if (c >= 0xF0 && c <= 0xF4) { n = 3; cp = c & 0x07; }
        else return false;
        if (i + n >= s.size()) return false;
        for (size_t k = 1; k <= n; ++k) {
            const unsigned char d = s[i + k];
            if ((d & 0xC0) != 0x80) return false;
            cp = cp << 6 | (d & 0x3F);
        }
        if ((n == 2 && cp < 0x800) || (n == 3 && (cp < 0x10000 || cp > 0x10FFFF)) ||
            (cp >= 0xD800 && cp <= 0xDFFF) || cp <= 0x9F) return false;
        i += n + 1;
    }
    return true;
}
std::optional<std::filesystem::path> Relative(std::string_view text) {
    if (!SafeText(text)) return {};
    std::string s(text);
    std::replace(s.begin(), s.end(), '\\', '/');
    if (s.front() == '/' || s.find(':') != s.npos) return {};
    auto path = FromUtf8(s);
    if (path.is_absolute() || path.has_root_name() || path.has_root_directory()) return {};
    for (const auto& part : path) if (part == "..") return {};
    return path.lexically_normal();
}
// Lexical containment only. MO2's virtual file system and symlink deployments
// (Vortex, Steam Deck) place the real files outside the root by design, so
// canonical paths must not decide whether a mod file is visible.
bool ContainedFile(const std::filesystem::path& root, const std::filesystem::path& file) {
    const auto base = root.lexically_normal(), path = file.lexically_normal();
    auto b = base.begin(), p = path.begin();
    for (; b != base.end() && !b->empty(); ++b, ++p) if (p == path.end() || *b != *p) return false;
    std::error_code ec;
    return p != path.end() && std::filesystem::is_regular_file(file, ec) && !ec;
}
std::string CanonicalKey(std::string_view key) {
    const auto hash = key.rfind('#');
    if (hash == key.npos) return {};
    const auto colon = key.find(':', hash + 1);
    uint32_t index = 0;
    if (colon == key.npos || !Number(key.substr(hash + 1, colon - hash - 1), index)) return {};
    return MakeManifestKey(key.substr(0, hash), index, key.substr(colon + 1));
}
// Archive member paths: relative, '/' separators, ASCII lower case like the FPI names.
std::string CanonicalTextKey(std::string_view key) {
    const auto relative = Relative(key);
    if (!relative || *relative == ".") return {};
    auto path = Utf8(*relative);
    if (path.empty() || path.back() == '/' || path.size() > 1024) return {};
    return Lower(std::move(path));
}
// Texture fingerprints are matched exactly, never normalized.
std::string CanonicalKey(AssetKind kind, std::string_view key) {
    if (kind == AssetKind::Text) return CanonicalTextKey(key);
    if (kind != AssetKind::Texture) return CanonicalKey(key);
    return Hex16(key) ? std::string(key) : std::string{};
}
// Overlay-relative name with '/' separators for a canonical key.
std::string OverlayKey(AssetKind kind, const std::string& key) {
    // A fingerprint is already a fixed-size name, so two manager mods that
    // replace the same image collide on the same path.
    if (kind == AssetKind::Texture) return "overlay/textures/fp-" + key + ".lotex2";
    if (kind == AssetKind::Text) return "overlay/text/" + key + ".json";
    uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : key) { hash ^= c; hash *= 1099511628211ull; }
    char name[40];
    std::snprintf(name, sizeof(name), "key-fnv1a64-%016llx", static_cast<unsigned long long>(hash));
    return std::string("overlay/") + kKindFolders[static_cast<uint32_t>(kind)] + "/" + name +
        (kind == AssetKind::Image ? ".lotex" : ".loasset");
}
void Diagnose(Snapshot& snapshot, const std::filesystem::path& file, size_t line, std::string message) {
    if (snapshot.diagnostics.size() < 256)
        snapshot.diagnostics.push_back({file, line, std::move(message)});
}
void Problem(Snapshot& snapshot, ModInfo& info, const std::filesystem::path& file, size_t line, std::string message) {
    if (info.problems.size() < 32) info.problems.push_back({file, line, message});
    Diagnose(snapshot, file, line, std::move(message));
}
bool ModIdValid(std::string_view id) {
    return !id.empty() && id.size() <= 128 && id != "." && id != ".." &&
        std::all_of(id.begin(), id.end(), [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
        });
}
// Bound the read itself, not just the lines after allocation, and reject files
// changed between stat/open/read rather than allocating an unbounded buffer.
bool ReadBounded(const std::filesystem::path& file, std::string& bytes, std::string& error) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(file, ec);
    if (ec || size > kMaxManifestBytes) { error = "file exceeds 1 MiB or cannot be read"; return false; }
    std::ifstream in(file, std::ios::binary);
    if (!in) { error = "cannot open file"; return false; }
    bytes.assign(static_cast<size_t>(size), '\0');
    if (!in.read(bytes.data(), static_cast<std::streamsize>(bytes.size())) ||
        in.peek() != std::char_traits<char>::eof() || in.bad()) {
        error = "file changed or could not be read"; return false;
    }
    return true;
}
// Raw lines without the line break; the BOM is removed from the first.
std::vector<std::string> RawLines(const std::string& bytes) {
    std::vector<std::string> lines;
    std::istringstream input(bytes);
    for (std::string line; std::getline(input, line);) {
        if (lines.empty() && line.compare(0, 3, "\xef\xbb\xbf") == 0) line.erase(0, 3);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(std::move(line));
    }
    return lines;
}
// Trimmed lines with their numbers, without blank lines and whole-line comments.
std::vector<std::pair<size_t, std::string>> Lines(const std::string& bytes) {
    std::vector<std::pair<size_t, std::string>> lines;
    size_t n = 0;
    for (const auto& line : RawLines(bytes)) {
        ++n;
        const auto text = Trim(line);
        if (!text.empty() && text.front() != '#' && text.front() != ';') lines.emplace_back(n, text);
    }
    return lines;
}
// Reads mod.ini metadata; resource lines are kept for later, when the order
// says whether the mod is active. nullopt: the folder has no mod.ini.
std::optional<Loaded> LoadManifest(Snapshot& snapshot, const std::filesystem::path& dir) {
    const auto manifest = dir / "mod.ini";
    std::error_code ec;
    if (!std::filesystem::is_regular_file(manifest, ec)) return {};
    Loaded l;
    l.dirName = Utf8(dir.filename());
    l.info.folder = dir;
    l.info.id = l.dirName;
    auto problem = [&](size_t line, std::string message) {
        Problem(snapshot, l.info, manifest, line, std::move(message));
        l.resources.clear();
        return std::move(l);
    };
    std::string bytes, error;
    if (!ReadBounded(manifest, bytes, error)) return problem(0, "manifest: " + error);
    const auto lines = Lines(bytes);
    std::vector<std::tuple<size_t, std::string_view, std::string_view>> metadata;
    uint32_t version = 0;
    for (const auto& [n, text] : lines) {
        const auto equal = text.find('=');
        if (equal == text.npos) return problem(n, "expected key=value");
        const auto key = Trim(std::string_view(text).substr(0, equal));
        if (key.find(':') != key.npos) { l.resources.emplace_back(n, text); continue; }
        const auto value = Trim(std::string_view(text).substr(equal + 1));
        metadata.emplace_back(n, key, value);
        // Find the version first: it decides which metadata keys are known.
        if (key == "api_version" && !version && (!Number(value, version) || version < 1 || version > kModApiVersion))
            return problem(n, "api_version must be 1 or 2");
    }
    if (!version) return problem(0, "api_version=1 or api_version=2 is required");
    std::string id = l.info.id;
    std::set<std::string_view> seen;
    for (const auto& [n, key, value] : metadata) {
        bool valid = seen.insert(key).second;
        auto display = [&](std::string& field, size_t limit) { valid = valid && DisplayText(value, limit); field = value; };
        if (key == "api_version") {}
        else if (key == "id") { id = value; valid = valid && ModIdValid(id); }
        else if (key == "priority") valid = valid && Number(value, l.info.priority);
        else if (key == "enabled") {
            valid = valid && (value == "true" || value == "false" || value == "1" || value == "0");
            l.info.manifestEnabled = value == "true" || value == "1";
        } else if (version >= 2 && key == "name") display(l.info.name, 128);
        else if (version >= 2 && key == "version") display(l.info.version, 64);
        else if (version >= 2 && key == "author") display(l.info.author, 128);
        else if (version >= 2 && key == "description") display(l.info.description, 1024);
        else valid = false;
        if (!valid) {
            l.info.name.clear(); l.info.version.clear(); l.info.author.clear(); l.info.description.clear();
            l.info.priority = 0; l.info.manifestEnabled = true;
            return problem(n, "unknown, duplicate or invalid metadata: " + std::string(key));
        }
    }
    if (!ModIdValid(id)) return problem(0, "invalid mod id: " + id);
    l.info.id = id;
    l.info.apiVersion = version;
    return l;
}
void Add(Snapshot& snapshot, const std::string& key, Entry&& entry) {
    snapshot.textureEntries |= entry.id.kind == AssetKind::Texture;
    snapshot.entries.try_emplace(key, std::move(entry));
}
// Lists a mod's overlay/ kind folders once. Names match the top-level overlay
// (ASCII case-insensitive); paths stay as written, so a VFS or symlinks work.
size_t IndexOverlay(Snapshot& snapshot, Loaded& l, uint32_t mod) {
    size_t count = 0;
    std::error_code ec;
    for (std::filesystem::directory_iterator top(l.info.folder / "overlay", ec), end; !ec && top != end; top.increment(ec)) {
        std::error_code entryError;
        const auto folder = Lower(Utf8(top->path().filename()));
        const auto kind = FolderKind(folder);
        if (!kind || !top->is_directory(entryError)) continue;
        std::error_code listError;
        if (*kind == AssetKind::Text) {
            const auto base = top->path();
            for (std::filesystem::recursive_directory_iterator it(base, listError), last; !listError && it != last; it.increment(listError)) {
                std::error_code fileError;
                if (!it->is_regular_file(fileError)) continue;
                const auto relative = Utf8(it->path().lexically_relative(base));
                if (relative.size() <= 5 || Lower(relative.substr(relative.size() - 5)) != ".json") continue;
                auto key = CanonicalTextKey(std::string_view(relative).substr(0, relative.size() - 5));
                if (key.empty()) continue;
                const auto name = OverlayKey(AssetKind::Text, key);
                Add(snapshot, name, Entry{{AssetKind::Text, std::move(key)}, it->path(), mod});
                ++count;
            }
        } else {
            const std::string_view prefix = *kind == AssetKind::Texture ? "fp-" : "key-fnv1a64-";
            const std::string_view suffix = *kind == AssetKind::Texture ? ".lotex2" : *kind == AssetKind::Image ? ".lotex" : ".loasset";
            for (std::filesystem::directory_iterator it(top->path(), listError), last; !listError && it != last; it.increment(listError)) {
                std::error_code fileError;
                const auto name = Lower(Utf8(it->path().filename()));
                if (name.size() != prefix.size() + 16 + suffix.size() || name.compare(0, prefix.size(), prefix) != 0 ||
                    name.compare(prefix.size() + 16, suffix.size(), suffix) != 0 || !Hex16(std::string_view(name).substr(prefix.size(), 16)) ||
                    !it->is_regular_file(fileError)) continue;
                AssetId id{*kind, *kind == AssetKind::Texture ? name.substr(prefix.size(), 16) : std::string{}};
                Add(snapshot, "overlay/" + folder + "/" + name, Entry{std::move(id), it->path(), mod});
                ++count;
            }
        }
        if (listError && listError != std::errc::no_such_file_or_directory)
            Problem(snapshot, l.info, top->path(), 0, "overlay folder listing incomplete: " + listError.message());
    }
    return count;
}
// Explicit mod.ini lines first (the last line of an identity wins), then the
// overlay/ files: within a mod an explicit line beats the discovered file, and
// mods added earlier (higher in the order) keep what they claimed.
void AddModFiles(Snapshot& snapshot, Loaded& l, uint32_t mod) {
    const auto manifest = l.info.folder / "mod.ini";
    std::unordered_map<std::string, Entry> declared;
    for (const auto& [n, text] : l.resources) {
        const auto equal = text.find('=');
        const auto left = Trim(std::string_view(text).substr(0, equal));
        const auto colon = left.find(':');
        const auto kind = ParseKind(Trim(left.substr(0, colon)));
        auto key = kind ? CanonicalKey(*kind, Trim(left.substr(colon + 1))) : std::string{};
        const auto relative = Relative(Trim(std::string_view(text).substr(equal + 1)));
        if (!kind || key.empty() || !relative) { Problem(snapshot, l.info, manifest, n, "invalid asset kind, identity or relative path"); continue; }
        // Relative() rejects absolute paths and '..', so the file is inside the
        // folder as written; only its existence is checked.
        auto file = l.info.folder / *relative;
        std::error_code ec;
        if (!std::filesystem::is_regular_file(file, ec)) {
            Problem(snapshot, l.info, manifest, n, "replacement is missing or escapes the mod directory"); continue;
        }
        ++l.info.manifestEntries;
        auto name = OverlayKey(*kind, key);
        declared.insert_or_assign(std::move(name), Entry{{*kind, std::move(key)}, std::move(file), mod});
    }
    for (auto& [name, entry] : declared) Add(snapshot, name, std::move(entry));
    if (l.info.apiVersion >= 2) l.info.overlayFiles = IndexOverlay(snapshot, l, mod);
}
std::optional<bool> Switch(std::string_view value) {
    if (value == "on" || value == "true" || value == "1") return true;
    if (value == "off" || value == "false" || value == "0") return false;
    return {};
}
// mod-list.ini: <mod id>=on|off, first line = highest precedence.
std::vector<std::pair<std::string, bool>> ReadModList(Snapshot& snapshot) {
    std::vector<std::pair<std::string, bool>> list;
    std::error_code ec;
    if (snapshot.modList.empty() || !std::filesystem::is_regular_file(snapshot.modList, ec)) return list;
    std::string bytes, error;
    if (!ReadBounded(snapshot.modList, bytes, error)) { Diagnose(snapshot, snapshot.modList, 0, "mod list: " + error); return list; }
    std::set<std::string> seen;
    for (const auto& [n, text] : Lines(bytes)) {
        const auto equal = text.find('=');
        const auto id = Trim(std::string_view(text).substr(0, equal));
        const auto on = equal == text.npos ? std::nullopt : Switch(Trim(std::string_view(text).substr(equal + 1)));
        if (!on || !ModIdValid(id)) { Diagnose(snapshot, snapshot.modList, n, "expected <mod id>=on or <mod id>=off"); continue; }
        if (!seen.emplace(id).second) { Diagnose(snapshot, snapshot.modList, n, "mod listed twice; the first line counts: " + std::string(id)); continue; }
        list.emplace_back(id, *on);
    }
    return list;
}
void LoadLanguagePack(Snapshot& snapshot, const std::filesystem::path& file, std::set<std::string>& ids) {
    if (!ContainedFile(snapshot.root, file)) return;
    std::error_code ec;
    const auto size = std::filesystem::file_size(file, ec);
    if (ec || size > 4096) { Diagnose(snapshot, file, 0, "language.ini exceeds 4 KiB or cannot be read"); return; }
    std::ifstream input(file, std::ios::binary);
    std::map<std::string, std::string, std::less<>> values;
    std::string line;
    size_t lineNo = 0;
    while (std::getline(input, line)) {
        ++lineNo;
        if (lineNo == 1 && line.compare(0, 3, "\xef\xbb\xbf") == 0) line.erase(0, 3);
        const auto text = Trim(line);
        if (text.empty() || text.front() == '#' || text.front() == ';') continue;
        const auto equal = text.find('=');
        const auto key = equal == text.npos ? std::string_view{} : Trim(text.substr(0, equal));
        if ((key != "id" && key != "name" && key != "base") ||
            !values.emplace(std::string(key), std::string(Trim(text.substr(equal + 1)))).second) {
            Diagnose(snapshot, file, lineNo, "expected one each of id=, name= and base="); return;
        }
    }
    LanguagePack pack{values["id"], values["name"], values["base"], file.parent_path()};
    for (auto* value : {&pack.id, &pack.base})
        for (char& c : *value) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    const bool base = pack.base.size() == 3 &&
        std::all_of(pack.base.begin(), pack.base.end(), [](char c) { return c >= 'a' && c <= 'z'; });
    if (!ModIdValid(pack.id) || !SafeText(pack.name) || pack.name.size() > 64 || !base) {
        Diagnose(snapshot, file, 0, "language.ini needs an id (letters, digits, - _ .), a name of up to 64 bytes and a base such as int");
        return;
    }
    if (!ids.insert(pack.id).second) { Diagnose(snapshot, file, 0, "language pack id already used: " + pack.id); return; }
    snapshot.languagePacks.push_back(std::move(pack));
}
// Translation JSON under a text folder; the member path is the relative path without .json.
void ListTextFolder(const std::filesystem::path& folder, const std::string& modId, std::map<std::string, ResolvedAsset>& found) {
    std::error_code ec;
    std::filesystem::recursive_directory_iterator it(folder, ec), end;
    for (; !ec && it != end; it.increment(ec)) {
        std::error_code entryError;
        if (!it->is_regular_file(entryError)) continue;
        auto relative = Utf8(it->path().lexically_relative(folder));
        if (relative.size() <= 5 || relative.compare(relative.size() - 5, 5, ".json") != 0) continue;
        const auto key = CanonicalTextKey(std::string_view(relative).substr(0, relative.size() - 5));
        if (!key.empty() && ContainedFile(folder, it->path()))
            found.emplace(key, ResolvedAsset{{AssetKind::Text, key}, it->path(), modId, 0});
    }
}
void Scan(Snapshot& snapshot) {
    std::error_code ec;
    if (snapshot.mode != ResolutionMode::Standalone)
        for (std::filesystem::directory_iterator it(snapshot.root / "overlay", ec), end; !ec && it != end; it.increment(ec)) {
            std::error_code entryError;
            if (const auto kind = FolderKind(Lower(Utf8(it->path().filename()))); kind && it->is_directory(entryError))
                snapshot.overlayKinds |= KindBit(*kind);
        }
    ec.clear();
    if (!std::filesystem::is_directory(snapshot.root, ec)) return;
    std::vector<std::filesystem::path> dirs;
    std::filesystem::directory_iterator it(snapshot.root, ec), end;
    for (; !ec && it != end; it.increment(ec)) {
        const auto name = Utf8(it->path().filename());
        std::error_code entryError;
        if (name != "overlay" && !name.empty() && name.front() != '.' && it->is_directory(entryError)) dirs.push_back(it->path());
    }
    if (ec) Diagnose(snapshot, snapshot.root, 0, "directory scan incomplete: " + ec.message());
    std::sort(dirs.begin(), dirs.end(), [](const auto& a, const auto& b) { return Utf8(a.filename()) < Utf8(b.filename()); });
    std::vector<Loaded> mods;
    std::set<std::string> languages;
    for (const auto& dir : dirs) {
        try { if (auto loaded = LoadManifest(snapshot, dir)) mods.push_back(std::move(*loaded)); }
        catch (const std::exception& e) { Diagnose(snapshot, dir / "mod.ini", 0, e.what()); }
        // A pack applies only when the player selects it, so no load
        // order is involved and every mode lists it.
        try { LoadLanguagePack(snapshot, dir / "language.ini", languages); }
        catch (const std::exception& e) { Diagnose(snapshot, dir / "language.ini", 0, e.what()); }
    }
    // Enabled duplicate ids: the lexically later folder is rejected.
    std::set<std::string> ids;
    for (auto& l : mods)
        if (l.info.apiVersion && l.info.manifestEnabled && !ids.insert(l.info.id).second) {
            l.duplicate = true;
            Problem(snapshot, l.info, l.info.folder / "mod.ini", 0, "duplicate mod id: " + l.info.id);
        }
    const auto list = ReadModList(snapshot);
    std::unordered_map<std::string, size_t> position;
    for (size_t i = 0; i != list.size(); ++i) position.emplace(list[i].first, i);
    for (auto& l : mods)
        if (const auto found = position.find(l.info.id); found != position.end()) {
            l.rank = found->second;
            l.info.listEnabled = list[found->second].second;
        }
    // Listed mods in list order, then the rest by priority; ties: later folder first.
    std::sort(mods.begin(), mods.end(), [](const Loaded& a, const Loaded& b) {
        if (a.rank != b.rank) return a.rank < b.rank;
        if (a.info.priority != b.info.priority) return a.info.priority > b.info.priority;
        return a.dirName > b.dirName;
    });
    snapshot.mods.reserve(mods.size());
    for (auto& l : mods) {
        auto& info = l.info;
        info.active = snapshot.mode != ResolutionMode::Overlay && info.apiVersion && info.manifestEnabled &&
            info.listEnabled && !l.duplicate;
        if (info.active) {
            try { AddModFiles(snapshot, l, static_cast<uint32_t>(snapshot.mods.size())); }
            catch (const std::exception& e) { Problem(snapshot, info, info.folder, 0, e.what()); }
            snapshot.modIds.push_back(info.id);
        }
        snapshot.mods.push_back(std::move(info));
    }
    std::sort(snapshot.modIds.begin(), snapshot.modIds.end());
}
std::shared_ptr<const Snapshot> Current() { std::lock_guard lock(gMutex); return gSnapshot; }
}

std::string MakeManifestKey(std::string_view package, uint32_t exportIndex, std::string_view object) {
    if (!SafeText(package) || !SafeText(object) || package.find_first_of("#=") != package.npos ||
        object.find_first_of("#:=/\\") != object.npos || Trim(package) != package || Trim(object) != object) return {};
    const auto relative = Relative(package);
    if (!relative || *relative == ".") return {};
    auto path = Utf8(*relative);
    if (path.empty() || path.back() == '/') return {};
    path = Lower(std::move(path));
    const auto key = path + "#" + std::to_string(exportIndex) + ":" + std::string(object);
    return key.size() <= 4096 ? key : std::string{};
}
std::filesystem::path OverlayRelativePath(const AssetId& id) {
    if (!KindValid(id.kind)) return {};
    const auto key = CanonicalKey(id.kind, id.key);
    if (key.empty()) return {};
    return FromUtf8(OverlayKey(id.kind, key)).make_preferred();
}
void Initialize(const std::filesystem::path& requestedRoot, const std::filesystem::path& modList) {
    auto snapshot = std::make_shared<Snapshot>();
    snapshot->defaultRoot = requestedRoot;
    snapshot->modList = modList;
    try {
        auto root = requestedRoot;
        if (const auto* overrideRoot = std::getenv("LO_MODS_DIR"); overrideRoot && *overrideRoot)
            root = FromUtf8(overrideRoot);
        std::error_code ec;
        snapshot->root = std::filesystem::absolute(root, ec).lexically_normal();
        snapshot->enabled = !ec && !root.empty();
        if (const auto* mode = std::getenv("LO_MODS_MODE"); mode && *mode) {
            const std::string_view value(mode);
            if (value == "overlay") snapshot->mode = ResolutionMode::Overlay;
            else if (value == "standalone") snapshot->mode = ResolutionMode::Standalone;
            else if (value != "combined") {
                snapshot->enabled = false;
                Diagnose(*snapshot, snapshot->root, 0, "LO_MODS_MODE must be combined, standalone or overlay");
            }
        }
        if (const auto* flag = std::getenv("LO_MODS"); flag && (std::string_view(flag) == "0" || std::string_view(flag) == "false"))
            snapshot->enabled = false;
        if (snapshot->enabled) Scan(*snapshot);
    } catch (const std::exception& e) { snapshot->enabled = false; Diagnose(*snapshot, requestedRoot, 0, e.what()); }
    std::lock_guard lock(gMutex);
    gSnapshot = std::move(snapshot);
    ++gGeneration;
}
void Reload() {
    std::filesystem::path root, modList;
    { std::lock_guard lock(gMutex); root = gSnapshot->defaultRoot; modList = gSnapshot->modList; }
    Initialize(root, modList);
}
void Shutdown() {
    // Release providers outside the mutex: their destructors may call this API.
    std::map<AssetKind, std::shared_ptr<AssetProvider>> retired;
    {
        std::lock_guard lock(gMutex);
        gSnapshot = std::make_shared<Snapshot>();
        retired.swap(gProviders);
        ++gGeneration;
    }
}
uint64_t Generation() { std::lock_guard lock(gMutex); return gGeneration; }
std::filesystem::path Root() { return Current()->root; }
ResolutionMode Mode() { return Current()->mode; }
std::vector<Diagnostic> Diagnostics() { return Current()->diagnostics; }
std::vector<std::string> ModIds() { return Current()->modIds; }
std::vector<ModInfo> ListMods() { return Current()->mods; }
std::filesystem::path ModListPath() { return Current()->modList; }
bool Enabled() { return Current()->enabled; }
bool SaveModList(const std::vector<std::pair<std::string, bool>>& order, std::string* error) {
    auto fail = [&](std::string message) { if (error) *error = std::move(message); return false; };
    const auto path = ModListPath();
    if (path.empty()) return fail("no mod list path");
    std::unordered_map<std::string, size_t> index;
    for (size_t i = 0; i != order.size(); ++i) {
        if (!ModIdValid(order[i].first)) return fail("invalid mod id: " + order[i].first);
        if (!index.emplace(order[i].first, i).second) return fail("mod listed twice: " + order[i].first);
    }
    std::lock_guard lock(gSaveMutex);
    try {
        // Lines for ids order leaves out (and comments) follow the listed id
        // that preceded them; kept[0] holds the lines before the first one.
        std::vector<std::vector<std::string>> kept(order.size() + 1);
        std::error_code ec;
        const bool existed = std::filesystem::exists(path, ec);
        if (existed) {
            std::string bytes, readError;
            if (!ReadBounded(path, bytes, readError)) return fail("cannot read " + Utf8(path) + ": " + readError);
            size_t slot = 0;
            for (auto& line : RawLines(bytes)) {
                const auto text = Trim(line);
                if (text.empty()) continue;
                const bool comment = text.front() == '#' || text.front() == ';';
                if (const auto found = comment ? index.end() : index.find(std::string(Trim(text.substr(0, text.find('='))))); found != index.end())
                    slot = found->second + 1;
                else
                    kept[slot].push_back(std::move(line));
            }
        }
        std::string out = existed ? std::string{} :
            "# Mod order for LostOdysseyRecomp, highest priority first: <mod id>=on or off.\n"
            "# Mods that are not listed are on and come after the listed ones.\n";
        for (size_t i = 0; i <= order.size(); ++i) {
            if (i) out += order[i - 1].first + (order[i - 1].second ? "=on\n" : "=off\n");
            for (const auto& line : kept[i]) out += line + "\n";
        }
        if (out.size() > kMaxManifestBytes) return fail("mod list exceeds 1 MiB");
        if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ec);
        auto temp = path;
        temp += ".tmp";
        {
            std::ofstream file(temp, std::ios::binary | std::ios::trunc);
            file.write(out.data(), static_cast<std::streamsize>(out.size()));
            file.close();
            if (!file) { std::filesystem::remove(temp, ec); return fail("cannot write " + Utf8(temp)); }
        }
        std::filesystem::rename(temp, path, ec);
        if (ec) { std::error_code ignored; std::filesystem::remove(temp, ignored); return fail("cannot replace " + Utf8(path) + ": " + ec.message()); }
        return true;
    } catch (const std::exception& e) { return fail(e.what()); }
}
bool HasTextureReplacements() {
    std::lock_guard lock(gMutex);
    const auto& s = *gSnapshot;
    return s.enabled && ((s.overlayKinds & KindBit(AssetKind::Texture)) || (s.mode != ResolutionMode::Overlay &&
        (s.textureEntries || gProviders.count(AssetKind::Texture))));
}
std::vector<ResolvedAsset> ListTexts() {
    const auto snapshot = Current();
    std::map<std::string, ResolvedAsset> found;
    if (!snapshot->enabled) return {};
    if (snapshot->mode != ResolutionMode::Standalone && (snapshot->overlayKinds & KindBit(AssetKind::Text)))
        ListTextFolder(snapshot->root / "overlay" / "text", "@overlay", found);
    for (const auto& [name, e] : snapshot->entries)
        if (e.id.kind == AssetKind::Text) {
            const auto& mod = snapshot->mods[e.mod];
            found.emplace(e.id.key, ResolvedAsset{e.id, e.file, mod.id, mod.priority});
        }
    std::vector<ResolvedAsset> result;
    for (auto& [key, asset] : found) result.push_back(std::move(asset));
    return result;
}
std::vector<LanguagePack> LanguagePacks() {
    std::lock_guard lock(gMutex);
    return gSnapshot->enabled ? gSnapshot->languagePacks : std::vector<LanguagePack>{};
}
std::vector<ResolvedAsset> ListTexts(const LanguagePack& pack) {
    const auto snapshot = Current();
    std::map<std::string, ResolvedAsset> found;
    if (!snapshot->enabled || !ContainedFile(snapshot->root, pack.folder / "language.ini")) return {};
    ListTextFolder(pack.folder / "text", pack.id, found);
    std::vector<ResolvedAsset> result;
    for (auto& [key, asset] : found) result.push_back(std::move(asset));
    return result;
}
std::optional<ResolvedAsset> Resolve(const AssetRequest& request) {
    if (!KindValid(request.id.kind)) return {};
    auto key = CanonicalKey(request.id.kind, request.id.key);
    if (key.empty()) return {};
    const AssetRequest normalized{{request.id.kind, std::move(key)}, request.originalPath};
    std::shared_ptr<const Snapshot> snapshot;
    std::shared_ptr<AssetProvider> provider;
    {
        std::lock_guard lock(gMutex);
        snapshot = gSnapshot;
        if (!gInProvider && snapshot->mode != ResolutionMode::Overlay)
            if (const auto it = gProviders.find(request.id.kind); it != gProviders.end()) provider = it->second;
    }
    if (!snapshot->enabled) return {};
    const auto name = OverlayKey(normalized.id.kind, normalized.id.key);
    // The top-level overlay is merged by a manager (MO2) and probed per request,
    // only for kinds whose folder existed at Initialize. The name is canonical
    // (relative, no '..'), so the file is inside overlay/ as written.
    if (snapshot->mode != ResolutionMode::Standalone && (snapshot->overlayKinds & KindBit(normalized.id.kind))) {
        auto overlay = (snapshot->root / FromUtf8(name)).make_preferred();
        std::error_code ec;
        if (std::filesystem::is_regular_file(overlay, ec) && !ec)
            return ResolvedAsset{normalized.id, std::move(overlay), "@overlay", 0};
    }
    if (snapshot->mode == ResolutionMode::Overlay) return {};
    if (provider) {
        struct Guard { Guard() { gInProvider = true; } ~Guard() { gInProvider = false; } } guard;
        try {
            auto result = provider->Resolve(normalized);
            std::error_code ec;
            if (result && result->id.kind == normalized.id.kind && CanonicalKey(result->id.kind, result->id.key) == normalized.id.key &&
                std::filesystem::is_regular_file(result->path, ec) && !ec) { result->id = normalized.id; return result; }
        } catch (...) { /* Optional providers must not prevent vanilla loading. */ }
    }
    // Mod folder files were found at Initialize; a file removed since then
    // fails in the reader, which keeps the original.
    if (const auto it = snapshot->entries.find(name); it != snapshot->entries.end()) {
        const auto& mod = snapshot->mods[it->second.mod];
        return ResolvedAsset{normalized.id, it->second.file, mod.id, mod.priority};
    }
    return {};
}
bool RegisterProvider(AssetKind kind, std::shared_ptr<AssetProvider> provider) {
    if (!KindValid(kind) || !provider) return false;
    std::lock_guard lock(gMutex);
    const bool inserted = gProviders.emplace(kind, provider).second;
    if (inserted) ++gGeneration;
    return inserted;
}
void UnregisterProvider(AssetKind kind, const AssetProvider* provider) {
    std::shared_ptr<AssetProvider> retired;
    {
        std::lock_guard lock(gMutex);
        const auto it = gProviders.find(kind);
        if (it != gProviders.end() && it->second.get() == provider) {
            retired = std::move(it->second); gProviders.erase(it); ++gGeneration;
        }
    }
}
}
