#pragma once
#include <filesystem>
#include <optional>
#include <string>

// `--export-assets <dir> [--export-kinds textures,fingerprints,movies,text] [--export-filter <text>]`
// copies the player's own game data into a folder for mod authors: textures as
// PNG (keyed like image mods), movies as the original WMV bytes and the game's
// text as JSON (keyed like language packs). Reads game files only: no window,
// GPU device, installer, logs or guest code.
namespace modding::asset_export
{
struct Request
{
    std::filesystem::path output;
    bool textures = true, movies = true, text = true;
    bool pngs = true; // false with --export-kinds fingerprints: index.csv only
    std::string filter; // ASCII lower case; matched against package / file paths
    std::string error;  // malformed arguments; Run reports it and fails
};
// Empty when --export-assets is absent. argv is UTF-8.
std::optional<Request> ParseArguments(int argc, char **argv);
// gameRoot is the boot disc (or an install root holding disc1); sibling
// disc2..disc4 folders are read too, and for text the DLC indexes in the
// sibling dlc folder. Returns the process exit code.
int Run(const Request &request, const std::filesystem::path &gameRoot);
}
