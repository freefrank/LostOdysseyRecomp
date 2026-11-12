#include "steam_shortcut.h"
#include "steam_artwork.h"
#include <os/platform.h>
#include <os/user_paths.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <system_error>

#if LO_PLATFORM_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <signal.h>
#include <unistd.h>
#endif
#if LO_PLATFORM_MACOS
#include <mach-o/dyld.h>
#endif

namespace steam_shortcut
{
namespace
{
namespace fs = std::filesystem;
constexpr std::string_view AppName = "Lost Odyssey Recomp";
constexpr std::string_view AppId = "io.github.freefrank.LostOdysseyRecomp";
constexpr std::string_view Marker = "LostOdysseyRecomp";
constexpr size_t MaxFileSize = 64 * 1024 * 1024;

std::string Utf8(const fs::path& path)
{
    const auto value = path.u8string();
    return {value.begin(), value.end()};
}
std::string Quote(std::string value)
{
    if (value.find_first_of("\"\r\n") != std::string::npos || value.find('\0') != std::string::npos)
        throw std::runtime_error("The launch path contains a quote or a control character");
    return '"' + value + '"';
}
bool EqualKey(std::string_view a, std::string_view b)
{
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](unsigned char x, unsigned char y) {
        return (x >= 'A' && x <= 'Z' ? x + 32 : x) == (y >= 'A' && y <= 'Z' ? y + 32 : y);
    });
}
const VdfNode* Find(const std::vector<VdfNode>& nodes, std::string_view key)
{
    const auto it = std::find_if(nodes.begin(), nodes.end(), [&](const auto& n) { return EqualKey(n.key, key); });
    return it == nodes.end() ? nullptr : &*it;
}
VdfNode* Find(std::vector<VdfNode>& nodes, std::string_view key)
{
    const auto it = std::find_if(nodes.begin(), nodes.end(), [&](const auto& n) { return EqualKey(n.key, key); });
    return it == nodes.end() ? nullptr : &*it;
}
std::string String(const VdfNode* node)
{
    return node && node->type == 1 && !node->value.empty()
        ? std::string(node->value.begin(), node->value.end() - 1) : std::string{};
}
VdfNode StringNode(std::string key, std::string_view value)
{
    VdfNode node{1, std::move(key), {value.begin(), value.end()}, {}};
    node.value.push_back(0);
    return node;
}
VdfNode IntNode(std::string key, uint32_t value)
{
    return {2, std::move(key), {uint8_t(value), uint8_t(value >> 8), uint8_t(value >> 16), uint8_t(value >> 24)}, {}};
}
void Set(std::vector<VdfNode>& nodes, VdfNode value, bool onlyIfMissing = false)
{
    auto found = std::find_if(nodes.begin(), nodes.end(), [&](const auto& n) { return EqualKey(n.key, value.key); });
    if (found == nodes.end()) nodes.push_back(std::move(value));
    else if (!onlyIfMissing) {
        // Steam's key spelling varies. Preserve its spelling, replacing the
        // value in place and removing conflicting case variants of our field.
        value.key = found->key;
        *found = std::move(value);
        const auto key = found->key;
        nodes.erase(std::remove_if(std::next(found), nodes.end(), [&](const auto& n) { return EqualKey(n.key, key); }), nodes.end());
    }
}

