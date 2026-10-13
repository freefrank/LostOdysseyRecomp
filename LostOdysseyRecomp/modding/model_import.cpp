#include "model_import.h"
#include "gltf_reader.h"
#include "ue3_package.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <bit>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>

namespace modding::model_import
{
namespace
{
using ue3::Package;
using ue3::Reader;
using Vec3 = std::array<float, 3>;

// The game's GPU skinning takes 75 bones per draw (the largest chunk on the discs).
constexpr size_t kMaxChunkBones = 75;
constexpr size_t kMaxVertices = 0xffff, kMaxSectionTriangles = 0xffff;
constexpr size_t kMaxStaticVertices = 0x10000; // 16-bit indices, one vertex buffer
constexpr float kCentimetres = 100.0f;

void Require(bool ok, const std::string &message) { if (!ok) throw std::runtime_error(message); }

std::string Lower(std::string value)
{
    for (auto &c : value) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return value;
}

struct Writer
{
    std::vector<uint8_t> bytes;
    void U8(uint8_t v) { bytes.push_back(v); }
    void U16(uint16_t v) { bytes.insert(bytes.end(), {uint8_t(v >> 8), uint8_t(v)}); }
    void U32(uint32_t v) { bytes.insert(bytes.end(), {uint8_t(v >> 24), uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v)}); }
    void F32(float v) { U32(std::bit_cast<uint32_t>(v)); }
    void Raw(std::span<const uint8_t> b) { bytes.insert(bytes.end(), b.begin(), b.end()); }
};
void PutBe32(std::vector<uint8_t> &out, size_t at, uint32_t v)
{
    for (int i = 0; i < 4; ++i) out[at + size_t(i)] = uint8_t(v >> (24 - 8 * i));
}

// ---- The original SkeletalMesh ---------------------------------------------
// Byte positions in the package of the parts that are kept as they are.

struct Original
{
    size_t propertiesBegin = 0, propertiesEnd = 0;
    size_t lodInfoSizeField = 0, lodInfoValue = 0, lodInfoSize = 0; // 0: no LODInfo property
    std::array<float, 7> bounds{};                                   // origin, extent, radius
    size_t materialsBegin = 0, materialsEnd = 0;                     // count + references, then origin and rotation
    std::vector<std::string> materials;
    size_t skeletonBegin = 0, skeletonEnd = 0;                       // bones and SkeletalDepth
    std::vector<std::string> bones;
    size_t requiredBegin = 0, requiredEnd = 0;                       // LOD 0's RequiredBones array
    std::vector<uint16_t> activeBones;
    size_t tailBegin = 0, end = 0;                                   // after the LODs (NameIndexMap)
};

// One LOD as the exporter reads it (asset_export.cpp ReadSkeletalMesh), only skipped.
void SkipLod(Reader &r, Original *first)
{
    ue3::SkipArray(r, 10);                    // Sections.
    ue3::SkipArray(r, 2);                     // Indices.
    ue3::SkipArray(r, 2);                     // f68.
    const auto active = ue3::Count(r, 2);     // ActiveBoneIndices.
    for (uint32_t i = 0; i < active; ++i) { const auto bone = r.U16(); if (first) first->activeBones.push_back(bone); }
    ue3::SkipArray(r, 1);                     // f74.
    for (uint32_t c = 0, n = ue3::Count(r, 28); c < n; ++c)
    {
        r.U32(); ue3::SkipArray(r, 33); ue3::SkipArray(r, 40); ue3::SkipArray(r, 2);
        r.U32(); r.U32(); r.U32();
    }
    r.U32(); r.U32();                         // f80, NumVertices.
    ue3::SkipArray(r, 16);                    // Edges.
    if (first) first->requiredBegin = r.pos;
    ue3::SkipArray(r, 1);                     // RequiredBones.
    if (first) first->requiredEnd = r.pos;
    const auto flags = r.U32(); r.U32(); const auto stored = r.U32(); r.U32();
    if (!(flags & 0x21)) r.Take(stored);
    ue3::SkipArray(r, 40);                    // GPU vertices.
}

Original ReadOriginal(const Package &package, const ue3::Export &e)
{
    Original o;
    auto r = ue3::ObjectReader(package, e);
    o.propertiesBegin = r.pos;
    r.I32(); // Net index.
    for (size_t count = 0;; ++count)
    {
        Require(count < 65536, "too many properties");
        const auto name = package.Name(r);
        if (name == "None") break;
        const auto type = package.Name(r);
        const auto sizeField = r.pos;
        const auto size = r.U32(); r.U32();
        if (type == "StructProperty") package.Name(r);
        if (type == "BoolProperty") r.U32();
        if (name == "LODInfo" && type == "ArrayProperty") { o.lodInfoSizeField = sizeField; o.lodInfoValue = r.pos; o.lodInfoSize = size; }
        r.Take(size);
    }
    o.propertiesEnd = r.pos;
    for (auto &f : o.bounds) f = r.F32();
    o.materialsBegin = r.pos;
    for (uint32_t i = 0, n = ue3::Count(r, 4); i < n; ++i) o.materials.push_back(package.RefName(r.I32()));
    r.Take(24); // Origin, RotOrigin.
    o.materialsEnd = r.pos;
    o.skeletonBegin = r.pos;
    for (uint32_t i = 0, n = ue3::Count(r, 48); i < n; ++i)
    {
        o.bones.push_back(package.Name(r));
        r.Take(40);
    }
    r.I32(); // SkeletalDepth.
    o.skeletonEnd = r.pos;
    const auto lods = ue3::Count(r, 4);
    Require(lods >= 1 && !o.bones.empty(), "the original has no LOD or no skeleton");
    for (uint32_t i = 0; i < lods; ++i) SkipLod(r, i ? nullptr : &o);
    o.tailBegin = r.pos;
    o.end = size_t(e.offset) + e.size;
    return o;
}

// ---- The model from the .glb, in Unreal space ------------------------------

constexpr size_t kMaxUvSets = 8;
using Uv = std::array<float, 2>;
struct Vertex
{
    Vec3 position{}, normal{}, tangent{}, binormal{};
    std::array<Uv, kMaxUvSets> uvs{}; // skeletal meshes have one set
    uint32_t color = 0xffffffffu;     // ARGB, static meshes
    uint16_t borrowed = 0;            // static meshes: UV sets (bit n) and colour (bit 8) the file lacks
    std::array<uint16_t, 4> bones{};  // skeleton indices
    std::array<float, 4> weights{};   // largest first, sum 1
};
struct Triangle { std::array<uint32_t, 3> v{}; uint16_t slot = 0; };
struct Model { std::vector<Vertex> vertices; std::vector<Triangle> triangles; };

Vec3 Normalize(Vec3 v, Vec3 fallback = {0, 0, 1})
{
    const float length = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    return length > 1e-12f && std::isfinite(length) ? Vec3{v[0] / length, v[1] / length, v[2] / length} : fallback;
}
Vec3 Sub(const Vec3 &a, const Vec3 &b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
Vec3 Add(const Vec3 &a, const Vec3 &b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
Vec3 Scale(const Vec3 &a, float s) { return {a[0] * s, a[1] * s, a[2] * s}; }
float Dot(const Vec3 &a, const Vec3 &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 Cross(const Vec3 &a, const Vec3 &b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
// glTF (right-handed, Y up, metres) to Unreal (left-handed, Z up, centimetres): the exporter's axes reversed.
Vec3 ToUnreal(const Vec3 &v, float scale) { return {v[0] * scale, v[2] * scale, v[1] * scale}; }

// Blender adds .001 to a duplicated material's name.
std::string MaterialName(std::string name)
{
    const auto dot = name.rfind('.');
    if (dot != std::string::npos && dot + 4 == name.size() &&
        std::all_of(name.begin() + std::ptrdiff_t(dot) + 1, name.end(), [](char c) { return c >= '0' && c <= '9'; }))
        name.resize(dot);
    return Lower(name);
}

// The original material slot a primitive's material names.
uint16_t SlotOf(const gltf::Document &doc, const std::vector<std::string> &materials, int material)
{
    if (material < 0 && materials.size() == 1) return 0;
    Require(material >= 0 && size_t(material) < doc.materials.size(), "a mesh part has no material; give it one of the original material names");
    const auto name = MaterialName(doc.materials[size_t(material)]);
    for (size_t i = 0; i < materials.size(); ++i)
        if (MaterialName(materials[i]) == name) return uint16_t(i);
    std::string names;
    for (const auto &m : materials) names += (names.empty() ? "" : ", ") + m;
    throw std::runtime_error("material \"" + doc.materials[size_t(material)] + "\" is not one of the original's (" + names + ")");
}

// A primitive's triangles, its vertices starting at base; degenerate ones dropped.
void AppendTriangles(const gltf::Document &doc, const gltf::Primitive &primitive, size_t count, uint32_t base, uint16_t slot, Model &model)
{
    std::vector<uint32_t> indices;
    if (primitive.indices >= 0) indices = doc.Integers(primitive.indices, 1);
    else for (uint32_t i = 0; i < count; ++i) indices.push_back(i);
    Require(indices.size() % 3 == 0, "a mesh part's index count is not a multiple of 3");
    for (size_t t = 0; t < indices.size(); t += 3)
    {
        Triangle triangle{{indices[t], indices[t + 1], indices[t + 2]}, slot};
        for (auto &i : triangle.v) { Require(i < count, "an index is outside its mesh part"); i += base; }
        if (triangle.v[0] == triangle.v[1] || triangle.v[1] == triangle.v[2] || triangle.v[0] == triangle.v[2]) continue;
        model.triangles.push_back(triangle);
    }
}

// Primitives that share their vertex accessors (the exporter's sections do)
// share their vertices: the first vertex read for a node's attribute set.
using ReadVertices = std::map<std::pair<size_t, std::map<std::string, int>>, uint32_t>;

// Removes vertices no triangle uses, keeping the order of the rest.
void DropUnused(Model &model)
{
    std::vector<uint32_t> remap(model.vertices.size(), UINT32_MAX);
    for (const auto &t : model.triangles) for (const auto i : t.v) remap[i] = 0;
    uint32_t next = 0;
    for (size_t i = 0; i < remap.size(); ++i)
        if (remap[i] == 0) { remap[i] = next; model.vertices[next++] = model.vertices[i]; }
    model.vertices.resize(next);
    for (auto &t : model.triangles) for (auto &i : t.v) i = remap[i];
}

// Splits mirrored UV seams and fills in normals, tangents and binormals from UV
// set 0. A split that would take the model past maxVertices is left out: the
// seam's tangents are averaged instead.
void FinishModel(Model &model, size_t maxVertices = std::numeric_limits<size_t>::max())
{
    // A vertex on a mirrored UV seam serves triangles of both UV handedness; their
    // tangents would cancel, so the mirrored side gets its own copy.
    std::vector<uint8_t> sides(model.vertices.size());
    std::vector<bool> mirrored(model.triangles.size());
    for (size_t t = 0; t < model.triangles.size(); ++t)
    {
        const auto &tri = model.triangles[t];
        const auto &a = model.vertices[tri.v[0]].uvs[0], &b = model.vertices[tri.v[1]].uvs[0], &c = model.vertices[tri.v[2]].uvs[0];
        mirrored[t] = (b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1]) < 0;
        for (const auto i : tri.v) sides[i] |= mirrored[t] ? 2 : 1;
    }
    size_t seams = 0;
    for (const auto side : sides) seams += side == 3;
    const bool split = model.vertices.size() + seams <= maxVertices;
    std::map<uint32_t, uint32_t> copies;
    for (size_t t = 0; split && t < model.triangles.size(); ++t)
        if (mirrored[t])
            for (auto &i : model.triangles[t].v)
                if (sides[i] == 3)
                {
                    const auto [it, added] = copies.emplace(i, uint32_t(model.vertices.size()));
                    if (added) model.vertices.push_back(model.vertices[i]);
                    i = it->second;
                }

    // Unreal-space faces wind the other way from glTF's (the axis swap mirrors them).
    auto faceNormal = [&](const Triangle &t) {
        const auto &a = model.vertices[t.v[0]].position, &b = model.vertices[t.v[1]].position, &c = model.vertices[t.v[2]].position;
        return Cross(Sub(c, a), Sub(b, a));
    };
    std::vector<Vec3> faceSum(model.vertices.size()), tangents(model.vertices.size()), binormals(model.vertices.size());
    for (const auto &t : model.triangles)
    {
        const auto &a = model.vertices[t.v[0]], &b = model.vertices[t.v[1]], &c = model.vertices[t.v[2]];
        const auto e1 = Sub(b.position, a.position), e2 = Sub(c.position, a.position);
        const float du1 = b.uvs[0][0] - a.uvs[0][0], dv1 = b.uvs[0][1] - a.uvs[0][1];
        const float du2 = c.uvs[0][0] - a.uvs[0][0], dv2 = c.uvs[0][1] - a.uvs[0][1];
        const float det = du1 * dv2 - du2 * dv1;
        const auto face = faceNormal(t);
        for (const auto i : t.v) faceSum[i] = Add(faceSum[i], face);
        if (std::abs(det) < 1e-12f) continue;
        // The stored tangent follows dP/du and the binormal dP/dv (checked on the discs' meshes).
        const auto tangent = Scale(Sub(Scale(e1, dv2), Scale(e2, dv1)), 1 / det);
        const auto binormal = Scale(Sub(Scale(e2, du1), Scale(e1, du2)), 1 / det);
        for (const auto i : t.v) { tangents[i] = Add(tangents[i], tangent); binormals[i] = Add(binormals[i], binormal); }
    }
    for (size_t i = 0; i < model.vertices.size(); ++i)
    {
        auto &v = model.vertices[i];
        // Parts without normals (zero after reading) take their faces'.
        if (Dot(v.normal, v.normal) < 0.5f) v.normal = Normalize(faceSum[i]);
        const auto &n = v.normal;
        auto t = Sub(tangents[i], Scale(n, Dot(n, tangents[i])));
        const auto fallback = Normalize(Cross(n, std::abs(n[2]) < 0.9f ? Vec3{0, 0, 1} : Vec3{1, 0, 0}));
        v.tangent = Normalize(t, fallback);
        auto b = Sub(Sub(binormals[i], Scale(n, Dot(n, binormals[i]))), Scale(v.tangent, Dot(v.tangent, binormals[i])));
        v.binormal = Normalize(b, Cross(n, v.tangent));
    }
}

Model ReadModel(const gltf::Document &doc, const Original &original)
{
    std::map<std::string, uint16_t> bones;
    for (size_t i = 0; i < original.bones.size(); ++i) bones.emplace(Lower(original.bones[i]), uint16_t(i));
    auto boneOf = [&](const std::string &name) -> int {
        const auto it = bones.find(Lower(name));
        return it == bones.end() ? -1 : it->second;
    };

    Model model;
    for (size_t n = 0; n < doc.nodes.size(); ++n)
    {
        const auto &node = doc.nodes[n];
        if (node.mesh < 0) continue;
        // A skinned mesh moves with its joints (bound at rest by World x inverse bind); an
        // unskinned one follows its node and the nearest bone above it.
        std::vector<gltf::Matrix> skinning;
        std::vector<int> jointBone;
        int rigidBone = 0;
        const auto world = doc.World(int(n));
        if (node.skin >= 0)
        {
            const auto &skin = doc.skins[size_t(node.skin)];
            for (size_t j = 0; j < skin.joints.size(); ++j)
            {
                const auto &joint = doc.nodes[size_t(skin.joints[j])];
                const int bone = boneOf(joint.name);
                Require(bone >= 0, "bone \"" + joint.name + "\" is not in the original skeleton; keep the original armature");
                jointBone.push_back(bone);
                skinning.push_back(gltf::Multiply(doc.World(skin.joints[j]), skin.inverseBind[j]));
            }
        }
        else
            for (int p = node.parent; p >= 0; p = doc.nodes[size_t(p)].parent)
                if (const int bone = boneOf(doc.nodes[size_t(p)].name); bone >= 0) { rigidBone = bone; break; }

        for (const auto &primitive : doc.meshes[size_t(node.mesh)])
        {
            const auto attribute = [&](const char *name) {
                const auto it = primitive.attributes.find(name);
                return it == primitive.attributes.end() ? -1 : it->second;
            };
            const int position = attribute("POSITION"), normal = attribute("NORMAL"), uv = attribute("TEXCOORD_0");
            Require(position >= 0, "a mesh part has no positions");
            Require(uv >= 0, "a mesh part has no UV map; the game needs one");
            const uint16_t slot = SlotOf(doc, original.materials, primitive.material);
            const auto positions = doc.Floats(position, 3);
            const auto uvs = doc.Floats(uv, 2);
            const auto normals = normal >= 0 ? doc.Floats(normal, 3) : std::vector<float>{};
            const size_t count = positions.size() / 3;
            Require(uvs.size() == count * 2 && (normals.empty() || normals.size() == count * 3), "a mesh part's attributes differ in length");
            std::vector<std::vector<uint32_t>> joints;
            std::vector<std::vector<float>> weights;
            if (node.skin >= 0)
                for (int set = 0;; ++set)
                {
                    const int j = attribute(("JOINTS_" + std::to_string(set)).c_str()), w = attribute(("WEIGHTS_" + std::to_string(set)).c_str());
                    if (j < 0 || w < 0) break;
                    joints.push_back(doc.Integers(j, 4));
                    weights.push_back(doc.Floats(w, 4));
                    Require(joints.back().size() == count * 4 && weights.back().size() == count * 4, "joint data differs in length from the positions");
                }
            Require(node.skin < 0 || !joints.empty(), "a skinned mesh part has no JOINTS_0/WEIGHTS_0");

            const auto base = uint32_t(model.vertices.size());
            for (size_t i = 0; i < count; ++i)
            {
                Vertex v;
                const Vec3 p{positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2]};
                const Vec3 nrm = normals.empty() ? Vec3{0, 0, 0} : Vec3{normals[i * 3], normals[i * 3 + 1], normals[i * 3 + 2]};
                Vec3 placed = p, turned = nrm;
                if (node.skin >= 0)
                {
                    std::map<uint16_t, float> influence;
                    std::vector<std::pair<float, gltf::Matrix>> blend;
                    for (size_t s = 0; s < joints.size(); ++s)
                        for (int k = 0; k < 4; ++k)
                        {
                            const float w = weights[s][i * 4 + size_t(k)];
                            if (!(w > 0)) continue;
                            const auto joint = joints[s][i * 4 + size_t(k)];
                            Require(joint < jointBone.size(), "a vertex refers to a joint the skin does not have");
                            influence[uint16_t(jointBone[joint])] += w;
                            blend.emplace_back(w, skinning[joint]);
                        }
                    Require(!influence.empty(), "a vertex has no bone weights");
                    float total = 0;
                    for (const auto &[w, m] : blend) total += w;
                    placed = {0, 0, 0}; turned = {0, 0, 0};
                    for (const auto &[w, m] : blend)
                    {
                        placed = Add(placed, Scale(gltf::TransformPoint(m, p), w / total));
                        turned = Add(turned, Scale(gltf::TransformVector(m, nrm), w / total));
                    }
                    std::vector<std::pair<float, uint16_t>> sorted;
                    for (const auto &[bone, w] : influence) sorted.emplace_back(w, bone);
                    std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
                    sorted.resize(std::min<size_t>(sorted.size(), 4));
                    float kept = 0;
                    for (const auto &s : sorted) kept += s.first;
                    for (size_t k = 0; k < sorted.size(); ++k) { v.bones[k] = sorted[k].second; v.weights[k] = sorted[k].first / kept; }
                }
                else
                {
                    placed = gltf::TransformPoint(world, p);
                    turned = gltf::TransformVector(world, nrm);
                    v.bones[0] = uint16_t(rigidBone);
                    v.weights[0] = 1;
                }
                v.position = ToUnreal(placed, kCentimetres);
                v.normal = Normalize(ToUnreal(turned, 1), {0, 0, 0});
                v.uvs[0] = {uvs[i * 2], uvs[i * 2 + 1]};
                for (const auto f : v.position) Require(std::isfinite(f), "a vertex position is not a number");
                model.vertices.push_back(v);
            }
            AppendTriangles(doc, primitive, count, base, slot, model);
        }
    }
    Require(!model.triangles.empty(), "the file has no triangles");
    FinishModel(model);
    return model;
}

// ---- The new LOD -------------------------------------------------------------

uint32_t PackNormal(const Vec3 &v)
{
    auto byte = [](float f) { return uint32_t(std::clamp(std::lround((f + 1) * 127.5f), 0L, 255L)); };
    return byte(v[0]) | byte(v[1]) << 8 | byte(v[2]) << 16; // W stays 0, as on the discs.
}

// Four weight bytes summing to 255, with the skeleton bones they belong to;
// zeros first, so a single influence sits in the last byte as on the discs.
struct Influences { std::array<uint16_t, 4> bones{}; std::array<uint8_t, 4> weights{}; int count = 0; };
Influences Quantize(const Vertex &v)
{
    std::array<int, 4> bytes{};
    int total = 0, largest = 0;
    for (int k = 0; k < 4; ++k)
    {
        bytes[size_t(k)] = int(std::lround(v.weights[size_t(k)] * 255));
        total += bytes[size_t(k)];
        if (bytes[size_t(k)] > bytes[size_t(largest)]) largest = k;
    }
    bytes[size_t(largest)] += 255 - total;
    std::vector<std::pair<int, uint16_t>> kept;
    for (int k = 0; k < 4; ++k) if (bytes[size_t(k)] > 0) kept.emplace_back(bytes[size_t(k)], v.bones[size_t(k)]);
    std::sort(kept.begin(), kept.end());
    Influences out;
    out.count = int(kept.size());
    for (size_t k = 0; k < kept.size(); ++k)
    {
        out.weights[4 - kept.size() + k] = uint8_t(kept[k].first);
        out.bones[4 - kept.size() + k] = kept[k].second;
    }
    return out;
}

struct Chunk
{
    uint32_t first = 0, rigid = 0, soft = 0, maxInfluences = 0;
    std::vector<uint16_t> bones; // sorted skeleton indices
    std::vector<uint32_t> vertices; // model vertices, rigid first
};
struct Section { uint16_t slot = 0, chunk = 0; uint32_t firstIndex = 0, triangles = 0; };
struct Lod
{
    std::vector<Section> sections;
    std::vector<Chunk> chunks;
    std::vector<uint32_t> vertices; // model vertex of each LOD vertex
    std::vector<Influences> influences;
    std::vector<uint16_t> indices;
};

// One section per chunk, as on the discs: a material's triangles split where
// a chunk would pass 75 bones or a section 65535 triangles.
Lod BuildLod(const Model &model)
{
    std::vector<Influences> quantized;
    for (const auto &v : model.vertices) quantized.push_back(Quantize(v));
    std::map<uint16_t, std::vector<const Triangle *>> bySlot;
    for (const auto &t : model.triangles) bySlot[t.slot].push_back(&t);

    Lod lod;
    for (const auto &[slot, triangles] : bySlot)
    {
        size_t at = 0;
        while (at < triangles.size())
        {
            std::set<uint16_t> bones;
            std::vector<const Triangle *> taken;
            for (; at < triangles.size() && taken.size() < kMaxSectionTriangles; ++at)
            {
                auto with = bones;
                for (const auto i : triangles[at]->v)
                    for (int k = 4 - quantized[i].count; k < 4; ++k) with.insert(quantized[i].bones[size_t(k)]);
                if (with.size() > kMaxChunkBones) break;
                bones = std::move(with);
                taken.push_back(triangles[at]);
            }
            Require(!taken.empty(), "a triangle needs more than 75 bones");
            Chunk chunk;
            chunk.first = uint32_t(lod.vertices.size());
            chunk.bones.assign(bones.begin(), bones.end());
            std::vector<uint32_t> rigid, soft;
            std::map<uint32_t, uint32_t> local; // model vertex -> LOD vertex
            for (const auto *t : taken)
                for (const auto i : t->v)
                    if (local.emplace(i, 0).second) (quantized[i].count == 1 ? rigid : soft).push_back(i);
            uint32_t next = chunk.first;
            for (const auto i : rigid) local[i] = next++;
            for (const auto i : soft) local[i] = next++;
            chunk.rigid = uint32_t(rigid.size());
            chunk.soft = uint32_t(soft.size());
            for (const auto list : {&rigid, &soft})
                for (const auto i : *list)
                {
                    lod.vertices.push_back(i);
                    lod.influences.push_back(quantized[i]);
                    chunk.maxInfluences = std::max<uint32_t>(chunk.maxInfluences, uint32_t(quantized[i].count));
                }
            Require(lod.vertices.size() <= kMaxVertices, "the model has " + std::to_string(lod.vertices.size()) +
                                                            "+ vertices after splitting by bones; the game takes 65535");
            Section section{slot, uint16_t(lod.chunks.size()), uint32_t(lod.indices.size()), uint32_t(taken.size())};
            for (const auto *t : taken)
                for (const auto i : t->v) lod.indices.push_back(uint16_t(local[i]));
            lod.sections.push_back(section);
            lod.chunks.push_back(std::move(chunk));
        }
    }
    return lod;
}

// The LOD array with the one new LOD. exportBase: the export's offset in the
// package, for the inline bulk data's absolute offset.
void WriteLod(Writer &w, size_t exportBase, const Model &model, const Lod &lod, const Original &original,
              std::span<const uint8_t> package)
{
    w.U32(1);
    w.U32(uint32_t(lod.sections.size()));
    for (const auto &s : lod.sections) { w.U16(s.slot); w.U16(s.chunk); w.U32(s.firstIndex); w.U16(uint16_t(s.triangles)); }
    w.U32(uint32_t(lod.indices.size()));
    for (const auto i : lod.indices) w.U16(i);
    w.U32(0); // f68
    std::set<uint16_t> active(original.activeBones.begin(), original.activeBones.end());
    for (const auto &c : lod.chunks) active.insert(c.bones.begin(), c.bones.end());
    w.U32(uint32_t(active.size()));
    for (const auto b : active) w.U16(b);
    w.U32(0); // f74
    w.U32(uint32_t(lod.chunks.size()));
    for (const auto &c : lod.chunks)
    {
        w.U32(c.first); w.U32(0); w.U32(0); // CPU rigid and soft vertices stay empty, as on the discs.
        w.U32(uint32_t(c.bones.size()));
        for (const auto b : c.bones) w.U16(b);
        w.U32(c.rigid); w.U32(c.soft); w.U32(c.maxInfluences);
    }
    const auto vertices = uint32_t(lod.vertices.size());
    w.U32(0); w.U32(vertices);
    w.U32(0); // Edges.
    w.Raw(package.subspan(original.requiredBegin, original.requiredEnd - original.requiredBegin));
    // Raw point indices (one word per vertex, inline): the vertex's own index.
    w.U32(0); w.U32(vertices); w.U32(vertices * 2); w.U32(uint32_t(exportBase + w.bytes.size() + 4));
    for (uint32_t i = 0; i < vertices; ++i) w.U16(uint16_t(i));
    w.U32(vertices);
    for (uint32_t i = 0; i < vertices; ++i)
    {
        const auto &v = model.vertices[lod.vertices[i]];
        const auto &influence = lod.influences[i];
        const auto &chunk = *std::find_if(lod.chunks.rbegin(), lod.chunks.rend(), [&](const Chunk &c) { return c.first <= i; });
        for (const auto f : v.position) w.F32(f);
        w.U32(PackNormal(v.tangent)); w.U32(PackNormal(v.binormal)); w.U32(PackNormal(v.normal));
        w.F32(v.uvs[0][0]); w.F32(v.uvs[0][1]);
        for (int k = 0; k < 4; ++k)
        {
            const auto it = std::lower_bound(chunk.bones.begin(), chunk.bones.end(), influence.bones[size_t(k)]);
            w.U8(influence.weights[size_t(k)] ? uint8_t(it - chunk.bones.begin()) : 0);
        }
        for (int k = 0; k < 4; ++k) w.U8(influence.weights[size_t(k)]);
    }
}

// The original properties; a multi-LOD original's LODInfo keeps only its first
// entry (DisplayFactor, LODHysteresis and an empty LODMaterialMap on the discs).
std::vector<uint8_t> WriteProperties(const Package &package, const Original &o)
{
    std::vector<uint8_t> out(package.bytes.begin() + std::ptrdiff_t(o.propertiesBegin), package.bytes.begin() + std::ptrdiff_t(o.propertiesEnd));
    if (!o.lodInfoValue) return out;
    Reader value{package.bytes.subspan(o.lodInfoValue, o.lodInfoSize)};
    if (value.U32() <= 1) return out;
    ue3::Properties(package, value, false, [](auto &&...) {});
    const size_t keep = value.pos, at = o.lodInfoValue - o.propertiesBegin;
    PutBe32(out, o.lodInfoSizeField - o.propertiesBegin, uint32_t(keep));
    PutBe32(out, at, 1);
    out.erase(out.begin() + std::ptrdiff_t(at + keep), out.begin() + std::ptrdiff_t(at + o.lodInfoSize));
    return out;
}

// The replaced export's bytes, to be placed at exportBase.
std::vector<uint8_t> WriteExport(const Package &package, const Original &o, const Model &model, const Lod &lod, size_t exportBase)
{
    Writer w;
    w.Raw(WriteProperties(package, o));

    // Bounds around the original and the new model, so the game culls no earlier than before.
    Vec3 low{o.bounds[0] - o.bounds[3], o.bounds[1] - o.bounds[4], o.bounds[2] - o.bounds[5]};
    Vec3 high{o.bounds[0] + o.bounds[3], o.bounds[1] + o.bounds[4], o.bounds[2] + o.bounds[5]};
    for (const auto &v : model.vertices)
        for (int k = 0; k < 3; ++k) { low[size_t(k)] = std::min(low[size_t(k)], v.position[size_t(k)]); high[size_t(k)] = std::max(high[size_t(k)], v.position[size_t(k)]); }
    const Vec3 origin = Scale(Add(low, high), 0.5f), extent = Scale(Sub(high, low), 0.5f);
    for (const auto f : origin) w.F32(f);
    for (const auto f : extent) w.F32(f);
    w.F32(std::sqrt(Dot(extent, extent)));

    const auto bytes = package.bytes;
    w.Raw(bytes.subspan(o.materialsBegin, o.materialsEnd - o.materialsBegin));
    w.Raw(bytes.subspan(o.skeletonBegin, o.skeletonEnd - o.skeletonBegin));
    WriteLod(w, exportBase, model, lod, o, bytes);
    w.Raw(bytes.subspan(o.tailBegin, o.end - o.tailBegin));
    return std::move(w.bytes);
}

// ---- StaticMesh and the game's StaticMeshRPG ---------------------------------
// As asset_export.cpp ReadStaticMesh reads them: properties, Bounds, BodySetup,
// the kDOP collision tree, InternalVersion 15 and one LOD (every mesh on the
// discs has one): an empty bulk array, sections of 28 bytes, the vertex stream
// (40 bytes and 8 per UV set past the first), the vertex count, u16 indices,
// then empty wireframe indices, edges and fEC. The 24 or 32 bytes after the
// LOD are kept as they are.

struct StaticOriginal
{
    size_t propertiesBegin = 0, propertiesEnd = 0;
    std::array<float, 7> bounds{};
    int32_t bodySetup = 0;
    uint32_t version = 0;
    // The distinct section materials in first-use order, as the exporter numbers
    // them, with the collision flags of their first section.
    std::vector<int32_t> materialRefs;
    std::vector<std::string> materials;
    std::vector<std::pair<uint32_t, uint32_t>> collision;
    uint32_t uvSets = 0;
    std::vector<Vertex> vertices;    // LOD 0: position, normal, UV sets, colour
    std::vector<Triangle> triangles; // LOD 0, with material slots
    size_t tailBegin = 0, end = 0;
};

Vec3 UnpackNormal(uint32_t packed)
{
    auto axis = [&](int shift) { return float(packed >> shift & 0xff) / 127.5f - 1; };
    return {axis(0), axis(8), axis(16)};
}

StaticOriginal ReadStaticOriginal(const Package &package, const ue3::Export &e)
{
    StaticOriginal o;
    auto r = ue3::ObjectReader(package, e);
    o.propertiesBegin = r.pos;
    ue3::Properties(package, r, true, [](auto &&...) {});
    o.propertiesEnd = r.pos;
    for (auto &f : o.bounds) f = r.F32();
    o.bodySetup = r.I32();
    ue3::SkipArray(r, 36); ue3::SkipArray(r, 16); // kDOP nodes and triangles.
    o.version = r.U32();
    Require(o.version < 17, "unexpected static mesh version " + std::to_string(o.version));
    Require(ue3::Count(r, 4) == 1, "the original has more than one LOD");
    const auto flags = r.U32(); r.U32(); const auto stored = r.U32(); r.U32();
    if (!(flags & 0x21)) r.Take(stored);
    struct Section { uint16_t slot; uint32_t first, triangles; };
    std::vector<Section> sections;
    for (uint32_t i = 0, n = ue3::Count(r, 28); i < n; ++i)
    {
        const auto material = r.I32();
        const auto collision = r.U32(), oldCollision = r.U32();
        const auto first = r.U32(), triangles = r.U32();
        r.Take(8); // MinVertexIndex, MaxVertexIndex.
        auto slot = std::find(o.materialRefs.begin(), o.materialRefs.end(), material);
        if (slot == o.materialRefs.end())
        {
            o.materialRefs.push_back(material);
            o.materials.push_back(package.RefName(material));
            o.collision.emplace_back(collision, oldCollision);
            slot = o.materialRefs.end() - 1;
        }
        sections.push_back({uint16_t(slot - o.materialRefs.begin()), first, triangles});
    }
    Require(!o.materials.empty(), "the original has no sections");
    o.uvSets = r.U32();
    const auto itemSize = r.U32(), vertices = r.U32();
    Require(o.uvSets >= 1 && o.uvSets <= kMaxUvSets && itemSize == 32 + 8 * o.uvSets, "unexpected static vertex layout");
    Require(ue3::Count(r, itemSize) == vertices && vertices, "vertex count mismatch");
    o.vertices.resize(vertices);
    for (auto &v : o.vertices)
    {
        v.position = {r.F32(), r.F32(), r.F32()};
        r.U32(); r.U32(); r.U32(); // Padding, tangent, binormal.
        v.normal = UnpackNormal(r.U32());
        v.color = r.U32();
        for (uint32_t s = 0; s < o.uvSets; ++s) v.uvs[s] = {r.F32(), r.F32()};
    }
    Require(r.U32() == vertices, "vertex count mismatch");
    std::vector<uint32_t> indices(ue3::Count(r, 2));
    for (auto &i : indices) i = r.U16();
    for (const auto &section : sections)
        for (uint32_t t = 0; t < section.triangles; ++t)
        {
            const size_t at = size_t(section.first) + 3 * size_t(t);
            Require(at + 3 <= indices.size(), "section outside the indices");
            Triangle triangle{{indices[at], indices[at + 1], indices[at + 2]}, section.slot};
            for (const auto i : triangle.v) Require(i < vertices, "index outside the vertices");
            o.triangles.push_back(triangle);
        }
    ue3::SkipArray(r, 2); ue3::SkipArray(r, 16); ue3::SkipArray(r, 1); // Wireframe indices, edges, fEC.
    o.tailBegin = r.pos;
    o.end = size_t(e.offset) + e.size;
    return o;
}

// An original vertex at the same spot as a new one: a k-d tree over the
// original positions. Among vertices there (hard edges, UV seams, overlapping
// pieces) it takes the one whose normal and first UV agree best.
class NearestVertex
{
public:
    explicit NearestVertex(const std::vector<Vertex> &vertices) : vertices_(vertices), order_(vertices.size())
    {
        for (uint32_t i = 0; i < order_.size(); ++i) order_[i] = i;
        Build(0, order_.size(), 0);
    }
    static constexpr uint32_t kNone = UINT32_MAX;
    // reach: the largest distance that counts as the same spot.
    uint32_t Find(const Vertex &v, float reach) const
    {
        float best = std::numeric_limits<float>::infinity();
        Search(0, order_.size(), 0, v.position, best);
        if (best > reach * reach) return kNone;
        // Within a hair of the nearest: the best normal, then the closest UV.
        reach = best + 1e-4f;
        uint32_t found = kNone;
        float score = -std::numeric_limits<float>::infinity();
        Visit(0, order_.size(), 0, v.position, reach, [&](uint32_t i) {
            const auto &o = vertices_[i];
            const float du = o.uvs[0][0] - v.uvs[0][0], dv = o.uvs[0][1] - v.uvs[0][1];
            const float s = Dot(o.normal, v.normal) * 4 - std::sqrt(du * du + dv * dv);
            if (s > score) { score = s; found = i; }
        });
        return found;
    }

private:
    static constexpr size_t kLeaf = 8;
    float Distance(uint32_t i, const Vec3 &p) const
    {
        const auto d = Sub(vertices_[i].position, p);
        return Dot(d, d);
    }
    void Build(size_t begin, size_t end, int axis)
    {
        if (end - begin <= kLeaf) return;
        const size_t middle = begin + (end - begin) / 2;
        std::nth_element(order_.begin() + std::ptrdiff_t(begin), order_.begin() + std::ptrdiff_t(middle), order_.begin() + std::ptrdiff_t(end),
                         [&](uint32_t a, uint32_t b) { return vertices_[a].position[size_t(axis)] < vertices_[b].position[size_t(axis)]; });
        Build(begin, middle, (axis + 1) % 3);
        Build(middle + 1, end, (axis + 1) % 3);
    }
    void Search(size_t begin, size_t end, int axis, const Vec3 &p, float &best) const
    {
        if (end - begin <= kLeaf)
        {
            for (size_t i = begin; i < end; ++i) best = std::min(best, Distance(order_[i], p));
            return;
        }
        const size_t middle = begin + (end - begin) / 2;
        best = std::min(best, Distance(order_[middle], p));
        const float delta = p[size_t(axis)] - vertices_[order_[middle]].position[size_t(axis)];
        const int next = (axis + 1) % 3;
        if (delta < 0) { Search(begin, middle, next, p, best); if (delta * delta < best) Search(middle + 1, end, next, p, best); }
        else { Search(middle + 1, end, next, p, best); if (delta * delta < best) Search(begin, middle, next, p, best); }
    }
    template <typename Each> void Visit(size_t begin, size_t end, int axis, const Vec3 &p, float reach, Each &&each) const
    {
        if (end - begin <= kLeaf)
        {
            for (size_t i = begin; i < end; ++i) if (Distance(order_[i], p) <= reach) each(order_[i]);
            return;
        }
        const size_t middle = begin + (end - begin) / 2;
        if (Distance(order_[middle], p) <= reach) each(order_[middle]);
        const float delta = p[size_t(axis)] - vertices_[order_[middle]].position[size_t(axis)];
        const int next = (axis + 1) % 3;
        if (delta < 0 || delta * delta <= reach) Visit(begin, middle, next, p, reach, each);
        if (delta >= 0 || delta * delta <= reach) Visit(middle + 1, end, next, p, reach, each);
    }

    const std::vector<Vertex> &vertices_;
    std::vector<uint32_t> order_;
};

// Barycentric weights of the point of triangle abc closest to p (Ericson,
// Real-Time Collision Detection 5.1.5).
std::array<float, 3> ClosestOnTriangle(const Vec3 &p, const Vec3 &a, const Vec3 &b, const Vec3 &c)
{
    const auto ab = Sub(b, a), ac = Sub(c, a), ap = Sub(p, a);
    const float d1 = Dot(ab, ap), d2 = Dot(ac, ap);
    if (d1 <= 0 && d2 <= 0) return {1, 0, 0};
    const auto bp = Sub(p, b);
    const float d3 = Dot(ab, bp), d4 = Dot(ac, bp);
    if (d3 >= 0 && d4 <= d3) return {0, 1, 0};
    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0 && d1 >= 0 && d3 <= 0) { const float v = d1 / (d1 - d3); return {1 - v, v, 0}; }
    const auto cp = Sub(p, c);
    const float d5 = Dot(ab, cp), d6 = Dot(ac, cp);
    if (d6 >= 0 && d5 <= d6) return {0, 0, 1};
    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0 && d2 >= 0 && d6 <= 0) { const float w = d2 / (d2 - d6); return {1 - w, 0, w}; }
    const float va = d3 * d6 - d5 * d4;
    if (va <= 0 && d4 - d3 >= 0 && d5 - d6 >= 0) { const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6)); return {0, 1 - w, w}; }
    const float scale = 1 / (va + vb + vc), v = vb * scale, w = vc * scale;
    return {1 - v - w, v, w};
}

