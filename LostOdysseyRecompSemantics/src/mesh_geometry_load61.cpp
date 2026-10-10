#include "lo_semantics/mesh_geometry_load61.h"
#include "lo_semantics/mesh_hull_incremental61.h"
#include "lo_semantics/mesh_stream_codec61.h"
#include "lo_semantics/mesh_polygon_collect61.h"
#include "lo_semantics/recovery_abi.h"
#include <algorithm>
#include <array>
#include <bit>
namespace lo::semantic::gpu::mesh_geometry_load61 {
namespace {
using recovery_abi::Address;
struct Reader {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    unsigned Word(unsigned p) { return m.ReadU32(p); }
    float Float(unsigned p) { return std::bit_cast<float>(Word(p)); }
    void Float(unsigned p, float v) { m.WriteU32(p, std::bit_cast<unsigned>(v)); }
    float Trig(float angle, bool cosine) {
        s.fpr_bits[1] = std::bit_cast<std::uint64_t>(double(angle));
        (void)mesh_hull_incremental61::Apply(cosine ? 0x822a2f08u : 0x822a2fe0u, m, d, s);
        return float(std::bit_cast<double>(s.fpr_bits[1]));
    }
    void Normals(unsigned count, unsigned out, unsigned swap, unsigned stream) {
        auto frame = s.r[1];
        s.r[12] = std::uint32_t(0 - 2 * count) & 0xfffffff0u;
        mesh_polygon_collect61::ProbeStack(m, s);
        auto back = Word(Address(s.r[1]));
        s.r[1] += s.r[12];
        m.WriteU32(Address(s.r[1]), back);
        auto packed = Address(s.r[1] + 80);
        s.r[3] = packed;
        s.r[4] = count;
        s.r[5] = swap;
        s.r[6] = stream;
        (void)mesh_stream_codec61::Apply(0x82bd7ed8u, m, {d.lifetime.guest, d.lifetime.fp}, s);
        constexpr unsigned registered = 0x832df53cu, initialized = 0x832df538u, table = 0x832dc538u;
        if (!(Word(registered) & 1)) {
            m.WriteU32(registered, Word(registered) | 1);
            s.r[3] = 0xffffffff830d9a60ull;
            s.lr = 0x82bc6a50u;
            d.lifetime.guest.CallDirect(0x82b7be48u, m, s);
        }
        if (!m.ReadU8(initialized)) {
            m.WriteU8(initialized, 1);
            auto step = Float(0x820d6954u);
            for (unsigned i = 0; i < 32; ++i) {
                float angle = float(float(i) * step), cosine = Trig(angle, true),
                      sine = Trig(angle, false);
                for (unsigned j = 0; j < 32; ++j) {
                    float phi = float(float(j) * step), c = Trig(phi, true), a = float(c * cosine),
                          b = Trig(phi, false), z = float(c * sine);
                    if (a > b)
                        std::swap(a, b);
                    if (b > z)
                        std::swap(b, z);
                    if (a > b)
                        std::swap(a, b);
                    if (b > z)
                        std::swap(b, z);
                    auto p = table + 12 * (32 * i + j);
                    Float(p, a);
                    Float(p + 4, b);
                    Float(p + 8, z);
                }
            }
        }
        constexpr unsigned permutations[]{1176, 8544, 6660};
        for (unsigned i = 0; i < count; ++i) {
            auto code = m.ReadU16(packed + 2 * i);
            unsigned point = table + 12 * (code >> 6), shift = 2 * (code & 7),
                     sign = (code >> 3) & 7;
            for (unsigned axis = 0; axis < 3; ++axis) {
                auto component = (permutations[axis] >> shift) & 3;
                auto bits = Word(point + 4 * component) | (((sign >> axis) & 1u) << 31);
                m.WriteU32(out + 12 * i + 4 * axis, bits);
            }
        }
        s.r[1] = frame;
    }
    void Run() {
        auto a = s.r;
        auto old = s.r[1];
        m.WriteU32(Address(old - 8), Address(s.lr));
        for (unsigned i = 14; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(old - 16 - 8 * (31 - i)), s.r[i]);
        s.r[1] -= 512;
        m.WriteU32(Address(s.r[1]), Address(old));
        Normals(Address(a[3]), Address(a[4]), Address(a[5]), Address(a[6]));
        s.r[1] += 512;
        for (unsigned i = 14; i < 32; ++i)
            s.r[i] = recovery_abi::ReadU64(m, Address(s.r[1] - 16 - 8 * (31 - i)));
        s.lr = m.ReadU32(Address(s.r[1] - 8));
    }
};
} // namespace
bool Apply(GuestAddress entry, GuestMemory &m, Dependencies d, Registers &s) {
    if (entry != 0x82bc69e0u)
        return false;
    Reader{m, d, s}.Run();
    return true;
}
} // namespace lo::semantic::gpu::mesh_geometry_load61
