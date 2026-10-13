#include "model_import.h"
#include "gltf_reader.h"
#include "ue3_package.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <bit>
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

struct Vertex
{
    Vec3 position{}, normal{}, tangent{}, binormal{};
    std::array<float, 2> uv{};
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

Model ReadModel(const gltf::Document &doc, const Original &original)
{
    std::map<std::string, uint16_t> bones, slots;
    for (size_t i = 0; i < original.bones.size(); ++i) bones.emplace(Lower(original.bones[i]), uint16_t(i));
    for (size_t i = 0; i < original.materials.size(); ++i) slots.emplace(MaterialName(original.materials[i]), uint16_t(i));
    auto boneOf = [&](const std::string &name) -> int {
        const auto it = bones.find(Lower(name));
        return it == bones.end() ? -1 : it->second;
    };
    auto slotOf = [&](int material) -> uint16_t {
        if (material < 0 && original.materials.size() == 1) return 0;
        Require(material >= 0 && size_t(material) < doc.materials.size(), "a mesh part has no material; give it one of the original material names");
        const auto it = slots.find(MaterialName(doc.materials[size_t(material)]));
        if (it != slots.end()) return it->second;
        std::string names;
        for (const auto &m : original.materials) names += (names.empty() ? "" : ", ") + m;
        throw std::runtime_error("material \"" + doc.materials[size_t(material)] + "\" is not one of the original's (" + names + ")");
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
            const uint16_t slot = slotOf(primitive.material);
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
                v.uv = {uvs[i * 2], uvs[i * 2 + 1]};
                for (const auto f : v.position) Require(std::isfinite(f), "a vertex position is not a number");
                model.vertices.push_back(v);
            }
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
    }
    Require(!model.triangles.empty(), "the file has no triangles");

    // A vertex on a mirrored UV seam serves triangles of both UV handedness; their
    // tangents would cancel, so the mirrored side gets its own copy.
    std::vector<uint8_t> sides(model.vertices.size());
    std::vector<bool> mirrored(model.triangles.size());
    for (size_t t = 0; t < model.triangles.size(); ++t)
    {
        const auto &tri = model.triangles[t];
        const auto &a = model.vertices[tri.v[0]].uv, &b = model.vertices[tri.v[1]].uv, &c = model.vertices[tri.v[2]].uv;
        mirrored[t] = (b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1]) < 0;
        for (const auto i : tri.v) sides[i] |= mirrored[t] ? 2 : 1;
    }
    std::map<uint32_t, uint32_t> copies;
    for (size_t t = 0; t < model.triangles.size(); ++t)
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
        const float du1 = b.uv[0] - a.uv[0], dv1 = b.uv[1] - a.uv[1], du2 = c.uv[0] - a.uv[0], dv2 = c.uv[1] - a.uv[1];
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
        w.F32(v.uv[0]); w.F32(v.uv[1]);
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
}

Result RebuildPackage(std::span<const uint8_t> bytes, const std::vector<Replacement> &replacements)
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
            Require(package->ClassName(e) == "SkeletalMesh", e.name + " is a " + package->ClassName(e) + "; only SkeletalMesh can be replaced");
            const auto original = ReadOriginal(*package, e);
            const auto doc = gltf::ReadGlb(replacement.glb);
            const auto model = ReadModel(doc, original);
            const auto lod = BuildLod(model);
            while (out.size() % 16) out.push_back(0);
            const size_t base = out.size();
            Require(base < 0xffffffffu, "package too large");
            const auto data = WriteExport(*package, original, model, lod, base);
            out.insert(out.end(), data.begin(), data.end());
            PutBe32(out, e.record + 32, uint32_t(data.size()));
            PutBe32(out, e.record + 36, uint32_t(base));
            ++result.replaced;
            result.notes.push_back(e.name + ": " + std::to_string(lod.vertices.size()) + " vertices, " +
                                   std::to_string(lod.indices.size() / 3) + " triangles, " + std::to_string(lod.sections.size()) +
                                   " sections");
        }
        catch (const std::exception &error)
        {
            result.notes.push_back(replacement.object + " kept original (" + replacement.source + "): " + error.what());
        }
    }
    if (result.replaced) result.package = std::move(out);
    return result;
}
}
