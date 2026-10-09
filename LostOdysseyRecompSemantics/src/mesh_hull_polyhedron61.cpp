#include "lo_semantics/mesh_hull_polyhedron61.h"
#include "lo_semantics/recovery_abi.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <vector>
#include <map>
namespace lo::semantic::gpu::mesh_hull_polyhedron61 {
namespace {
using recovery_abi::Address;
using Vec = std::array<float, 3>;
struct Polyhedron {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    std::uint32_t Word(std::uint32_t p) { return m.ReadU32(p); }
    void Word(std::uint32_t p, std::uint32_t v) { m.WriteU32(p, v); }
    float Float(std::uint32_t p) { return std::bit_cast<float>(Word(p)); }
    void Float(std::uint32_t p, float v) { Word(p, std::bit_cast<std::uint32_t>(v)); }
    Vec Point(std::uint32_t p) { return {Float(p), Float(p + 4), Float(p + 8)}; }
    void Point(std::uint32_t p, const Vec &v) {
        for (unsigned i = 0; i < 3; ++i)
            Float(p + 4 * i, v[i]);
    }
    float Dot(const Vec &a, const Vec &b) {
        float value = float(a[1] * b[1]);
        value = float(double(a[2]) * b[2] + value);
        return float(double(a[0]) * b[0] + value);
    }
    Vec Sub(const Vec &a, const Vec &b) {
        return {float(a[0] - b[0]), float(a[1] - b[1]), float(a[2] - b[2])};
    }
    void Enter() {
        s.r[12] = s.lr;
        Word(Address(s.r[1] - 8), unsigned(s.lr));
        for (unsigned i = 14; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
        auto old = s.r[1];
        s.r[1] -= 512;
        Word(Address(s.r[1]), unsigned(old));
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            d.edge.engine.fp.SetHostFpControl(s.cached_fp_control);
        }
    }
    void Leave() {
        s.r[1] += 512;
        for (unsigned i = 14; i < 32; ++i)
            s.r[i] = recovery_abi::ReadU64(m, Address(s.r[1] - 16 - 8 * (31 - i)));
        s.r[12] = Word(Address(s.r[1] - 8));
        s.lr = s.r[12];
    }
    std::uint32_t Allocate(unsigned bytes, GuestAddress ret) {
        s.r[3] = Word(0x832df548u);
        s.r[4] = bytes;
        s.r[5] = 254;
        s.ctr = Word(Word(s.r[3]) + 8);
        s.lr = ret;
        d.edge.engine.sort.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
        return Address(s.r[3]);
    }
    void Free(unsigned pointer, GuestAddress ret) {
        if (!pointer)
            return;
        s.r[3] = Word(0x832df548u);
        s.r[4] = pointer;
        s.ctr = Word(Word(s.r[3]) + 20);
        s.lr = ret;
        d.edge.engine.sort.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void ReserveEdges(unsigned array, unsigned capacity) {
        auto old = Word(array);
        Word(array + 8, capacity);
        auto buffer = Allocate(4 * capacity, 0x82ba1004u);
        Word(array, buffer);
        for (unsigned i = 0; i < Word(array + 4); ++i)
            Word(buffer + 4 * i, Word(old + 4 * i));
        Free(old, 0x82ba106cu);
    }
    void Reserve(unsigned array, unsigned capacity, GuestAddress entry) {
        s.r[3] = array;
        s.r[4] = capacity;
        if (entry == 0x82ba1078u)
            (void)mesh_hull_incremental61::Apply(entry, m, d, s);
        else
            (void)mesh_hull_preprocess61::Apply(entry, m, d, s);
    }
    void Release(unsigned self) {
        constexpr GuestAddress returns[]{0x82ba1e68u, 0x82ba1e98u, 0x82ba1ec8u};
        unsigned i = 0;
        for (unsigned off : {24u, 12u, 0u}) {
            Free(Word(self + off), returns[i++]);
            for (unsigned j : {0, 4, 8})
                Word(self + off + j, 0);
        }
    }
    unsigned Box(const Vec &lo, const Vec &hi) {
        auto self = Allocate(36, 0x82ba235cu);
        for (unsigned i = 0; i < 36; i += 4)
            Word(self + i, 0);
        Reserve(self, 8, 0x82ba11d8u);
        ReserveEdges(self + 12, 24);
        Reserve(self + 24, 6, 0x82ba1078u);
        Word(self + 4, 8);
        Word(self + 16, 24);
        Word(self + 28, 6);
        for (unsigned i = 0; i < 8; ++i) {
            Vec p;
            for (unsigned a = 0; a < 3; ++a)
                p[a] = (i & (4u >> a)) ? hi[a] : lo[a];
            Point(Word(self) + 12 * i, p);
        }
        const auto zero = Float(0x82000e50u), plus = Float(0x82007784u), minus = Float(0x82000e40u);
        for (unsigned i = 0; i < 6; ++i) {
            Vec n{zero, zero, zero};
            n[i / 2] = (i & 1) ? plus : minus;
            auto plane = Word(self + 24) + 16 * i;
            Point(plane, n);
            Float(plane + 12, (i & 1) ? -hi[i / 2] : lo[i / 2]);
        }
        constexpr unsigned loops[6][4]{{0, 1, 3, 2}, {6, 7, 5, 4}, {0, 4, 5, 1},
                                       {3, 7, 6, 2}, {0, 2, 6, 4}, {1, 5, 7, 3}};
        auto edges = Word(self + 12);
        for (unsigned face = 0; face < 6; ++face)
            for (unsigned j = 0; j < 4; ++j) {
                auto e = edges + 4 * (4 * face + j);
                m.WriteU8(e + 2, loops[face][j]);
                m.WriteU8(e + 3, face);
                for (unsigned other = 0; other < 6; ++other)
                    for (unsigned k = 0; k < 4; ++k)
                        if (loops[other][k] == loops[face][(j + 1) % 4] &&
                            loops[other][(k + 1) % 4] == loops[face][j])
                            m.WriteU16(e, 4 * other + k);
            }
        return self;
    }
    Vec Normal(unsigned a, unsigned b, unsigned c) {
        auto out = Address(s.r[1] + 80);
        s.r[3] = out;
        s.r[4] = a;
        s.r[5] = b;
        s.r[6] = c;
        (void)mesh_hull_incremental61::Apply(0x82b9fd50u, m, d, s);
        return Point(out);
    }
    using Matrix = std::array<float, 9>;
    Matrix ReadMatrix(unsigned p) {
        Matrix a;
        for (unsigned i = 0; i < 9; ++i)
            a[i] = Float(p + 4 * i);
        return a;
    }
    void WriteMatrix(unsigned p, const Matrix &a) {
        for (unsigned i = 0; i < 9; ++i)
            Float(p + 4 * i, a[i]);
    }
    Matrix Transpose(const Matrix &a) {
        Matrix b;
        for (unsigned r = 0; r < 3; ++r)
            for (unsigned c = 0; c < 3; ++c)
                b[3 * r + c] = a[3 * c + r];
        return b;
    }
    Matrix Inverse(const Matrix &a) {
        float d = float(float(a[0] * a[4]) * a[8]);
        d = float(double(float(a[1] * a[5])) * a[6] + d);
        d = float(double(float(a[2] * a[3])) * a[7] + d);
        d = float(d - double(float(a[0] * a[5])) * a[7]);
        d = float(d - double(float(a[8] * a[1])) * a[3]);
        d = float(d - double(float(a[4] * a[6])) * a[2]);
        float inv = float(Float(0x82007784u) / d);
        Matrix b;
        for (unsigned r = 0; r < 3; ++r)
            for (unsigned c = 0; c < 3; ++c) {
                auto r1 = (c + 1) % 3, r2 = (c + 2) % 3, c1 = (r + 1) % 3, c2 = (r + 2) % 3;
                b[3 * r + c] = float(float(double(a[3 * r1 + c1]) * a[3 * r2 + c2] -
                                           float(a[3 * r1 + c2] * a[3 * r2 + c1])) *
                                     inv);
            }
        return b;
    }
    Vec Intersection(unsigned first, unsigned second, unsigned third) {
        Matrix rows;
        auto n0 = Point(first), n1 = Point(second), n2 = Point(third);
        for (unsigned i = 0; i < 3; ++i) {
            rows[i] = n0[i];
            rows[3 + i] = n1[i];
            rows[6 + i] = n2[i];
        }
        auto inverse = Inverse(rows);
        Vec out;
        auto d0 = -Float(first + 12), d1 = -Float(second + 12), d2 = -Float(third + 12);
        for (unsigned i = 0; i < 3; ++i) {
            float value = float(inverse[3 * i + 1] * d1);
            value = float(double(inverse[3 * i + 2]) * d2 + value);
            out[i] = float(double(inverse[3 * i]) * d0 + value);
        }
        return out;
    }
    void InitClipScratch() {
        constexpr unsigned flag = 0x832dc444u;
        constexpr unsigned descriptors[]{0x832dc438u, 0x832dc42cu};
        constexpr unsigned callbacks[]{0x830e9990u, 0x830e9930u};
        for (unsigned i = 0; i < 2; ++i) {
            auto bits = Word(flag);
            if (!(bits & (1u << i))) {
                Word(flag, bits | (1u << i));
                for (unsigned off : {0, 4, 8})
                    Word(descriptors[i] + off, 0);
                s.r[3] = callbacks[i];
                s.lr = i ? 0x82ba2eacu : 0x82ba2e68u;
                d.lifetime.guest.CallDirect(0x82b7be48u, m, s);
            }
            Word(descriptors[i] + 4, 0);
        }
    }
    struct Polygon {
        std::vector<unsigned> vertices;
        std::array<std::uint32_t, 4> plane;
    };
    unsigned Clip(unsigned self, unsigned cuttingPlane) {
        InitClipScratch();
        auto epsilon = Float(0x83216160u);
        auto normal = Point(cuttingPlane);
        auto offset = Float(cuttingPlane + 12);
        unsigned count = Word(self + 4), edges = Word(self + 12), planes = Word(self + 24);
        std::vector<int> side(count);
        std::vector<unsigned> remap(count, 0xffffffffu);
        std::vector<Vec> points;
        for (unsigned i = 0; i < count; ++i) {
            auto p = Point(Word(self) + 12 * i);
            auto distance = float(Dot(p, normal) + offset);
            side[i] = distance > epsilon ? 2 : (distance < -epsilon ? 1 : 0);
            if (side[i] != 2) {
                remap[i] = unsigned(points.size());
                points.push_back(p);
            }
        }
        std::map<unsigned, unsigned> intersections;
        std::vector<Polygon> polygons;
        std::vector<std::pair<unsigned, unsigned>> cap;
        auto planeWords = [&](unsigned p) {
            return std::array<std::uint32_t, 4>{Word(p), Word(p + 4), Word(p + 8), Word(p + 12)};
        };
        for (unsigned start = 0; start < Word(self + 16);) {
            auto face = m.ReadU8(edges + 4 * start + 3);
            unsigned end = start + 1;
            while (end < Word(self + 16) && m.ReadU8(edges + 4 * end + 3) == face)
                ++end;
            bool inside = false;
            Polygon polygon;
            polygon.plane = planeWords(planes + 16 * face);
            for (unsigned e = start; e < end; ++e) {
                auto next = e + 1 < end ? e + 1 : start;
                unsigned a = m.ReadU8(edges + 4 * e + 2), b = m.ReadU8(edges + 4 * next + 2);
                inside |= side[a] == 1;
                if (side[a] != 2)
                    polygon.vertices.push_back(remap[a]);
                if ((side[a] == 1 && side[b] == 2) || (side[a] == 2 && side[b] == 1)) {
                    auto opposite = m.ReadU16(edges + 4 * e);
                    unsigned key = std::min(e, unsigned(opposite));
                    auto found = intersections.find(key);
                    unsigned vertex;
                    if (found != intersections.end())
                        vertex = found->second;
                    else {
                        vertex = unsigned(points.size());
                        points.push_back(Intersection(
                            planes + 16 * face, planes + 16 * m.ReadU8(edges + 4 * opposite + 3),
                            cuttingPlane));
                        intersections[key] = vertex;
                    }
                    polygon.vertices.push_back(vertex);
                }
            }
            if (inside && polygon.vertices.size() >= 3) {
                for (unsigned j = 0; j < polygon.vertices.size(); ++j) {
                    auto a = polygon.vertices[j],
                         b = polygon.vertices[(j + 1) % polygon.vertices.size()];
                    if (std::abs(float(Dot(points[a], normal) + offset)) <= epsilon &&
                        std::abs(float(Dot(points[b], normal) + offset)) <= epsilon)
                        cap.emplace_back(b, a);
                }
                polygons.push_back(std::move(polygon));
            }
            start = end;
        }
        if (!cap.empty()) {
            Polygon closing;
            closing.plane = planeWords(cuttingPlane);
            for (unsigned i = 0; i + 1 < cap.size(); ++i) {
                unsigned next = i + 1;
                while (next < cap.size() && cap[next].first != cap[i].second)
                    ++next;
                if (next == cap.size())
                    return 0;
                std::swap(cap[i + 1], cap[next]);
            }
            for (auto edge : cap)
                closing.vertices.push_back(edge.first);
            polygons.push_back(std::move(closing));
        }
        unsigned edgeCount = 0;
        for (const auto &p : polygons)
            edgeCount += unsigned(p.vertices.size());
        auto out = Allocate(36, 0x82ba3668u);
        for (unsigned i = 0; i < 36; i += 4)
            Word(out + i, 0);
        if (!points.empty())
            Reserve(out, unsigned(points.size()), 0x82ba11d8u);
        if (edgeCount)
            ReserveEdges(out + 12, edgeCount);
        if (!polygons.empty())
            Reserve(out + 24, unsigned(polygons.size()), 0x82ba1078u);
        Word(out + 4, unsigned(points.size()));
        Word(out + 16, edgeCount);
        Word(out + 28, unsigned(polygons.size()));
        for (unsigned i = 0; i < points.size(); ++i)
            Point(Word(out) + 12 * i, points[i]);
        std::map<std::pair<unsigned, unsigned>, unsigned> directed;
        unsigned cursor = 0;
        auto outEdges = Word(out + 12);
        for (unsigned face = 0; face < polygons.size(); ++face) {
            const auto &p = polygons[face];
            for (unsigned j = 0; j < 4; ++j)
                Word(Word(out + 24) + 16 * face + 4 * j, p.plane[j]);
            for (unsigned j = 0; j < p.vertices.size(); ++j) {
                auto a = p.vertices[j], b = p.vertices[(j + 1) % p.vertices.size()];
                m.WriteU16(outEdges + 4 * cursor, 0xffffu);
                m.WriteU8(outEdges + 4 * cursor + 2, a);
                m.WriteU8(outEdges + 4 * cursor + 3, face);
                directed[{a, b}] = cursor++;
            }
        }
        for (const auto &[key, e] : directed) {
            auto opposite = directed.find({key.second, key.first});
            if (opposite != directed.end())
                m.WriteU16(outEdges + 4 * e, opposite->second);
        }
        return out;
    }
    bool Inflate(unsigned input, unsigned planeCount, unsigned vertices, unsigned vertexCount,
                 unsigned iterations, unsigned pointsOut, unsigned countOut, unsigned polygonsOut,
                 unsigned wordsOut, double inflation) {
        if (vertexCount < 4)
            return false;
        iterations = std::min(iterations, planeCount);
        auto lo = Point(vertices), hi = lo;
        for (unsigned i = 0; i < vertexCount; ++i) {
            auto p = Point(vertices + 12 * i);
            for (unsigned a = 0; a < 3; ++a) {
                lo[a] = std::min(lo[a], p[a]);
                hi[a] = std::max(hi[a], p[a]);
            }
        }
        auto extent = Sub(hi, lo);
        float diagonal = float(std::sqrt(Dot(extent, extent)));
        float pad = float(inflation * Float(0x82003660u));
        for (unsigned a = 0; a < 3; ++a) {
            lo[a] = float(lo[a] - pad);
            hi[a] = float(hi[a] + pad);
        }
        for (unsigned i = 0; i < planeCount; ++i)
            Float(input + 16 * i + 12, float(Float(input + 16 * i + 12) - inflation));
        extent = Sub(hi, lo);
        Float(0x83216160u, float(float(std::sqrt(Dot(extent, extent))) * Float(0x82000d6cu)));
        auto radians = float(Float(0x83216164u) * Float(0x820009c8u));
        s.fpr_bits[1] = std::bit_cast<std::uint64_t>(double(radians));
        (void)mesh_hull_incremental61::Apply(0x822a2f08u, m, d, s);
        auto close = float(std::bit_cast<double>(s.fpr_bits[1]));
        auto zero = Float(0x82000e50u);
        for (unsigned i = 0; i < 6; ++i) {
            Vec axis{zero, zero, zero};
            axis[i / 2] = Float((i & 1) ? 0x82007784u : 0x82000e40u);
            bool found = false;
            for (unsigned j = 0; j < planeCount; ++j)
                if (Dot(axis, Point(input + 16 * j)) > close) {
                    found = true;
                    break;
                }
            if (found) {
                auto &bound = (i & 1) ? hi : lo;
                for (unsigned a = 0; a < 3; ++a)
                    bound[a] =
                        float(bound[a] + float(axis[a] * float(diagonal * Float(0x8201f9f0u))));
            }
        }
        auto body = Box(lo, hi);
        for (unsigned i = 0; i < iterations; ++i) {
            auto index = Select(input, std::int32_t(planeCount), body, Float(0x82000d7cu));
            if (index < 0)
                break;
            auto next = Clip(body, input + 16 * unsigned(index));
            if (!next ||
                !Valid(
                    next)) { /* Original retains the previous body on failed clipping/validity. */
                break;
            }
            Release(body);
            Free(body, 0x82ba589cu);
            body = next;
        }
        auto polygons = Allocate(4 * (Word(body + 16) + Word(body + 28) + 1), 0x82ba58d8u);
        Word(polygonsOut, polygons);
        unsigned cursor = 1, faces = 0;
        auto edges = Word(body + 12);
        for (unsigned begin = 0; begin < Word(body + 16);) {
            unsigned end = begin + 1;
            while (end < Word(body + 16) &&
                   m.ReadU8(edges + 4 * end + 3) == m.ReadU8(edges + 4 * begin + 3))
                ++end;
            Word(polygons + 4 * cursor++, end - begin);
            for (unsigned j = begin; j < end; ++j)
                Word(polygons + 4 * cursor++, m.ReadU8(edges + 4 * j + 2));
            ++faces;
            begin = end;
        }
        Word(polygons, faces);
        Word(wordsOut, cursor);
        Word(pointsOut, Word(body));
        Word(countOut, Word(body + 4));
        for (unsigned off : {0, 4, 8})
            Word(body + off, 0);
        Release(body);
        Free(body, 0x82ba5a5cu);
        return true;
    }
    unsigned Next(unsigned self, unsigned edge, unsigned start) {
        auto count = Word(self + 16), edges = Word(self + 12);
        return edge + 1 < count &&
                       m.ReadU8(edges + 4 * (edge + 1) + 3) == m.ReadU8(edges + 4 * edge + 3)
                   ? edge + 1
                   : start;
    }
    bool Valid(unsigned self) {
        auto count = Word(self + 16), edges = Word(self + 12), vertices = Word(self),
             planes = Word(self + 24);
        unsigned start = 0;
        for (unsigned i = 0; i < count; ++i) {
            if (i && m.ReadU8(edges + 4 * i + 3) != m.ReadU8(edges + 4 * (i - 1) + 3))
                start = i;
            auto next = Next(self, i, start);
            auto opposite = std::int16_t(m.ReadU16(edges + 4 * i));
            if (opposite == -1 || opposite == 255 ||
                m.ReadU8(edges + 4 * unsigned(opposite) + 2) != m.ReadU8(edges + 4 * next + 2))
                return false;
        }
        start = 0;
        const auto epsilon = Float(0x83216160u), zero = Float(0x82000e50u);
        for (unsigned i = 0; i < count; ++i) {
            auto edge = edges + 4 * i;
            auto face = m.ReadU8(edge + 3);
            auto plane = planes + 16 * face, vertex = vertices + 12 * m.ReadU8(edge + 2);
            float distance = float(Dot(Point(plane), Point(vertex)) + Float(plane + 12));
            if (distance > epsilon || distance < -epsilon)
                return false;
            if (i && face != m.ReadU8(edges + 4 * (i - 1) + 3))
                start = i;
            auto next = Next(self, i, start), third = Next(self, next, start);
            if (third != i) {
                auto n = Normal(vertex, vertices + 12 * m.ReadU8(edges + 4 * next + 2),
                                vertices + 12 * m.ReadU8(edges + 4 * third + 2));
                if (Dot(n, Point(plane)) <= zero)
                    return false;
            }
        }
        return true;
    }
    int Select(unsigned input, int count, unsigned self, double minimum) {
        auto radians = float(Float(0x83216164u) * Float(0x820009c8u));
        s.fpr_bits[1] = std::bit_cast<std::uint64_t>(double(radians));
        (void)mesh_hull_incremental61::Apply(0x822a2f08u, m, d, s);
        auto close = float(std::bit_cast<double>(s.fpr_bits[1]));
        auto zero = Float(0x82000e50u), epsilon = Float(0x83216160u);
        float best = zero;
        int selected = -1;
        for (int i = 0; i < count; ++i) {
            auto candidate = input + 16 * unsigned(i);
            auto n = Point(candidate);
            auto offset = Float(candidate + 12);
            float hi = zero, lo = zero;
            for (unsigned j = 0; j < Word(self + 4); ++j) {
                auto point = Point(Word(self) + 12 * j);
                float value =
                    float(float(float(offset + float(point[0] * n[0])) + float(point[1] * n[1])) +
                          float(point[2] * n[2]));
                hi = std::max(hi, value);
                lo = std::min(lo, value);
            }
            auto width = float(hi - lo);
            if (width < epsilon)
                width = Float(0x82007784u);
            float score = float(hi / width);
            if (score <= best)
                continue;
            for (unsigned planeID = 0; planeID < Word(self + 28); ++planeID) {
                auto plane = Word(self + 24) + 16 * planeID;
                auto other = Point(plane);
                if (n == other && offset == Float(plane + 12)) {
                    score = zero;
                    continue;
                }
                if (Dot(n, other) <= close)
                    continue;
                for (unsigned e = 0; e < Word(self + 16); ++e) {
                    auto edge = Word(self + 12) + 4 * e;
                    if (m.ReadU8(edge + 3) == planeID &&
                        float(Dot(n, Point(Word(self) + 12 * m.ReadU8(edge + 2))) + offset) <
                            zero) {
                        score = zero;
                        break;
                    }
                }
            }
            if (score > best) {
                best = score;
                selected = i;
            }
        }
        return best > minimum ? selected : -1;
    }
};
} // namespace
bool Apply(GuestAddress entry, GuestMemory &m, Dependencies d, Registers &s) {
    switch (entry) {
    case 0x82b9f9d0u:
    case 0x82b9fba8u:
    case 0x82b9fc58u:
    case 0x82ba2e00u:
    case 0x82ba5480u:
    case 0x82ba0fc8u:
    case 0x82ba2330u:
    case 0x82ba29d8u:
    case 0x82ba1528u:
    case 0x82ba17e8u:
    case 0x82ba1e30u:
        break;
    default:
        return false;
    }
    const auto a = s.r;
    auto f = std::bit_cast<double>(s.fpr_bits[1]);
    Polyhedron p{m, d, s};
    const auto extra = entry == 0x82ba5480u ? m.ReadU32(Address(s.r[1] + 84)) : 0u;
    p.Enter();
    switch (entry) {
    case 0x82b9f9d0u:
        p.WriteMatrix(Address(a[3]), p.Inverse(p.ReadMatrix(Address(a[4]))));
        s.r[3] = a[3];
        break;
    case 0x82b9fba8u:
        p.WriteMatrix(Address(a[3]), p.Transpose(p.ReadMatrix(Address(a[4]))));
        s.r[3] = a[3];
        break;
    case 0x82b9fc58u:
        p.Point(Address(a[3]), p.Intersection(Address(a[4]), Address(a[5]), Address(a[6])));
        s.r[3] = a[3];
        break;
    case 0x82ba2e00u:
        s.r[3] = p.Clip(Address(a[3]), Address(a[4]));
        break;
    case 0x82ba5480u:
        s.r[3] = p.Inflate(Address(a[3]), Address(a[4]), Address(a[5]), Address(a[6]),
                           Address(a[7]), Address(a[8]), Address(a[9]), Address(a[10]), extra, f)
                     ? 1
                     : 0;
        break;
    case 0x82ba0fc8u:
        p.ReserveEdges(Address(a[3]), Address(a[4]));
        break;
    case 0x82ba2330u: {
        auto zero = p.Float(0x82000e50u), one = p.Float(0x82007784u);
        s.r[3] = p.Box({zero, zero, zero}, {one, one, one});
        break;
    }
    case 0x82ba29d8u:
        s.r[3] = p.Box(p.Point(Address(a[3])), p.Point(Address(a[4])));
        break;
    case 0x82ba1528u:
        s.r[3] = p.Valid(Address(a[3])) ? 1 : 0;
        break;
    case 0x82ba17e8u:
        s.r[3] = std::uint64_t(
            std::int64_t(p.Select(Address(a[3]), std::int32_t(a[4]), Address(a[5]), f)));
        break;
    case 0x82ba1e30u:
        p.Release(Address(a[3]));
        break;
    }
    p.Leave();
    return true;
}
} // namespace lo::semantic::gpu::mesh_hull_polyhedron61