// The original surface as a bounding-box tree over its triangles. Find gives
// the point of it nearest to a new vertex. A point whose original normal faces
// away from the vertex's counts up to kFacing farther, so the two sides of a
// thin wall, and of a hard edge, keep their own lighting.
class OriginalSurface
{
public:
    static constexpr float kFacing = 50.0f; // centimetres
    struct Hit { SurfacePoint point; uint32_t triangle = 0; };

    explicit OriginalSurface(const StaticOriginal &original) : o_(original)
    {
        for (uint32_t t = 0; t < o_.triangles.size(); ++t)
        {
            const auto &tri = o_.triangles[t].v;
            const auto &a = o_.vertices[tri[0]], &b = o_.vertices[tri[1]], &c = o_.vertices[tri[2]];
            const auto face = Cross(Sub(c.position, a.position), Sub(b.position, a.position));
            if (Dot(face, face) < 1e-12f) continue; // no area: never the nearest point alone
            triangleOf_.push_back(t);
            normals_.push_back(Normalize(Add(Add(a.normal, b.normal), c.normal), Normalize(face)));
            centroids_.push_back(Scale(Add(Add(a.position, b.position), c.position), 1.0f / 3));
        }
        Require(!triangleOf_.empty(), "the original has no faces");
        order_.resize(triangleOf_.size());
        for (uint32_t i = 0; i < order_.size(); ++i) order_[i] = i;
        Build(0, order_.size());
    }

