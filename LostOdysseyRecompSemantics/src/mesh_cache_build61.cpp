#include "lo_semantics/mesh_cache_build61.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/mesh_cook_storage61.h"
#include "lo_semantics/mesh_edge_flags61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::mesh_cache_build61 {
namespace {
using recovery_abi::Address;
struct Builder {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Store(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    void Compare(std::uint64_t a, std::uint64_t b = 0) {
        auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    static std::uint32_t Shift(std::uint64_t v, unsigned bits) { return Address(v) << bits; }
    bool Success() {
        s.r[11] = Address(s.r[3]) & 255u;
        Compare(s.r[11]);
        return !s.cr6.eq;
    }
    void Load(unsigned f, std::uint64_t p) {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            d.lifetime.fp.SetHostFpControl(s.cached_fp_control);
        }
        s.fpr_bits[f] = std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(Word(p))));
    }
    void Lower(GuestAddress e, GuestAddress cont) {
        s.lr = cont;
        switch (e) {
        case 0x82bd0798u:
            (void)crt_close_recursive_buffer_context::Apply(e, m, d.lifetime.guest, s);
            break;
        case 0x82bbce58u:
        case 0x82bbd1e0u:
        case 0x82bbd4c0u:
            (void)mesh_edge_build61::Apply(e, m, d.edge, s);
            break;
        case 0x82bbd4e8u:
            (void)mesh_edge_flags61::Apply(e, m, d.edge, s);
            break;
        case 0x82bbd4e0u:
            (void)mesh_cook_storage61::Apply(e, m, d.lifetime, s);
            break;
        case 0x82b7bc40u:
            crt_reader_chain61::ApplySupport_B7BC40(m, d.edge.engine.sort.accepted, s);
            break;
        default:
            (void)mesh_cache_lifetime61::Apply(e, m, d.lifetime, s);
            break;
        }
    }
    void Call(GuestAddress cont) {
        s.ctr = s.r[11];
        s.lr = cont;
        d.lifetime.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void Enter(unsigned frame, GuestAddress cont) {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = cont;
        for (unsigned i = 29; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        Store(r[1] - 8, r[12]);
        auto old = r[1];
        r[1] -= frame;
        Store(r[1], old);
    }
    void Leave(unsigned frame) {
        auto &r = s.r;
        r[1] += frame;
        for (unsigned i = 29; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
    void TopologyArguments() {
        auto &r = s.r;
        r[3] = r[30];
        r[6] = Word(r[31] + 8);
        r[5] = Word(r[31] + 4);
        r[4] = Word(r[31]);
    }
    void Topology() {
        Enter(112, 0x82bbddf8u);
        TopologyBody();
        Leave(112);
    }
    void TopologyBody() {
        auto &r = s.r;
        r[31] = r[4];
        r[30] = r[3];
        r[10] = 1;
        r[11] = Word(r[31] + 16);
        Compare(r[11]);
        if (!s.cr6.eq)
            r[29] = 1;
        else {
            r[10] = m.ReadU8(Address(r[31] + 12));
            r[29] = m.ReadU8(Address(r[31] + 13));
        }
        r[11] = Address(r[10]) & 255u;
        Compare(r[11]);
        if (!s.cr6.eq) {
            TopologyArguments();
            Lower(0x82bbce58u, 0x82bbde48u);
            if (!Success()) {
                r[3] = 0;
                return;
            }
        }
        r[11] = Address(r[29]) & 255u;
        Compare(r[11]);
        if (!s.cr6.eq) {
            TopologyArguments();
            Lower(0x82bbd1e0u, 0x82bbde80u);
            if (!Success()) {
                r[3] = 0;
                return;
            }
        }
        r[7] = Word(r[31] + 16);
        Compare(r[7]);
        if (!s.cr6.eq) {
            r[3] = r[30];
            Load(1, r[31] + 20);
            r[6] = Word(r[31] + 8);
            r[5] = Word(r[31] + 4);
            r[4] = Word(r[31]);
            Lower(0x82bbd4e8u, 0x82bbdeb0u);
            if (!Success()) {
                r[3] = 0;
                return;
            }
        }
        r[11] = m.ReadU8(Address(r[31] + 12));
        r[29] = 0;
        Compare(r[11]);
        if (s.cr6.eq) {
            r[11] = Word(r[30] + 12);
            Compare(r[11]);
            if (!s.cr6.eq) {
                Lower(0x82bd0798u, 0x82bbdedcu);
                r[11] = Word(r[3]);
                r[4] = Word(r[30] + 12);
                r[11] = Word(r[11] + 12);
                Call(0x82bbdef0u);
                Store(r[30] + 12, r[29]);
            }
        }
        r[11] = m.ReadU8(Address(r[31] + 13));
        Compare(r[11]);
        if (s.cr6.eq) {
            constexpr unsigned offsets[]{16, 20};
            constexpr GuestAddress sites[]{0x82bbdf10u, 0x82bbdf38u};
            for (unsigned i = 0; i < 2; ++i) {
                r[11] = Word(r[30] + offsets[i]);
                Compare(r[11]);
                if (!s.cr6.eq) {
                    Lower(0x82bd0798u, sites[i]);
                    r[11] = Word(r[3]);
                    r[4] = Word(r[30] + offsets[i]);
                    r[11] = Word(r[11] + 12);
                    Call(sites[i] + 20);
                    Store(r[30] + offsets[i], r[29]);
                }
            }
        }
        r[3] = 1;
    }
    void DisposeEdges(GuestAddress cont) {
        s.r[3] = s.r[1] + 80;
        Lower(0x82bbd4e0u, cont);
    }
    void Valence() {
        Enter(176, 0x82bbc9f8u);
        ValenceBody();
        Leave(176);
    }
    void ValenceBody() {
        auto &r = s.r;
        r[30] = r[4];
        r[31] = r[3];
        r[11] = Word(r[30]);
        Store(r[31] + 4, r[11]);
        Lower(0x82bd0798u, 0x82bbca10u);
        r[11] = Word(r[31] + 4);
        r[5] = 56;
        r[4] = Shift(r[11], 2);
        r[11] = Word(r[3]);
        r[11] = Word(r[11]);
        Call(0x82bbca2cu);
        Compare(r[3]);
        Store(r[31] + 12, r[3]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        r[11] = Word(r[31] + 4);
        r[4] = 0;
        r[5] = Shift(r[11], 2);
        Lower(0x82b7bc40u, 0x82bbca48u);
        r[3] = r[1] + 80;
        Lower(0x82bbd4c0u, 0x82bbca50u);
        r[11] = Word(r[30] + 4);
        r[29] = 0;
        r[4] = r[1] + 112;
        r[3] = r[1] + 80;
        Store(r[1] + 112, r[11]);
        r[11] = Word(r[30] + 8);
        m.WriteU8(Address(r[1] + 125), std::uint8_t(r[29]));
        Store(r[1] + 128, r[29]);
        Store(r[1] + 116, r[11]);
        r[11] = Word(r[30] + 12);
        Store(r[1] + 120, r[11]);
        r[11] = 0xffffffff82000000ull;
        Load(0, r[11] + 3500);
        r[11] = 1;
        Store(r[1] + 132,
              std::bit_cast<std::uint32_t>(float(std::bit_cast<double>(s.fpr_bits[0]))));
        m.WriteU8(Address(r[1] + 124), std::uint8_t(r[11]));
        s.lr = 0x82bbca94u;
        Topology();
        if (!Success()) {
            DisposeEdges(0x82bbcaa8u);
            r[3] = 0;
            return;
        }
        r[11] = Word(r[1] + 80);
        r[10] = r[29];
        Compare(r[11]);
        if (!s.cr6.eq) {
            r[11] = r[29];
            do {
                r[9] = Word(r[1] + 84);
                ++r[10];
                r[8] = Word(r[31] + 12);
                r[9] = Word(r[11] + r[9]);
                r[9] = Shift(r[9], 2);
                r[7] = m.ReadU16(Address(r[9] + r[8]));
                ++r[7];
                m.WriteU16(Address(r[9] + r[8]), std::uint16_t(r[7]));
                r[9] = Word(r[1] + 84);
                r[8] = Word(r[31] + 12);
                r[9] += r[11];
                r[11] += 8;
                r[9] = Word(r[9] + 4);
                r[9] = Shift(r[9], 2);
                r[7] = m.ReadU16(Address(r[9] + r[8]));
                ++r[7];
                m.WriteU16(Address(r[9] + r[8]), std::uint16_t(r[7]));
                r[9] = Word(r[1] + 80);
                Compare(r[10], r[9]);
            } while (s.cr6.lt);
        }
        r[11] = m.ReadU8(Address(r[30] + 16));
        Compare(r[11]);
        if (!s.cr6.eq) {
            r[3] = r[31];
            Lower(0x82bc7f48u, 0x82bbcb2cu);
            r[11] = Word(r[31] + 4);
            r[10] = Word(r[31] + 12);
            r[11] = Shift(r[11], 2);
            r[11] += r[10];
            r[10] = m.ReadU16(Address(r[11] - 4));
            r[11] = m.ReadU16(Address(r[11] - 2));
            r[11] += r[10];
            Store(r[31] + 8, r[11]);
            Lower(0x82bd0798u, 0x82bbcb50u);
            r[11] = Word(r[3]);
            r[5] = 58;
            r[4] = Word(r[31] + 8);
            r[11] = Word(r[11]);
            Call(0x82bbcb68u);
            Compare(r[3]);
            Store(r[31] + 16, r[3]);
            if (s.cr6.eq) {
                DisposeEdges(0x82bbcaa8u);
                r[3] = 0;
                return;
            }
            r[11] = Word(r[1] + 80);
            r[9] = r[29];
            Compare(r[11]);
            if (!s.cr6.eq) {
                r[11] = r[29];
                do {
                    r[10] = Word(r[1] + 84);
                    ++r[9];
                    r[6] = Word(r[31] + 12);
                    r[10] += r[11];
                    r[5] = Word(r[31] + 16);
                    r[11] += 8;
                    r[8] = Word(r[10]);
                    r[10] = Word(r[10] + 4);
                    r[7] = Shift(r[8], 2);
                    r[3] = r[8];
                    r[8] = r[6] + r[7];
                    r[4] = r[10];
                    r[10] = Shift(r[10], 2);
                    r[8] = m.ReadU16(Address(r[8] + 2));
                    m.WriteU8(Address(r[8] + r[5]), std::uint8_t(r[4]));
                    r[8] = Word(r[31] + 12);
                    r[8] += r[7];
                    r[7] = m.ReadU16(Address(r[8] + 2));
                    ++r[7];
                    m.WriteU16(Address(r[8] + 2), std::uint16_t(r[7]));
                    r[8] = Word(r[31] + 12);
                    r[7] = Word(r[31] + 16);
                    r[8] += r[10];
                    r[8] = m.ReadU16(Address(r[8] + 2));
                    m.WriteU8(Address(r[8] + r[7]), std::uint8_t(r[3]));
                    r[8] = Word(r[31] + 12);
                    r[10] += r[8];
                    r[8] = m.ReadU16(Address(r[10] + 2));
                    ++r[8];
                    m.WriteU16(Address(r[10] + 2), std::uint16_t(r[8]));
                    r[10] = Word(r[1] + 80);
                    Compare(r[9], r[10]);
                } while (s.cr6.lt);
            }
            r[3] = r[31];
            Lower(0x82bc7f48u, 0x82bbcc14u);
        }
        DisposeEdges(0x82bbcc1cu);
        r[3] = 1;
    }
    void Lazy() {
        Enter(144, 0x82bb3138u);
        LazyBody();
        Leave(144);
    }
    void LazyBody() {
        auto &r = s.r;
        r[31] = r[3];
        Lower(0x82bd0798u, 0x82bb3144u);
        r[11] = Word(r[3]);
        r[5] = 4;
        r[4] = 20;
        r[11] = Word(r[11]);
        Call(0x82bb315cu);
        r[30] = 0;
        Compare(r[3]);
        if (!s.cr6.eq)
            Lower(0x82bbc9b8u, 0x82bb316cu);
        else
            r[3] = r[30];
        Compare(r[3]);
        Store(r[31] + 16, r[3]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        r[11] = r[1] + 80;
        r[10] = Word(r[31] + 12);
        r[4] = r[1] + 80;
        recovery_abi::WriteU64(m, Address(r[11]), r[30]);
        recovery_abi::WriteU64(m, Address(r[11] + 8), r[30]);
        Store(r[11] + 16, r[30]);
        r[11] = Word(r[10] + 12);
        Store(r[1] + 80, r[11]);
        r[11] = Word(r[10] + 4);
        Store(r[1] + 84, r[11]);
        r[11] = Word(r[10] + 8);
        Store(r[1] + 88, r[11]);
        r[11] = 1;
        m.WriteU8(Address(r[1] + 96), std::uint8_t(r[11]));
        s.lr = 0x82bb31bcu;
        Valence();
        if (!Success()) {
            r[29] = Word(r[31] + 16);
            Compare(r[29]);
            if (!s.cr6.eq) {
                r[3] = r[29];
                Lower(0x82bbc9e8u, 0x82bb31dcu);
                Lower(0x82bd0798u, 0x82bb31e0u);
                r[11] = Word(r[3]);
                r[4] = r[29];
                r[11] = Word(r[11] + 12);
                Call(0x82bb31f4u);
                Store(r[31] + 16, r[30]);
            }
            r[3] = 0;
            return;
        }
        r[11] = Word(r[31] + 16);
        r[3] = 1;
        r[10] = Word(r[31] + 12);
        r[11] += 4;
        Store(r[10] + 84, r[11]);
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    Builder b{m, d, s};
    switch (e) {
    case 0x82bbddf0u:
        b.Topology();
        return true;
    case 0x82bbc9f0u:
        b.Valence();
        return true;
    case 0x82bb3130u:
        b.Lazy();
        return true;
    default:
        return false;
    }
}
} // namespace lo::semantic::gpu::mesh_cache_build61
