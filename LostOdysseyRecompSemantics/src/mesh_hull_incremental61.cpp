#include "lo_semantics/mesh_hull_incremental61.h"
#include "lo_semantics/recovery_abi.h"
#include <array>
#include <bit>
#include <cmath>
#include <limits>
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
    double Double(std::uint32_t p) { return std::bit_cast<double>(recovery_abi::ReadU64(m, p)); }
    double Trig(double x, bool cosine) {
        constexpr std::uint32_t table = 0x83214d80u;
        double magnitude = std::abs(x);
        if (!cosine && magnitude == 0)
            return x;
        double shifted = cosine ? Double(table) + magnitude : magnitude;
        double scaled = Double(table + 8) * shifted, rounded = std::nearbyint(scaled);
        std::int64_t quadrant;
        if (scaled > double(std::numeric_limits<std::int64_t>::max()))
            quadrant = std::numeric_limits<std::int64_t>::max();
        else if (!std::isfinite(rounded) || rounded >= 0x1p63 || rounded < -0x1p63)
            quadrant = std::numeric_limits<std::int64_t>::min();
        else
            quadrant = static_cast<std::int64_t>(rounded);
        double turns = double(quadrant) - (cosine ? double(Float(table + 36)) : 0.);
        double reduced = -(Double(table + 40) * turns - magnitude);
        reduced = -(Double(table + 48) * turns - reduced);
        double squared = reduced * reduced, poly = Double(table + 112);
        for (int offset = 104; offset >= 56; offset -= 8)
            poly = poly * squared + Double(table + unsigned(offset));
        double value = (poly * squared + Double(0x82000f28u)) * reduced;
        if (quadrant & 1)
            value = -value;
        if (cosine && magnitude == double(Float(table + 24)))
            return Float(table + 28);
        if (shifted - Double(table + 16) >= 0)
            return Double(0x83215508u);
        if (!cosine)
            value *= x >= 0 ? Float(table + 28) : Float(table + 32);
        return value;
    }
    std::uint32_t StableSupport(std::uint32_t vertices, unsigned count, std::uint32_t direction,
                                std::uint32_t mask) {
        const auto axis = Point(direction);
        const auto end = Float(0x82000de0u), coarseStep = Float(0x82000b7cu),
                   fineStep = Float(0x82000e44u), back = Float(0x82000dc0u),
                   scale = Float(0x820d57f0u), radians = Float(0x820009c8u);
        for (;;) {
            auto best = Support(vertices, std::int32_t(count), direction, mask);
            if (Word(Word(mask) + 4 * best) == 3)
                return best;
            auto u = Orthogonal(axis), v = Cross(u, axis);
            std::uint32_t previous = 0xffffffffu;
            auto probe = [&](float angle) {
                auto theta = float(angle * radians);
                float sine = float(Trig(theta, false)), cosine = float(Trig(theta, true));
                Vec perturbed;
                for (unsigned a = 0; a < 3; ++a)
                    perturbed[a] = float(
                        axis[a] + float(float(float(u[a] * sine) + float(v[a] * cosine)) * scale));
                auto temp = Address(s.r[1] + 176);
                Point(temp, perturbed);
                return Support(vertices, std::int32_t(count), temp, mask);
            };
            bool accepted = false;
            for (float angle = Float(0x82000e50u); angle <= end;
                 angle = float(angle + coarseStep)) {
                auto current = probe(angle);
                if (previous == best && current == best) {
                    accepted = true;
                    break;
                }
                if (previous != 0xffffffffu && previous != current) {
                    for (float fine = float(angle - back); fine <= angle;
                         fine = float(fine + fineStep)) {
                        auto next = probe(fine);
                        if (previous == best && next == best) {
                            accepted = true;
                            break;
                        }
                        previous = next;
                    }
                    if (accepted)
                        break;
                }
                previous = current;
            }
            Word(Word(mask) + 4 * best, accepted ? 3 : 0);
            if (accepted)
                return best;
        }
    }
    void Simplex(std::uint32_t out, std::uint32_t vertices, unsigned count, std::uint32_t mask) {
        auto sp = Address(s.r[1]);
        auto choose = [&](const Vec &dir) {
            Point(sp + 112, dir);
            return StableSupport(vertices, count, sp + 112, mask);
        };
        auto a = choose({Float(0x82000d7cu), Float(0x82000b58u), Float(0x82007784u)});
        auto b = choose({Float(0x822183e8u), Float(0x82218644u), Float(0x82000e40u)});
        auto fail = [&]() {
            for (unsigned i = 0; i < 4; ++i)
                Word(out + 4 * i, 0xffffffffu);
        };
        auto pa = Point(vertices + 12 * a), pb = Point(vertices + 12 * b);
        auto line = Sub(pa, pb);
        const auto zero = Float(0x82000e50u), plus = Float(0x82000b58u), minus = Float(0x82218644u);
        if (a == b || (line[0] == zero && line[1] == zero && line[2] == zero)) {
            fail();
            return;
        }
        // Two explicit source cross products avoid an ill-conditioned axis.
        Vec u{float(double(line[2]) * plus - float(line[1] * zero)),
              float(float(line[0] * zero) - line[2]), float(-(double(line[0]) * plus - line[1]))};
        Vec v{float(line[2] - float(line[1] * zero)),
              float(-(double(line[2]) * minus - float(line[0] * zero))),
              float(double(line[1]) * minus - line[0])};
        Vec perpendicular = Unit(Length(u) > Length(v) ? u : v);
        auto c = choose(perpendicular);
        if (c == a || c == b) {
            for (auto &x : perpendicular)
                x = -x;
            c = choose(perpendicular);
        }
        if (c == a || c == b) {
            fail();
            return;
        }
        auto pc = Point(vertices + 12 * c), normal = Unit(Cross(Sub(pc, pa), line));
        auto dpoint = choose(normal);
        if (dpoint == a || dpoint == b || dpoint == c || !NonCoplanar(vertices, a, b, c, dpoint)) {
            for (auto &x : normal)
                x = -x;
            dpoint = choose(normal);
        }
        if (dpoint == a || dpoint == b || dpoint == c) {
            fail();
            return;
        }
        auto n = Cross(Sub(pb, pa), Sub(pc, pa)), delta = Sub(Point(vertices + 12 * dpoint), pa);
        float dot = float(delta[1] * n[1]);
        dot = float(double(delta[2]) * n[2] + dot);
        dot = float(double(delta[0]) * n[0] + dot);
        if (dot < zero)
            std::swap(c, dpoint);
        Word(out, a);
        Word(out + 4, b);
        Word(out + 8, c);
        Word(out + 12, dpoint);
    }
    void ArrayReserve(std::uint32_t descriptor, unsigned count) {
        s.r[3] = descriptor;
        s.r[4] = count;
        (void)mesh_hull_preprocess61::Apply(0x82ba1138u, m, d, s);
    }
    void ArrayRelease(std::uint32_t descriptor) {
        s.r[3] = descriptor;
        (void)mesh_hull_preprocess61::Apply(0x82ba0d68u, m, d, s);
    }
    bool Build(std::uint32_t vertices, std::int32_t count, std::int32_t limit) {
        if (count < 4)
            return false;
        if (limit == 0)
            limit = 1000000000;
        auto sp = Address(s.r[1]), mask = sp + 80, used = sp + 96, simplex = sp + 224;
        for (unsigned off : {0, 4, 8}) {
            Word(mask + off, 0);
            Word(used + off, 0);
        }
        ArrayReserve(mask, unsigned(count));
        ArrayReserve(used, unsigned(count));
        auto minimum = Point(vertices), maximum = minimum;
        for (std::int32_t i = 0; i < count; ++i) {
            Word(Word(mask) + 4 * unsigned(i), 1);
            Word(Word(used) + 4 * unsigned(i), 0);
            auto p = Point(vertices + 12 * unsigned(i));
            for (unsigned a = 0; a < 3; ++a) {
                minimum[a] = std::min(minimum[a], p[a]);
                maximum[a] = std::max(maximum[a], p[a]);
            }
        }
        Word(mask + 4, unsigned(count));
        Word(used + 4, unsigned(count));
        auto diagonal = Sub(maximum, minimum);
        float squared = float(diagonal[2] * diagonal[2]);
        squared = float(double(diagonal[0]) * diagonal[0] + squared);
        squared = float(double(diagonal[1]) * diagonal[1] + squared);
        const float epsilon = float(float(std::sqrt(squared)) * Float(0x82000d6cu));
        Simplex(simplex, vertices, unsigned(count), mask);
        if (Word(simplex) == 0xffffffffu) {
            ArrayRelease(mask);
            ArrayRelease(used);
            return false;
        }
        unsigned a = Word(simplex), b = Word(simplex + 4), c = Word(simplex + 8),
                 dpoint = Word(simplex + 12);
        Vec center;
        auto pa = Point(vertices + 12 * a), pb = Point(vertices + 12 * b),
             pc = Point(vertices + 12 * c), pd = Point(vertices + 12 * dpoint);
        for (unsigned axis = 0; axis < 3; ++axis)
            center[axis] = float(float(float(float(pa[axis] + pb[axis]) + pc[axis]) + pd[axis]) *
                                 Float(0x82000da4u));
        constexpr unsigned neighbor[4][3]{{2, 3, 1}, {3, 2, 0}, {0, 1, 3}, {1, 0, 2}};
        const unsigned corners[4][3]{{c, dpoint, b}, {dpoint, c, a}, {a, b, dpoint}, {b, a, c}};
        constexpr GuestAddress allocations[]{0x82ba449cu, 0x82ba44f4u, 0x82ba454cu, 0x82ba45a4u};
        for (unsigned i = 0; i < 4; ++i) {
            auto face = AllocateFace(allocations[i]);
            if (face)
                Face(face, corners[i][0], corners[i][1], corners[i][2]);
            for (unsigned j = 0; j < 3; ++j)
                Word(face + 12 + 4 * j, neighbor[i][j]);
        }
        for (auto vertex : {a, b, c, dpoint})
            Word(Word(used) + 4 * vertex, 1);
        auto update = [&](std::uint32_t face, bool skipUsed) {
            auto origin = Point(vertices + 12 * Word(face));
            auto n = TriangleNormal(origin, Point(vertices + 12 * Word(face + 4)),
                                    Point(vertices + 12 * Word(face + 8)));
            Point(sp + 256, n);
            auto vertex = StableSupport(vertices, unsigned(count), sp + 256, mask);
            Word(face + 28, vertex);
            if (skipUsed && Word(Word(used) + 4 * vertex)) {
                Word(face + 28, 0xffffffffu);
                return;
            }
            auto delta = Sub(Point(vertices + 12 * vertex), origin);
            float distance = float(n[1] * delta[1]);
            distance = float(double(n[2]) * delta[2] + distance);
            distance = float(double(n[0]) * delta[0] + distance);
            Float(face + 32, distance);
        };
        for (unsigned i = 0; i < Word(0x832dc424u); ++i)
            update(FaceAt(i), false);
        const float visibilityScale = Float(0x82000d7cu), areaScale = Float(0x82000dacu);
        for (std::int32_t remaining = limit - 4; remaining > 0; --remaining) {
            auto next = FurthestFace(epsilon);
            if (!next)
                break;
            auto vertex = Word(next + 28);
            Word(Word(used) + 4 * vertex, 1);
            auto before = Word(0x832dc424u);
            for (unsigned i = before; i > 0;) {
                auto face = FaceAt(--i);
                if (face &&
                    Above(vertices, face, vertices + 12 * vertex, float(epsilon * visibilityScale)))
                    Extrude(face, vertex);
            }
            unsigned i = Word(0x832dc424u);
            while (i) {
                auto face = FaceAt(--i);
                if (!face)
                    continue;
                if (Word(face) != vertex && Word(face + 4) != vertex && Word(face + 8) != vertex)
                    break;
                Point(sp + 288, center);
                bool repair = Above(vertices, face, sp + 288, float(epsilon * visibilityScale));
                if (!repair) {
                    auto a0 = Point(vertices + 12 * Word(face)),
                         b0 = Point(vertices + 12 * Word(face + 4)),
                         c0 = Point(vertices + 12 * Word(face + 8));
                    auto n = Cross(Sub(b0, a0), Sub(c0, b0));
                    float yz = float(double(n[2]) * n[2] + float(n[1] * n[1]));
                    float area = float(std::sqrt(float(double(n[0]) * n[0] + yz)));
                    repair = area < float(float(epsilon * epsilon) * areaScale);
                }
                if (repair) {
                    Extrude(FaceAt(Word(face + 12)), vertex);
                    i = Word(0x832dc424u);
                }
            }
            for (unsigned j = Word(0x832dc424u); j > 0;) {
                auto face = FaceAt(--j);
                if (!face)
                    continue;
                if (std::int32_t(Word(face + 28)) >= 0)
                    break;
                update(face, true);
            }
        }
        ArrayRelease(mask);
        ArrayRelease(used);
        return true;
    }
    float Dot(const Vec &a, const Vec &b) {
        float value = float(a[1] * b[1]);
        value = float(double(a[2]) * b[2] + value);
        return float(double(a[0]) * b[0] + value);
    }
    std::uint32_t UnfilteredSupport(std::uint32_t vertices, std::int32_t count,
                                    const Vec &direction) {
        unsigned best = 0;
        for (std::int32_t i = 1; i < count; ++i)
            if (Dot(Point(vertices + 12 * unsigned(i)), direction) >
                Dot(Point(vertices + 12 * best), direction))
                best = unsigned(i);
        return best;
    }
    void ReservePlanes(std::uint32_t descriptor, unsigned capacity) {
        auto old = Word(descriptor);
        Word(descriptor + 8, capacity);
        s.r[3] = Word(0x832df548u);
        s.r[4] = 16 * capacity;
        s.r[5] = 254;
        s.ctr = Word(Word(s.r[3]) + 8);
        s.lr = 0x82ba10b4u;
        d.edge.engine.sort.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
        auto buffer = Address(s.r[3]);
        Word(descriptor, buffer);
        for (unsigned i = 0; i < Word(descriptor + 4); ++i)
            for (unsigned j = 0; j < 16; j += 4)
                Word(buffer + 16 * i + j, Word(old + 16 * i + j));
        if (old) {
            s.r[3] = Word(0x832df548u);
            s.r[4] = old;
            s.ctr = Word(Word(s.r[3]) + 20);
            s.lr = 0x82ba112cu;
            d.edge.engine.sort.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
        }
    }
    std::uint32_t AppendPlaneWords(std::uint32_t descriptor,
                                   const std::array<std::uint32_t, 4> &words) {
        auto count = Word(descriptor + 4), capacity = Word(descriptor + 8);
        if (count == capacity)
            ReservePlanes(descriptor, capacity ? 2 * capacity : 16);
        auto out = Word(descriptor) + 16 * count;
        for (unsigned j = 0; j < 4; ++j)
            Word(out + 4 * j, words[j]);
        Word(descriptor + 4, count + 1);
        return out;
    }
    void AppendPlane(std::uint32_t descriptor, const Vec &normal, float offset) {
        AppendPlaneWords(descriptor, {std::bit_cast<std::uint32_t>(normal[0]),
                                      std::bit_cast<std::uint32_t>(normal[1]),
                                      std::bit_cast<std::uint32_t>(normal[2]),
                                      std::bit_cast<std::uint32_t>(offset)});
    }
    bool Planes(std::uint32_t vertices, unsigned count, std::int32_t limit, std::uint32_t output,
                double angle) {
        auto temporary = Address(s.r[1] + 80);
        for (unsigned off : {0, 4, 8})
            Word(temporary + off, 0);
        Word(output + 4, 0);
        Enter();
        bool built = Build(vertices, std::int32_t(count), limit);
        Leave();
        if (!built) {
            ArrayRelease(temporary);
            return false;
        }
        const auto radians = Float(0x820009c8u), zero = Float(0x82000e50u);
        const auto mergeCos = float(Trig(float(Float(0x83216164u) * radians), true)),
                   edgeCos = float(Trig(float(angle * radians), true));
        auto normal = [&](unsigned face) {
            return TriangleNormal(Point(vertices + 12 * Word(face)),
                                  Point(vertices + 12 * Word(face + 4)),
                                  Point(vertices + 12 * Word(face + 8)));
        };
        for (unsigned i = 0; i < Word(0x832dc424u); ++i) {
            auto face = FaceAt(i);
            if (!face)
                continue;
            auto n = normal(face);
            for (unsigned slot = 0; slot < 3; ++slot) {
                auto adjacent = Word(face + 12 + 4 * slot);
                if (adjacent < Word(face + 24))
                    continue;
                auto other = normal(FaceAt(adjacent));
                if (Dot(n, other) >= edgeCos)
                    continue;
                auto edge = Sub(Point(vertices + 12 * Word(face + 4 * ((slot + 2) % 3))),
                                Point(vertices + 12 * Word(face + 4 * ((slot + 1) % 3))));
                Vec candidate;
                if (edge[0] != zero || edge[1] != zero || edge[2] != zero) {
                    auto left = Cross(edge, n), right = Cross(other, edge);
                    for (unsigned a = 0; a < 3; ++a)
                        candidate[a] = float(left[a] + right[a]);
                } else
                    for (unsigned a = 0; a < 3; ++a)
                        candidate[a] = float(n[a] + other[a]);
                if (candidate[0] == zero && candidate[1] == zero && candidate[2] == zero) {
                    ArrayRelease(temporary);
                    return false;
                }
                candidate = Unit(candidate);
                auto support = UnfilteredSupport(vertices, std::int32_t(count), candidate);
                AppendPlane(temporary, candidate, -Dot(Point(vertices + 12 * support), candidate));
            }
        }
        auto areaSquared = [&](unsigned face) {
            auto a = Point(vertices + 12 * Word(face)), b = Point(vertices + 12 * Word(face + 4)),
                 c = Point(vertices + 12 * Word(face + 8));
            auto cross = Cross(Sub(c, a), Sub(a, b));
            return Dot(cross, cross);
        };
        for (unsigned i = 0; i < Word(0x832dc424u); ++i)
            for (unsigned j = i + 1; j < Word(0x832dc424u); ++j) {
                auto a = FaceAt(i), b = FaceAt(j);
                if (!a || !b)
                    continue;
                if (Dot(normal(a), normal(b)) > mergeCos) {
                    if (areaSquared(a) < areaSquared(b))
                        FreeFace(a, 0x82ba51d0u);
                    else
                        FreeFace(b, 0x82ba5248u);
                }
            }
        for (unsigned i = 0; i < Word(0x832dc424u); ++i) {
            auto face = FaceAt(i);
            if (face) {
                auto n = normal(face);
                AppendPlane(output, n, -Dot(Point(vertices + 12 * Word(face)), n));
            }
        }
        for (unsigned i = 0; i < Word(temporary + 4); ++i) {
            auto candidate = Word(temporary) + 16 * i;
            auto n = Point(candidate);
            bool duplicate = false;
            for (unsigned j = 0; j < Word(output + 4); ++j)
                if (Dot(n, Point(Word(output) + 16 * j)) > mergeCos) {
                    duplicate = true;
                    break;
                }
            if (!duplicate)
                AppendPlane(output, n, Float(candidate + 12));
        }
        for (unsigned i = 0; i < Word(0x832dc424u); ++i)
            if (auto face = FaceAt(i))
                FreeFace(face, 0x82ba542cu);
        Word(0x832dc424u, 0);
        ArrayRelease(temporary);
        return true;
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
    case 0x822a2f08u:
    case 0x822a2fe0u:
    case 0x82ba2010u:
    case 0x82ba0dd0u:
    case 0x82ba1078u:
    case 0x82ba1ee0u:
    case 0x82ba4bf8u:
    case 0x82ba40b8u:
    case 0x82ba3be0u:
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
    case 0x822a2f08u:
    case 0x822a2fe0u:
        s.fpr_bits[1] = std::bit_cast<std::uint64_t>(h.Trig(f1, e == 0x822a2f08u));
        break;
    case 0x82ba2010u:
        s.r[3] = h.StableSupport(Address(a[3]), Address(a[4]), Address(a[5]), Address(a[6]));
        break;
    case 0x82ba0dd0u:
        s.r[3] = h.UnfilteredSupport(Address(a[3]), std::int32_t(a[4]), h.Point(Address(a[5])));
        break;
    case 0x82ba1078u:
        h.ReservePlanes(Address(a[3]), Address(a[4]));
        break;
    case 0x82ba1ee0u:
        s.r[3] = h.AppendPlaneWords(Address(a[3]), {unsigned(a[4] >> 32), unsigned(a[4]),
                                                    unsigned(a[5] >> 32), unsigned(a[5])});
        break;
    case 0x82ba4bf8u:
        s.r[3] =
            h.Planes(Address(a[3]), Address(a[4]), std::int32_t(a[5]), Address(a[6]), f1) ? 1 : 0;
        break;
    case 0x82ba40b8u:
        s.r[3] = h.Build(Address(a[3]), std::int32_t(a[4]), std::int32_t(a[5])) ? 1 : 0;
        break;
    case 0x82ba3be0u:
        h.Simplex(Address(a[3]), Address(a[4]), Address(a[5]), Address(a[6]));
        s.r[3] = a[3];
        break;
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
