#pragma once
#include <filesystem>
#include <optional>
#include <string>

// `--export-assets <dir> [--export-kinds textures,fingerprints,movies,text,models,animations,fonts]
//  [--export-model-textures] [--export-filter <text>] [--export-select <file>]
//  [--export-language <code>] [--export-language-pack <id>] [--export-list] [--export-preview]`
// copies the player's own game data into a folder for mod authors: textures as
// PNG (keyed like image mods), movies as the original WMV bytes, the game's
// text as JSON (keyed like language packs), and on request models (LOD 0 with
// skeleton) and animation sets as glTF binaries (.glb); an animation set
// carries the model whose skeleton it moves; fonts as JSON, their metrics and
// glyph tables (modding/font_patch.h FontInfo), for language-pack glyphs.
// --export-model-textures writes each
// material's colour texture beside the models and links it. --export-select names a UTF-8
// file of list.csv names, one per line, and exports only those. --export-language
// keeps the text of one language (int, jpn...); --export-language-pack writes
// only that text (int by default) as a language pack to translate:
// <dir>/<id>/language.ini and <dir>/<id>/text/. --export-list writes only
// <dir>/list.csv, one row per texture, movie, text file, model and animation
// set the same options would export, without decoding textures (the MO2 tool's
// preview); with models or animations it adds the game's own place and
// character names, in every language on the discs, as "name" rows.
// --export-preview writes each model or animation set as a PNG still
// (model_preview.h) in place of its .glb. Reads game files only: no window, GPU
// device, installer, logs or guest code.
namespace modding::asset_export
{
struct Request
{
    std::filesystem::path output;
    bool textures = true, movies = true, text = true;
    bool models = false, animations = false; // not in the default kinds: tens of thousands of files
    bool fonts = false;
    bool modelTextures = false;
    bool pngs = true; // false with --export-kinds fingerprints: index.csv only
    std::string filter; // ASCII lower case; matched against package / file paths
    std::filesystem::path select; // list of names to export, or empty for all
    std::string language; // text of this language code only; empty for all
    std::string languagePack; // pack id (lower case): text only, written as a language pack
    bool list = false;        // list.csv only
    bool preview = false;     // models and animation sets as PNG stills, not .glb
    std::string error;  // malformed arguments; Run reports it and fails
};
// Empty when --export-assets is absent. argv is UTF-8.
std::optional<Request> ParseArguments(int argc, char **argv);
// gameRoot is the boot disc (or an install root holding disc1); sibling
// disc2..disc4 folders are read too, and for text the DLC indexes in the
// sibling dlc folder. Returns the process exit code.
int Run(const Request &request, const std::filesystem::path &gameRoot);
}