    Hit Find(const Vec3 &p, const Vec3 &normal) const
    {
        Hit best;
        float score = std::numeric_limits<float>::infinity();
        const bool facing = Dot(normal, normal) > 0.5f;
        std::array<uint32_t, 128> stack{};
        size_t depth = 0;
        stack[depth++] = 0;
        while (depth)
        {
            const auto &node = nodes_[stack[--depth]];
            if (BoxDistance(node, p) >= score) continue;
            if (node.count)
            {
                for (uint32_t i = node.first; i < node.first + node.count; ++i)
                {
                    const auto item = order_[i];
                    const auto t = triangleOf_[item];
                    const auto &tri = o_.triangles[t].v;
                    const auto &a = o_.vertices[tri[0]].position, &b = o_.vertices[tri[1]].position, &c = o_.vertices[tri[2]].position;
                    const auto w = ClosestOnTriangle(p, a, b, c);
                    const auto q = Add(Add(Scale(a, w[0]), Scale(b, w[1])), Scale(c, w[2]));
                    const auto d = Sub(p, q);
                    float s = std::sqrt(Dot(d, d));
                    if (facing)
                    {
                        // The original's own normal there: at a hard edge each side has its vertices.
                        const auto &na = o_.vertices[tri[0]].normal, &nb = o_.vertices[tri[1]].normal, &nc = o_.vertices[tri[2]].normal;
                        const auto there = Normalize(Add(Add(Scale(na, w[0]), Scale(nb, w[1])), Scale(nc, w[2])), normals_[item]);
                        s += kFacing * 0.5f * (1 - Dot(normal, there));
                    }
                    if (s < score) { score = s; best = {{tri, w}, t}; }
                }
                continue;
            }
            // Nearer child last, so it is searched first.
            const uint32_t left = uint32_t(&node - nodes_.data()) + 1, right = node.right;
            const bool leftFirst = BoxDistance(nodes_[left], p) <= BoxDistance(nodes_[right], p);
            Require(depth + 2 <= stack.size(), "surface tree too deep");
            stack[depth++] = leftFirst ? right : left;
            stack[depth++] = leftFirst ? left : right;
        }
        return best;
    }

private:
    struct Node { Vec3 low{}, high{}; uint32_t first = 0, count = 0, right = 0; }; // left child follows its parent

