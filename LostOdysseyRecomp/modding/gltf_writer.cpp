#include "gltf_writer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace modding::gltf
{
namespace
{
constexpr int kByte = 5121, kShort = 5123, kInt = 5125, kFloat = 5126;
constexpr int kVertexTarget = 34962, kIndexTarget = 34963;

std::string Json(std::string_view value)
{
    std::string out = "\"";
    for (const unsigned char c : value)
    {
        if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
        else if (c < 0x20) { char hex[8]; std::snprintf(hex, sizeof(hex), "\\u%04x", c); out += hex; }
        else out += char(c);
    }
    return out + "\"";
}

// A relative URI: unreserved characters and '/' stay, the rest is percent-encoded.
std::string Uri(std::string_view path)
{
    std::string out;
    for (const unsigned char c : path)
    {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || std::strchr("-._~/", c)) out += char(c);
        else { char hex[4]; std::snprintf(hex, sizeof(hex), "%%%02X", c); out += hex; }
    }
    return out;
}

std::string Number(float value)
{
    char text[32];
    std::snprintf(text, sizeof(text), "%.9g", std::isfinite(value) ? double(value) : 0.0);
    return text;
}

// glTF requires unit rotations; some of the game's are slightly off.
std::array<float, 4> Unit(const std::array<float, 4> &q)
{
    const float length = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    return length > 1e-6f ? std::array<float, 4>{q[0] / length, q[1] / length, q[2] / length, q[3] / length}
                          : std::array<float, 4>{0, 0, 0, 1};
}

// Column-major 4x4, as glTF stores matrices.
using Matrix = std::array<float, 16>;
Matrix FromBone(const Bone &bone)
{
    const auto [x, y, z, w] = Unit(bone.rotation);
    return {1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w), 0,
            2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w), 0,
            2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y), 0,
            bone.translation[0], bone.translation[1], bone.translation[2], 1};
}
Matrix Multiply(const Matrix &a, const Matrix &b)
{
    Matrix m{};
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            for (int k = 0; k < 4; ++k) m[c * 4 + r] += a[k * 4 + r] * b[c * 4 + k];
    return m;
}
// Inverse of a rotation plus translation.
Matrix InverseRigid(const Matrix &m)
{
    Matrix inv{};
    for (int c = 0; c < 3; ++c)
        for (int r = 0; r < 3; ++r) inv[c * 4 + r] = m[r * 4 + c];
    for (int r = 0; r < 3; ++r) inv[12 + r] = -(inv[r] * m[12] + inv[4 + r] * m[13] + inv[8 + r] * m[14]);
    inv[15] = 1;
    return inv;
}

struct Builder
{
    std::vector<uint8_t> bin;
    std::string views, accessors;
    int viewCount = 0, accessorCount = 0;

    int View(const void *data, size_t size, int target)
    {
        while (bin.size() % 4) bin.push_back(0);
        const auto offset = bin.size();
        bin.resize(offset + size);
        if (size) std::memcpy(bin.data() + offset, data, size);
        views += std::string(viewCount ? "," : "") + "{\"buffer\":0,\"byteOffset\":" + std::to_string(offset) +
                 ",\"byteLength\":" + std::to_string(size) + (target ? ",\"target\":" + std::to_string(target) : "") + "}";
        return viewCount++;
    }
    int Accessor(int view, size_t byteOffset, int component, size_t count, const char *type, bool normalized = false,
                 const std::string &bounds = {})
    {
        accessors += std::string(accessorCount ? "," : "") + "{\"bufferView\":" + std::to_string(view) +
                     (byteOffset ? ",\"byteOffset\":" + std::to_string(byteOffset) : "") +
                     ",\"componentType\":" + std::to_string(component) + (normalized ? ",\"normalized\":true" : "") +
                     ",\"count\":" + std::to_string(count) + ",\"type\":\"" + type + "\"" + bounds + "}";
        return accessorCount++;
    }
    template <typename T> int Attribute(const std::vector<T> &values, int component, const char *type, bool normalized = false,
                                        const std::string &bounds = {})
    {
        return Accessor(View(values.data(), values.size() * sizeof(T), kVertexTarget), 0, component, values.size(), type,
                        normalized, bounds);
    }
};
}

