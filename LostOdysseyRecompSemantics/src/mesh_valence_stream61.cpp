#include "lo_semantics/mesh_valence_stream61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/serialization_control61.h"
namespace lo::semantic::gpu::mesh_valence_stream61 {
namespace {
using recovery_abi::Address;
struct Valence {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Store(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    void Compare(std::uint64_t a, std::uint64_t b = 0) {
        auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    void Enter(unsigned first, GuestAddress cont) {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = cont;
        Store(r[1] - 8, r[12]);
        for (unsigned i = first; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        auto old = r[1];
        r[1] -= 128;
        Store(r[1], old);
    }
    void Leave(unsigned first) {
        auto &r = s.r;
        r[1] += 128;
        for (unsigned i = first; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
    void Lower(GuestAddress e, GuestAddress cont) {
        s.lr = cont;
        if (e == 0x82b9cb70u)
            (void)serialization_control61::Apply(e, m, d, s);
        else if (e == 0x82bd0798u)
            (void)crt_close_recursive_buffer_context::Apply(e, m, d.guest, s);
        else
            (void)mesh_stream_write61::Apply(e, m, d, s);
    }
    void Call(GuestAddress cont) {
        s.ctr = s.r[11];
        s.lr = cont;
        d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void Maximum() {
        auto &r = s.r;
        r[11] = r[3];
        r[3] = 0;
        Compare(r[4]);
        if (s.cr6.eq)
            return;
        do {
            r[10] = m.ReadU16(Address(r[11]));
            r[9] = Address(r[3]) & 65535u;
            --r[4];
            r[8] = r[10];
            r[11] += 2;
            Compare(r[8], r[9]);
            if (s.cr6.gt)
                r[3] = r[10];
            Compare(r[4]);
        } while (!s.cr6.eq);
    }
    void Packed() {
        Enter(28, 0x82bd83a8u);
        auto &r = s.r;
        r[11] = Address(r[3]) & 65535u;
        r[29] = r[6];
        Compare(r[11], 255);
        const bool wide = s.cr6.gt;
        Compare(r[4]);
        if (!s.cr6.eq) {
            if (!wide) {
                r[30] = r[5];
                r[31] = r[4];
                do {
                    r[11] = Word(r[29]);
                    r[3] = r[29];
                    r[10] = m.ReadU16(Address(r[30]));
                    r[4] = Address(r[10]) & 255u;
                    r[11] = Word(r[11] + 28);
                    Call(0x82bd83e8u);
                    --r[31];
                    r[30] += 2;
                    Compare(r[31]);
                } while (!s.cr6.eq);
            } else {
                r[28] = Address(r[7]) & 255u;
                r[31] = r[5];
                r[30] = r[4];
                do {
                    r[11] = m.ReadU16(Address(r[31]));
                    Compare(r[28]);
                    m.WriteU16(Address(r[1] + 80), std::uint16_t(r[11]));
                    if (!s.cr6.eq) {
                        r[11] = m.ReadU8(Address(r[1] + 80));
                        r[10] = m.ReadU8(Address(r[1] + 81));
                        m.WriteU8(Address(r[1] + 81), std::uint8_t(r[11]));
                        m.WriteU8(Address(r[1] + 80), std::uint8_t(r[10]));
                    }
                    r[11] = Word(r[29]);
                    r[3] = r[29];
                    r[4] = m.ReadU16(Address(r[1] + 80));
                    r[11] = Word(r[11] + 32);
                    Call(0x82bd844cu);
                    --r[30];
                    r[31] += 2;
                    Compare(r[30]);
                } while (!s.cr6.eq);
            }
        }
        Leave(28);
    }
    void Write() {
        Enter(27, 0x82bbcc30u);
        auto &r = s.r;
        r[31] = r[3];
        r[29] = r[4];
        Lower(0x82b9cb70u, 0x82bbcc40u);
        r[28] = r[3];
        r[9] = r[29];
        r[7] = 2;
        r[6] = 69;
        r[5] = 76;
        r[4] = 65;
        r[3] = 86;
        r[8] = r[28];
        Lower(0x82bd8078u, 0x82bbcc64u);
        r[11] = Address(r[3]) & 255u;
        Compare(r[11]);
        if (s.cr6.eq) {
            r[3] = 0;
            Leave(27);
            return;
        }
        r[5] = r[29];
        r[3] = Word(r[31] + 4);
        r[4] = r[28];
        Lower(0x82bd7db0u, 0x82bbcc8cu);
        r[5] = r[29];
        r[4] = r[28];
        r[3] = Word(r[31] + 8);
        Lower(0x82bd7db0u, 0x82bbcc9cu);
        Lower(0x82bd0798u, 0x82bbcca0u);
        r[11] = Word(r[31] + 4);
        r[5] = 1;
        r[4] = (Address(r[11]) << 1u) & 0xfffffffeu;
        r[11] = Word(r[3]);
        r[11] = Word(r[11]);
        Call(0x82bbccbcu);
        r[11] = Word(r[31] + 4);
        r[30] = r[3];
        r[9] = 0;
        Compare(r[11]);
        if (s.cr6.gt) {
            r[11] = 0;
            r[10] = r[30];
            do {
                r[8] = Word(r[31] + 12);
                ++r[9];
                r[8] = m.ReadU16(Address(r[8] + r[11]));
                r[11] += 4;
                m.WriteU16(Address(r[10]), std::uint16_t(r[8]));
                r[10] += 2;
                r[8] = Word(r[31] + 4);
                Compare(r[9], r[8]);
            } while (s.cr6.lt);
        }
        r[3] = r[30];
        r[4] = Word(r[31] + 4);
        s.lr = 0x82bbcd08u;
        Maximum();
        r[27] = Address(r[3]) & 65535u;
        r[5] = r[29];
        r[4] = r[28];
        r[3] = r[27];
        Lower(0x82bd7db0u, 0x82bbcd1cu);
        r[7] = r[28];
        r[6] = r[29];
        r[4] = Word(r[31] + 4);
        r[5] = r[30];
        r[3] = r[27];
        s.lr = 0x82bbcd34u;
        Packed();
        Compare(r[30]);
        if (!s.cr6.eq) {
            Lower(0x82bd0798u, 0x82bbcd40u);
            r[11] = Word(r[3]);
            r[4] = r[30];
            r[11] = Word(r[11] + 12);
            Call(0x82bbcd54u);
        }
        r[11] = Word(r[29]);
        r[3] = r[29];
        r[5] = Word(r[31] + 8);
        r[4] = Word(r[31] + 16);
        r[11] = Word(r[11] + 48);
        Call(0x82bbcd70u);
        r[3] = 1;
        Leave(27);
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    Valence v{m, d, s};
    switch (e) {
    case 0x82bd8360u:
        v.Maximum();
        return true;
    case 0x82bd83a0u:
        v.Packed();
        return true;
    case 0x82bbcc28u:
        v.Write();
        return true;
    default:
        return false;
    }
}
} // namespace lo::semantic::gpu::mesh_valence_stream61
