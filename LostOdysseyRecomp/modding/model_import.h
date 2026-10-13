#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>

// Model replacements: a mod's .glb, edited from a model --export-assets wrote,
// takes the place of a SkeletalMesh in its package. The package is rebuilt when
// the game opens the archive that holds it (modding/text_overlay.h): the new
// mesh is serialized at the end of the package and the export table points
// there, so nothing else in the package moves. The skeleton and the material
// slots stay the original's; vertices, triangles, UVs and weights are the
// file's. See docs/wiki/Creating-Mods.md#replace-a-model.
namespace modding::model_import
{
struct Replacement
{
    uint32_t exportIndex = 0;
    std::string object;       // the export's name as the file names it
    std::vector<uint8_t> glb;
    std::string source;       // for messages
};
struct Result
{
    std::vector<uint8_t> package; // empty when nothing was replaced
    std::vector<std::string> notes; // one line per replacement, done or skipped
    size_t replaced = 0;
};
// package: the decoded package bytes (no CPX).
Result RebuildPackage(std::span<const uint8_t> package, const std::vector<Replacement> &replacements);
}