std::vector<uint8_t> Write(const Mesh &mesh, const std::string &name)
{
    Builder b;
    const auto count = mesh.positions.size();
    std::array<float, 3> lo{}, hi{};
    if (count) lo = hi = mesh.positions[0];
    for (const auto &p : mesh.positions)
        for (int i = 0; i < 3; ++i) { lo[i] = std::min(lo[i], p[i]); hi[i] = std::max(hi[i], p[i]); }
    const auto bounds = ",\"min\":[" + Number(lo[0]) + "," + Number(lo[1]) + "," + Number(lo[2]) + "],\"max\":[" +
                        Number(hi[0]) + "," + Number(hi[1]) + "," + Number(hi[2]) + "]";
    std::string attributes = "\"POSITION\":" + std::to_string(b.Attribute(mesh.positions, kFloat, "VEC3", false, bounds));
    if (!mesh.normals.empty())
    {
        // glTF requires unit normals; the packed ones are only close.
        auto normals = mesh.normals;
        for (auto &n : normals)
        {
            const float length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            n = length > 1e-6f ? std::array<float, 3>{n[0] / length, n[1] / length, n[2] / length} : std::array<float, 3>{0, 1, 0};
        }
        attributes += ",\"NORMAL\":" + std::to_string(b.Attribute(normals, kFloat, "VEC3"));
    }
    for (size_t set = 0; set < mesh.uvs.size(); ++set)
        attributes += ",\"TEXCOORD_" + std::to_string(set) + "\":" + std::to_string(b.Attribute(mesh.uvs[set], kFloat, "VEC2"));
    if (!mesh.colors.empty()) attributes += ",\"COLOR_0\":" + std::to_string(b.Attribute(mesh.colors, kByte, "VEC4", true));
    const bool skinned = !mesh.bones.empty() && !mesh.joints.empty();
    if (skinned)
    {
        int joints;
        if (mesh.bones.size() <= 256)
        {
            std::vector<std::array<uint8_t, 4>> narrow(mesh.joints.size());
            for (size_t i = 0; i < narrow.size(); ++i)
                for (int k = 0; k < 4; ++k) narrow[i][k] = uint8_t(mesh.joints[i][k]);
            joints = b.Attribute(narrow, kByte, "VEC4");
        }
        else joints = b.Attribute(mesh.joints, kShort, "VEC4");
        attributes += ",\"JOINTS_0\":" + std::to_string(joints) +
                      ",\"WEIGHTS_0\":" + std::to_string(b.Attribute(mesh.weights, kFloat, "VEC4"));
    }

    // One index view; each section is an accessor into it.
    const bool wide = count > 0xffff;
    int indexView;
    if (wide) indexView = b.View(mesh.indices.data(), mesh.indices.size() * 4, kIndexTarget);
    else
    {
        std::vector<uint16_t> narrow(mesh.indices.begin(), mesh.indices.end());
        indexView = b.View(narrow.data(), narrow.size() * 2, kIndexTarget);
    }
    std::string primitives;
    for (const auto &section : mesh.sections)
    {
        if (!section.indexCount) continue;
        const auto indices = b.Accessor(indexView, size_t(section.firstIndex) * (wide ? 4 : 2), wide ? kInt : kShort,
                                        section.indexCount, "SCALAR");
        primitives += std::string(primitives.empty() ? "" : ",") + "{\"attributes\":{" + attributes +
                      "},\"indices\":" + std::to_string(indices) + ",\"material\":" + std::to_string(section.material) + "}";
    }
    std::string materials, images, textures;
    for (size_t i = 0; i < mesh.images.size(); ++i)
    {
        images += std::string(i ? "," : "") + "{\"uri\":" + Json(Uri(mesh.images[i])) + "}";
        textures += std::string(i ? "," : "") + "{\"sampler\":0,\"source\":" + std::to_string(i) + "}";
    }
    for (const auto &material : mesh.materials)
    {
        const bool textured = material.baseColor >= 0 && size_t(material.baseColor) < mesh.images.size();
        materials += std::string(materials.empty() ? "" : ",") + "{\"name\":" + Json(material.name) +
                     ",\"pbrMetallicRoughness\":{\"metallicFactor\":0" +
                     (textured ? ",\"baseColorTexture\":{\"index\":" + std::to_string(material.baseColor) + "}" : "") + "}" +
                     (textured && material.mask ? ",\"alphaMode\":\"MASK\"" : "") + "}";
    }

    // Node 0 is the mesh; bones follow as nodes 1..n.
    std::string nodes = "{\"name\":" + Json(name) + ",\"mesh\":0" + (skinned ? ",\"skin\":0" : "") + "}";
    std::string sceneNodes = "0", skins;
    if (skinned)
    {
        std::vector<Matrix> world(mesh.bones.size()), inverse(mesh.bones.size());
        std::vector<std::string> children(mesh.bones.size());
        for (size_t i = 0; i < mesh.bones.size(); ++i)
        {
            const auto &bone = mesh.bones[i];
            const auto local = FromBone(bone);
            world[i] = bone.parent >= 0 ? Multiply(world[size_t(bone.parent)], local) : local;
            inverse[i] = InverseRigid(world[i]);
            const auto node = std::to_string(i + 1);
            if (bone.parent >= 0) children[size_t(bone.parent)] += (children[size_t(bone.parent)].empty() ? "" : ",") + node;
            else sceneNodes += "," + node;
        }
        std::string joints;
        for (size_t i = 0; i < mesh.bones.size(); ++i)
        {
            const auto &bone = mesh.bones[i];
            const auto rotation = Unit(bone.rotation);
            nodes += ",{\"name\":" + Json(bone.name) + (children[i].empty() ? "" : ",\"children\":[" + children[i] + "]") +
                     ",\"translation\":[" + Number(bone.translation[0]) + "," + Number(bone.translation[1]) + "," +
                     Number(bone.translation[2]) + "],\"rotation\":[" + Number(rotation[0]) + "," + Number(rotation[1]) + "," +
                     Number(rotation[2]) + "," + Number(rotation[3]) + "]}";
            joints += std::string(i ? "," : "") + std::to_string(i + 1);
        }
        const auto matrices = b.Accessor(b.View(inverse.data(), inverse.size() * sizeof(Matrix), 0), 0, kFloat,
                                         inverse.size(), "MAT4");
        skins = ",\"skins\":[{\"inverseBindMatrices\":" + std::to_string(matrices) + ",\"joints\":[" + joints + "]}]";
    }
    std::string animations;
    for (const auto &animation : skinned ? mesh.animations : std::vector<Animation>{})
    {
        std::string samplers, channels;
        int index = 0;
        for (const auto &channel : animation.channels)
        {
            const size_t width = channel.rotation ? 4 : 3;
            if (channel.times.empty() || channel.values.size() != channel.times.size() * width ||
                channel.bone >= mesh.bones.size())
                continue;
            const auto input = b.Accessor(b.View(channel.times.data(), channel.times.size() * 4, 0), 0, kFloat,
                                          channel.times.size(), "SCALAR", false,
                                          ",\"min\":[" + Number(channel.times.front()) + "],\"max\":[" +
                                              Number(channel.times.back()) + "]");
            auto values = channel.values;
            if (channel.rotation)
                for (size_t k = 0; k < values.size(); k += 4)
                {
                    const auto q = Unit({values[k], values[k + 1], values[k + 2], values[k + 3]});
                    std::copy(q.begin(), q.end(), values.begin() + std::ptrdiff_t(k));
                }
            const auto output = b.Accessor(b.View(values.data(), values.size() * 4, 0), 0, kFloat,
                                           channel.times.size(), channel.rotation ? "VEC4" : "VEC3");
            samplers += std::string(index ? "," : "") + "{\"input\":" + std::to_string(input) + ",\"output\":" +
                        std::to_string(output) + "}";
            channels += std::string(index ? "," : "") + "{\"sampler\":" + std::to_string(index) +
                        ",\"target\":{\"node\":" + std::to_string(channel.bone + 1) + ",\"path\":\"" +
                        (channel.rotation ? "rotation" : "translation") + "\"}}";
            ++index;
        }
        if (index)
            animations += std::string(animations.empty() ? "" : ",") + "{\"name\":" + Json(animation.name) +
                          ",\"samplers\":[" + samplers + "],\"channels\":[" + channels + "]}";
    }
    while (b.bin.size() % 4) b.bin.push_back(0);

    std::string json = "{\"asset\":{\"version\":\"2.0\",\"generator\":\"LostOdysseyRecomp --export-assets\"},"
                       "\"scene\":0,\"scenes\":[{\"nodes\":[" + sceneNodes + "]}],\"nodes\":[" + nodes +
                       "],\"meshes\":[{\"name\":" + Json(name) + ",\"primitives\":[" + primitives + "]}],\"materials\":[" +
                       materials + "]" + skins + (animations.empty() ? "" : ",\"animations\":[" + animations + "]") +
                       (images.empty() ? "" : ",\"images\":[" + images + "],\"textures\":[" + textures +
                                                  "],\"samplers\":[{\"magFilter\":9729,\"minFilter\":9987}]") +
                       ",\"accessors\":[" + b.accessors + "],\"bufferViews\":[" + b.views +
                       "],\"buffers\":[{\"byteLength\":" + std::to_string(b.bin.size()) + "}]}";
    while (json.size() % 4) json += ' ';

    std::vector<uint8_t> glb;
    auto u32 = [&](uint32_t value) { for (int i = 0; i < 4; ++i) glb.push_back(uint8_t(value >> (8 * i))); };
    u32(0x46546c67); u32(2); u32(uint32_t(12 + 8 + json.size() + 8 + b.bin.size()));
    u32(uint32_t(json.size())); u32(0x4e4f534a);
    glb.insert(glb.end(), json.begin(), json.end());
    u32(uint32_t(b.bin.size())); u32(0x004e4942);
    glb.insert(glb.end(), b.bin.begin(), b.bin.end());
    return glb;
}
}
