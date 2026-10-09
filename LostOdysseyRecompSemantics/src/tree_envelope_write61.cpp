#include "lo_semantics/tree_envelope_write61.h"
#include "lo_semantics/crt_close_next61.h"
#include "lo_semantics/crt_reader_units61.h"
#include "lo_semantics/mesh_valence_stream61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::tree_envelope_write61 {
namespace {
using recovery_abi::Address;
struct Envelope {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Store(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    void Compare(std::uint64_t a, std::uint64_t b = 0) {
        auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    void Enter(unsigned first, unsigned frame, GuestAddress continuation) {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = continuation;
        for (unsigned i = first; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        Store(r[1] - 8, r[12]);
        auto old = r[1];
        r[1] -= frame;
        Store(r[1], old);
    }
    void Leave(unsigned first, unsigned frame) {
        auto &r = s.r;
        r[1] += frame;
        for (unsigned i = first; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
    void Lower(GuestAddress e, GuestAddress continuation) {
        s.lr = continuation;
        switch (e) {
        case 0x82bd0938u:
        case 0x82bd0fc8u:
            (void)crt_close_next61::Apply(e, m, d.guest, s);
            break;
        case 0x82bd1050u:
            (void)crt_reader_units61::Apply(e, m, d.guest, s);
            break;
        case 0x82badfa0u:
            (void)mesh_valence_stream61::Apply(e, m, d, s);
            break;
        default:
            (void)tree_scalar_write61::Apply(e, m, d, s);
            break;
        }
    }
    void Header() {
        Enter(29, 112, 0x82bd7cc0u);
        auto &r = s.r;
        r[11] = r[3];
        r[31] = r[4];
        r[3] = r[7];
        r[4] = r[11];
        r[30] = r[5];
        r[29] = r[6];
        Lower(0x82bd0938u, 0x82bd7ce0u);
        r[4] = r[31];
        Lower(0x82bd0938u, 0x82bd7ce8u);
        r[4] = r[30];
        Lower(0x82bd0938u, 0x82bd7cf0u);
        r[4] = r[29];
        Lower(0x82bd0938u, 0x82bd7cf8u);
        Leave(29, 112);
    }
    void Packed() {
        Enter(28, 128, 0x82bd8558u);
        auto &r = s.r;
        r[31] = r[5];
        r[28] = r[6];
        Compare(r[3], 255);
        if (!s.cr6.gt) {
            Compare(r[4]);
            if (!s.cr6.eq) {
                r[30] = r[31];
                r[31] = r[4];
                do {
                    r[11] = Word(r[30]);
                    r[3] = r[28];
                    r[4] = r[11] & 255u;
                    Lower(0x82bd0938u, 0x82bd858cu);
                    --r[31];
                    r[30] += 4;
                    Compare(r[31]);
                } while (!s.cr6.eq);
            }
            Leave(28, 128);
            return;
        }
        Compare(r[3], 65535);
        bool wide = s.cr6.gt;
        Compare(r[4]);
        if (!s.cr6.eq) {
            r[29] = r[7] & 255u;
            r[30] = r[4];
            do {
                r[11] = Word(r[31]);
                Compare(r[29]);
                if (wide)
                    Store(r[1] + 80, r[11]);
                else
                    m.WriteU16(Address(r[1] + 80), std::uint16_t(r[11]));
                if (!s.cr6.eq)
                    for (unsigned i = 0; i < (wide ? 2u : 1u); ++i) {
                        auto p = r[1] + 80 + i, q = r[1] + (wide ? 83 : 81) - i;
                        r[11] = m.ReadU8(Address(p));
                        r[10] = m.ReadU8(Address(q));
                        m.WriteU8(Address(q), std::uint8_t(r[11]));
                        m.WriteU8(Address(p), std::uint8_t(r[10]));
                    }
                r[3] = r[28];
                r[4] = wide ? Word(r[1] + 80) : m.ReadU16(Address(r[1] + 80));
                Lower(wide ? 0x82bd1050u : 0x82bd0fc8u, wide ? 0x82bd864cu : 0x82bd85e8u);
                --r[30];
                r[31] += 4;
                Compare(r[30]);
            } while (!s.cr6.eq);
        }
        Leave(28, 128);
    }
    void Base() {
        Enter(29, 112, 0x82bd14c0u);
        auto &r = s.r;
        r[31] = r[4];
        r[29] = r[3];
        r[11] = r[31] & 255u;
        r[30] = r[5];
        r[6] = 0;
        Compare(r[11]);
        if (!s.cr6.eq)
            r[6] = 1;
        r[7] = r[30];
        r[5] = 'C';
        r[4] = 'P';
        r[3] = 'O';
        s.lr = 0x82bd14f8u;
        Header();
        r[5] = r[30];
        r[4] = r[31];
        r[3] = 1;
        Lower(0x82bd7d58u, 0x82bd1508u);
        r[5] = r[30];
        r[4] = r[31];
        r[3] = Word(r[29] + 8);
        Lower(0x82bd7d58u, 0x82bd1518u);
        r[11] = Word(r[29] + 16);
        Compare(r[11]);
        if (!s.cr6.eq) {
            r[3] = Address(r[11]);
            r[5] = r[30];
            r[4] = r[31];
            r[11] = Word(r[3]);
            r[11] = Word(r[11] + 20);
            s.ctr = r[11];
            s.lr = 0x82bd1540u;
            d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
        } else {
            r[11] = Word(r[29] + 8);
            r[3] = (r[11] >> 2) & 1u;
        }
        Leave(29, 112);
    }
    void Map(unsigned countOffset, unsigned dataOffset, GuestAddress maximum, GuestAddress write,
             GuestAddress packed) {
        auto &r = s.r;
        r[3] = Word(r[31] + dataOffset);
        Lower(0x82badfa0u, maximum);
        r[5] = r[30];
        r[4] = r[29];
        r[28] = r[3];
        Lower(0x82bd7d58u, write);
        r[7] = r[29];
        r[6] = r[30];
        r[5] = Word(r[31] + dataOffset);
        r[3] = r[28];
        r[4] = Word(r[31] + countOffset);
        s.lr = packed;
        Packed();
    }
    void Mesh() {
        Enter(28, 128, 0x82bd1c00u);
        auto &r = s.r;
        r[31] = r[3];
        r[29] = r[4];
        r[30] = r[5];
        s.lr = 0x82bd1c14u;
        Base();
        r[11] = r[3] & 255u;
        Compare(r[11]);
        if (s.cr6.eq) {
            r[3] = 0;
            Leave(28, 128);
            return;
        }
        r[11] = r[29] & 255u;
        r[6] = 0;
        Compare(r[11]);
        if (!s.cr6.eq)
            r[6] = 1;
        r[7] = r[30];
        r[5] = 'M';
        r[4] = 'B';
        r[3] = 'H';
        s.lr = 0x82bd1c54u;
        Header();
        r[5] = r[30];
        r[4] = r[29];
        r[3] = 0;
        Lower(0x82bd7d58u, 0x82bd1c64u);
        r[5] = r[30];
        r[4] = r[29];
        r[3] = Word(r[31] + 20);
        Lower(0x82bd7d58u, 0x82bd1c74u);
        r[4] = Word(r[31] + 20);
        Compare(r[4], 1);
        if (s.cr6.gt)
            Map(20, 24, 0x82bd1c88u, 0x82bd1c98u, 0x82bd1cb0u);
        r[5] = r[30];
        r[3] = Word(r[31] + 28);
        r[4] = r[29];
        Lower(0x82bd7d58u, 0x82bd1cc0u);
        r[4] = Word(r[31] + 28);
        Compare(r[4]);
        if (!s.cr6.eq)
            Map(28, 32, 0x82bd1cd4u, 0x82bd1ce4u, 0x82bd1cfcu);
        r[3] = 1;
        Leave(28, 128);
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    Envelope w{m, d, s};
    switch (e) {
    case 0x82bd7cb8u:
        w.Header();
        return true;
    case 0x82bd8550u:
        w.Packed();
        return true;
    case 0x82bd14b8u:
        w.Base();
        return true;
    case 0x82bd1bf8u:
        w.Mesh();
        return true;
    default:
        return false;
    }
}
} // namespace lo::semantic::gpu::tree_envelope_write61