class BinaryReader
{
    std::span<const uint8_t> bytes;
    size_t offset = 0;
    size_t nodes = 0;
    void Need(size_t size) const
    {
        if (size > bytes.size() - offset) throw std::runtime_error("Truncated binary VDF");
    }
    size_t CStringSize(bool wide = false)
    {
        const size_t start = offset;
        for (;;) {
            Need(wide ? 2 : 1);
            const bool end = bytes[offset] == 0 && (!wide || bytes[offset + 1] == 0);
            offset += wide ? 2 : 1;
            if (end) return offset - start;
        }
    }
public:
    explicit BinaryReader(std::span<const uint8_t> data) : bytes(data) {}
    std::vector<VdfNode> Read(unsigned depth = 0)
    {
        if (depth > 64) throw std::runtime_error("Binary VDF nesting is too deep");
        std::vector<VdfNode> result;
        for (;;) {
            Need(1);
            const auto type = bytes[offset++];
            if (type == 8) return result;
            if (++nodes > 1000000) throw std::runtime_error("Binary VDF has too many fields");
            const auto keyStart = offset;
            const auto keyLength = CStringSize();
            VdfNode node{type, std::string(reinterpret_cast<const char*>(bytes.data() + keyStart), keyLength - 1), {}, {}};
            const auto valueStart = offset;
            size_t length = 0;
            switch (type) {
            case 0: node.children = Read(depth + 1); break;
            case 1: length = CStringSize(); break;
            case 5: length = CStringSize(true); break;
            case 2: case 3: case 4: case 6: length = 4; Need(length); offset += length; break;
            case 7: case 10: length = 8; Need(length); offset += length; break;
            default: throw std::runtime_error("Unsupported binary VDF type " + std::to_string(type));
            }
            if (type != 0) node.value.assign(bytes.begin() + valueStart, bytes.begin() + valueStart + length);
            result.push_back(std::move(node));
        }
    }
    bool Finished() const { return offset == bytes.size(); }
};
void WriteNodes(std::vector<uint8_t>& bytes, const std::vector<VdfNode>& nodes)
{
    for (const auto& node : nodes) {
        bytes.push_back(node.type);
        bytes.insert(bytes.end(), node.key.begin(), node.key.end());
        bytes.push_back(0);
        if (node.type == 0) WriteNodes(bytes, node.children);
        else bytes.insert(bytes.end(), node.value.begin(), node.value.end());
    }
    bytes.push_back(8);
}
std::vector<uint8_t> ReadFile(const fs::path& path)
{
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) throw std::runtime_error("Cannot read " + Utf8(path));
    const auto size = stream.tellg();
    if (size < 0 || uint64_t(size) > MaxFileSize) throw std::runtime_error("Invalid file size: " + Utf8(path));
    std::vector<uint8_t> result(static_cast<size_t>(size));
    stream.seekg(0);
    if (!result.empty() && !stream.read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(result.size())))
        throw std::runtime_error("Cannot read " + Utf8(path));
    return result;
}

// Text KeyValues is used only for loginusers.vdf. Read full nested objects,
// quoted escapes and // comments; do not confuse MostRecent in another object.
class TextReader
{
    std::string_view text;
    size_t offset = 0;
    std::string Token()
    {
        for (;;) {
            while (offset < text.size() && static_cast<unsigned char>(text[offset]) <= ' ') ++offset;
            if (offset + 1 < text.size() && text.substr(offset, 2) == "//") {
                while (offset < text.size() && text[offset] != '\n') ++offset;
            } else break;
        }
        if (offset == text.size()) return {};
        const char first = text[offset++];
        if (first == '{' || first == '}') return std::string(1, first);
        std::string result;
        if (first == '"') {
            while (offset < text.size()) {
                char c = text[offset++];
                if (c == '"') return result;
                if (c == '\\' && offset < text.size()) {
                    const char next = text[offset];
                    if (next == '\\' || next == '"') { c = next; ++offset; }
                }
                result += c;
            }
            throw std::runtime_error("Unterminated loginusers.vdf string");
        }
        result += first;
        while (offset < text.size() && static_cast<unsigned char>(text[offset]) > ' ' && text[offset] != '{' && text[offset] != '}')
            result += text[offset++];
        return result;
    }
public:
    explicit TextReader(std::string_view value) : text(value) {}
    VdfDocument Read(bool nested = false, unsigned depth = 0)
    {
        if (depth > 64) throw std::runtime_error("loginusers.vdf nesting is too deep");
        VdfDocument result;
        for (;;) {
            const auto key = Token();
            if (key.empty()) {
                if (nested) throw std::runtime_error("Truncated loginusers.vdf");
                return result;
            }
            if (key == "}") {
                if (!nested) throw std::runtime_error("Unexpected loginusers.vdf brace");
                return result;
            }
            if (key == "{") throw std::runtime_error("Unexpected loginusers.vdf brace");
            const auto value = Token();
            if (value == "{") result.push_back({0, key, {}, Read(true, depth + 1)});
            else if (value == "}") throw std::runtime_error("Missing loginusers.vdf value");
            else result.push_back(StringNode(key, value));
        }
    }
};
bool Number(std::string_view text, uint64_t& value)
{
    if (text.empty()) return false;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size();
}

