#include "lo_semantics/mesh_cook_hull61.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/integer_leaf.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::mesh_cook_hull61 {
namespace {
using recovery_abi::Address;
struct Cook {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Word(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    void Compare(std::uint64_t a) {
        auto v = Address(a);
        s.cr6 = {0, std::uint8_t(v > 0), std::uint8_t(v == 0), s.xer_so};
    }
    void Gradual() {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            d.edge.engine.fp.SetHostFpControl(s.cached_fp_control);
        }
    }
    void Lower(GuestAddress e, GuestAddress cont) {
        s.lr = cont;
        switch (e) {
        case 0x82bba028u:
            (void)mesh_convex_hull61::Apply(e, m, d, s);
            break;
        case 0x82bb3130u:
            (void)mesh_cache_build61::Apply(e, m, d, s);
            break;
        case 0x82bb3008u:
        case 0x82bb32d8u:
            (void)mesh_cache_lifetime61::Apply(e, m, d.lifetime, s);
            break;
        case 0x82bb3350u:
            WithValence();
            break;
        case 0x82b9e3f8u:
            Temporary();
            break;
        case 0x82b7e504u:
            mesh_polygon_collect61::ProbeStack(m, s);
            break;
        case 0x82b7a0b0u:
            (void)crt_copy_full_context::Apply(e, m, s);
            break;
        case 0x82b9c298u:
            (void)diagnostic_format_routes61::Apply(e, m, d.edge.diagnostics, s);
            break;
        case 0x82b9cb60u: {
            integer_leaf::Registers leaf{};
            (void)integer_leaf::Apply(e, leaf);
            s.r[3] = leaf.r3;
            s.r[11] = leaf.r11;
            break;
        }
        }
    }
    void Enter(unsigned first, unsigned frame, GuestAddress cont = 0) {
        auto &r = s.r;
        r[12] = s.lr;
        if (cont)
            s.lr = cont;
        Word(r[1] - 8, r[12]);
        for (unsigned i = first; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        auto old = r[1];
        r[1] -= frame;
        Word(r[1], old);
    }
    void Leave(unsigned first, unsigned frame) {
        auto &r = s.r;
        r[1] += frame;
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
        for (unsigned i = first; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
    }
    void WithValence() {
        Enter(31, 96);
        auto &r = s.r;
        r[31] = r[3];
        Lower(0x82bba028u, 0x82bb3368u);
        r[11] = Address(r[3]) & 255u;
        Compare(r[11]);
        if (s.cr6.eq)
            r[3] = 0;
        else {
            r[3] = r[31];
            Lower(0x82bb3130u, 0x82bb3394u);
        }
        Leave(31, 96);
    }
    void Diagnostic(unsigned text, unsigned line, GuestAddress cont) {
        auto &r = s.r;
        r[11] = 0xffffffff820d0000ull;
        r[6] = 0;
        r[7] = r[11] + text;
        r[11] = 0xffffffff820d0000ull;
        r[5] = line;
        r[4] = r[11] + 23768;
        r[3] = 4;
        Lower(0x82b9c298u, cont);
    }
    void Temporary() {
        Enter(32, 128);
        auto &r = s.r;
        r[11] = r[1] + 80;
        r[9] = 0;
        r[10] = r[4];
        r[4] = r[3];
        r[3] = r[1] + 96;
        recovery_abi::WriteU64(m, Address(r[11]), r[9]);
        recovery_abi::WriteU64(m, Address(r[11] + 8), r[9]);
        r[11] = 1;
        Gradual();
        Word(r[1] + 88, std::bit_cast<std::uint32_t>(float(std::bit_cast<double>(s.fpr_bits[1]))));
        Word(r[1] + 80, r[10]);
        Word(r[1] + 84, r[5]);
        m.WriteU8(Address(r[1] + 92), std::uint8_t(r[11]));
        m.WriteU8(Address(r[1] + 93), std::uint8_t(r[11]));
        Lower(0x82bb3008u, 0x82b9e43cu);
        r[4] = r[1] + 80;
        r[3] = r[1] + 96;
        Lower(0x82bb3350u, 0x82b9e448u);
        r[11] = Address(r[3]) & 255u;
        Compare(r[11]);
        if (s.cr6.eq) {
            Diagnostic(23808, 53, 0x82b9e474u);
            r[3] = r[1] + 96;
            Lower(0x82bb32d8u, 0x82b9e47cu);
            r[3] = 0;
        } else {
            r[3] = r[1] + 96;
            Lower(0x82bb32d8u, 0x82b9e498u);
            r[3] = 1;
        }
        Leave(32, 128);
    }
    void Strided() {
        auto &r = s.r;
        auto frame = r[1] - 160;
        Enter(24, 160, 0x82b9e7b8u);
        r[31] = frame;
        r[26] = r[3];
        r[27] = r[4];
        r[11] = Word(r[26] + 108);
        r[11] = Address(r[11]) & 0xfffffffeu;
        Word(r[26] + 108, r[11]);
        r[30] = Word(r[27]);
        r[11] = Address(r[30]) << 1;
        r[11] += r[30];
        r[11] = Address(r[11]) << 2;
        r[11] = 0 - r[11];
        r[12] = Address(r[11]) & 0xfffffff0u;
        Lower(0x82b7e504u, 0x82b9e7f0u);
        r[11] = Word(r[1]);
        r[25] = Word(r[27] + 8);
        Compare(r[30]);
        r[28] = Word(r[27] + 16);
        r[1] += r[12];
        Word(r[1], r[11]);
        r[24] = r[1] + 80;
        r[29] = r[24];
        if (!s.cr6.eq) {
            do {
                r[5] = 12;
                r[4] = r[28];
                r[3] = r[29];
                --r[30];
                Lower(0x82b7a0b0u, 0x82b9e824u);
                r[29] += 12;
                r[28] += r[25];
                Compare(r[30]);
            } while (!s.cr6.eq);
        }
        Lower(0x82b9cb60u, 0x82b9e838u);
        r[11] = r[3];
        r[5] = r[24];
        r[4] = Word(r[27]);
        r[3] = r[26] + 156;
        Gradual();
        s.fpr_bits[1] = std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(Word(r[11] + 4))));
        Lower(0x82b9e3f8u, 0x82b9e850u);
        r[11] = Address(r[3]) & 255u;
        Compare(r[11]);
        if (s.cr6.eq) {
            Diagnostic(23992, 443, 0x82b9e87cu);
            r[3] = 0;
        } else {
            r[11] = Word(r[26] + 108);
            r[3] = 1;
            r[11] |= 1;
            Word(r[26] + 108, r[11]);
        }
        r[1] = r[31];
        Leave(24, 160);
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    Cook c{m, d, s};
    switch (e) {
    case 0x82bb3350u:
        c.WithValence();
        break;
    case 0x82b9e3f8u:
        c.Temporary();
        break;
    case 0x82b9e7b0u:
        c.Strided();
        break;
    default:
        return false;
    }
    return true;
}
} // namespace lo::semantic::gpu::mesh_cook_hull61
