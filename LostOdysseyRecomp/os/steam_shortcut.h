#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace steam_shortcut
{
enum class Status { Added, AddedFromFlatpak, SteamRunning, SteamNotFound, UserNotFound, Error };
struct Result
{
    Status status = Status::Error;
    std::string detail;
    bool ok() const { return status == Status::Added || status == Status::AddedFromFlatpak; }
};

enum class Platform { Linux, Windows, MacOS };
struct Environment
{
    Platform platform = Platform::Linux;
    std::filesystem::path home;
    std::filesystem::path executable;
    std::filesystem::path appImage;
    std::filesystem::path windowsSteamRoot;
    bool flatpak = false;
};
struct SteamRoot
{
    std::filesystem::path path;
    bool flatpak = false;
};
struct LaunchTarget
{
    std::string exe, startDir, launchOptions, flatpakAppId;
};
struct Options
{
    Environment environment;
    std::filesystem::path artworkDirectory;
    std::filesystem::path gameRoot;
    // Optional fixture injection. An empty callback uses the platform's PID
    // liveness check; Flatpak always skips host PID checks.
    std::function<bool(const SteamRoot&)> steamRunning;
};

uint32_t ComputeAppId(std::string_view exe, std::string_view appName);
LaunchTarget BuildLaunchTarget(const Environment& environment, bool flatpakSteam);
std::vector<SteamRoot> DiscoverSteamRoots(const Environment& environment);
std::optional<std::filesystem::path> DiscoverUser(const SteamRoot& root);
Result AddToSteam(const Options& options);
Result AddToSteam(const std::filesystem::path &gameRoot = {});

// Binary KeyValues retain ordered keys, duplicate keys, and uninterpreted
// scalar bytes so another game's fields round-trip without coercion.
struct VdfNode
{
    uint8_t type = 0;
    std::string key;
    std::vector<uint8_t> value;
    std::vector<VdfNode> children;
};
using VdfDocument = std::vector<VdfNode>;
VdfDocument ParseVdf(std::span<const uint8_t> bytes); // throws on malformed/unknown types
std::vector<uint8_t> SerializeVdf(const VdfDocument& document);
}