void AtomicWrite(const fs::path& destination, std::span<const uint8_t> bytes)
{
    static std::atomic<unsigned> sequence{0};
#if LO_PLATFORM_WINDOWS
    const auto process = GetCurrentProcessId();
#else
    const auto process = getpid();
#endif
    auto temporary = destination;
    temporary += ".lo-" + std::to_string(process) + "-" + std::to_string(sequence++) + ".tmp";
    try {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) throw std::runtime_error("Cannot create " + Utf8(temporary));
        stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        stream.flush();
        if (!stream) throw std::runtime_error("Cannot write " + Utf8(temporary));
        stream.close();
        if (!stream) throw std::runtime_error("Cannot close " + Utf8(temporary));
#if LO_PLATFORM_WINDOWS
        if (!MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::system_error(GetLastError(), std::system_category(), "Cannot replace " + Utf8(destination));
#else
        fs::rename(temporary, destination);
#endif
    } catch (...) {
        std::error_code error;
        fs::remove(temporary, error);
        throw;
    }
}

#if LO_PLATFORM_WINDOWS
fs::path RegistrySteamPath()
{
    DWORD length = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", RRF_RT_REG_SZ, nullptr, nullptr, &length) != ERROR_SUCCESS)
        return {};
    std::wstring path(length / sizeof(wchar_t), L'\0');
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", RRF_RT_REG_SZ, nullptr, path.data(), &length) != ERROR_SUCCESS)
        return {};
    while (!path.empty() && path.back() == L'\0') path.pop_back();
    return path;
}
#endif
bool ProcessAlive(uint64_t pid)
{
    if (pid == 0 || pid > std::numeric_limits<int32_t>::max()) return false;
#if LO_PLATFORM_WINDOWS
    const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!process) return GetLastError() == ERROR_ACCESS_DENIED;
    DWORD exitCode = 0;
    const bool alive = GetExitCodeProcess(process, &exitCode) && exitCode == STILL_ACTIVE;
    CloseHandle(process);
    return alive;
#else
    return kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
#endif
}
bool SteamRunning(const Environment& environment, const SteamRoot& root)
{
#if LO_PLATFORM_WINDOWS
    if (environment.platform == Platform::Windows) {
        DWORD pid = 0, length = sizeof(pid);
        return RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam\\ActiveProcess", L"pid", RRF_RT_REG_DWORD,
            nullptr, &pid, &length) == ERROR_SUCCESS && ProcessAlive(pid);
    }
#endif
    const std::array candidates = {environment.home / ".steam/steam.pid", root.path / "steam.pid"};
    for (const auto& candidate : candidates) {
        std::ifstream stream(candidate);
        uint64_t pid = 0;
        if (stream >> pid && ProcessAlive(pid)) return true;
    }
    return false;
}
fs::path ExecutablePath()
{
#if LO_PLATFORM_WINDOWS
    std::wstring buffer(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length && length < buffer.size()) { buffer.resize(length); return buffer; }
#elif LO_PLATFORM_LINUX
    std::array<char, 4096> buffer{};
    const auto length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (length > 0) return fs::path(std::string(buffer.data(), static_cast<size_t>(length)));
#elif LO_PLATFORM_MACOS
    uint32_t length = 0;
    _NSGetExecutablePath(nullptr, &length);
    std::string buffer(length, '\0');
    if (_NSGetExecutablePath(buffer.data(), &length) == 0) return fs::canonical(buffer.c_str());
#endif
    throw std::runtime_error("Cannot find the current executable");
}
}

