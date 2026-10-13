#include "gltf_reader.h"

#include <os/json.h>

#include <cmath>
#include <cstring>
#include <stdexcept>

namespace modding::gltf
{
namespace
{
void Require(bool ok, const std::string &message) { if (!ok) throw std::runtime_error(message); }

uint32_t Le32(const uint8_t *p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }

size_t ComponentSize(int component)
{
    switch (component)
    {
    case 5120: case 5121: return 1; // (unsigned) byte
    case 5122: case 5123: return 2; // (unsigned) short
    case 5125: case 5126: return 4; // unsigned int, float
    default: throw std::runtime_error("unsupported accessor component type " + std::to_string(component));
    }
}

int Components(const std::string &type)
{
    if (type == "SCALAR") return 1;
    if (type == "VEC2") return 2;
    if (type == "VEC3") return 3;
    if (type == "VEC4") return 4;
    if (type == "MAT4") return 16;
    throw std::runtime_error("unsupported accessor type " + type);
}

Matrix FromTrs(const nlohmann::json &node)
{
    if (node.contains("matrix"))
    {
        Matrix m{};
        Require(node["matrix"].size() == 16, "node matrix needs 16 numbers");
        for (int i = 0; i < 16; ++i) m[size_t(i)] = node["matrix"][size_t(i)].get<float>();
        return m;
    }
    std::array<float, 3> t{0, 0, 0}, s{1, 1, 1};
    std::array<float, 4> q{0, 0, 0, 1};
    if (node.contains("translation")) for (int i = 0; i < 3; ++i) t[size_t(i)] = node["translation"][size_t(i)].get<float>();
    if (node.contains("scale")) for (int i = 0; i < 3; ++i) s[size_t(i)] = node["scale"][size_t(i)].get<float>();
    if (node.contains("rotation")) for (int i = 0; i < 4; ++i) q[size_t(i)] = node["rotation"][size_t(i)].get<float>();
    const float length = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (length > 1e-6f) for (auto &v : q) v /= length;
    const auto [x, y, z, w] = q;
    return {(1 - 2 * (y * y + z * z)) * s[0], 2 * (x * y + z * w) * s[0], 2 * (x * z - y * w) * s[0], 0,
            2 * (x * y - z * w) * s[1], (1 - 2 * (x * x + z * z)) * s[1], 2 * (y * z + x * w) * s[1], 0,
            2 * (x * z + y * w) * s[2], 2 * (y * z - x * w) * s[2], (1 - 2 * (x * x + y * y)) * s[2], 0,
            t[0], t[1], t[2], 1};
}
}

Matrix Multiply(const Matrix &a, const Matrix &b)
{
    Matrix m{};
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            for (int k = 0; k < 4; ++k) m[size_t(c * 4 + r)] += a[size_t(k * 4 + r)] * b[size_t(c * 4 + k)];
    return m;
}
std::array<float, 3> TransformPoint(const Matrix &m, const std::array<float, 3> &p)
{
    return {m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12], m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13],
            m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14]};
}
std::array<float, 3> TransformVector(const Matrix &m, const std::array<float, 3> &v)
{
    return {m[0] * v[0] + m[4] * v[1] + m[8] * v[2], m[1] * v[0] + m[5] * v[1] + m[9] * v[2], m[2] * v[0] + m[6] * v[1] + m[10] * v[2]};
}

size_t Document::Count(int accessor) const
{
    Require(accessor >= 0 && size_t(accessor) < accessors.size(), "accessor " + std::to_string(accessor) + " does not exist");
    return accessors[size_t(accessor)].count;
}