    static float BoxDistance(const Node &node, const Vec3 &p)
    {
        float d = 0;
        for (size_t k = 0; k < 3; ++k)
        {
            const float out = std::max({node.low[k] - p[k], 0.0f, p[k] - node.high[k]});
            d += out * out;
        }
        return std::sqrt(d);
    }
    uint32_t Build(size_t begin, size_t end)
    {
        const auto index = uint32_t(nodes_.size());
        nodes_.emplace_back();
        Node node;
        node.low = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
        node.high = Scale(node.low, -1);
        Vec3 low = node.low, high = node.high;
        for (size_t i = begin; i < end; ++i)
        {
            for (const auto v : o_.triangles[triangleOf_[order_[i]]].v)
                for (size_t k = 0; k < 3; ++k)
                {
                    node.low[k] = std::min(node.low[k], o_.vertices[v].position[k]);
                    node.high[k] = std::max(node.high[k], o_.vertices[v].position[k]);
                }
            for (size_t k = 0; k < 3; ++k)
            {
                low[k] = std::min(low[k], centroids_[order_[i]][k]);
                high[k] = std::max(high[k], centroids_[order_[i]][k]);
            }
        }
        if (end - begin <= 4)
        {
            node.first = uint32_t(begin);
            node.count = uint32_t(end - begin);
            nodes_[index] = node;
            return index;
        }
        size_t axis = 0;
        for (size_t k = 1; k < 3; ++k) if (high[k] - low[k] > high[axis] - low[axis]) axis = k;
        const size_t middle = begin + (end - begin) / 2;
        std::nth_element(order_.begin() + std::ptrdiff_t(begin), order_.begin() + std::ptrdiff_t(middle), order_.begin() + std::ptrdiff_t(end),
                         [&](uint32_t a, uint32_t b) { return centroids_[a][axis] < centroids_[b][axis]; });
        Build(begin, middle);
        node.right = Build(middle, end);
        nodes_[index] = node;
        return index;
    }