VdfDocument ParseVdf(std::span<const uint8_t> bytes)
{
    if (bytes.size() > MaxFileSize) throw std::runtime_error("Binary VDF is too large");
    BinaryReader reader(bytes);
    auto result = reader.Read();
    if (!reader.Finished()) throw std::runtime_error("Trailing data in binary VDF");
    return result;
}
std::vector<uint8_t> SerializeVdf(const VdfDocument& document)
{
    std::vector<uint8_t> result;
    WriteNodes(result, document);
    return result;
}
uint32_t ComputeAppId(std::string_view exe, std::string_view appName)
{
    uint32_t crc = 0xffffffffU;
    for (const auto part : {exe, appName})
        for (const unsigned char byte : part) {
            crc ^= byte;
            for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320U : 0);
        }
    return (crc ^ 0xffffffffU) | 0x80000000U;
}
LaunchTarget BuildLaunchTarget(const Environment& environment, bool flatpakSteam)
{
    LaunchTarget result;
    if (environment.flatpak) {
        result.exe = flatpakSteam ? "flatpak-spawn" : "flatpak";
        result.startDir = Quote(Utf8(environment.home));
        result.launchOptions = (flatpakSteam ? "--host flatpak run " : "run ") + std::string(AppId);
        result.flatpakAppId = AppId;
    } else {
        const auto path = environment.appImage.empty() ? environment.executable : environment.appImage;
        const auto spelling = Utf8(path);
        // Keep target construction testable on either host. A Windows drive
        // path does not count as absolute to POSIX std::filesystem.
        if (environment.platform == Platform::Windows) {
            const bool drive = spelling.size() >= 3 && spelling[1] == ':' && (spelling[2] == '\\' || spelling[2] == '/');
            const bool unc = spelling.starts_with("\\\\") || spelling.starts_with("//");
            if (!drive && !unc) throw std::runtime_error("The Windows launch executable path is not absolute");
            const auto slash = spelling.find_last_of("\\/");
            result.startDir = Quote(spelling.substr(0, drive && slash == 2 ? 3 : slash));
        } else {
            if (path.empty() || !path.is_absolute()) throw std::runtime_error("The launch executable path is not absolute");
            result.startDir = Quote(Utf8(path.parent_path()));
        }
        result.exe = Quote(spelling);
    }
    return result;
}
std::vector<SteamRoot> DiscoverSteamRoots(const Environment& environment)
{
    std::vector<SteamRoot> candidates;
    if (environment.platform == Platform::Windows) {
        if (!environment.windowsSteamRoot.empty()) candidates.push_back({environment.windowsSteamRoot, false});
    } else if (!environment.home.empty()) {
        if (environment.platform == Platform::MacOS) candidates.push_back({environment.home / "Library/Application Support/Steam", false});
        else {
            candidates.push_back({environment.home / ".local/share/Steam", false});
            candidates.push_back({environment.home / ".steam/steam", false});
            candidates.push_back({environment.home / ".var/app/com.valvesoftware.Steam/.local/share/Steam", true});
            candidates.push_back({environment.home / ".var/app/com.valvesoftware.Steam/data/Steam", true});
        }
    }
    std::vector<SteamRoot> result;
    std::error_code flatpakError;
    const auto flatpakBase = fs::weakly_canonical(environment.home / ".var/app/com.valvesoftware.Steam", flatpakError);
    for (auto candidate : candidates) {
        std::error_code error;
        if (!fs::is_directory(candidate.path, error)) continue;
        auto path = fs::canonical(candidate.path, error);
        if (error) continue;
        candidate.path = std::move(path);
        // ~/.steam/steam (and sometimes ~/.local/share/Steam) can itself point
        // to Flatpak Steam. Classify the resolved target before deduplication.
        if (environment.platform == Platform::Linux && !flatpakError) {
            const auto relative = candidate.path.lexically_relative(flatpakBase);
            if (!relative.empty() && *relative.begin() != "..") candidate.flatpak = true;
        }
        const auto existing = std::find_if(result.begin(), result.end(), [&](const auto& found) { return found.path == candidate.path; });
        if (existing == result.end()) result.push_back(std::move(candidate));
        else existing->flatpak = existing->flatpak || candidate.flatpak;
    }
    return result;
}
std::optional<fs::path> DiscoverUser(const SteamRoot& root)
{
    const auto loginUsers = root.path / "config/loginusers.vdf";
    if (fs::exists(loginUsers)) {
        const auto bytes = ReadFile(loginUsers);
        const auto document = TextReader(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size())).Read();
        const auto* users = Find(document, "users");
        std::optional<fs::path> recent;
        if (users && users->type == 0) {
            for (const auto& user : users->children) {
                if (user.type != 0 || String(Find(user.children, "MostRecent")) != "1") continue;
                uint64_t steamId = 0;
                if (!Number(user.key, steamId)) continue;
                // SteamID64 stores the account ID in its low 32 bits.
                const auto accountId = steamId & 0xffffffffULL;
                if (accountId == 0) continue;
                const auto directory = root.path / "userdata" / std::to_string(accountId);
                if (!fs::is_directory(directory)) continue;
                if (recent) return std::nullopt; // Ambiguous: never choose an arbitrary account.
                recent = directory;
            }
        }
        if (recent) return recent;
    }
    std::optional<fs::path> single;
    std::error_code error;
    for (fs::directory_iterator it(root.path / "userdata", error), end; !error && it != end; it.increment(error)) {
        uint64_t id = 0;
        if (!it->is_directory() || !Number(Utf8(it->path().filename()), id) || id == 0 || id > 0xffffffffULL) continue;
        if (single) return std::nullopt;
        single = it->path();
    }
    if (error && error != std::errc::no_such_file_or_directory) throw fs::filesystem_error("Cannot inspect Steam userdata", root.path, error);
    return single;
}