namespace
{
// Calls read(element, component, bytes) for each value of an accessor.
template <typename Read> void Each(const Document &doc, int index, int components, Read &&read)
{
    Require(index >= 0 && size_t(index) < doc.accessors.size(), "accessor " + std::to_string(index) + " does not exist");
    const auto &a = doc.accessors[size_t(index)];
    Require(a.components == components, "accessor " + std::to_string(index) + " has " + std::to_string(a.components) +
                                             " components, expected " + std::to_string(components));
    Require(a.view >= 0 && size_t(a.view) < doc.views.size(), "accessor " + std::to_string(index) + " has no buffer view");
    const auto &v = doc.views[size_t(a.view)];
    const size_t size = ComponentSize(a.component), element = size * size_t(components);
    const size_t stride = v.stride ? v.stride : element;
    Require(stride >= element, "buffer view stride smaller than its elements");
    Require(!a.count || a.offset + (a.count - 1) * stride + element <= v.length, "accessor " + std::to_string(index) + " runs past its buffer view");
    for (size_t i = 0; i < a.count; ++i)
        for (int c = 0; c < components; ++c) read(i, c, doc.bin.data() + v.offset + a.offset + i * stride + size_t(c) * size, a);
}
}

std::vector<float> Document::Floats(int accessor, int components) const
{
    std::vector<float> out(Count(accessor) * size_t(components));
    Each(*this, accessor, components, [&](size_t i, int c, const uint8_t *p, const Accessor &a) {
        float value = 0;
        switch (a.component)
        {
        case 5126: { uint32_t bits = Le32(p); std::memcpy(&value, &bits, 4); break; }
        case 5121: value = a.normalized ? p[0] / 255.0f : p[0]; break;
        case 5123: { const auto u = uint16_t(p[0] | p[1] << 8); value = a.normalized ? u / 65535.0f : u; break; }
        case 5120: value = a.normalized ? std::max(int8_t(p[0]) / 127.0f, -1.0f) : int8_t(p[0]); break;
        case 5122: { const auto s = int16_t(p[0] | p[1] << 8); value = a.normalized ? std::max(s / 32767.0f, -1.0f) : s; break; }
        default: throw std::runtime_error("unsupported component type for floats");
        }
        out[i * size_t(components) + size_t(c)] = value;
    });
    return out;
}

std::vector<uint32_t> Document::Integers(int accessor, int components) const
{
    std::vector<uint32_t> out(Count(accessor) * size_t(components));
    Each(*this, accessor, components, [&](size_t i, int c, const uint8_t *p, const Accessor &a) {
        uint32_t value = 0;
        if (a.component == 5121) value = p[0];
        else if (a.component == 5123) value = uint32_t(p[0] | p[1] << 8);
        else if (a.component == 5125) value = Le32(p);
        else throw std::runtime_error("indices and joints must be unsigned integers");
        out[i * size_t(components) + size_t(c)] = value;
    });
    return out;
}

Matrix Document::World(int node) const
{
    Require(node >= 0 && size_t(node) < nodes.size(), "node " + std::to_string(node) + " does not exist");
    Matrix m = nodes[size_t(node)].local;
    for (int parent = nodes[size_t(node)].parent, depth = 0; parent >= 0; parent = nodes[size_t(parent)].parent)
    {
        Require(++depth < 1024, "node tree has a cycle");
        m = Multiply(nodes[size_t(parent)].local, m);
    }
    return m;
}