    const StaticOriginal &o_;
    std::vector<uint32_t> order_, triangleOf_; // tree order -> item; item -> original triangle
    std::vector<Vec3> normals_, centroids_;    // per item
    std::vector<Node> nodes_;
};

// A colour or a UV set at a surface point.
uint32_t BlendColor(const std::vector<Vertex> &vertices, const SurfacePoint &p)
{
    uint32_t out = 0;
    for (int shift = 0; shift < 32; shift += 8)
    {
        float value = 0;
        for (size_t k = 0; k < 3; ++k) value += p.weights[k] * float(vertices[p.vertices[k]].color >> shift & 0xff);
        out |= uint32_t(std::clamp(std::lround(value), 0L, 255L)) << shift;
    }
    return out;
}
Uv BlendUv(const std::vector<Vertex> &vertices, const SurfacePoint &p, uint32_t set)
{
    Uv out{};
    for (size_t k = 0; k < 3; ++k)
        for (size_t c = 0; c < 2; ++c) out[c] += p.weights[k] * vertices[p.vertices[k]].uvs[set][c];
    return out;
}

// The file's meshes as one static model in Unreal space; points[i] is where
// model vertex i lies on the original surface (or the original vertex it sits on). UV sets and the colour the file
// lacks are the original's at that point. Triangles whose material is missing
// or not one of the original's take the material of the original triangle
// nearest to them (note says how many).
constexpr uint16_t kNoSlot = 0xffff;
Model ReadStaticModel(const gltf::Document &doc, const StaticOriginal &original, std::vector<SurfacePoint> &points, std::string &note)
{
    Model model;
    ReadVertices read;
    std::set<std::string> unmatched;
    for (size_t n = 0; n < doc.nodes.size(); ++n)
    {
        const auto &node = doc.nodes[n];
        if (node.mesh < 0) continue;
        const auto world = doc.World(int(n));
        for (const auto &primitive : doc.meshes[size_t(node.mesh)])
        {
            const auto attribute = [&](const std::string &name) {
                const auto it = primitive.attributes.find(name);
                return it == primitive.attributes.end() ? -1 : it->second;
            };
            const int position = attribute("POSITION"), normal = attribute("NORMAL"), color = attribute("COLOR_0");
            Require(position >= 0, "a mesh part has no positions");
            Require(attribute("TEXCOORD_0") >= 0, "a mesh part has no UV map; the game needs one");
            uint16_t slot = kNoSlot;
            const auto name = primitive.material >= 0 && size_t(primitive.material) < doc.materials.size()
                                  ? MaterialName(doc.materials[size_t(primitive.material)]) : std::string();
            for (size_t i = 0; i < original.materials.size(); ++i)
                if (MaterialName(original.materials[i]) == name) slot = uint16_t(i);
            if (original.materials.size() == 1) slot = 0;
            if (slot == kNoSlot) unmatched.insert(name.empty() ? "(none)" : doc.materials[size_t(primitive.material)]);
            const auto [seen, added] = read.try_emplace({n, primitive.attributes}, uint32_t(model.vertices.size()));
            if (!added)
            {
                AppendTriangles(doc, primitive, doc.Count(position), seen->second, slot, model);
                continue;
            }
            const auto positions = doc.Floats(position, 3);
            const size_t count = positions.size() / 3;
            const auto normals = normal >= 0 ? doc.Floats(normal, 3) : std::vector<float>{};
            Require(normals.empty() || normals.size() == count * 3, "a mesh part's attributes differ in length");
            std::vector<std::vector<float>> uvs(original.uvSets);
            for (uint32_t s = 0; s < original.uvSets; ++s)
                if (const int uv = attribute("TEXCOORD_" + std::to_string(s)); uv >= 0)
                {
                    uvs[s] = doc.Floats(uv, 2);
                    Require(uvs[s].size() == count * 2, "a mesh part's attributes differ in length");
                }
            const int colorComponents = color >= 0 ? doc.accessors.at(size_t(color)).components : 0;
            Require(color < 0 || colorComponents == 3 || colorComponents == 4, "COLOR_0 needs 3 or 4 components");
            const auto colors = color >= 0 ? doc.Floats(color, colorComponents) : std::vector<float>{};
            Require(colors.empty() || colors.size() == count * size_t(colorComponents), "a mesh part's attributes differ in length");

            const auto base = uint32_t(model.vertices.size());
            for (size_t i = 0; i < count; ++i)
            {
                Vertex v;
                const Vec3 p{positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2]};
                const Vec3 nrm = normals.empty() ? Vec3{0, 0, 0} : Vec3{normals[i * 3], normals[i * 3 + 1], normals[i * 3 + 2]};
                v.position = ToUnreal(gltf::TransformPoint(world, p), kCentimetres);
                v.normal = Normalize(ToUnreal(gltf::TransformVector(world, nrm), 1), {0, 0, 0});
                for (const auto f : v.position) Require(std::isfinite(f), "a vertex position is not a number");
                for (uint32_t s = 0; s < original.uvSets; ++s)
                    if (uvs[s].empty()) v.borrowed |= uint16_t(1u << s);
                    else v.uvs[s] = {uvs[s][i * 2], uvs[s][i * 2 + 1]};
                if (colors.empty()) v.borrowed |= 1u << 8;
                else
                {
                    auto byte = [&](int c) { return uint32_t(std::clamp(std::lround(colors[i * size_t(colorComponents) + size_t(c)] * 255), 0L, 255L)); };
                    v.color = (colorComponents == 4 ? byte(3) : 255u) << 24 | byte(0) << 16 | byte(1) << 8 | byte(2);
                }
                model.vertices.push_back(v);
            }
            AppendTriangles(doc, primitive, count, base, slot, model);
        }
    }
    Require(!model.triangles.empty(), "the file has no triangles");
    DropUnused(model);
    FinishModel(model, kMaxStaticVertices);
    const OriginalSurface surface(original);
    size_t reassigned = 0;
    for (auto &t : model.triangles)
    {
        if (t.slot != kNoSlot) continue;
        const auto &a = model.vertices[t.v[0]].position, &b = model.vertices[t.v[1]].position, &c = model.vertices[t.v[2]].position;
        const auto centre = Scale(Add(Add(a, b), c), 1.0f / 3);
        t.slot = original.triangles[surface.Find(centre, Normalize(Cross(Sub(c, a), Sub(b, a)), {0, 0, 0})).triangle].slot;
        ++reassigned;
    }
    // A vertex on an original vertex keeps that vertex's lighting exactly (the
    // original has coinciding vertices of different surfaces, where the nearest
    // surface point is ambiguous); the others take the surface point.
    constexpr float kSameSpot = 0.05f; // centimetres
    const NearestVertex corners(original.vertices);
    points.resize(model.vertices.size());
    for (size_t i = 0; i < model.vertices.size(); ++i)
    {
        auto &v = model.vertices[i];
        if (const auto corner = corners.Find(v, kSameSpot); corner != NearestVertex::kNone)
            points[i] = {{corner, corner, corner}, {1, 0, 0}};
        else
            points[i] = surface.Find(v.position, v.normal).point;
        for (uint32_t s = 0; s < original.uvSets; ++s) if (v.borrowed & (1u << s)) v.uvs[s] = BlendUv(original.vertices, points[i], s);
        if (v.borrowed & (1u << 8)) v.color = BlendColor(original.vertices, points[i]);
    }
    if (reassigned)
    {
        std::string names;
        for (const auto &m : unmatched) names += (names.empty() ? "" : ", ") + m;
        note = "; " + std::to_string(reassigned) + " triangles took the material of the original surface beneath them (" + names +
               " is not one of the original's)";
    }
    return model;
}