Result AddToSteam(const Options& options)
{
    try {
        const auto roots = DiscoverSteamRoots(options.environment);
        if (roots.empty()) return {Status::SteamNotFound, {}};
        std::optional<fs::path> user;
        SteamRoot root;
        for (const auto& candidate : roots) {
            user = DiscoverUser(candidate);
            if (user) { root = candidate; break; }
        }
        if (!user) return {Status::UserNotFound, {}};
        const auto running = [&] {
            return !options.environment.flatpak && (options.steamRunning ? options.steamRunning(root) : SteamRunning(options.environment, root));
        };
        if (running()) return {Status::SteamRunning, {}};
        const auto config = *user / "config";
        const auto shortcutsPath = config / "shortcuts.vdf";
        const bool existing = fs::exists(shortcutsPath);
        const auto original = existing ? ReadFile(shortcutsPath) : std::vector<uint8_t>{};
        VdfDocument emptyDocument;
        emptyDocument.push_back({0, "shortcuts", {}, {}});
        auto document = existing ? ParseVdf(original) : emptyDocument;
        auto* shortcuts = Find(document, "shortcuts");
        if (!shortcuts || shortcuts->type != 0) throw std::runtime_error("shortcuts.vdf has no shortcuts object");
        if (std::count_if(document.begin(), document.end(), [](const auto& n) { return EqualKey(n.key, "shortcuts"); }) != 1)
            throw std::runtime_error("shortcuts.vdf has duplicate shortcuts objects");
        for (const auto& entry : shortcuts->children)
            if (entry.type != 0) throw std::runtime_error("shortcuts.vdf has a non-object shortcut entry");
        auto& entries = shortcuts->children;
        auto own = std::find_if(entries.begin(), entries.end(), [](const auto& n) { return String(Find(n.children, Marker)) == AppId; });
        if (own == entries.end()) own = std::find_if(entries.begin(), entries.end(), [](const auto& n) { return String(Find(n.children, "AppName")) == AppName; });
        if (own == entries.end()) {
            uint64_t next = 0;
            for (const auto& entry : entries) {
                uint64_t index = 0;
                if (!Number(entry.key, index) || index == std::numeric_limits<uint64_t>::max())
                    throw std::runtime_error("shortcuts.vdf has an invalid shortcut index");
                next = std::max(next, index + 1);
            }
            entries.push_back({0, std::to_string(next), {}, {}});
            own = std::prev(entries.end());
        }
        const auto target = BuildLaunchTarget(options.environment, root.flatpak);
        const uint32_t appId = ComputeAppId(target.exe, AppName);
        const auto grid = config / "grid";
        const auto iconPath = grid / (std::to_string(appId) + "_icon.png");
        auto& fields = own->children;
        Set(fields, IntNode("appid", appId));
        Set(fields, StringNode("AppName", AppName));
        Set(fields, StringNode("Exe", target.exe));
        Set(fields, StringNode("StartDir", target.startDir));
        Set(fields, StringNode("icon", Utf8(iconPath)));
        Set(fields, StringNode("ShortcutPath", ""));
        Set(fields, StringNode("LaunchOptions", target.launchOptions));
        Set(fields, StringNode("FlatpakAppID", target.flatpakAppId));
        Set(fields, StringNode(std::string(Marker), AppId));
        for (const auto& [name, value] : std::array<std::pair<const char*, uint32_t>, 8>{{
            {"IsHidden", 0}, {"AllowDesktopConfig", 1}, {"AllowOverlay", 1}, {"OpenVR", 0}, {"Devkit", 0},
            {"DevkitOverrideAppID", 0}, {"LastPlayTime", 0}, {"IsShortcut", 1}}}) {
            Set(fields, IntNode(name, value), true);
        }
        Set(fields, StringNode("DevkitGameID", ""), true);
        Set(fields, VdfNode{0, "tags", {}, {}}, true);
        const auto updated = SerializeVdf(document);
        // Read every asset and validate the VDF before creating a backup or
        // touching any existing art. The icon lives in Steam's host-visible
        // grid directory, never in an AppImage mount or /app sandbox path.
        const std::array<std::string, 5> art{
            std::to_string(appId) + "p.png", std::to_string(appId) + ".png", std::to_string(appId) + "_hero.png",
            std::to_string(appId) + "_logo.png", std::to_string(appId) + "_icon.png"};
        const auto artwork = steam_artwork::Generate(options.gameRoot, options.artworkDirectory);
        if (running()) return {Status::SteamRunning, {}};
        fs::create_directories(grid);
        // A new Steam profile gets a valid empty-file backup; existing files
        // are copied byte for byte, even when their key casing is unusual.
        const auto backup = existing ? original : SerializeVdf(emptyDocument);
        AtomicWrite(config / "shortcuts.vdf.bak", backup);
        for (size_t i = 0; i < art.size(); ++i) AtomicWrite(grid / art[i], artwork.pngs[i]);
        if (running()) return {Status::SteamRunning, {}};
        // Avoid overwriting another writer's changes made while art was copied.
        if (fs::exists(shortcutsPath) != existing || (existing && ReadFile(shortcutsPath) != original))
            throw std::runtime_error("shortcuts.vdf changed while adding the game; try again with Steam closed");
        AtomicWrite(shortcutsPath, updated);
        return {options.environment.flatpak ? Status::AddedFromFlatpak : Status::Added, artwork.source};
    } catch (const std::exception& exception) {
        return {Status::Error, exception.what()};
    }
}

Result AddToSteam(const fs::path &gameRoot)
{
#if LO_PLATFORM_ANDROID
    return {Status::Error, "Add to Steam is unavailable on Android"};
#else
    try {
        Options options;
        options.gameRoot = gameRoot;
#if LO_PLATFORM_WINDOWS
        options.environment.platform = Platform::Windows;
        options.environment.windowsSteamRoot = RegistrySteamPath();
        const auto* home = std::getenv("USERPROFILE");
#else
#if LO_PLATFORM_MACOS
        options.environment.platform = Platform::MacOS;
#endif
        const auto* home = std::getenv("HOME");
        if (const auto* appImage = std::getenv("APPIMAGE"); appImage && *appImage) options.environment.appImage = appImage;
        options.environment.flatpak = std::getenv("FLATPAK_ID") != nullptr || fs::exists("/.flatpak-info");
#endif
        if (home && *home) options.environment.home = home;
        options.environment.executable = ExecutablePath();
        options.artworkDirectory = os::user_paths::AssetPath("steam");
        return AddToSteam(options);
    } catch (const std::exception& exception) { return {Status::Error, exception.what()}; }
#endif
}
}