Document ReadGlb(std::span<const uint8_t> bytes)
{
    Require(bytes.size() >= 20 && Le32(bytes.data()) == 0x46546c67, "not a .glb file (export glTF Binary)");
    Require(Le32(bytes.data() + 4) == 2, "not glTF 2.0");
    const uint32_t jsonLength = Le32(bytes.data() + 12);
    Require(Le32(bytes.data() + 16) == 0x4e4f534a && 20 + uint64_t(jsonLength) <= bytes.size(), "damaged .glb JSON chunk");
    Document doc;
    const size_t binChunk = 20 + size_t(jsonLength);
    if (binChunk + 8 <= bytes.size() && Le32(bytes.data() + binChunk + 4) == 0x004e4942)
    {
        const uint32_t binLength = Le32(bytes.data() + binChunk);
        Require(binChunk + 8 + uint64_t(binLength) <= bytes.size(), "damaged .glb BIN chunk");
        doc.bin.assign(bytes.begin() + std::ptrdiff_t(binChunk + 8), bytes.begin() + std::ptrdiff_t(binChunk + 8 + binLength));
    }
    const auto json = nlohmann::json::parse(bytes.begin() + 20, bytes.begin() + std::ptrdiff_t(binChunk));
    if (json.contains("buffers"))
        for (const auto &buffer : json["buffers"])
            Require(!buffer.contains("uri"), "the model refers to an external .bin file; export it as glTF Binary (.glb)");
    for (const auto &view : json.value("bufferViews", nlohmann::json::array()))
    {
        Require(view.value("buffer", 0) == 0, "only the .glb's own buffer is supported");
        Document::View v{view.value("byteOffset", size_t(0)), view.at("byteLength").get<size_t>(), view.value("byteStride", size_t(0))};
        Require(v.offset + v.length <= doc.bin.size(), "buffer view runs past the .glb data");
        doc.views.push_back(v);
    }
    for (const auto &accessor : json.value("accessors", nlohmann::json::array()))
    {
        Require(!accessor.contains("sparse"), "sparse accessors are not supported");
        Document::Accessor a;
        a.view = accessor.value("bufferView", -1);
        a.offset = accessor.value("byteOffset", size_t(0));
        a.component = accessor.at("componentType").get<int>();
        a.components = Components(accessor.at("type").get<std::string>());
        a.count = accessor.at("count").get<size_t>();
        a.normalized = accessor.value("normalized", false);
        ComponentSize(a.component);
        doc.accessors.push_back(a);
    }
    for (const auto &material : json.value("materials", nlohmann::json::array())) doc.materials.push_back(material.value("name", std::string{}));
    for (const auto &mesh : json.value("meshes", nlohmann::json::array()))
    {
        std::vector<Primitive> primitives;
        for (const auto &primitive : mesh.at("primitives"))
        {
            Require(primitive.value("mode", 4) == 4, "mesh \"" + mesh.value("name", std::string{}) + "\" has a primitive that is not triangles");
            Primitive p;
            for (const auto &[name, value] : primitive.at("attributes").items()) p.attributes[name] = value.get<int>();
            p.indices = primitive.value("indices", -1);
            p.material = primitive.value("material", -1);
            primitives.push_back(std::move(p));
        }
        doc.meshes.push_back(std::move(primitives));
    }
    const auto &nodes = json.value("nodes", nlohmann::json::array());
    doc.nodes.resize(nodes.size());
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        auto &n = doc.nodes[i];
        n.name = nodes[i].value("name", std::string{});
        n.mesh = nodes[i].value("mesh", -1);
        n.skin = nodes[i].value("skin", -1);
        n.local = FromTrs(nodes[i]);
        for (const auto &child : nodes[i].value("children", nlohmann::json::array()))
        {
            const auto c = child.get<size_t>();
            Require(c < nodes.size() && doc.nodes[c].parent < 0, "node tree is not a tree");
            doc.nodes[c].parent = int(i);
        }
    }
    for (const auto &skin : json.value("skins", nlohmann::json::array()))
    {
        Skin s;
        for (const auto &joint : skin.at("joints")) s.joints.push_back(joint.get<int>());
        for (const auto j : s.joints) Require(j >= 0 && size_t(j) < doc.nodes.size(), "skin joint is not a node");
        if (skin.contains("inverseBindMatrices"))
        {
            const auto values = doc.Floats(skin["inverseBindMatrices"].get<int>(), 16);
            Require(values.size() == s.joints.size() * 16, "inverse bind matrices do not match the joints");
            for (size_t j = 0; j < s.joints.size(); ++j)
            {
                Matrix m{};
                std::copy_n(values.begin() + std::ptrdiff_t(j * 16), 16, m.begin());
                s.inverseBind.push_back(m);
            }
        }
        else s.inverseBind.assign(s.joints.size(), Matrix{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});
        doc.skins.push_back(std::move(s));
    }
    for (const auto &n : doc.nodes)
    {
        Require(n.mesh < int(doc.meshes.size()), "node refers to a missing mesh");
        Require(n.skin < int(doc.skins.size()), "node refers to a missing skin");
    }
    return doc;
}
}
