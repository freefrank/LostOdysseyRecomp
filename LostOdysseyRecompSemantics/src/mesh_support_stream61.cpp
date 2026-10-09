#include "lo_semantics/mesh_support_stream61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/serialization_control61.h"
namespace lo::semantic::gpu::mesh_support_stream61 {
namespace {
using recovery_abi::Address;
struct Adapter {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Store(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    void Compare(std::uint64_t v) {
        auto x = Address(v);
        s.cr6 = {0u, std::uint8_t(x != 0), std::uint8_t(x == 0), s.xer_so};
    }
    void Enter(unsigned first, GuestAddress cont = 0) {
        auto &r = s.r;
        r[12] = s.lr;
        if (cont)
            s.lr = cont;
        Store(r[1] - 8, r[12]);
        for (unsigned i = first; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        auto old = r[1];
        r[1] -= 112;
        Store(r[1], old);
    }
    void Leave(unsigned first) {
        auto &r = s.r;
        r[1] += 112;
        for (unsigned i = first; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
    void Lower(GuestAddress e, GuestAddress cont) {
        s.lr = cont;
        if (e == 0x82b9cb70u)
            (void)serialization_control61::Apply(e, m, d, s);
        else
            (void)mesh_stream_write61::Apply(e, m, d, s);
    }
    void Construct(bool init) {
        auto &r = s.r;
        r[11] = 0xffffffff820d0000ull;
        if (init) {
            Store(r[3] + 4, r[4]);
            Store(r[3] + 8, r[4]);
        }
        r[11] += init ? 25216 : 25176;
        Store(r[3], r[11]);
    }
    void Counts() {
        Enter(29, 0x82bb3820u);
        auto &r = s.r;
        r[29] = r[3];
        r[31] = r[4];
        Lower(0x82b9cb70u, 0x82bb3830u);
        r[30] = r[3];
        r[9] = r[31];
        r[7] = 0;
        r[6] = 83;
        r[5] = 85;
        r[4] = 65;
        r[3] = 71;
        r[8] = r[30];
        Lower(0x82bd8078u, 0x82bb3854u);
        r[11] = Address(r[3]) & 255u;
        Compare(r[11]);
        if (s.cr6.eq) {
            r[3] = 0;
            Leave(29);
            return;
        }
        for (unsigned i = 0; i < 2; ++i) {
            r[11] = Word(r[29] + 4);
            r[5] = r[31];
            r[4] = r[30];
            r[3] = Word(r[11] + 4 + 4 * i);
            Lower(0x82bd7db0u, i ? 0x82bb3894u : 0x82bb3880u);
        }
        r[3] = 1;
        Leave(29);
    }
    void Write() {
        Enter(30);
        auto &r = s.r;
        r[30] = r[3];
        r[31] = r[4];
        Lower(0x82b9cb70u, 0x82bb3710u);
        r[8] = r[3];
        r[9] = r[31];
        r[7] = 0;
        r[6] = 77;
        r[5] = 80;
        r[4] = 85;
        r[3] = 83;
        Lower(0x82bd8078u, 0x82bb3730u);
        r[11] = Address(r[3]) & 255u;
        Compare(r[11]);
        if (s.cr6.eq) {
            r[3] = 0;
            Leave(30);
            return;
        }
        r[4] = r[31];
        r[3] = r[30];
        s.lr = 0x82bb3750u;
        Counts();
        r[11] = Address(r[3]) & 255u;
        Compare(r[11]);
        if (s.cr6.eq) {
            r[3] = 0;
            Leave(30);
            return;
        }
        r[11] = Word(r[31]);
        r[3] = r[31];
        r[10] = Word(r[11] + 48);
        r[11] = Word(r[30] + 8);
        r[5] = Word(r[11] + 8);
        r[4] = Word(r[11] + 24);
        s.ctr = r[10];
        s.lr = 0x82bb377cu;
        d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
        r[11] = Word(r[30] + 8);
        r[10] = Word(r[31]);
        r[3] = r[31];
        r[5] = Word(r[11] + 8);
        r[10] = Word(r[10] + 48);
        r[4] = Word(r[11] + 28);
        s.ctr = r[10];
        s.lr = 0x82bb379cu;
        d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
        r[3] = 1;
        Leave(30);
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    Adapter a{m, d, s};
    switch (e) {
    case 0x82bb3408u:
        a.Construct(true);
        return true;
    case 0x82bb3420u:
        a.Construct(false);
        return true;
    case 0x82bb3818u:
        a.Counts();
        return true;
    case 0x82bb36f0u:
        a.Write();
        return true;
    default:
        return false;
    }
}
} // namespace lo::semantic::gpu::mesh_support_stream61
