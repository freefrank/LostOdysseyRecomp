#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

// Model replacements: a mod's .glb, edited from a model --export-assets wrote,
// takes the place of a SkeletalMesh, StaticMesh or StaticMeshRPG in its package.
// The package is rebuilt when the game opens the archive that holds it
// (modding/text_overlay.h): the new mesh is serialized at the end of the
// package and the export table points there, so nothing else in the package
// moves; the old export's bytes are zeroed. The skeleton and the material slots stay the original's; vertices,
// triangles, UVs and weights are the file's. A static mesh gets a new collision
// tree, and the baked lighting of its placements follows its new vertices
// (RemapLighting). See docs/wiki/Creating-Mods.md#replace-a-model.
namespace modding::model_import
{
struct Replacement
{
    uint32_t exportIndex = 0;
    std::string object;       // the export's name as the file names it
    std::vector<uint8_t> glb;
    std::string source;       // for messages
};
// A replaced static mesh: for each new vertex, the point of the original
// surface nearest to it, as three original vertices and their weights. The
// mesh's placements keep baked lighting per original vertex; a new vertex gets
// it interpolated there, as the GPU did across the original triangle.
struct SurfacePoint
{
    std::array<uint32_t, 3> vertices{};
    std::array<float, 3> weights{1, 0, 0};
};
struct VertexMap
{
    uint32_t originalVertices = 0;
    std::vector<SurfacePoint> points;
};
// Key: "<package file stem>.<object>", lower case (u3b_0_map.staticmesh_467).
using VertexMaps = std::map<std::string, VertexMap>;

struct Result
{
    std::vector<uint8_t> package; // empty when nothing was replaced
    std::vector<std::string> notes; // one line per replacement, done or skipped
    size_t replaced = 0;
    VertexMaps vertexMaps;          // the static meshes replaced
};
// package: the decoded package bytes (no CPX); stem: its file name without extension.
Result RebuildPackage(std::span<const uint8_t> package, const std::string &stem, const std::vector<Replacement> &replacements);

// The StaticMeshComponents of a package that place a mesh in maps: their
// per-vertex light maps and ShadowMap1D buffers rebuilt for the new vertices,
// interpolated at each one's surface point. result.replaced counts the light
// maps and buffers changed; the package is empty when none was.
Result RemapLighting(std::span<const uint8_t> package, const std::string &stem, const VertexMaps &maps);
}
