#include "lo_semantics/mesh_cook_stream61.h"
#include "lo_semantics/crt_reader_object_chain61.h"
#include "lo_semantics/mesh_cache_lifetime61.h"
#include "lo_semantics/mesh_mass_cache61.h"
#include "lo_semantics/mesh_support_stream61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/serialization_control61.h"
#include <bit>
namespace lo::semantic::gpu::mesh_cook_stream61 {
namespace {
using recovery_abi::Address;
struct CookStream {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Store(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    void Compare(std::uint64_t v) {
        auto x = Address(v);
        s.cr6 = {0, std::uint8_t(x != 0), std::uint8_t(x == 0), s.xer_so};
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
        case 0x82b9cb70u:
            (void)serialization_control61::Apply(e, m, stream, s);
            break;
        case 0x82bada00u:
        case 0x82bb44a8u:
            (void)crt_reader_object_chain61::Apply(
                e, m, {d.edge.engine.sort.guest, d.edge.engine.sort.accepted}, s);
            break;
        case 0x82bb3008u:
        case 0x82bb32d8u:
            (void)mesh_cache_lifetime61::Apply(e, m, d.lifetime, s);
            break;
        case 0x82bb3220u:
            (void)mesh_geometry_stream61::Apply(e, m, d, s);
            break;
        case 0x82b9f418u:
            (void)mesh_mass_cache61::Apply(e, m, {d.edge.engine.fp, d.edge.diagnostics}, s);
            break;
        case 0x82bb3408u:
        case 0x82bb36f0u:
        case 0x82bb3420u:
            (void)mesh_support_stream61::Apply(e, m, stream, s);
            break;
        default:
            (void)mesh_stream_write61::Apply(e, m, stream, s);
            break;
        }
    }
    void ScalarArgs() {
        s.r[5] = s.r[31];
        s.r[4] = s.r[30];
    }
    void Body() {
        auto &r = s.r;
        r[29] = r[3];
        r[31] = r[4];
        r[28] = r[5];
        Lower(0x82b9cb70u, 0x82b9f70cu);
        r[11] = 0xffffffff83210000ull;
        r[30] = r[3];
        r[9] = r[31];
        r[6] = 'M';
        r[5] = 'X';
        r[4] = 'V';
        r[7] = Word(r[11] + 24920);
        r[3] = 'C';
        r[8] = r[30];
        Lower(0x82badd60u, 0x82b9f734u);
        r[11] = r[3] & 255u;
        Compare(r[11]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        ScalarArgs();
        r[3] = 0;
        Lower(0x82bada00u, 0x82b9f750u);
        r[4] = r[29] + 156;
        r[3] = r[1] + 112;
        Lower(0x82bb3008u, 0x82b9f75cu);
        r[11] = 0xffffffff820d0000ull;
        r[10] = std::countl_zero(Address(r[28]));
        Store(r[1] + 84, r[31]);
        r[27] = r[11] + 23920;
        r[11] = (r[10] >> 5) & 1u;
        r[4] = r[1] + 80;
        r[11] ^= 1u;
        r[3] = r[1] + 112;
        Store(r[1] + 80, r[27]);
        m.WriteU16(Address(r[1] + 120), std::uint16_t(r[11]));
        Lower(0x82bb3220u, 0x82b9f788u);
        r[11] = r[3] & 255u;
        Compare(r[11]);
        if (s.cr6.eq) {
            r[11] = 0xffffffff820d0000ull;
            r[3] = r[1] + 112;
            r[11] += 23008;
            Store(r[1] + 80, r[11]);
            Lower(0x82bb32d8u, 0x82b9f7a8u);
            r[3] = 0;
            return;
        }
        ScalarArgs();
        r[3] = r[29] + 8;
        Lower(0x82bb44a8u, 0x82b9f7c4u);
        constexpr unsigned fields[]{152, 136, 140, 144, 148, 112, 116, 120, 124, 128, 132};
        for (unsigned i = 0; i < 11; ++i) {
            ScalarArgs();
            Load(1, r[29] + fields[i]);
            Lower(0x82bada70u, 0x82b9f7d4u + 16 * i);
        }
        r[3] = r[29];
        Lower(0x82b9f418u, 0x82b9f87cu);
        r[28] = r[3];
        r[5] = r[31];
        Compare(r[28]);
        r[4] = r[30];
        if (!s.cr6.eq) {
            Load(1, r[28]);
            Lower(0x82bada70u, 0x82b9f898u);
            r[6] = r[31];
            r[5] = r[30];
            r[4] = 9;
            r[3] = r[28] + 4;
            Lower(0x82badcd0u, 0x82b9f8acu);
            r[6] = r[31];
            r[5] = r[30];
            r[4] = 3;
            r[3] = r[28] + 40;
            Lower(0x82badcd0u, 0x82b9f8c0u);
        } else {
            r[11] = 0xffffffff82000000ull;
            Load(1, r[11] + 3648);
            Lower(0x82bada70u, 0x82b9f8d0u);
        }
        r[4] = Word(r[29] + 288);
        r[11] = 0xffffffff820d0000ull;
        Compare(r[4]);
        r[30] = r[11] + 23008;
        if (!s.cr6.eq) {
            r[3] = r[1] + 96;
            Lower(0x82bb3408u, 0x82b9f8ecu);
            r[4] = r[1] + 88;
            r[3] = r[1] + 96;
            Store(r[1] + 88, r[27]);
            Store(r[1] + 92, r[31]);
            Lower(0x82bb36f0u, 0x82b9f900u);
            r[3] = r[1] + 96;
            Store(r[1] + 88, r[30]);
            Lower(0x82bb3420u, 0x82b9f90cu);
        }
        r[3] = r[1] + 112;
        Store(r[1] + 80, r[30]);
        Lower(0x82bb32d8u, 0x82b9f918u);
        r[3] = 1;
    }
    void Run() {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = 0x82b9f6f8u;
        for (unsigned i = 27; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        Store(r[1] - 8, r[12]);
        auto old = r[1];
        r[1] -= 192;
        Store(r[1], old);
        Body();
        r[1] += 192;
        for (unsigned i = 27; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    if (e != 0x82b9f6f0u)
        return false;
    CookStream{m, d, s}.Run();
    return true;
}
} // namespace lo::semantic::gpu::mesh_cook_stream61