// UE3's kDOP tree build (TkDOPTree::Build): a node splits its triangles at the
// mean centroid on the axis where the centroids vary most, until five or fewer
// remain. Same nodes and the same triangles per leaf as the discs' trees.
struct KdopNode { std::array<float, 6> box{}; uint32_t leaf = 0, a = 0, b = 0; }; // a, b: children, or count and first triangle
struct KdopTriangle { std::array<uint32_t, 3> v{}; uint32_t section = 0; };

std::vector<KdopNode> BuildKdop(const std::vector<Vertex> &vertices, std::vector<KdopTriangle> &triangles)
{
    constexpr uint32_t kMaxTrianglesPerLeaf = 5;
    std::vector<Vec3> centroids;
    for (const auto &t : triangles)
    {
        const auto &a = vertices[t.v[0]].position, &b = vertices[t.v[1]].position, &c = vertices[t.v[2]].position;
        centroids.push_back({(a[0] + b[0] + c[0]) / 3.0f, (a[1] + b[1] + c[1]) / 3.0f, (a[2] + b[2] + c[2]) / 3.0f});
    }
    std::vector<uint32_t> order(triangles.size());
    for (uint32_t i = 0; i < order.size(); ++i) order[i] = i;
    std::vector<KdopNode> nodes(1);
    // Depth first, left before right, as the recursive original allocates them.
    struct Job { uint32_t node, start, count; };
    std::vector<Job> jobs{{0, 0, uint32_t(triangles.size())}};
    while (!jobs.empty())
    {
        const auto job = jobs.back();
        jobs.pop_back();
        KdopNode node;
        node.box = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                    -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()};
        for (uint32_t i = job.start; i < job.start + job.count; ++i)
            for (const auto v : triangles[order[i]].v)
                for (size_t k = 0; k < 3; ++k)
                {
                    node.box[k] = std::min(node.box[k], vertices[v].position[k]);
                    node.box[k + 3] = std::max(node.box[k + 3], vertices[v].position[k]);
                }
        if (job.count <= kMaxTrianglesPerLeaf)
        {
            node.leaf = 1; node.a = job.count; node.b = job.start;
            nodes[job.node] = node;
            continue;
        }
        size_t plane = 0;
        float bestMean = 0, bestVariance = 0;
        for (size_t k = 0; k < 3; ++k)
        {
            float mean = 0, variance = 0;
            for (uint32_t i = job.start; i < job.start + job.count; ++i) mean += centroids[order[i]][k];
            mean /= float(job.count);
            for (uint32_t i = job.start; i < job.start + job.count; ++i)
            {
                const float d = centroids[order[i]][k] - mean;
                variance += d * d;
            }
            variance /= float(job.count);
            if (variance >= bestVariance) { plane = k; bestVariance = variance; bestMean = mean; }
        }
        int64_t left = int64_t(job.start) - 1, right = int64_t(job.start) + job.count;
        while (left < right)
        {
            float d;
            do d = centroids[order[size_t(++left)]][plane]; while (d < bestMean && left < right);
            do d = centroids[order[size_t(--right)]][plane]; while (d >= bestMean && right > 0 && left < right);
            if (left < right) std::swap(order[size_t(left)], order[size_t(right)]);
        }
        if (left == int64_t(job.start) + job.count || right == int64_t(job.start)) left = int64_t(job.start) + job.count / 2;
        node.a = uint32_t(nodes.size()); node.b = node.a + 1;
        nodes[job.node] = node;
        nodes.resize(nodes.size() + 2);
        const auto split = uint32_t(left);
        jobs.push_back({node.b, split, job.start + job.count - split});
        jobs.push_back({node.a, job.start, split - job.start});
    }
    std::vector<KdopTriangle> sorted;
    for (const auto i : order) sorted.push_back(triangles[i]);
    triangles = std::move(sorted);
    return nodes;
}

