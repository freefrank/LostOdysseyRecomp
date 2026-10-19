#include <os/steam_shortcut.h>
#include <os/user_paths.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace
{
namespace fs = std::filesystem;
using namespace steam_shortcut;
unsigned checks = 0;
void Check(bool condition, const char* message)
{
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
template<class F> void Reject(F&& function, const char* message)
{
    bool rejected = false;
    try { function(); } catch (const std::exception&) { rejected = true; }
    Check(rejected, message);
}
std::string Utf8(const fs::path& path)
{
    const auto text = path.u8string();
    return {text.begin(), text.end()};
}
VdfNode S(std::string key, std::string value)
{
    VdfNode node{1, std::move(key), {value.begin(), value.end()}, {}};
    node.value.push_back(0);
    return node;
}
VdfNode I(std::string key, uint32_t value)
{
    return {2, std::move(key), {uint8_t(value), uint8_t(value >> 8), uint8_t(value >> 16), uint8_t(value >> 24)}, {}};
}
const VdfNode& Field(const std::vector<VdfNode>& nodes, std::string_view key)
{
    const auto found = std::find_if(nodes.begin(), nodes.end(), [&](const auto& n) {
        return n.key.size() == key.size() && std::equal(n.key.begin(), n.key.end(), key.begin(), [](unsigned char a, unsigned char b) {
            return std::tolower(a) == std::tolower(b);
        });
    });
    if (found == nodes.end()) throw std::runtime_error("Missing field: " + std::string(key));
    return *found;
}
std::string Text(const VdfNode& node)
{
    Check(node.type == 1 && !node.value.empty() && node.value.back() == 0, "field is not a terminated string");
    return {node.value.begin(), node.value.end() - 1};
}
uint32_t Integer(const VdfNode& node)
{
    Check(node.type == 2 && node.value.size() == 4, "field is not int32");
    return uint32_t(node.value[0]) | uint32_t(node.value[1]) << 8 | uint32_t(node.value[2]) << 16 | uint32_t(node.value[3]) << 24;
}
void Write(const fs::path& path, const std::vector<uint8_t>& bytes)
{
    fs::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    if (!stream) throw std::runtime_error("fixture write failed");
}
void WriteText(const fs::path& path, std::string_view text)
{
    Write(path, {text.begin(), text.end()});
}
std::vector<uint8_t> Read(const fs::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("fixture read failed");
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}
VdfDocument ExistingShortcuts()
{
    // Steam/BoilR-shaped entry with every usual shortcut field plus raw binary
    // types an editor must preserve even when it does not interpret them.
    std::vector<VdfNode> fields = {
        I("appid", 3546676531U), S("AppName", "A Plague Tale Innocence"),
        S("Exe", R"("C:\MySmallPrograms\epic_launcher.exe")"), S("StartDir", R"("C:\MySmallPrograms\")"),
        S("icon", ""), S("ShortcutPath", ""), S("LaunchOptions", "--existing-option"), I("IsHidden", 0),
        I("AllowDesktopConfig", 1), I("AllowOverlay", 1), I("OpenVR", 0), I("Devkit", 0), S("DevkitGameID", ""),
        I("DevkitOverrideAppID", 0), I("LastPlayTime", 1728000000), S("FlatpakAppID", ""),
        {0, "tags", {}, {S("0", "Favorites"), S("1", "Installed")}},
        {3, "FutureFloat", {0, 0, 0xc0, 0x7f}, {}}, // NaN payload, must not be normalized.
        {4, "FuturePointer", {0xef, 0xbe, 0xad, 0xde}, {}},
        {5, "FutureWideString", {'A', 0, 0x2d, 0x4e, 0, 0}, {}},
        {6, "FutureColor", {0x10, 0x20, 0x30, 0xff}, {}},
        {7, "FutureUint64", {0xff, 0xfe, 0xfd, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8}, {}},
        {10, "FutureInt64", {0x80, 0, 0, 0, 0, 0, 0, 0xff}, {}},
        S("DuplicateUnknown", "first"), S("DuplicateUnknown", "second")
    };
    return {{0, "shortcuts", {}, {{0, "0", {}, fields}, {0, "3", {}, {S("appname", "Other game"), S("exe", "/other")}}}},
            S("UnrelatedRootKey", "kept")};
}
void BinaryAndAppId()
{
    const auto original = SerializeVdf(ExistingShortcuts());
    Check(SerializeVdf(ParseVdf(original)) == original, "real-shaped binary VDF did not round-trip exactly");
    for (size_t length = 0; length < original.size(); ++length)
        Reject([&] { ParseVdf(std::span(original).first(length)); }, "truncated VDF was accepted");
    auto trailing = original;
    trailing.push_back(0);
    Reject([&] { ParseVdf(trailing); }, "trailing binary garbage was accepted");
    Reject([&] { ParseVdf(std::array<uint8_t, 4>{9, 'a', 0, 8}); }, "unknown VDF type was accepted");
    // Existing tool value: steam_shortcuts_util (used by BoilR),
    // src/testdata/shortcuts2.vdf, A Plague Tale Innocence.
    // https://github.com/PhilipK/steam_shortcuts_util/blob/main/src/testdata/shortcuts2.vdf
    Check(ComputeAppId(R"("C:\MySmallPrograms\epic_launcher.exe")", "A Plague Tale Innocence") == 3546676531U,
          "appid differs from BoilR's existing tool fixture");
    Check(ComputeAppId(R"(C:\MySmallPrograms\epic_launcher.exe)", "A Plague Tale Innocence") != 3546676531U,
          "appid ignored the executable's literal quotes");
    Check((ComputeAppId("flatpak", "Lost Odyssey Recomp") & 0x80000000U) != 0, "appid high bit missing");
}
void LaunchTargets(const fs::path& temporary)
{
    Environment environment;
    environment.home = temporary / "home";
    environment.executable = temporary / "bin/LostOdysseyRecomp";
    auto target = BuildLaunchTarget(environment, false);
    Check(target.exe == '"' + Utf8(environment.executable) + '"', "native executable not quoted");
    Check(target.startDir == '"' + Utf8(environment.executable.parent_path()) + '"', "native start dir incorrect");
    Check(target.launchOptions.empty() && target.flatpakAppId.empty(), "native launch has Flatpak parameters");
    environment.appImage = temporary / "with spaces/LostOdysseyRecomp.AppImage";
    target = BuildLaunchTarget(environment, false);
    Check(target.exe == '"' + Utf8(environment.appImage) + '"', "AppImage target used temporary mount executable");
    Check(target.startDir == '"' + Utf8(environment.appImage.parent_path()) + '"', "AppImage start dir incorrect");
    environment.flatpak = true;
    target = BuildLaunchTarget(environment, false);
    Check(target.exe == "flatpak", "Flatpak native-Steam target incorrect");
    Check(target.launchOptions == "run io.github.freefrank.LostOdysseyRecomp", "Flatpak launch options incorrect");
    Check(target.flatpakAppId == "io.github.freefrank.LostOdysseyRecomp", "Deck FlatpakAppID missing");
    target = BuildLaunchTarget(environment, true);
    Check(target.exe == "flatpak-spawn", "Flatpak-Steam target did not use host spawn");
    Check(target.launchOptions == "--host flatpak run io.github.freefrank.LostOdysseyRecomp", "Flatpak-Steam launch options incorrect");
    environment.flatpak = false;
    environment.appImage.clear();
    environment.platform = Platform::Windows;
    environment.executable = R"(C:\Games\Lost Odyssey\LostOdysseyRecomp.exe)";
    target = BuildLaunchTarget(environment, false);
    Check(target.exe == R"("C:\Games\Lost Odyssey\LostOdysseyRecomp.exe")", "Windows target incorrect");
    Check(target.startDir == R"("C:\Games\Lost Odyssey")", "Windows start dir incorrect");
    environment.executable = R"(C:\LostOdysseyRecomp.exe)";
    Check(BuildLaunchTarget(environment, false).startDir == R"("C:\")", "Windows drive-root start dir became drive-relative");
    environment.executable = R"(\\server\games\LostOdysseyRecomp.exe)";
    Check(BuildLaunchTarget(environment, false).startDir == R"("\\server\games")", "Windows UNC start dir incorrect");
    environment.executable = "relative.exe";
    Reject([&] { BuildLaunchTarget(environment, false); }, "relative launch executable accepted");
}
void Discovery(const fs::path& temporary)
{
    Environment environment;
    environment.home = temporary / "discovery";
    Check(DiscoverSteamRoots(environment).empty(), "missing Steam installation discovered");
    const auto native = environment.home / ".local/share/Steam";
    const auto flatpak = environment.home / ".var/app/com.valvesoftware.Steam/.local/share/Steam";
    const auto flatpakData = environment.home / ".var/app/com.valvesoftware.Steam/data/Steam";
    for (const auto& path : {native, flatpak, flatpakData}) fs::create_directories(path);
    fs::create_directories(environment.home / ".steam");
    std::error_code linkError;
    fs::create_directory_symlink(native, environment.home / ".steam/steam", linkError);
    const auto roots = DiscoverSteamRoots(environment);
    Check(roots.size() == 3, "Steam roots were not canonicalized and deduplicated");
    Check(roots[0].path == fs::canonical(native) && !roots[0].flatpak, "native Steam root incorrect");
    Check(roots[1].flatpak && roots[2].flatpak, "Flatpak Steam roots not identified");
    if (!linkError) Check(roots[0].path != environment.home / ".steam/steam", "Steam symlink was not resolved");
    else std::cout << "symlink fixture unavailable: " << linkError.message() << '\n';
    if (!linkError) {
        fs::remove(environment.home / ".steam/steam");
        fs::create_directory_symlink(flatpak, environment.home / ".steam/steam");
        const auto aliasRoots = DiscoverSteamRoots(environment);
        Check(aliasRoots.size() == 3 && aliasRoots[1].flatpak && aliasRoots[1].path == fs::canonical(flatpak),
              "Flatpak Steam symlink classified as native Steam");
        auto sandbox = environment;
        sandbox.flatpak = true;
        Check(BuildLaunchTarget(sandbox, aliasRoots[1].flatpak).exe == "flatpak-spawn", "Flatpak Steam alias did not select host spawn");
    }
    SteamRoot root{native, false};
    Check(!DiscoverUser(root), "empty userdata discovered a user");
    fs::create_directories(native / "userdata/0");
    fs::create_directories(native / "userdata/not-an-id");
    fs::create_directories(native / "userdata/123");
    Check(DiscoverUser(root) == native / "userdata/123", "single-user fallback incorrect");
    fs::create_directories(native / "userdata/456");
    Check(!DiscoverUser(root), "ambiguous users selected arbitrarily");
    WriteText(native / "config/loginusers.vdf", R"VDF(
"users"
{
    "76561197960265851" { "AccountName" "one" "MostRecent" "0" }
    // SteamID64 = 76561197960265728 + 456
    "76561197960266184" { "AccountName" "two" "MostRecent" "1" }
}
)VDF");
    Check(DiscoverUser(root) == native / "userdata/456", "MostRecent SteamID64 was not converted to account ID");
    WriteText(native / "config/loginusers.vdf", R"("users" { "76561197960266184" { "MostRecent" "0" } })");
    Check(!DiscoverUser(root), "no-MostRecent multi-user profile selected arbitrarily");
    WriteText(native / "config/loginusers.vdf", R"("users" { "76561197960266184" { "MostRecent" "1" })");
    Reject([&] { DiscoverUser(root); }, "malformed loginusers.vdf accepted");
    environment.platform = Platform::Windows;
    environment.windowsSteamRoot = native;
    Check(DiscoverSteamRoots(environment).size() == 1, "injected registry Steam root not used");
    environment.platform = Platform::MacOS;
    fs::create_directories(environment.home / "Library/Application Support/Steam");
    Check(DiscoverSteamRoots(environment).size() == 1, "macOS Steam root not discovered");
}
void Install(const fs::path& temporary)
{
    Options options;
    options.environment.home = temporary / "install-home";
    options.environment.executable = temporary / "runtime/LostOdysseyRecomp";
    options.artworkDirectory = temporary / "art";
    options.steamRunning = [](const SteamRoot&) { return false; };
    const auto native = options.environment.home / ".local/share/Steam";
    const auto user = native / "userdata/123";
    const auto config = user / "config";
    const auto shortcuts = config / "shortcuts.vdf";
    Check(AddToSteam(options).status == Status::SteamNotFound, "missing Steam status incorrect");
    fs::create_directories(native);
    Check(AddToSteam(options).status == Status::UserNotFound, "missing user status incorrect");
    fs::create_directories(config);
    const auto original = SerializeVdf(ExistingShortcuts());
    Write(shortcuts, original);
    options.steamRunning = [](const SteamRoot&) { return true; };
    Check(AddToSteam(options).status == Status::SteamRunning, "running Steam not blocked");
    Check(Read(shortcuts) == original && !fs::exists(config / "shortcuts.vdf.bak") && !fs::exists(config / "grid"), "running Steam modified files");
    options.steamRunning = [](const SteamRoot&) { return false; };
    Check(AddToSteam(options).status == Status::Error, "missing art was accepted");
    Check(Read(shortcuts) == original && !fs::exists(config / "shortcuts.vdf.bak"), "asset preflight modified shortcuts/backup");
    for (const auto* name : {"portrait.png", "header.png", "hero.png", "logo.png", "icon.png"})
        WriteText(options.artworkDirectory / name, name);
    Write(shortcuts, {0, 'x', 0, 8});
    Check(AddToSteam(options).status == Status::Error, "malformed VDF not rejected");
    Check(Read(shortcuts) == std::vector<uint8_t>({0, 'x', 0, 8}) && !fs::exists(config / "shortcuts.vdf.bak"), "malformed VDF modified");
    Write(shortcuts, original);
    auto result = AddToSteam(options);
    Check(result.ok() && result.status == Status::Added, result.detail.c_str());
    Check(Read(config / "shortcuts.vdf.bak") == original, "backup is not original VDF bytes");
    auto after = Read(shortcuts);
    auto document = ParseVdf(after);
    const auto& entries = Field(document, "shortcuts").children;
    Check(entries.size() == 3 && entries.back().key == "4", "existing indices/entries not preserved");
    const auto beforeDocument = ExistingShortcuts();
    const auto& oldEntries = Field(beforeDocument, "shortcuts").children;
    Check(SerializeVdf({entries[0], entries[1]}) == SerializeVdf(oldEntries), "other shortcuts changed");
    Check(Text(Field(document, "UnrelatedRootKey")) == "kept", "unknown root field lost");
    const auto& fields = entries.back().children;
    const auto appId = Integer(Field(fields, "appid"));
    Check(appId == ComputeAppId(Text(Field(fields, "Exe")), Text(Field(fields, "AppName"))), "stored appid does not match exact strings");
    Check(Integer(Field(fields, "AllowOverlay")) == 1 && Integer(Field(fields, "AllowDesktopConfig")) == 1 && Integer(Field(fields, "IsShortcut")) == 1,
          "Steam default flags incorrect");
    Check(Field(fields, "tags").type == 0 && Field(fields, "tags").children.empty(), "tags not an object");
    Check(Text(Field(fields, "FlatpakAppID")).empty(), "native FlatpakAppID not empty");
    const auto id = std::to_string(appId);
    const std::array<std::pair<const char*, std::string>, 5> mappings{{
        {"portrait.png", id + "p.png"}, {"header.png", id + ".png"}, {"hero.png", id + "_hero.png"},
        {"logo.png", id + "_logo.png"}, {"icon.png", id + "_icon.png"}}};
    for (const auto& [source, destination] : mappings)
        Check(Read(config / "grid" / destination) == Read(options.artworkDirectory / source), "art copied to wrong appid path");
    const auto iconText = Text(Field(fields, "icon"));
    const auto icon = fs::path(std::u8string(iconText.begin(), iconText.end()));
    Check(fs::equivalent(icon, config / "grid" / (id + "_icon.png")) && fs::exists(icon), "icon not in durable host-visible Steam grid");
    Check(AddToSteam(options).ok() && Read(shortcuts) == after, "repeated add is not byte-idempotent");
    Check(Read(config / "shortcuts.vdf.bak") == after, "second add backup not refreshed before mutation");
    // Steam itself may discard unknown fields: name fallback must still update
    // in place after a move, preserving the player's tags and last-play time.
    auto& mutableFields = document[0].children.back().children;
    mutableFields.erase(std::remove_if(mutableFields.begin(), mutableFields.end(), [](const auto& node) { return node.key == "LostOdysseyRecomp"; }), mutableFields.end());
    Field(mutableFields, "tags");
    for (auto& field : mutableFields) {
        if (field.key == "tags") field.children.push_back(S("0", "My games"));
        if (field.key == "LastPlayTime") field = I("LastPlayTime", 42);
        if (field.key == "AppName") field.key = "appname";
        if (field.key == "Exe") field.key = "exe";
    }
    Write(shortcuts, SerializeVdf(document));
    options.environment.executable = temporary / "moved/runtime/LostOdysseyRecomp";
    result = AddToSteam(options);
    Check(result.ok(), result.detail.c_str());
    document = ParseVdf(Read(shortcuts));
    Check(document[0].children.size() == 3, "moved game duplicated shortcut");
    Check(Integer(Field(document[0].children.back().children, "LastPlayTime")) == 42, "player's last play time reset");
    Check(Text(Field(Field(document[0].children.back().children, "tags").children, "0")) == "My games", "player's tag reset");
    // The stable marker also survives the user renaming the entry.
    for (auto& field : document[0].children.back().children) if (field.key == "appname") field = S("AppName", "My custom name");
    Write(shortcuts, SerializeVdf(document));
    Check(AddToSteam(options).ok() && ParseVdf(Read(shortcuts))[0].children.size() == 3, "marker did not identify renamed entry");
    // Sandbox can't interpret host PID liveness and must use the special text.
    options.environment.flatpak = true;
    options.steamRunning = [](const SteamRoot&) -> bool { throw std::runtime_error("host PID callback must not be called"); };
    result = AddToSteam(options);
    Check(result.status == Status::AddedFromFlatpak, result.detail.c_str());
    document = ParseVdf(Read(shortcuts));
    Check(Text(Field(document[0].children.back().children, "Exe")) == "flatpak", "Flatpak runtime target incorrect");
    Check(Text(Field(document[0].children.back().children, "FlatpakAppID")) == "io.github.freefrank.LostOdysseyRecomp", "FlatpakAppID not written");
    // A first install creates a valid empty backup before its first shortcut.
    options.environment.home = temporary / "fresh-home";
    const auto freshConfig = options.environment.home / ".var/app/com.valvesoftware.Steam/data/Steam/userdata/456/config";
    fs::create_directories(freshConfig);
    result = AddToSteam(options);
    Check(result.ok(), result.detail.c_str());
    const auto fresh = ParseVdf(Read(freshConfig / "shortcuts.vdf"));
    Check(fresh[0].children.size() == 1, "fresh shortcut count incorrect");
    Check(ParseVdf(Read(freshConfig / "shortcuts.vdf.bak"))[0].children.empty(), "first-install backup invalid");
    Check(Text(Field(fresh[0].children[0].children, "Exe")) == "flatpak-spawn", "Flatpak Steam integration did not select host spawn");
    // Liveness also covers a genuine process ID and stale PID without using
    // the real Steam home or registry.
    options.environment.flatpak = false;
    options.steamRunning = {};
#ifdef _WIN32
    const auto pid = GetCurrentProcessId();
#else
    const auto pid = getpid();
#endif
    WriteText(options.environment.home / ".steam/steam.pid", std::to_string(pid));
    const auto snapshot = Read(freshConfig / "shortcuts.vdf");
    Check(AddToSteam(options).status == Status::SteamRunning, "live PID file did not block write");
    Check(Read(freshConfig / "shortcuts.vdf") == snapshot, "live PID changed shortcuts");
    WriteText(options.environment.home / ".steam/steam.pid", "2147483647");
    Check(AddToSteam(options).ok(), "stale PID file blocked write");
}
void AssetPaths(const fs::path& temporary)
{
    const auto bin = temporary / "assets-test/usr/bin";
    fs::create_directories(bin);
    os::user_paths::Initialize(bin);
    fs::create_directories(bin.parent_path() / "share/lost-odyssey-recomp/steam");
    Check(os::user_paths::AssetPath("steam") == bin.parent_path() / "share/lost-odyssey-recomp/steam", "installed asset path incorrect");
    fs::create_directories(bin / "assets/steam");
    Check(os::user_paths::AssetPath("steam") == bin / "assets/steam", "executable-relative asset path incorrect");
}
}

int main()
{
    const auto temporary = fs::temp_directory_path() / ("lo-steam-fixture-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        fs::create_directories(temporary);
        BinaryAndAppId();
        LaunchTargets(temporary);
        Discovery(temporary);
        Install(temporary);
        AssetPaths(temporary);
        fs::remove_all(temporary);
        std::cout << "Steam shortcut fixtures passed (" << checks << " checks).\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << "\nFixture retained at " << temporary << '\n';
        return 1;
    }
}
