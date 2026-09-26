#include "mod_api.h"

#include <algorithm>
#include <fstream>
#include <mutex>

namespace modding {
namespace {
struct Entry {
    AssetKind kind = AssetKind::Image;
    std::string key;
    std::filesystem::path file;
    std::string modId;
    int32_t priority = 0;
};

std::mutex gMutex;
std::vector<Entry> gEntries;
std::unordered_map<uint32_t, AssetProvider*> gProviders;

AssetKind ParseKind(std::string_view s) {
    if (s == "font") return AssetKind::Font;
    if (s == "model") return AssetKind::Model;
    if (s == "movie") return AssetKind::Movie;
    return AssetKind::Image;
}

void LoadManifest(const std::filesystem::path& manifest) {
    std::ifstream in(manifest);
    if (!in) return;
    std::string modId = manifest.parent_path().filename().string();
    int32_t priority = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line.rfind("id=", 0) == 0) { modId = line.substr(3); continue; }
        if (line.rfind("priority=", 0) == 0) {
            try { priority = std::stoi(line.substr(9)); } catch (...) { priority = 0; }
            continue;
        }
        const auto colon = line.find(':');
        const auto equals = line.find('=');
        if (colon == std::string::npos || equals == std::string::npos || colon >= equals) continue;
        Entry e;
        e.kind = ParseKind(std::string_view(line).substr(0, colon));
        e.key = line.substr(colon + 1, equals - colon - 1);
        e.file = manifest.parent_path() / std::filesystem::u8path(line.substr(equals + 1));
        e.modId = modId;
        e.priority = priority;
        if (!e.key.empty() && std::filesystem::is_regular_file(e.file))
            gEntries.push_back(std::move(e));
    }
}
}

std::string MakeManifestKey(std::string_view package, uint32_t exportIndex, std::string_view object) {
    std::string key(package);
    std::replace(key.begin(), key.end(), '\\', '/');
    while (!key.empty() && key.front() == '/') key.erase(key.begin());
    key += "#";
    key += std::to_string(exportIndex);
    key += ":";
    key.append(object);
    return key;
}

void Initialize(const std::filesystem::path& root) {
    std::lock_guard lock(gMutex);
    gEntries.clear();
    std::error_code ec;
    if (!std::filesystem::is_directory(root, ec)) return;
    for (const auto& dir : std::filesystem::directory_iterator(root, ec)) {
        if (ec) break;
        if (!dir.is_directory()) continue;
        const auto manifest = dir.path() / "mod.ini";
        if (std::filesystem::is_regular_file(manifest, ec)) LoadManifest(manifest);
    }
    std::stable_sort(gEntries.begin(), gEntries.end(),
        [](const Entry& a, const Entry& b) { return a.priority < b.priority; });
}

void Shutdown() {
    std::lock_guard lock(gMutex);
    gEntries.clear();
    gProviders.clear();
}

std::optional<ResolvedAsset> Resolve(const AssetRequest& request) {
    std::lock_guard lock(gMutex);
    if (auto p = gProviders.find(static_cast<uint32_t>(request.id.kind)); p != gProviders.end() && p->second)
        if (auto r = p->second->Resolve(request)) return r;
    for (auto it = gEntries.rbegin(); it != gEntries.rend(); ++it)
        if (it->kind == request.id.kind && it->key == request.id.key)
            return ResolvedAsset{request.id, it->file, it->modId, it->priority};
    return std::nullopt;
}

bool RegisterProvider(AssetKind kind, AssetProvider* provider) {
    if (!provider) return false;
    std::lock_guard lock(gMutex);
    return gProviders.emplace(static_cast<uint32_t>(kind), provider).second;
}

void UnregisterProvider(AssetKind kind, AssetProvider* provider) {
    std::lock_guard lock(gMutex);
    auto it = gProviders.find(static_cast<uint32_t>(kind));
    if (it != gProviders.end() && it->second == provider) gProviders.erase(it);
}
}
