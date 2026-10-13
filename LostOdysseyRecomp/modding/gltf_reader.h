#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

// Reading a binary glTF 2.0 file (.glb) for the model import: the node tree,
// skins, triangle primitives and material names. Images and animations are
// ignored; buffers must be the file's own BIN chunk (no external .bin, no
// sparse accessors). Errors throw std::runtime_error with a message for modders.
namespace modding::gltf
{
using Matrix = std::array<float, 16>; // column-major, as glTF stores it

struct Node
{
    std::string name;
    int parent = -1, mesh = -1, skin = -1;
    Matrix local{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};
struct Skin { std::vector<int> joints; std::vector<Matrix> inverseBind; };
struct Primitive { std::map<std::string, int> attributes; int indices = -1, material = -1; };

struct Document
{
    std::vector<Node> nodes;
    std::vector<Skin> skins;
    std::vector<std::vector<Primitive>> meshes;
    std::vector<std::string> materials;

    // count x components values; normalized integers become [0, 1] floats.
    std::vector<float> Floats(int accessor, int components) const;
    // Integer accessors (indices, joints).
    std::vector<uint32_t> Integers(int accessor, int components) const;
    size_t Count(int accessor) const;
    // The node's transform in the scene at rest (parents applied).
    Matrix World(int node) const;

    struct Accessor { int view = -1, component = 0, components = 0; size_t offset = 0, count = 0; bool normalized = false; };
    struct View { size_t offset = 0, length = 0, stride = 0; };
    std::vector<Accessor> accessors;
    std::vector<View> views;
    std::vector<uint8_t> bin;
};

Document ReadGlb(std::span<const uint8_t> bytes);

Matrix Multiply(const Matrix &a, const Matrix &b);
std::array<float, 3> TransformPoint(const Matrix &m, const std::array<float, 3> &p);
std::array<float, 3> TransformVector(const Matrix &m, const std::array<float, 3> &v);
}
