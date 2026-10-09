#include "lo_semantics/mesh_geometry_stream61.h"
#include "lo_semantics/mesh_normal_encode61.h"
#include "lo_semantics/mesh_valence_stream61.h"
#include "lo_semantics/mesh_vertex_normals61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/serialization_control61.h"
#include <bit>
namespace lo::semantic::gpu::mesh_geometry_stream61 {
namespace {
using recovery_abi::Address;
struct GeometryStream {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    std::uint16_t Half(std::uint64_t p) { return m.ReadU16(Address(p)); }
    std::uint8_t Byte(std::uint64_t p) { return m.ReadU8(Address(p)); }
    void Store(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    static std::uint32_t Shift(std::uint64_t v, unsigned n) { return Address(v) << n; }
    void Compare(std::uint64_t a, std::uint64_t b = 0) {
        auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    void Load(unsigned i, std::uint64_t p) {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            d.edge.engine.fp.SetHostFpControl(s.cached_fp_control);
        }
        s.fpr_bits[i] = std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(Word(p))));
    }
    void Lower(GuestAddress e, GuestAddress continuation) {
        s.lr = continuation;
        mesh_stream_write61::Dependencies stream{d.edge.engine.sort.guest, d.edge.engine.fp};
        switch (e) {
        case 0x82bd0798u:
            (void)crt_close_recursive_buffer_context::Apply(e, m, stream.guest, s);
            break;
        case 0x82b9cb70u:
            (void)serialization_control61::Apply(e, m, stream, s);
            break;
        case 0x82bbb728u:
            (void)mesh_polygon_topology61::Apply(e, m, d, s);
            break;
        case 0x82bb9aa8u:
            (void)mesh_polygon_build61::Apply(e, m, d, s);
            break;
        case 0x82bb9160u:
            (void)mesh_vertex_normals61::Apply(e, m, {d.lifetime, d.edge.engine.sort.accepted}, s);
            break;
        case 0x82bb8fa0u:
            (void)mesh_normal_encode61::Apply(e, m, d.edge.engine.fp, s);
            break;
        case 0x82badfa0u:
        case 0x82bd8668u:
        case 0x82bd8360u:
        case 0x82bd83a0u:
            (void)mesh_valence_stream61::Apply(e, m, stream, s);
            break;
        default:
            (void)mesh_stream_write61::Apply(e, m, stream, s);
            break;
        }
    }
    void Call(unsigned reg, GuestAddress continuation) {
        s.ctr = s.r[reg];
        s.lr = continuation;
        d.edge.engine.sort.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void ScalarArgs() {
        s.r[5] = s.r[30];
        s.r[4] = s.r[29];
    }
    void MeshCount(unsigned offset, GuestAddress continuation) {
        auto &r = s.r;
        r[11] = Word(r[31] + 4);
        ScalarArgs();
        r[3] = Word(r[11] + offset);
        Lower(0x82bd7db0u, continuation);
    }
    void PackNormal(unsigned first, int step, GuestAddress continuation) {
        auto &r = s.r;
        r[11] = Word(r[1] + first);
        r[10] = Word(r[1] + first + step);
        ScalarArgs();
        r[11] = Shift(r[11], 5) | r[10];
        r[10] = Word(r[1] + first + 2 * step);
        r[11] = Shift(r[11], 3) | r[10];
        r[10] = Word(r[1] + first + 3 * step);
        r[11] = Shift(r[11], 3) | r[10];
        r[3] = r[11] & 0xffffu;
        Lower(0x82bd7d00u, continuation);
    }
    void HeaderAndGeometry() {
        auto &r = s.r;
        r[31] = r[3];
        r[30] = r[4];
        Lower(0x82b9cb70u, 0x82bbc128u);
        r[29] = r[3];
        r[9] = r[30];
        r[7] = 5;
        r[6] = 'L';
        r[5] = 'H';
        r[4] = 'V';
        r[3] = 'C';
        r[8] = r[29];
        Lower(0x82bd8078u, 0x82bbc14cu);
        r[11] = r[3] & 255u;
        Compare(r[11]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        r[11] = Word(r[31] + 4);
        r[11] = Word(r[11] + 52);
        Compare(r[11]);
        if (s.cr6.eq) {
            r[3] = r[31];
            Lower(0x82bbb728u, 0x82bbc17cu);
        }
        MeshCount(12, 0x82bbc190u);
        MeshCount(4, 0x82bbc1a4u);
        MeshCount(52, 0x82bbc1b8u);
        r[11] = Word(r[31] + 4);
        r[11] = Word(r[11] + 36);
        Compare(r[11]);
        if (s.cr6.eq) {
            r[3] = r[31];
            Lower(0x82bb9aa8u, 0x82bbc1d0u);
        }
        MeshCount(36, 0x82bbc1e4u);
        r[10] = Word(r[31] + 4);
        r[25] = 0;
        r[11] = Word(r[10] + 36);
        Compare(r[11]);
        if (!s.cr6.eq) {
            r[10] = Word(r[10] + 40);
            do {
                r[9] = Half(r[10]);
                --r[11];
                r[10] += 36;
                r[25] = r[9] + r[25];
                Compare(r[11]);
            } while (!s.cr6.eq);
        }
        ScalarArgs();
        r[3] = r[25];
        Lower(0x82bd7db0u, 0x82bbc224u);
        r[11] = Word(r[31] + 4);
        ScalarArgs();
        r[9] = Word(r[11] + 52);
        r[10] = Word(r[11] + 64);
        r[11] = Shift(r[9], 3) + r[10];
        r[10] = Half(r[11] - 6);
        r[11] = Word(r[11] - 4);
        r[23] = r[10] + r[11];
        r[3] = r[23];
        Lower(0x82bd7db0u, 0x82bbc254u);
        r[11] = Word(r[31] + 4);
        r[6] = r[30];
        r[5] = r[29];
        r[10] = Word(r[11] + 12);
        r[3] = Word(r[11] + 16);
        r[11] = Shift(r[10], 1);
        r[4] = r[10] + r[11];
        Lower(0x82bd7ff0u, 0x82bbc274u);
        r[11] = Word(r[31] + 4);
        r[10] = Word(r[11] + 4);
        r[3] = Word(r[11] + 8);
        r[11] = Shift(r[10], 1);
        r[4] = r[10] + r[11];
        Lower(0x82badfa0u, 0x82bbc28cu);
        ScalarArgs();
        r[28] = r[3];
        Lower(0x82bd7db0u, 0x82bbc29cu);
        r[11] = Word(r[31] + 4);
        r[7] = r[29];
        r[6] = r[30];
        r[3] = r[28];
        r[10] = Word(r[11] + 4);
        r[5] = Word(r[11] + 8);
        r[11] = Shift(r[10], 1);
        r[4] = r[10] + r[11];
        Lower(0x82bd8668u, 0x82bbc2c0u);
        VertexNormals();
        PolygonRecords();
        PolygonEdges();
        EdgeNormals();
        Incidence();
        r[3] = 1;
    }
    void VertexNormals() {
        auto &r = s.r;
        ScalarArgs();
        r[3] = Half(r[31] + 8);
        Lower(0x82bd7d00u, 0x82bbc2d0u);
        r[11] = Word(r[31] + 4);
        r[11] = Word(r[11] + 20);
        Compare(r[11]);
        if (s.cr6.eq) {
            r[3] = r[31];
            Lower(0x82bb9160u, 0x82bbc2e8u);
        }
        r[11] = Word(r[31] + 4);
        r[3] = Word(r[11] + 20);
        Compare(r[3]);
        if (!s.cr6.eq) {
            r[10] = Half(r[31] + 8);
            r[11] = Word(r[11] + 12);
            Compare(r[10]);
            if (!s.cr6.eq) {
                r[10] = Shift(r[11], 1);
                r[6] = r[30];
                r[5] = r[29];
                r[4] = r[11] + r[10];
                Lower(0x82bd7ff0u, 0x82bbc3a4u);
            } else {
                r[27] = 0;
                Compare(r[11]);
                if (s.cr6.gt) {
                    r[28] = r[3] + 4;
                    do {
                        r[10] = 5;
                        Load(3, r[28] + 4);
                        r[9] = r[1] + 80;
                        Load(2, r[28]);
                        r[8] = r[1] + 84;
                        Load(1, r[28] - 4);
                        r[7] = r[1] + 88;
                        r[6] = r[1] + 92;
                        Lower(0x82bb8fa0u, 0x82bbc33cu);
                        PackNormal(80, 4, 0x82bbc374u);
                        r[11] = Word(r[31] + 4);
                        ++r[27];
                        r[28] += 12;
                        r[11] = Word(r[11] + 12);
                        Compare(r[27], r[11]);
                    } while (s.cr6.lt);
                }
            }
        }
        for (unsigned i = 0; i < 3; ++i) {
            r[11] = Word(r[31] + 4);
            ScalarArgs();
            Load(1, r[11] + 24 + 4 * i);
            Lower(0x82bd7e70u, 0x82bbc3b8u + 20 * i);
        }
    }
    void SwapPolygonCopy() {
        auto &r = s.r;
        const auto p = r[1] + 96;
        // Preserve scratch registers observable by the subsequent stream callback.
        r[6] = Byte(p + 25);
        r[7] = Byte(p + 21);
        r[8] = Byte(p + 17);
        r[9] = Byte(p + 13);
        r[10] = Byte(p + 5);
        r[22] = Byte(p + 34);
        r[28] = Byte(p + 2);
        for (unsigned i = 0; i < 4; i += 2) {
            auto a = Byte(p + i), b = Byte(p + i + 1);
            m.WriteU8(Address(p + i), b);
            m.WriteU8(Address(p + i + 1), a);
        }
        for (unsigned i = 4; i < 36; i += 4) {
            auto v = Word(p + i);
            Store(p + i,
                  ((v & 255u) << 24) | ((v & 0xff00u) << 8) | ((v >> 8) & 0xff00u) | (v >> 24));
        }
    }
    void PolygonRecords() {
        auto &r = s.r;
        r[11] = Word(r[31] + 4);
        r[26] = 0;
        r[10] = Word(r[11] + 36);
        Compare(r[10]);
        if (!s.cr6.gt)
            return;
        r[24] = r[29] & 255u;
        r[27] = 0;
        do {
            r[11] = Word(r[11] + 40);
            r[10] = r[1] + 96;
            r[9] = 9;
            r[11] += r[27];
            s.ctr = r[9];
            do {
                r[9] = Word(r[11]);
                r[11] += 4;
                Store(r[10], r[9]);
                r[10] += 4;
                --s.ctr;
            } while (Address(s.ctr));
            r[11] = Word(r[31] + 4);
            Compare(r[24]);
            r[9] = Word(r[1] + 104);
            r[10] = Word(r[11] + 48);
            r[10] = r[9] - r[10];
            Store(r[1] + 104, r[10]);
            r[11] = Word(r[11] + 44);
            r[10] = Word(r[1] + 100);
            r[11] = r[10] - r[11];
            Store(r[1] + 100, r[11]);
            if (!s.cr6.eq)
                SwapPolygonCopy();
            r[11] = Word(r[30]);
            r[5] = 36;
            r[4] = r[1] + 96;
            r[3] = r[30];
            r[11] = Word(r[11] + 48);
            Call(11, 0x82bbc58cu);
            r[11] = Word(r[31] + 4);
            ++r[26];
            r[27] += 36;
            r[10] = Word(r[11] + 36);
            Compare(r[26], r[10]);
        } while (s.cr6.lt);
    }
    void PolygonEdges() {
        auto &r = s.r;
        r[28] = 0;
        Compare(r[25]);
        if (!s.cr6.eq)
            do {
                r[11] = Word(r[31] + 4);
                r[3] = r[30];
                r[10] = Word(r[30]);
                r[11] = Word(r[11] + 44);
                r[10] = Word(r[10] + 28);
                r[4] = Byte(r[11] + r[28]);
                Call(10, 0x82bbc5d0u);
                ++r[28];
                Compare(r[28], r[25]);
            } while (s.cr6.lt);
        r[11] = Word(r[31] + 4);
        r[4] = r[25];
        r[3] = Word(r[11] + 48);
        Lower(0x82bd8360u, 0x82bbc5ecu);
        r[28] = r[3] & 0xffffu;
        ScalarArgs();
        r[3] = r[28];
        Lower(0x82bd7db0u, 0x82bbc600u);
        r[11] = Word(r[31] + 4);
        r[7] = r[29];
        r[6] = r[30];
        r[4] = r[25];
        r[3] = r[28];
        r[5] = Word(r[11] + 48);
        Lower(0x82bd83a0u, 0x82bbc61cu);
        ScalarArgs();
        r[3] = 0;
        Lower(0x82bd7db0u, 0x82bbc62cu);
        ScalarArgs();
        r[3] = 0;
        Lower(0x82bd7db0u, 0x82bbc63cu);
        r[11] = Word(r[31] + 4);
        r[27] = 0;
        r[10] = Word(r[11] + 52);
        Compare(r[10]);
        if (s.cr6.gt) {
            r[28] = 0;
            do {
                r[10] = Word(r[30]);
                r[3] = r[30];
                r[11] = Word(r[11] + 56);
                r[10] = Word(r[10] + 28);
                r[4] = Byte(r[11] + r[28]);
                Call(10, 0x82bbc670u);
                r[11] = Word(r[31] + 4);
                r[10] = Word(r[30]);
                r[3] = r[30];
                r[11] = Word(r[11] + 56);
                r[10] = Word(r[10] + 28);
                r[11] += r[28];
                r[4] = Byte(r[11] + 1);
                Call(10, 0x82bbc694u);
                r[11] = Word(r[31] + 4);
                ++r[27];
                r[28] += 2;
                r[10] = Word(r[11] + 52);
                Compare(r[27], r[10]);
            } while (s.cr6.lt);
        }
    }
    void EdgeNormals() {
        auto &r = s.r;
        r[11] = Half(r[31] + 8);
        Compare(r[11]);
        r[11] = Word(r[31] + 4);
        r[10] = Word(r[11] + 52);
        if (!s.cr6.eq) {
            r[3] = Word(r[11] + 60);
            r[11] = Shift(r[10], 1);
            r[6] = r[30];
            r[5] = r[29];
            r[4] = r[10] + r[11];
            Lower(0x82bd7ff0u, 0x82bbc768u);
        } else {
            r[28] = 0;
            Compare(r[10]);
            if (s.cr6.gt) {
                r[27] = 0;
                do {
                    r[11] = Word(r[11] + 60);
                    r[10] = 5;
                    r[9] = r[1] + 92;
                    r[11] += r[27];
                    r[8] = r[1] + 88;
                    r[7] = r[1] + 84;
                    r[6] = r[1] + 80;
                    Load(3, r[11] + 8);
                    Load(2, r[11] + 4);
                    Load(1, r[11]);
                    Lower(0x82bb8fa0u, 0x82bbc6fcu);
                    PackNormal(92, -4, 0x82bbc734u);
                    r[11] = Word(r[31] + 4);
                    ++r[28];
                    r[27] += 12;
                    r[10] = Word(r[11] + 52);
                    Compare(r[28], r[10]);
                } while (s.cr6.lt);
            }
        }
    }
    void AllocateScratch(unsigned shift, GuestAddress lookup, GuestAddress call) {
        auto &r = s.r;
        Lower(0x82bd0798u, lookup);
        r[11] = Word(r[31] + 4);
        r[5] = 1;
        r[11] = Word(r[11] + 52);
        r[4] = Shift(r[11], shift);
        r[11] = Word(r[3]);
        r[11] = Word(r[11]);
        Call(11, call);
    }
    void ReleaseScratch(unsigned reg, GuestAddress lookup, GuestAddress call) {
        auto &r = s.r;
        Compare(r[reg]);
        if (!s.cr6.eq) {
            Lower(0x82bd0798u, lookup);
            r[11] = Word(r[3]);
            r[4] = r[reg];
            r[11] = Word(r[11] + 12);
            Call(11, call);
        }
    }
    void IncidenceHalfwords(bool degrees) {
        auto &r = s.r;
        // Both halfword fields use the same scratch storage, with separate maxima.
        if (!degrees) {
            r[11] = Word(r[31] + 4);
            r[28] = r[3];
            r[8] = 0;
            r[10] = Word(r[11] + 52);
            Compare(r[10]);
            if (s.cr6.gt) {
                r[10] = 0;
                r[9] = r[28];
                do {
                    r[11] = Word(r[11] + 64);
                    ++r[8];
                    r[11] = Half(r[11] + r[10]);
                    r[10] += 8;
                    m.WriteU16(Address(r[9]), std::uint16_t(r[11]));
                    r[9] += 2;
                    r[11] = Word(r[31] + 4);
                    r[7] = Word(r[11] + 52);
                    Compare(r[8], r[7]);
                } while (s.cr6.lt);
            }
        } else {
            r[11] = Word(r[31] + 4);
            r[10] = 0;
            r[9] = Word(r[11] + 52);
            Compare(r[9]);
            if (s.cr6.gt) {
                r[9] = 0;
                r[8] = r[28];
                do {
                    r[11] = Word(r[11] + 64);
                    ++r[10];
                    r[11] += r[9];
                    r[9] += 8;
                    r[11] = Half(r[11] + 2);
                    m.WriteU16(Address(r[8]), std::uint16_t(r[11]));
                    r[8] += 2;
                    r[11] = Word(r[31] + 4);
                    r[7] = Word(r[11] + 52);
                    Compare(r[10], r[7]);
                } while (s.cr6.lt);
            }
        }
        r[11] = Word(r[31] + 4);
        r[3] = r[28];
        r[4] = Word(r[11] + 52);
        Lower(0x82bd8360u, degrees ? 0x82bbc86cu : 0x82bbc7e4u);
        r[27] = r[3] & 0xffffu;
        ScalarArgs();
        r[3] = r[27];
        Lower(0x82bd7db0u, degrees ? 0x82bbc880u : 0x82bbc7f8u);
        r[11] = Word(r[31] + 4);
        r[7] = r[29];
        r[6] = r[30];
        r[5] = r[28];
        r[3] = r[27];
        r[4] = Word(r[11] + 52);
        Lower(0x82bd83a0u, degrees ? 0x82bbc89cu : 0x82bbc814u);
    }
    void Incidence() {
        auto &r = s.r;
        AllocateScratch(1, 0x82bbc76cu, 0x82bbc78cu);
        IncidenceHalfwords(false);
        IncidenceHalfwords(true);
        ReleaseScratch(28, 0x82bbc8a8u, 0x82bbc8bcu);
        AllocateScratch(2, 0x82bbc8c0u, 0x82bbc8e0u);
        r[11] = Word(r[31] + 4);
        r[27] = r[3];
        r[8] = 0;
        r[10] = Word(r[11] + 52);
        Compare(r[10]);
        if (s.cr6.gt) {
            r[10] = 0;
            r[9] = r[27];
            do {
                r[11] = Word(r[11] + 64);
                ++r[8];
                r[11] += r[10];
                r[10] += 8;
                r[11] = Word(r[11] + 4);
                Store(r[9], r[11]);
                r[9] += 4;
                r[11] = Word(r[31] + 4);
                r[7] = Word(r[11] + 52);
                Compare(r[8], r[7]);
            } while (s.cr6.lt);
        }
        r[11] = Word(r[31] + 4);
        r[3] = r[27];
        r[4] = Word(r[11] + 52);
        Lower(0x82badfa0u, 0x82bbc93cu);
        ScalarArgs();
        r[28] = r[3];
        Lower(0x82bd7db0u, 0x82bbc94cu);
        r[11] = Word(r[31] + 4);
        r[7] = r[29];
        r[6] = r[30];
        r[5] = r[27];
        r[3] = r[28];
        r[4] = Word(r[11] + 52);
        Lower(0x82bd8668u, 0x82bbc968u);
        ReleaseScratch(27, 0x82bbc974u, 0x82bbc988u);
        r[11] = Word(r[30]);
        r[5] = r[23];
        r[10] = Word(r[31] + 4);
        r[3] = r[30];
        r[11] = Word(r[11] + 48);
        r[4] = Word(r[10] + 68);
        Call(11, 0x82bbc9a8u);
    }
    void Run() {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = 0x82bbc118u;
        for (unsigned i = 22; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        Store(r[1] - 8, r[12]);
        auto old = r[1];
        r[1] -= 224;
        Store(r[1], old);
        HeaderAndGeometry();
        r[1] += 224;
        for (unsigned i = 22; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    if (e != 0x82bbc110u)
        return false;
    GeometryStream{m, d, s}.Run();
    return true;
}
} // namespace lo::semantic::gpu::mesh_geometry_stream61