struct StaticSection { uint16_t slot = 0; uint32_t firstIndex = 0, triangles = 0, minVertex = 0, maxVertex = 0; };

std::vector<uint8_t> WriteStaticExport(const Package &package, const StaticOriginal &o, const Model &model, size_t exportBase,
                                       std::string &summary)
{
    Require(model.vertices.size() <= kMaxStaticVertices,
            "the model has " + std::to_string(model.vertices.size()) + " vertices; the game takes 65536");
    // One section per material, in the original's order.
    std::vector<StaticSection> sections;
    std::vector<uint16_t> indices;
    for (uint16_t slot = 0; slot < o.materials.size(); ++slot)
    {
        StaticSection s{slot, uint32_t(indices.size()), 0, std::numeric_limits<uint32_t>::max(), 0};
        for (const auto &t : model.triangles)
            if (t.slot == slot)
            {
                for (const auto i : t.v) { indices.push_back(uint16_t(i)); s.minVertex = std::min(s.minVertex, i); s.maxVertex = std::max(s.maxVertex, i); }
                ++s.triangles;
            }
        if (s.triangles) sections.push_back(s);
    }
    std::vector<KdopTriangle> collision;
    for (uint32_t s = 0; s < sections.size(); ++s)
        if (o.collision[sections[s].slot].first)
            for (uint32_t t = 0; t < sections[s].triangles; ++t)
            {
                const auto at = sections[s].firstIndex + 3 * t;
                collision.push_back({{indices[at], indices[at + 1], indices[at + 2]}, s});
            }
    const auto nodes = collision.empty() ? std::vector<KdopNode>{} : BuildKdop(model.vertices, collision);

    Writer w;
    w.Raw(package.bytes.subspan(o.propertiesBegin, o.propertiesEnd - o.propertiesBegin));
    // Bounds around the original and the new model, so the game culls no earlier than before.
    Vec3 low = model.vertices[0].position, high = low;
    auto extend = [&](const Vec3 &p) { for (size_t k = 0; k < 3; ++k) { low[k] = std::min(low[k], p[k]); high[k] = std::max(high[k], p[k]); } };
    for (const auto &v : model.vertices) extend(v.position);
    for (const auto &v : o.vertices) extend(v.position);
    const Vec3 origin = Scale(Add(low, high), 0.5f), extent = Scale(Sub(high, low), 0.5f);
    float radius = 0;
    for (const auto *list : {&model.vertices, &o.vertices})
        for (const auto &v : *list) { const auto d = Sub(v.position, origin); radius = std::max(radius, Dot(d, d)); }
    for (const auto f : origin) w.F32(f);
    for (const auto f : extent) w.F32(f);
    w.F32(std::sqrt(radius));
    w.U32(uint32_t(o.bodySetup));
    w.U32(uint32_t(nodes.size()));
    for (const auto &n : nodes) { for (const auto f : n.box) w.F32(f); w.U32(n.leaf); w.U32(n.a); w.U32(n.b); }
    w.U32(uint32_t(collision.size()));
    for (const auto &t : collision) { for (const auto v : t.v) w.U32(v); w.U32(t.section); }
    w.U32(o.version);
    w.U32(1);
    // Raw triangles: an empty bulk array whose offset is the byte after its header, as on the discs.
    w.U32(0); w.U32(0); w.U32(0); w.U32(uint32_t(exportBase + w.bytes.size() + 4));
    w.U32(uint32_t(sections.size()));
    for (const auto &s : sections)
    {
        w.U32(uint32_t(o.materialRefs[s.slot]));
        w.U32(o.collision[s.slot].first); w.U32(o.collision[s.slot].second);
        w.U32(s.firstIndex); w.U32(s.triangles); w.U32(s.minVertex); w.U32(s.maxVertex);
    }
    const auto vertices = uint32_t(model.vertices.size());
    w.U32(o.uvSets); w.U32(32 + 8 * o.uvSets); w.U32(vertices); w.U32(vertices);
    for (const auto &v : model.vertices)
    {
        for (const auto f : v.position) w.F32(f);
        w.U32(0);
        w.U32(PackNormal(v.tangent)); w.U32(PackNormal(v.binormal)); w.U32(PackNormal(v.normal));
        w.U32(v.color);
        for (uint32_t s = 0; s < o.uvSets; ++s) { w.F32(v.uvs[s][0]); w.F32(v.uvs[s][1]); }
    }
    w.U32(vertices);
    w.U32(uint32_t(indices.size()));
    for (const auto i : indices) w.U16(i);
    w.U32(0); w.U32(0); w.U32(0); // Wireframe indices, edges, fEC: empty on the discs.
    w.Raw(package.bytes.subspan(o.tailBegin, o.end - o.tailBegin));
    summary = std::to_string(vertices) + " vertices, " + std::to_string(indices.size() / 3) + " triangles, " +
              std::to_string(sections.size()) + " sections, " + std::to_string(nodes.size()) + " collision nodes";
    return std::move(w.bytes);
}

// ---- Baked lighting of the placements -----------------------------------------
// A StaticMeshComponent is TemplateOwnerClass, its properties (StaticMesh: the
// mesh it places), then per LOD: shadow maps, ShadowMap1D vertex buffers and a
// light map (type 0 none; 1 per vertex: light GUIDs, owner, the samples as an
// inline bulk array of 12 bytes each, three scale vectors; 2 textures: light
// GUIDs, three texture references with scale vectors, coordinate scale and
// bias). A ShadowMap1D is its properties, one float per vertex, a light GUID.

constexpr size_t kLightSample = 12;

struct ComponentLod
{
    std::vector<int32_t> shadowBuffers;
    size_t samplesHeader = 0, samplesEnd = 0; // 0: no per-vertex light map
    uint32_t samples = 0;
};

std::vector<ComponentLod> ReadComponent(const Package &package, const ue3::Export &e, int32_t &mesh)
{
    auto r = ue3::ObjectReader(package, e);
    r.I32(); // TemplateOwnerClass.
    ue3::Properties(package, r, true, [&](const std::string &name, const std::string &type, Reader &value, uint32_t) {
        if (name == "StaticMesh" && type == "ObjectProperty") mesh = value.I32();
    });
    std::vector<ComponentLod> lods(ue3::Count(r, 12));
    for (auto &lod : lods)
    {
        ue3::SkipArray(r, 4); // Shadow maps.
        for (uint32_t i = 0, n = ue3::Count(r, 4); i < n; ++i) lod.shadowBuffers.push_back(r.I32());
        const auto type = r.U32();
        if (type == 0) continue;
        Require(type <= 2, "unknown light map type " + std::to_string(type));
        ue3::SkipArray(r, 16); // Light GUIDs.
        if (type == 2) { r.Take(64); continue; }
        r.I32(); // Owner.
        lod.samplesHeader = r.pos;
        const auto flags = r.U32();
        lod.samples = r.U32();
        const auto size = r.U32();
        r.U32();
        Require(!(flags & 0x21) && size == lod.samples * kLightSample, "unexpected light map samples");
        r.Take(size);
        lod.samplesEnd = r.pos;
        r.Take(36); // Scale vectors.
    }
    return lods;
}

