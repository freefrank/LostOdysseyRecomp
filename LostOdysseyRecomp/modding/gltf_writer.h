#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

// Binary glTF 2.0 (.glb) for the asset export's models and animations. The mesh is
// already in glTF space: right-handed, Y up, metres; bones are local to their parent.
namespace modding::gltf
{
struct Bone
{
    std::string name;
    int parent = -1; // index of an earlier bone, or -1 for a root
    std::array<float, 3> translation{};
    std::array<float, 4> rotation{0, 0, 0, 1}; // x, y, z, w
};
struct Section { uint32_t material = 0, firstIndex = 0, indexCount = 0; };
struct Material
{
    std::string name;
    int baseColor = -1; // index into Mesh::images
    bool mask = false;  // cut-out alpha
};
// Keys of one bone: times in seconds, values xyz (translation) or xyzw (rotation).
struct Channel { uint32_t bone = 0; bool rotation = false; std::vector<float> times, values; };
struct Animation { std::string name; std::vector<Channel> channels; };
struct Mesh
{
    std::vector<std::array<float, 3>> positions, normals;
    std::vector<std::vector<std::array<float, 2>>> uvs; // one per UV set, each positions.size()
    std::vector<std::array<uint8_t, 4>> colors;         // RGBA, empty when absent
    std::vector<std::array<uint16_t, 4>> joints;        // skinned meshes only
    std::vector<std::array<float, 4>> weights;
    std::vector<uint32_t> indices;
    std::vector<Section> sections;
    std::vector<Material> materials;
    std::vector<std::string> images; // image file paths relative to the .glb
    std::vector<Bone> bones;
    std::vector<Animation> animations;
};
std::vector<uint8_t> Write(const Mesh &mesh, const std::string &name);
}
