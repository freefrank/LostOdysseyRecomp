#include "lo_semantics/mesh_hull_preprocess61.h"
#include "lo_semantics/mesh_hull_incremental61.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/recovery_abi.h"
#include <array>
#include <bit>
#include <cmath>
#include <algorithm>
namespace lo::semantic::gpu::mesh_hull_preprocess61 {
namespace {
using recovery_abi::Address;
using Vec = std::array<float, 3>;
struct Preprocess {
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
    void Enter() {
        s.r[12] = s.lr;
        Word(s.r[1] - 8, s.lr);
        for (unsigned i = 14; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
        auto old = s.r[1];
        s.r[1] -= 512;
        Word(s.r[1], old);
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            d.edge.engine.fp.SetHostFpControl(s.cached_fp_control);
        }
    }
    void Leave() {
        s.r[1] += 512;
        for (unsigned i = 14; i < 32; ++i)
            s.r[i] = recovery_abi::ReadU64(m, Address(s.r[1] - 16 - 8 * (31 - i)));
        s.r[12] = Word(s.r[1] - 8);
        s.lr = s.r[12];
    }
    std::uint32_t Allocate(std::uint32_t bytes, GuestAddress ret) {
        s.r[3] = Word(0x832df548u);
        s.r[4] = bytes;
        s.r[5] = 254;
        s.ctr = Word(Word(s.r[3]) + 8);
        s.lr = ret;
        d.edge.engine.sort.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
        return Address(s.r[3]);
    }
    void Free(std::uint32_t p, GuestAddress ret) {
        if (!p)
            return;
        s.r[3] = Word(0x832df548u);
        s.r[4] = p;
        s.ctr = Word(Word(s.r[3]) + 20);
        s.lr = ret;
        d.edge.engine.sort.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void Copy(std::uint32_t to, std::uint32_t from, std::uint32_t bytes, GuestAddress ret) {
        s.r[3] = to;
        s.r[4] = from;
        s.r[5] = bytes;
        s.lr = ret;
        (void)crt_copy_full_context::Apply(0x82b7a0b0u, m, s);
    }
    struct Bounds {
        Vec minimum, maximum, extent, center;
    };
    Bounds Measure(std::uint32_t count, std::uint32_t input, std::uint32_t stride) {
        Bounds b;
        b.minimum.fill(Float(0x82000e0cu));
        b.maximum.fill(Float(0x82000d64u));
        for (unsigned i = 0; i < count; ++i) {
            auto p = Point(input + i * stride);
            for (unsigned a = 0; a < 3; ++a) {
                if (p[a] < b.minimum[a])
                    b.minimum[a] = p[a];
                if (p[a] > b.maximum[a])
                    b.maximum[a] = p[a];
            }
        }
        for (unsigned a = 0; a < 3; ++a) {
            b.extent[a] = float(b.maximum[a] - b.minimum[a]);
            b.center[a] = float(double(b.extent[a]) * Float(0x8201f9f0u) + b.minimum[a]);
        }
        return b;
    }
    void Box(Bounds b, std::uint32_t countOut, std::uint32_t output, bool initial) {
        const auto tiny = Float(0x820a6b8cu), huge = Float(0x82000e0cu);
        float shortest = huge;
        for (auto extent : b.extent)
            if ((initial ? extent > tiny : extent >= tiny) && extent < shortest)
                shortest = extent;
        if (shortest == huge)
            b.extent.fill(Float(0x82000d7cu));
        else
            for (auto &e : b.extent)
                if (e < tiny)
                    e = float(shortest * Float(0x82000e10u));
        Vec lo, hi;
        for (unsigned a = 0; a < 3; ++a) {
            lo[a] = float(b.center[a] - b.extent[a]);
            hi[a] = float(b.center[a] + b.extent[a]);
        }
        constexpr unsigned corners[]{0, 1, 3, 2, 4, 5, 7, 6};
        Word(countOut, 0);
        for (unsigned i = 0; i < 8; ++i) {
            Vec p;
            for (unsigned a = 0; a < 3; ++a)
                p[a] = (corners[i] & (1u << a)) ? hi[a] : lo[a];
            Point(output + 12 * i, p);
            Word(countOut, i + 1);
        }
    }
    static float Distance(const Vec &p, const Vec &center) {
        float x = float(p[0] - center[0]), y = float(p[1] - center[1]), z = float(p[2] - center[2]);
        float yz = float(double(z) * z + float(y * y));
        return float(double(x) * x + yz);
    }
    bool Normalize(std::uint32_t count, std::uint32_t input, std::uint32_t stride,
                   std::uint32_t countOut, std::uint32_t output, std::uint32_t scale,
                   double tolerance) {
        if (!count)
            return false;
        Word(countOut, 0);
        if (scale)
            Point(scale, {Float(0x82007784u), Float(0x82007784u), Float(0x82007784u)});
        auto b = Measure(count, input, stride);
        auto tiny = Float(0x820a6b8cu);
        if (count < 3 || b.extent[0] < tiny || b.extent[1] < tiny || b.extent[2] < tiny) {
            Box(b, countOut, output, true);
            return true;
        }
        Vec factor{1, 1, 1}, center = b.center;
        if (scale) {
            Point(scale, b.extent);
            for (unsigned a = 0; a < 3; ++a) {
                factor[a] = float(Float(0x82007784u) / b.extent[a]);
                center[a] = float(center[a] * factor[a]);
            }
        }
        for (unsigned i = 0; i < count; ++i) {
            auto p = Point(input + i * stride);
            if (scale)
                for (unsigned a = 0; a < 3; ++a)
                    p[a] = float(p[a] * factor[a]);
            auto retained = Word(countOut);
            unsigned j = 0;
            for (; j < retained; ++j) {
                auto q = Point(output + 12 * j);
                if (std::abs(float(q[0] - p[0])) < tolerance &&
                    std::abs(float(q[1] - p[1])) < tolerance &&
                    std::abs(float(q[2] - p[2])) < tolerance) {
                    if (Distance(p, center) > Distance(q, center))
                        Point(output + 12 * j, p);
                    break;
                }
            }
            if (j == retained) {
                Point(output + 12 * j, p);
                Word(countOut, retained + 1);
            }
        }
        b = Measure(Word(countOut), output, 12);
        if (Word(countOut) < 3 || b.extent[0] < tiny || b.extent[1] < tiny || b.extent[2] < tiny)
            Box(b, countOut, output, false);
        return true;
    }
    void Compact(std::uint32_t input, std::uint32_t count, std::uint32_t output,
                 std::uint32_t countOut, std::uint32_t indices, std::uint32_t indexCount) {
        auto map = Allocate(4 * count, 0x82ba09dcu);
        s.r[3] = map;
        s.r[4] = 0;
        s.r[5] = 4 * count;
        s.lr = 0x82ba09ecu;
        crt_reader_chain61::ApplySupport_B7BC40(m, d.edge.engine.sort.accepted, s);
        Word(countOut, 0);
        for (unsigned i = 0; i < indexCount; ++i) {
            auto old = Word(indices + 4 * i), mapped = Word(map + 4 * old);
            if (mapped)
                Word(indices + 4 * i, mapped - 1);
            else {
                auto next = Word(countOut);
                Word(indices + 4 * i, next);
                Point(output + 12 * next, Point(input + 12 * old));
                Word(countOut, next + 1);
                Word(map + 4 * old, next + 1);
            }
        }
        Free(map, 0x82ba0d5cu);
    }
    void ReleaseArray(std::uint32_t descriptor, GuestAddress ret) {
        Free(Word(descriptor), ret);
        Word(descriptor, 0);
        Word(descriptor + 4, 0);
        Word(descriptor + 8, 0);
    }
    void Reserve(std::uint32_t descriptor, std::uint32_t capacity, unsigned stride) {
        Word(descriptor + 8, capacity);
        auto old = Word(descriptor);
        auto buffer = Allocate(stride * capacity, stride == 12 ? 0x82ba1218u : 0x82ba1174u);
        Word(descriptor, buffer);
        for (unsigned i = 0; i < Word(descriptor + 4); ++i)
            for (unsigned j = 0; j < stride; j += 4)
                Word(buffer + stride * i + j, Word(old + stride * i + j));
        Free(old, stride == 12 ? 0x82ba1288u : 0x82ba11ccu);
    }
    std::uint32_t AppendTriangle(std::uint32_t descriptor, std::uint32_t a, std::uint32_t b,
                                 std::uint32_t c) {
        auto count = Word(descriptor + 4), capacity = Word(descriptor + 8);
        if (count == capacity)
            Reserve(descriptor, capacity ? 2 * capacity : 16, 12);
        auto out = Word(descriptor) + 12 * count;
        Word(out, a);
        Word(out + 4, b);
        Word(out + 8, c);
        Word(descriptor + 4, count + 1);
        return out;
    }
    bool PlainHull(std::uint32_t vertices, std::uint32_t count, std::uint32_t indicesOut,
                   std::uint32_t facesOut, std::uint32_t limit) {
        s.r[3] = vertices;
        s.r[4] = count;
        s.r[5] = limit;
        s.lr = 0x82ba4aa4u;
        (void)mesh_hull_incremental61::Apply(0x82ba40b8u, m, d, s);
        if (Address(s.r[3]) == 0)
            return false;
        constexpr std::uint32_t global = 0x832dc420u;
        auto array = Address(s.r[1] + 80);
        Word(array, 0);
        Word(array + 4, 0);
        Word(array + 8, 0);
        for (unsigned i = 0; i < Word(global + 4); ++i) {
            auto face = Word(Word(global) + 4 * i);
            if (!face)
                continue;
            for (unsigned j = 0; j < 3; ++j) {
                auto countNow = Word(array + 4), capacity = Word(array + 8);
                auto value = Word(face + 4 * j);
                if (countNow == capacity)
                    Reserve(array, capacity ? 2 * capacity : 16, 4);
                Word(Word(array) + 4 * countNow, value);
                Word(array + 4, countNow + 1);
            }
            face = Word(Word(global) + 4 * i);
            if (face) {
                Word(Word(global) + 4 * Word(face + 24), 0);
                Free(face, 0x82ba4b84u);
            }
        }
        Word(facesOut, Word(array + 4) / 3);
        Word(indicesOut, Word(array));
        ReleaseArray(global, 0x82ba4bd8u);
        return true;
    }
    bool Hull(std::uint32_t count, std::uint32_t vertices, std::uint32_t output,
              std::uint32_t limit, double inflation) {
        const auto sp = Address(s.r[1]);
        if (inflation == double(Float(0x82000e50u))) {
            if (!PlainHull(vertices, count, sp + 100, sp + 96, limit))
                return false;
            Word(output, count);
            Word(output + 4, 3 * Word(sp + 96));
            Word(output + 8, Word(sp + 96));
            Word(output + 12, vertices);
            Word(output + 16, Word(sp + 100));
            return true;
        }
        bool ok = false;
        if (count) {
            for (unsigned off : {112, 116, 120})
                Word(sp + off, 0);
            s.r[3] = vertices;
            s.r[4] = count;
            s.r[5] = limit;
            s.r[6] = sp + 112;
            s.fpr_bits[1] = std::bit_cast<std::uint64_t>(double(Float(0x8204fc20u)));
            s.lr = 0x82ba5b28u;
            (void)mesh_hull_incremental61::Apply(0x82ba4bf8u, m, d, s);
            if (Address(s.r[3]) != 0) {
                s.r[3] = Word(sp + 112);
                s.r[4] = Word(sp + 116);
                s.r[5] = vertices;
                s.r[6] = count;
                s.r[7] = 35;
                s.r[8] = sp + 96;
                s.r[9] = sp + 104;
                s.r[10] = sp + 100;
                Word(sp + 84, sp + 108);
                s.fpr_bits[1] = std::bit_cast<std::uint64_t>(inflation);
                s.lr = 0x82ba5b6cu;
                d.lifetime.guest.CallDirect(0x82ba5480u, m, s);
                ok = Address(s.r[3]) != 0;
            }
            ReleaseArray(sp + 112, ok ? 0x82ba5b78u : 0x82ba5b38u);
        }
        constexpr std::uint32_t global = 0x832dc420u;
        if (!ok) {
            ReleaseArray(global, 0x82ba5bacu);
            return false;
        }
        auto polygons = Word(sp + 100), array = sp + 112;
        Word(array, 0);
        Word(array + 4, 0);
        Word(array + 8, 0);
        std::uint32_t cursor = 1;
        for (unsigned i = 0; i < Word(polygons); ++i) {
            auto degree = Word(polygons + 4 * cursor);
            ++cursor;
            for (unsigned j = 2; j < degree; ++j)
                AppendTriangle(array, Word(polygons + 4 * cursor),
                               Word(polygons + 4 * (cursor + j - 1)),
                               Word(polygons + 4 * (cursor + j)));
            cursor += degree;
        }
        auto triangles = Word(array + 4);
        Free(polygons, 0x82ba5c84u);
        Word(output, Word(sp + 104));
        Word(output + 4, 3 * triangles);
        Word(output + 8, triangles);
        Word(output + 12, Word(sp + 96));
        Word(output + 16, Word(array));
        ReleaseArray(global, 0x82ba5cd4u);
        return true;
    }
    bool Prepare(std::uint32_t self, std::uint32_t input, std::uint32_t output) {
        auto sp = Address(s.r[1]);
        for (unsigned off = 112; off <= 128; off += 4)
            Word(sp + off, 0);
        auto normalized = Allocate(12 * std::max(Word(input + 4), 8u), 0x82ba5d64u);
        if (!Normalize(Word(input + 4), Word(input + 8), Word(input + 12), sp + 80, normalized,
                       sp + 88, double(Float(input + 16)))) {
            Free(normalized, 0x82ba60e8u);
            return false;
        }
        auto scale = Point(sp + 88);
        for (unsigned i = 0; i < Word(sp + 80); ++i) {
            auto p = Point(normalized + 12 * i);
            for (unsigned a = 0; a < 3; ++a)
                p[a] = float(p[a] * scale[a]);
            Point(normalized + 12 * i, p);
        }
        const auto normalizedCount = Word(sp + 80), limit = Word(input + 24);
        const auto inflation = double(Float((Word(input) & 4) ? input + 20 : 0x82000e50u));
        // Hull owns separate stack scratch from this outer output descriptor.
        Enter();
        bool built = Hull(normalizedCount, normalized, sp + 112, limit, inflation);
        Leave();
        if (!built) {
            Free(normalized, 0x82ba60e8u);
            return false;
        }
        const auto hullCount = Word(sp + 112), indexCount = Word(sp + 116),
                   faceCount = Word(sp + 120), hull = Word(sp + 124), indices = Word(sp + 128);
        auto compacted = Allocate(12 * hullCount, 0x82ba5e54u);
        Compact(hull, hullCount, compacted, sp + 80, indices, indexCount);
        const auto retained = Word(sp + 80), flags = Word(input);
        Word(output + 4, retained);
        bool triangles = (flags & 1) != 0;
        m.WriteU8(output, triangles ? 0 : 1);
        auto points = Allocate(12 * retained, triangles ? 0x82ba5eccu : 0x82ba5fa8u);
        Word(output + 8, points);
        Word(output + 12, faceCount);
        auto words = triangles ? indexCount : indexCount + faceCount;
        Word(output + 16, words);
        auto outIndices = Allocate(4 * words, triangles ? 0x82ba5efcu : 0x82ba5fd8u);
        Word(output + 20, outIndices);
        Copy(points, compacted, 12 * retained, triangles ? 0x82ba5f14u : 0x82ba5ff0u);
        if (triangles) {
            if (flags & 2) {
                for (unsigned i = 0; i < faceCount; ++i)
                    for (unsigned j = 0; j < 3; ++j)
                        Word(outIndices + 12 * i + 4 * j, Word(indices + 12 * i + 4 * (2 - j)));
            } else
                Copy(outIndices, indices, 4 * indexCount, 0x82ba5f78u);
        } else
            for (unsigned i = 0; i < faceCount; ++i) {
                Word(outIndices + 16 * i, 3);
                for (unsigned j = 0; j < 3; ++j)
                    Word(outIndices + 16 * i + 4 + 4 * j,
                         Word(indices + 12 * i + 4 * ((flags & 2) ? 2 - j : j)));
            }
        if (hull == normalized)
            normalized = 0;
        Free(indices, 0x82ba6088u);
        Free(hull, 0x82ba60a8u);
        Free(compacted, 0x82ba60c8u);
        Free(normalized, 0x82ba60e8u);
        (void)self;
        return true;
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    if (e != 0x82ba0230u && e != 0x82ba0998u && e != 0x82ba5cf8u && e != 0x82ba5a70u &&
        e != 0x82ba4a88u && e != 0x82ba0d68u && e != 0x82ba1138u && e != 0x82ba11d8u &&
        e != 0x82ba2280u)
        return false;
    auto args = s.r;
    auto tolerance = std::bit_cast<double>(s.fpr_bits[1]);
    Preprocess p{m, d, s};
    p.Enter();
    if (e == 0x82ba0230u)
        s.r[3] = p.Normalize(Address(args[4]), Address(args[5]), Address(args[6]), Address(args[7]),
                             Address(args[8]), Address(args[10]), tolerance)
                     ? 1
                     : 0;
    else if (e == 0x82ba0998u)
        p.Compact(Address(args[4]), Address(args[5]), Address(args[6]), Address(args[7]),
                  Address(args[8]), Address(args[9]));
    else if (e == 0x82ba5a70u)
        s.r[3] = p.Hull(Address(args[3]), Address(args[4]), Address(args[5]), Address(args[6]),
                        tolerance)
                     ? 1
                     : 0;
    else if (e == 0x82ba4a88u)
        s.r[3] = p.PlainHull(Address(args[3]), Address(args[4]), Address(args[5]), Address(args[6]),
                             Address(args[7]))
                     ? 1
                     : 0;
    else if (e == 0x82ba0d68u)
        p.ReleaseArray(Address(args[3]), 0x82ba0da8u);
    else if (e == 0x82ba1138u || e == 0x82ba11d8u)
        p.Reserve(Address(args[3]), Address(args[4]), e == 0x82ba1138u ? 4 : 12);
    else if (e == 0x82ba2280u)
        s.r[3] = p.AppendTriangle(Address(args[3]), std::uint32_t(args[4] >> 32), Address(args[4]),
                                  std::uint32_t(args[5] >> 32));
    else
        s.r[3] = p.Prepare(Address(args[3]), Address(args[4]), Address(args[5])) ? 0 : 1;
    p.Leave();
    return true;
}
} // namespace lo::semantic::gpu::mesh_hull_preprocess61