// The replaced-mesh key of an object reference: "<package stem>.<object>" in lower case.
std::string MeshKey(const Package &package, int32_t ref, const std::string &stem)
{
    if (ref > 0) return Lower(stem + "." + package.RefName(ref));
    if (ref == 0 || uint64_t(-int64_t(ref)) > package.imports.size()) return {};
    const auto index = size_t(-int64_t(ref)) - 1;
    // The outermost import above it is the package it lives in.
    const auto valid = [&](int32_t r) { return r < 0 && uint64_t(-int64_t(r)) <= package.imports.size(); };
    int32_t outer = package.importOuters[index];
    for (size_t depth = 0; valid(outer) && depth < 16; ++depth)
    {
        const auto above = package.importOuters[size_t(-int64_t(outer)) - 1];
        if (!valid(above)) break;
        outer = above;
    }
    if (!valid(outer)) return {};
    return Lower(package.imports[size_t(-int64_t(outer)) - 1] + "." + package.imports[index]);
}
}

Result RebuildPackage(std::span<const uint8_t> bytes, const std::string &stem, const std::vector<Replacement> &replacements)
{
    Result result;
    std::unique_ptr<Package> package;
    try { package = std::make_unique<Package>(bytes); }
    catch (const std::exception &error) { result.notes.push_back(std::string("package unreadable: ") + error.what()); return result; }
    std::vector<uint8_t> out(bytes.begin(), bytes.end());
    for (const auto &replacement : replacements)
    {
        try
        {
            Require(replacement.exportIndex < package->exports.size(), "export " + std::to_string(replacement.exportIndex) + " does not exist");
            const auto &e = package->exports[replacement.exportIndex];
            // Archive entry names are lower case.
            Require(Lower(ue3::FileStem(e.name)) == Lower(replacement.object),
                    "export " + std::to_string(replacement.exportIndex) + " is " + e.name + ", not " + replacement.object);
            const auto type = package->ClassName(e);
            const bool skeletal = type == "SkeletalMesh";
            Require(skeletal || type == "StaticMesh" || type == "StaticMeshRPG",
                    e.name + " is a " + type + "; only SkeletalMesh, StaticMesh and StaticMeshRPG can be replaced");
            const auto doc = gltf::ReadGlb(replacement.glb);
            while (out.size() % 16) out.push_back(0);
            const size_t base = out.size();
            Require(base < 0xffffffffu, "package too large");
            std::vector<uint8_t> data;
            std::string summary;
            VertexMap vertexMap;
            if (skeletal)
            {
                const auto original = ReadOriginal(*package, e);
                const auto model = ReadModel(doc, original);
                const auto lod = BuildLod(model);
                data = WriteExport(*package, original, model, lod, base);
                summary = std::to_string(lod.vertices.size()) + " vertices, " + std::to_string(lod.indices.size() / 3) +
                          " triangles, " + std::to_string(lod.sections.size()) + " sections";
            }
            else
            {
                const auto original = ReadStaticOriginal(*package, e);
                std::string note;
                const auto model = ReadStaticModel(doc, original, vertexMap.points, note);
                data = WriteStaticExport(*package, original, model, base, summary);
                summary += note;
                vertexMap.originalVertices = uint32_t(original.vertices.size());
            }
            out.insert(out.end(), data.begin(), data.end());
            PutBe32(out, e.record + 32, uint32_t(data.size()));
            PutBe32(out, e.record + 36, uint32_t(base));
            ++result.replaced;
            result.notes.push_back(e.name + ": " + summary);
            if (!skeletal) result.vertexMaps[Lower(stem + "." + e.name)] = std::move(vertexMap);
        }
        catch (const std::exception &error)
        {
            result.notes.push_back(replacement.object + " kept original (" + replacement.source + "): " + error.what());
        }
    }
    if (result.replaced) result.package = std::move(out);
    return result;
}

Result RemapLighting(std::span<const uint8_t> bytes, const std::string &stem, const VertexMaps &maps)
{
    Result result;
    std::unique_ptr<Package> package;
    try { package = std::make_unique<Package>(bytes); }
    catch (const std::exception &error) { result.notes.push_back(std::string("package unreadable: ") + error.what()); return result; }
    // Only packages that place one of the meshes: an import of it, or the mesh itself.
    bool places = false;
    for (size_t i = 0; !places && i < package->imports.size(); ++i) places = maps.count(MeshKey(*package, -int32_t(i) - 1, stem)) != 0;
    for (size_t i = 0; !places && i < package->exports.size(); ++i) places = maps.count(MeshKey(*package, int32_t(i) + 1, stem)) != 0;
    if (!places) return result;

    std::vector<uint8_t> out(bytes.begin(), bytes.end());
    auto align = [&] {
        while (out.size() % 16) out.push_back(0);
        Require(out.size() < 0xffffffffu, "package too large");
        return out.size();
    };
    auto place = [&](const ue3::Export &e, size_t base, const std::vector<uint8_t> &data) {
        out.insert(out.end(), data.begin(), data.end());
        PutBe32(out, e.record + 32, uint32_t(data.size()));
        PutBe32(out, e.record + 36, uint32_t(base));
    };
    std::map<int32_t, const VertexMap *> shadowBuffers; // ShadowMap1D reference -> its mesh's map
    size_t lightMaps = 0, skipped = 0;
    for (const auto &e : package->exports)
    {
        if (package->ClassName(e) != "StaticMeshComponent") continue;
        try
        {
            int32_t mesh = 0;
            const auto lods = ReadComponent(*package, e, mesh);
            const auto it = maps.find(MeshKey(*package, mesh, stem));
            if (it == maps.end() || lods.empty()) continue;
            const auto &map = it->second;
            const auto &lod = lods[0]; // the replaced mesh has one LOD
            for (const auto ref : lod.shadowBuffers) if (ref > 0) shadowBuffers.emplace(ref, &map);
            if (!lod.samplesHeader) continue;
            Require(lod.samples == map.originalVertices, "its light map has " + std::to_string(lod.samples) + " samples for " +
                                                            std::to_string(map.originalVertices) + " vertices");
            const size_t base = align();
            const auto vertices = uint32_t(map.points.size());
            const auto samples = bytes.subspan(lod.samplesHeader + 16, size_t(lod.samples) * kLightSample);
            Writer w;
            w.Raw(bytes.subspan(e.offset, lod.samplesHeader - e.offset));
            w.U32(0); w.U32(vertices); w.U32(uint32_t(vertices * kLightSample)); w.U32(uint32_t(base + w.bytes.size() + 4));
            // Three colours of four bytes per sample, interpolated byte by byte.
            for (const auto &point : map.points)
                for (size_t b = 0; b < kLightSample; ++b)
                {
                    float value = 0;
                    for (size_t k = 0; k < 3; ++k) value += point.weights[k] * float(samples[size_t(point.vertices[k]) * kLightSample + b]);
                    w.U8(uint8_t(std::clamp(std::lround(value), 0L, 255L)));
                }
            w.Raw(bytes.subspan(lod.samplesEnd, size_t(e.offset) + e.size - lod.samplesEnd));
            place(e, base, w.bytes);
            ++lightMaps;
        }
        catch (const std::exception &error)
        {
            ++skipped;
            result.notes.push_back(e.name + " kept its lighting: " + error.what());
        }
    }
    size_t shadows = 0;
    for (const auto &[ref, map] : shadowBuffers)
    {
        const auto *e = package->ExportAt(ref);
        try
        {
            Require(e && package->ClassName(*e) == "ShadowMap1D", "not a ShadowMap1D");
            auto r = ue3::ObjectReader(*package, *e);
            ue3::Properties(*package, r, true, [](auto &&...) {});
            const size_t at = r.pos;
            const auto samples = ue3::Count(r, 4);
            Require(samples == map->originalVertices, "it has " + std::to_string(samples) + " samples for " +
                                                         std::to_string(map->originalVertices) + " vertices");
            const size_t after = at + 4 + size_t(samples) * 4;
            Writer w;
            w.Raw(bytes.subspan(e->offset, at - e->offset));
            w.U32(uint32_t(map->points.size()));
            Reader values{bytes.subspan(at + 4, size_t(samples) * 4)};
            std::vector<float> shadow(samples);
            for (auto &f : shadow) f = values.F32();
            for (const auto &point : map->points)
            {
                float value = 0;
                for (size_t k = 0; k < 3; ++k) value += point.weights[k] * shadow[point.vertices[k]];
                w.F32(value);
            }
            w.Raw(bytes.subspan(after, size_t(e->offset) + e->size - after));
            place(*e, align(), w.bytes);
            ++shadows;
        }
        catch (const std::exception &error)
        {
            ++skipped;
            result.notes.push_back((e ? e->name : "shadow buffer " + std::to_string(ref)) + " kept: " + error.what());
        }
    }
    result.replaced = lightMaps + shadows;
    if (result.replaced)
    {
        result.notes.push_back(std::to_string(lightMaps) + " vertex light maps and " + std::to_string(shadows) +
                               " vertex shadow buffers follow the new vertices" + (skipped ? " (" + std::to_string(skipped) + " kept)" : ""));
        result.package = std::move(out);
    }
    return result;
}
}
