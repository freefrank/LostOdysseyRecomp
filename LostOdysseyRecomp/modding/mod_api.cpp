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

namespace modding {
namespace {
struct Entry {
    AssetId id;
    std::filesystem::path base, file;
    std::string modId;
    int32_t priority = 0;
};
struct Snapshot {
    std::filesystem::path root, defaultRoot;
    std::map<std::string, Entry> entries;
    std::vector<Diagnostic> diagnostics;
    ResolutionMode mode = ResolutionMode::Combined;
    bool enabled = false;
};
std::mutex gMutex;
std::shared_ptr<const Snapshot> gSnapshot = std::make_shared<Snapshot>();
std::map<AssetKind, std::shared_ptr<AssetProvider>> gProviders;
uint64_t gGeneration = 0;
thread_local bool gInProvider = false;
constexpr size_t kMaxManifestBytes = 1024 * 1024;

std::string Utf8(const std::filesystem::path& path) {
    const auto text = path.generic_u8string();
    return {text.begin(), text.end()};
}
std::filesystem::path FromUtf8(std::string_view text) {
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}
std::string_view Trim(std::string_view s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == s.npos) return {};
    return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}
bool KindValid(AssetKind kind) {
    return kind >= AssetKind::Image && kind <= AssetKind::Movie;
}
std::optional<AssetKind> ParseKind(std::string_view s) {
    if (s == "image") return AssetKind::Image;
    if (s == "font") return AssetKind::Font;
    if (s == "model") return AssetKind::Model;
    if (s == "movie") return AssetKind::Movie;
    return {};
}
template<class T> bool Number(std::string_view s, T& value) {
    if (s.empty()) return false;
    const auto result = std::from_chars(s.data(), s.data() + s.size(), value);
    return result.ec == std::errc{} && result.ptr == s.data() + s.size();
}
bool SafeText(std::string_view s) {
    return !s.empty() && s.size() <= 4096 &&
        std::none_of(s.begin(), s.end(), [](unsigned char c) { return c < 32 || c == 127; });
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
bool ContainedFile(const std::filesystem::path& root, const std::filesystem::path& file) {
    std::error_code ec;
    const auto base = std::filesystem::weakly_canonical(root, ec);
    if (ec || base.empty()) return false;
    const auto path = std::filesystem::weakly_canonical(file, ec);
    if (ec) return false;
    auto b = base.begin(), p = path.begin();
    for (; b != base.end(); ++b, ++p) if (p == path.end() || *b != *p) return false;
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
std::string LookupKey(const AssetId& id) {
    return std::to_string(static_cast<uint32_t>(id.kind)) + ":" + id.key;
}
void Diagnose(Snapshot& snapshot, const std::filesystem::path& file, size_t line, std::string message) {
    if (snapshot.diagnostics.size() < 256)
        snapshot.diagnostics.push_back({file, line, std::move(message)});
}
bool ModIdValid(std::string_view id) {
    return !id.empty() && id.size() <= 128 && id != "." && id != ".." &&
        std::all_of(id.begin(), id.end(), [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
        });
}
void LoadManifest(Snapshot& snapshot, const std::filesystem::path& manifest, std::set<std::string>& ids) {
    if (!ContainedFile(snapshot.root, manifest)) return;
    std::error_code ec;
    const auto size = std::filesystem::file_size(manifest, ec);
    if (ec || size > kMaxManifestBytes) { Diagnose(snapshot, manifest, 0, "manifest exceeds 1 MiB or cannot be read"); return; }
    // Bound the read itself, not just getline after allocation. Reject files
    // changed between stat/open/read rather than allocating an unbounded line.
    std::ifstream file(manifest, std::ios::binary);
    if (!file) { Diagnose(snapshot, manifest, 0, "cannot open manifest"); return; }
    std::string bytes(static_cast<size_t>(size), '\0');
    if (!file.read(bytes.data(), static_cast<std::streamsize>(bytes.size())) ||
        file.peek() != std::char_traits<char>::eof() || file.bad()) {
        Diagnose(snapshot, manifest, 0, "manifest changed or could not be read"); return;
    }
    std::istringstream input(std::move(bytes));
    std::vector<std::pair<size_t, std::string>> lines;
    std::string line;
    size_t lineNo = 0, consumed = 0;
    while (std::getline(input, line)) {
        ++lineNo;
        consumed += line.size() + 1;
        if (consumed > kMaxManifestBytes + 1) { Diagnose(snapshot, manifest, lineNo, "manifest grew beyond 1 MiB"); return; }
        if (lineNo == 1 && line.compare(0, 3, "\xef\xbb\xbf") == 0) line.erase(0, 3);
        auto text = Trim(line);
        if (!text.empty() && text.front() != '#' && text.front() != ';') lines.emplace_back(lineNo, text);
    }
    if (!input.eof()) { Diagnose(snapshot, manifest, lineNo, "manifest read failed"); return; }
    std::string id = Utf8(manifest.parent_path().filename());
    int32_t priority = 0;
    bool enabled = true;
    std::set<std::string> metadata;
    // Parse metadata first: trailing priority/enabled fields affect every entry.
    for (const auto& [n, text] : lines) {
        const auto equal = text.find('=');
        if (equal == text.npos) { Diagnose(snapshot, manifest, n, "expected key=value"); return; }
        const auto key = Trim(std::string_view(text).substr(0, equal));
        if (key.find(':') != key.npos) continue;
        const auto value = Trim(std::string_view(text).substr(equal + 1));
        bool valid = metadata.insert(std::string(key)).second;
        if (key == "id") { id = value; valid = valid && ModIdValid(id); }
        else if (key == "priority") valid = valid && Number(value, priority);
        else if (key == "enabled") {
            valid = valid && (value == "true" || value == "false" || value == "1" || value == "0");
            enabled = value == "true" || value == "1";
        } else if (key == "api_version") { uint32_t version = 0; valid = valid && Number(value, version) && version == kModApiVersion; }
        else valid = false;
        if (!valid) { Diagnose(snapshot, manifest, n, "unknown, duplicate or invalid metadata: " + std::string(key)); return; }
    }
    if (!metadata.count("api_version")) { Diagnose(snapshot, manifest, 0, "api_version=1 is required"); return; }
    if (!enabled) return;
    if (!ModIdValid(id) || !ids.insert(id).second) { Diagnose(snapshot, manifest, 0, "invalid or duplicate mod id: " + id); return; }
    for (const auto& [n, text] : lines) {
        const auto equal = text.find('=');
        const auto left = Trim(std::string_view(text).substr(0, equal));
        const auto colon = left.find(':');
        if (colon == left.npos) continue;
        const auto kind = ParseKind(Trim(left.substr(0, colon)));
        const auto key = CanonicalKey(Trim(left.substr(colon + 1)));
        const auto relative = Relative(Trim(std::string_view(text).substr(equal + 1)));
        if (!kind || key.empty() || !relative) { Diagnose(snapshot, manifest, n, "invalid asset kind, identity or relative path"); continue; }
        Entry entry{{*kind, key}, manifest.parent_path(), manifest.parent_path() / *relative, id, priority};
        if (!ContainedFile(entry.base, entry.file) || !ContainedFile(snapshot.root, entry.file)) {
            Diagnose(snapshot, manifest, n, "replacement is missing or escapes the mod directory"); continue;
        }
        const auto lookup = LookupKey(entry.id);
        const auto it = snapshot.entries.find(lookup);
        // Sorted directories: later directory wins ties, last entry wins in a file.
        if (it == snapshot.entries.end() || priority >= it->second.priority)
            snapshot.entries.insert_or_assign(lookup, std::move(entry));
    }
}
}

std::string MakeManifestKey(std::string_view package, uint32_t exportIndex, std::string_view object) {
    if (!SafeText(package) || !SafeText(object) || package.find_first_of("#=") != package.npos ||
        object.find_first_of("#:=/\\") != object.npos || Trim(package) != package || Trim(object) != object) return {};
    const auto relative = Relative(package);
    if (!relative || *relative == ".") return {};
    auto path = Utf8(*relative);
    if (path.empty() || path.back() == '/') return {};
    for (char& c : path) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    const auto key = path + "#" + std::to_string(exportIndex) + ":" + std::string(object);
    return key.size() <= 4096 ? key : std::string{};
}
std::filesystem::path OverlayRelativePath(const AssetId& id) {
    if (!KindValid(id.kind)) return {};
    const auto key = CanonicalKey(id.key);
    if (key.empty()) return {};
    uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : key) { hash ^= c; hash *= 1099511628211ull; }
    char name[40];
    std::snprintf(name, sizeof(name), "key-fnv1a64-%016llx", static_cast<unsigned long long>(hash));
    constexpr const char* folders[] = {"", "images", "fonts", "models", "movies"};
    return std::filesystem::path("overlay") / folders[static_cast<uint32_t>(id.kind)] /
        (std::string(name) + (id.kind == AssetKind::Image ? ".lotex" : ".loasset"));
}
void Initialize(const std::filesystem::path& requestedRoot) {
    auto snapshot = std::make_shared<Snapshot>();
    snapshot->defaultRoot = requestedRoot;
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
        if (snapshot->enabled && snapshot->mode != ResolutionMode::Overlay && std::filesystem::is_directory(snapshot->root, ec)) {
            std::vector<std::filesystem::path> dirs;
            std::filesystem::directory_iterator it(snapshot->root, ec), end;
            for (; !ec && it != end; it.increment(ec)) {
                const auto name = Utf8(it->path().filename());
                std::error_code entryError;
                if (name != "overlay" && !name.empty() && name.front() != '.' && it->is_directory(entryError)) dirs.push_back(it->path());
            }
            if (ec) Diagnose(*snapshot, snapshot->root, 0, "directory scan incomplete: " + ec.message());
            std::sort(dirs.begin(), dirs.end(), [](const auto& a, const auto& b) { return Utf8(a.filename()) < Utf8(b.filename()); });
            std::set<std::string> ids;
            for (const auto& dir : dirs) {
                try { LoadManifest(*snapshot, dir / "mod.ini", ids); }
                catch (const std::exception& e) { Diagnose(*snapshot, dir / "mod.ini", 0, e.what()); }
            }
        }
    } catch (const std::exception& e) { snapshot->enabled = false; Diagnose(*snapshot, requestedRoot, 0, e.what()); }
    for (const auto& d : snapshot->diagnostics)
        std::fprintf(stderr, "[mods] %s:%zu: %s\n", Utf8(d.manifest).c_str(), d.line, d.message.c_str());
    std::lock_guard lock(gMutex);
    gSnapshot = std::move(snapshot);
    ++gGeneration;
}
void Reload() {
    std::filesystem::path root;
    { std::lock_guard lock(gMutex); root = gSnapshot->defaultRoot; }
    Initialize(root);
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
std::filesystem::path Root() { std::lock_guard lock(gMutex); return gSnapshot->root; }
ResolutionMode Mode() { std::lock_guard lock(gMutex); return gSnapshot->mode; }
std::vector<Diagnostic> Diagnostics() { std::lock_guard lock(gMutex); return gSnapshot->diagnostics; }
std::optional<ResolvedAsset> Resolve(const AssetRequest& request) {
    if (!KindValid(request.id.kind)) return {};
    const auto key = CanonicalKey(request.id.key);
    if (key.empty()) return {};
    const AssetRequest normalized{{request.id.kind, key}, request.originalPath};
    std::shared_ptr<const Snapshot> snapshot;
    std::shared_ptr<AssetProvider> provider;
    {
        std::lock_guard lock(gMutex);
        snapshot = gSnapshot;
        if (!gInProvider && snapshot->mode != ResolutionMode::Overlay)
            if (const auto it = gProviders.find(request.id.kind); it != gProviders.end()) provider = it->second;
    }
    if (!snapshot->enabled) return {};
    if (snapshot->mode != ResolutionMode::Standalone) {
        const auto overlay = snapshot->root / OverlayRelativePath(normalized.id);
        if (ContainedFile(snapshot->root / "overlay", overlay) && ContainedFile(snapshot->root, overlay))
            return ResolvedAsset{normalized.id, overlay, "@overlay", 0};
    }
    if (snapshot->mode == ResolutionMode::Overlay) return {};
    if (provider) {
        struct Guard { Guard() { gInProvider = true; } ~Guard() { gInProvider = false; } } guard;
        try {
            auto result = provider->Resolve(normalized);
            std::error_code ec;
            if (result && result->id.kind == normalized.id.kind && CanonicalKey(result->id.key) == normalized.id.key &&
                std::filesystem::is_regular_file(result->path, ec) && !ec) { result->id = normalized.id; return result; }
        } catch (...) { /* Optional providers must not prevent vanilla loading. */ }
    }
    if (const auto it = snapshot->entries.find(LookupKey(normalized.id)); it != snapshot->entries.end()) {
        const auto& e = it->second;
        if (ContainedFile(snapshot->root, e.file) && ContainedFile(e.base, e.file))
            return ResolvedAsset{normalized.id, e.file, e.modId, e.priority};
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
