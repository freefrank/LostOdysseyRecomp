#include "lo_semantics/mesh_hull_incremental61.h"
#include "lo_semantics/recovery_abi.h"
#include <array>
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::mesh_hull_incremental61 {
namespace {
using recovery_abi::Address;
using Vec = std::array<float, 3>;
struct Hull {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Word(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    float Float(std::uint64_t p) { return std::bit_cast<float>(Word(p)); }
    void Float(std::uint64_t p, float v) { Word(p, std::bit_cast<std::uint32_t>(v)); }
    Vec Point(std::uint32_t p) { return {Float(p), Float(p + 4), Float(p + 8)}; }
    void Point(std::uint32_t p, const Vec &v) {
        for (unsigned a = 0; a < 3; ++a)
            Float(p + 4 * a, v[a]);
    }
    static Vec Sub(const Vec &a, const Vec &b) {
        return {float(a[0] - b[0]), float(a[1] - b[1]), float(a[2] - b[2])};
    }
    static float Length(const Vec &v) {
        float yz = float(double(v[2]) * v[2] + float(v[1] * v[1]));
        return float(std::sqrt(float(double(v[0]) * v[0] + yz)));
    }
    static Vec Cross(const Vec &a, const Vec &b) {
        return {float(double(a[1]) * b[2] - float(a[2] * b[1])),
                float(double(a[2]) * b[0] - float(a[0] * b[2])),
                float(double(a[0]) * b[1] - float(a[1] * b[0]))};
    }
    void Enter() {
        s.r[12] = s.lr;
        Word(s.r[1] - 8, s.lr);
        for (unsigned i = 14; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
        auto old = s.r[1];
        s.r[1] -= 256;
        Word(s.r[1], old);
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            d.edge.engine.fp.SetHostFpControl(s.cached_fp_control);
        }
    }
    void Leave() {
        s.r[1] += 256;
        for (unsigned i = 14; i < 32; ++i)
            s.r[i] = recovery_abi::ReadU64(m, Address(s.r[1] - 16 - 8 * (31 - i)));
        s.r[12] = Word(s.r[1] - 8);
        s.lr = s.r[12];
    }
    Vec Unit(Vec v) {
        auto length = Length(v);
        if (length == Float(0x82000e50u)) {
            s.r[3] = 0xffffffff820d5f98ull;
            s.lr = 0x82b9f978u;
            d.lifetime.guest.CallDirect(0x82b85300u, m, s);
            length = Float(0x82000dacu);
        }
        auto inv = float(Float(0x82007784u) / length);
        for (auto &x : v)
            x = float(x * inv);
        return v;
    }
    Vec TriangleNormal(const Vec &a, const Vec &b, const Vec &c) {
        auto cross = Cross(Sub(b, a), Sub(c, b));
        float yz = float(double(cross[2]) * cross[2] + float(cross[1] * cross[1]));
        float length = float(std::sqrt(float(double(cross[0]) * cross[0] + yz)));
        if (length == Float(0x82000e50u))
            return {Float(0x82007784u), Float(0x82000e50u), Float(0x82000e50u)};
        auto inv = float(Float(0x82007784u) / length);
        for (auto &x : cross)
            x = float(x * inv);
        return cross;
    }
    Vec Orthogonal(const Vec &v) {
        auto zero = Float(0x82000e50u);
        float xz = float(v[0] * zero), yz = float(v[1] * zero), zz = float(v[2] * zero);
        Vec a{float(v[1] - zz), float(zz - v[0]), float(xz - yz)},
            b{float(yz - v[2]), float(zz - xz), float(v[0] - yz)};
        return Unit(Length(a) > Length(b) ? a : b);
    }
    bool Above(std::uint32_t vertices, std::uint32_t face, std::uint32_t point, double threshold) {
        auto a = Point(vertices + 12 * Word(face)), b = Point(vertices + 12 * Word(face + 4)),
             c = Point(vertices + 12 * Word(face + 8));
        auto normal = TriangleNormal(a, b, c), delta = Sub(Point(point), a);
        float z = float(normal[2] * delta[2]);
        float xz = float(double(delta[0]) * normal[0] + z);
        return float(double(normal[1]) * delta[1] + xz) > threshold;
    }
    bool NonCoplanar(std::uint32_t vertices, unsigned ia, unsigned ib, unsigned ic, unsigned id) {
        auto a = Point(vertices + 12 * ia), b = Sub(Point(vertices + 12 * ib), a),
             c = Sub(Point(vertices + 12 * ic), a);
        auto n = Cross(b, c);
        float xx = float(n[0] * n[0]), yy = float(n[1] * n[1]), zz = float(n[2] * n[2]);
        float length = float(std::sqrt(float(float(yy + zz) + xx)));
        auto upper = Float(0x820d5fbcu), lower = Float(0x820d5fb8u);
        if (length < upper && length > lower)
            return false;
        auto unit = Unit(n), delta = Sub(Point(vertices + 12 * id), a);
        float dot = float(unit[1] * delta[1]);
        dot = float(double(unit[2]) * delta[2] + dot);
        dot = float(double(unit[0]) * delta[0] + dot);
        return dot > upper || dot < lower;
    }
    std::uint32_t EdgeSlot(std::uint32_t face, std::uint32_t a, std::uint32_t b) {
        for (unsigned i = 0; i < 3; ++i) {
            auto x = Word(face + 4 * i), y = Word(face + 4 * ((i + 1) % 3));
            if ((x == a && y == b) || (x == b && y == a))
                return face + 12 + 4 * ((i + 2) % 3);
        }
        return 0x83216168u;
    }
    std::uint32_t FurthestFace(double threshold) {
        constexpr unsigned global = 0x832dc420;
        std::uint32_t best = 0;
        for (unsigned i = 0; i < Word(global + 4); ++i) {
            auto face = Word(Word(global) + 4 * i);
            if (!best || (face && Float(best + 32) < Float(face + 32)))
                best = face;
        }
        return Float(best + 32) > threshold ? best : 0; // Original requires a live face.
    }
    std::uint32_t Support(std::uint32_t vertices, std::int32_t count, std::uint32_t direction,
                          std::uint32_t mask) {
        auto axis = Point(direction);
        std::int32_t best = -1;
        auto dot = [&](unsigned i) {
            auto v = Point(vertices + 12 * i);
            float yz = float(double(v[1]) * axis[1] + float(v[2] * axis[2]));
            return float(double(axis[0]) * v[0] + yz);
        };
        for (std::int32_t i = 0; i < count; ++i)
            if (Word(Word(mask) + 4 * unsigned(i)) &&
                (best == -1 || dot(unsigned(i)) > dot(unsigned(best))))
                best = i;
        return std::uint32_t(best);
    }
    std::uint32_t AppendWord(std::uint32_t descriptor, std::uint32_t value) {
        auto count = Word(descriptor + 4), capacity = Word(descriptor + 8);
        if (count == capacity) {
            s.r[3] = descriptor;
            s.r[4] = capacity ? 2 * capacity : 16;
            s.lr = 0x82ba1fccu;
            (void)mesh_hull_preprocess61::Apply(0x82ba1138u, m, d, s);
        }
        auto out = Word(descriptor) + 4 * count;
        Word(out, value);
        Word(descriptor + 4, count + 1);
        return out;
    }
    std::uint32_t FaceAt(unsigned id) { return Word(Word(0x832dc420u) + 4 * id); }
    std::uint32_t AllocateFace(GuestAddress ret) {
        s.r[3] = Word(0x832df548u);
        s.r[4] = 36;
        s.r[5] = 254;
        s.ctr = Word(Word(s.r[3]) + 8);
        s.lr = ret;
        d.edge.engine.sort.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
        return Address(s.r[3]);
    }
    void FreeFace(std::uint32_t face, GuestAddress ret) {
        if (!face)
            return;
        Word(Word(0x832dc420u) + 4 * Word(face + 24), 0);
        s.r[3] = Word(0x832df548u);
        s.r[4] = face;
        s.ctr = Word(Word(s.r[3]) + 20);
        s.lr = ret;
        d.edge.engine.sort.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void StitchPair(std::uint32_t a, std::uint32_t b) {
        for (unsigned i = 0; i < 3; ++i) {
            auto v = Word(a + 4 * ((i + 1) % 3)), w = Word(a + 4 * ((i + 2) % 3));
            auto bSlot = EdgeSlot(b, w, v), aSlot = EdgeSlot(a, v, w);
            auto adjacent = FaceAt(Word(aSlot));
            Word(EdgeSlot(adjacent, w, v), Word(bSlot));
            aSlot = EdgeSlot(a, v, w);
            bSlot = EdgeSlot(b, w, v);
            adjacent = FaceAt(Word(bSlot));
            Word(EdgeSlot(adjacent, v, w), Word(aSlot));
        }
    }
    void RemovePair(std::uint32_t a, std::uint32_t b) {
        StitchPair(a, b);
        FreeFace(a, 0x82ba3934u);
        FreeFace(b, 0x82ba3964u);
    }
    void Extrude(std::uint32_t old, unsigned vertex) {
        const unsigned a = Word(old), b = Word(old + 4), c = Word(old + 8),
                       base = Word(0x832dc424u);
        constexpr GuestAddress allocators[]{0x82ba39bcu, 0x82ba3a34u, 0x82ba3aa4u};
        const unsigned edgeA[]{b, c, a}, edgeB[]{c, a, b};
        std::array<std::uint32_t, 3> faces{};
        for (unsigned i = 0; i < 3; ++i) {
            auto face = AllocateFace(allocators[i]);
            faces[i] = face;
            if (face)
                Face(face, vertex, edgeA[i], edgeB[i]);
            auto adjacent = Word(old + 12 + 4 * i);
            Word(face + 12, adjacent);
            Word(face + 16, base + (i + 1) % 3);
            Word(face + 20, base + (i + 2) % 3);
            Word(EdgeSlot(FaceAt(adjacent), edgeA[i], edgeB[i]), base + i);
        }
        for (auto face : faces) {
            auto adjacent = FaceAt(Word(face + 12));
            if (Word(adjacent) == vertex || Word(adjacent + 4) == vertex ||
                Word(adjacent + 8) == vertex)
                RemovePair(face, adjacent);
        }
        FreeFace(old, 0x82ba3bd8u);
    }
    void Face(std::uint32_t face, unsigned a, unsigned b, unsigned c) {
        Word(face, a);
        Word(face + 4, b);
        Word(face + 8, c);
        for (unsigned off : {12, 16, 20})
            Word(face + off, 0xffffffffu);
        Word(face + 24, Word(0x832dc424u));
        AppendWord(0x832dc420u, face);
        Word(face + 28, 0xffffffffu);
        Float(face + 32, Float(0x82000e50u));
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    switch (e) {
    case 0x82b9f928u:
    case 0x82b9fd50u:
    case 0x82b9fe00u:
    case 0x82b9feb8u:
    case 0x82b9ff70u:
    case 0x82ba0030u:
    case 0x82ba1290u:
    case 0x82ba1cd8u:
    case 0x82ba1f88u:
    case 0x82ba3868u:
    case 0x82ba1bc0u:
    case 0x82ba38e0u:
    case 0x82ba3970u:
        break;
    default:
        return false;
    }
    const auto a = s.r;
    const double f1 = std::bit_cast<double>(s.fpr_bits[1]);
    Hull h{m, d, s};
    h.Enter();
    switch (e) {
    case 0x82b9f928u:
        h.Point(Address(a[3]), h.Unit(h.Point(Address(a[4]))));
        s.r[3] = a[3];
        break;
    case 0x82b9fd50u:
        h.Point(Address(a[3]), h.TriangleNormal(h.Point(Address(a[4])), h.Point(Address(a[5])),
                                                h.Point(Address(a[6]))));
        s.r[3] = a[3];
        break;
    case 0x82b9fe00u:
        h.Point(Address(a[3]), h.Orthogonal(h.Point(Address(a[4]))));
        s.r[3] = a[3];
        break;
    case 0x82b9feb8u:
        s.r[3] = h.Above(Address(a[3]), Address(a[4]), Address(a[5]), f1);
        break;
    case 0x82b9ff70u:
        s.r[3] = h.EdgeSlot(Address(a[3]), Address(a[4]), Address(a[5]));
        break;
    case 0x82ba0030u:
        s.r[3] = h.NonCoplanar(Address(a[3]), Address(a[4]), Address(a[5]), Address(a[6]),
                               Address(a[7]));
        break;
    case 0x82ba1290u:
        s.r[3] = std::uint64_t(std::int64_t(std::int32_t(
            h.Support(Address(a[3]), std::int32_t(a[4]), Address(a[5]), Address(a[6])))));
        break;
    case 0x82ba1cd8u:
        s.r[3] = h.FurthestFace(f1);
        break;
    case 0x82ba1f88u:
        s.r[3] = h.AppendWord(Address(a[3]), Address(a[4]));
        break;
    case 0x82ba1bc0u:
        h.StitchPair(Address(a[3]), Address(a[4]));
        break;
    case 0x82ba38e0u:
        h.RemovePair(Address(a[3]), Address(a[4]));
        break;
    case 0x82ba3970u:
        h.Extrude(Address(a[3]), Address(a[4]));
        break;
    case 0x82ba3868u:
        h.Face(Address(a[3]), Address(a[4]), Address(a[5]), Address(a[6]));
        s.r[3] = a[3];
        break;
    }
    h.Leave();
    return true;
}
} // namespace lo::semantic::gpu::mesh_hull_incremental61
