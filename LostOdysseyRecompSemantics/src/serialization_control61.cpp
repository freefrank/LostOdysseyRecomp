#include "lo_semantics/serialization_control61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::serialization_control61 {
namespace {
using recovery_abi::Address;
struct Control {
    GuestMemory &m;
    Dependencies deps;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Store(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    void Compare(std::uint64_t a, std::uint64_t b = 0) {
        auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    void Enter() {
        auto &r = s.r;
        r[12] = s.lr;
        Store(r[1] - 8u, r[12]);
        const auto stack = r[1];
        r[1] -= 96u;
        Store(r[1], stack);
    }
    void Leave() {
        auto &r = s.r;
        r[1] += 96u;
        r[12] = Word(r[1] - 8u);
        s.lr = r[12];
    }
    void Call(GuestAddress continuation) {
        s.ctr = s.r[11];
        s.lr = continuation;
        deps.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void Write() {
        Enter();
        auto &r = s.r;
        r[11] = r[3];
        r[10] = Address(r[4]) & 255u;
        Compare(r[10]);
        Store(r[1] + 116u, r[11]);
        if (!s.cr6.eq) {
            for (unsigned i = 0; i < 4; ++i) {
                r[11] = m.ReadU8(Address(r[1] + 119u - i));
                m.WriteU8(Address(r[1] + 80u + i), std::uint8_t(r[11]));
            }
            r[11] = Word(r[1] + 80u);
        }
        r[10] = Word(r[5]);
        r[4] = r[11];
        r[3] = r[5];
        r[11] = Word(r[10] + 36u);
        Call(0x82bde9a4u);
        Leave();
    }
    void Register() {
        Enter();
        auto &r = s.r;
        r[10] = 0xffffffff832e0000ull;
        r[11] = Word(r[10] - 2680u);
        Compare(r[11]);
        if (!s.cr6.eq) {
            r[10] = Word(r[11]);
            r[3] = r[11];
            r[11] = Word(r[10] + 132u);
            Call(0x82bde9e8u);
            r[11] = Word(r[3]);
            r[11] = Word(r[11] + 8u);
            Call(0x82bde9f8u);
            r[11] = 0xffffffff820d0000ull;
            r[7] = 888;
            r[6] = r[11] + 29152u;
            r[11] = 0xffffffff820d0000ull;
            r[4] = 4;
            r[5] = r[11] + 29260u;
            r[11] = Word(r[3]);
            r[11] = Word(r[11]);
            Call(0x82bdea20u);
            r[3] = 0;
        } else {
            Compare(r[3]);
            if (s.cr6.eq)
                r[3] = 0;
            else {
                Store(r[10] - 2680u, r[3]);
                r[3] = 1;
            }
        }
        Leave();
    }
    void Endian() {
        auto &r = s.r;
        r[11] = 0xffffffff832e0000ull;
        r[11] = Word(r[11] - 16000u);
        Compare(r[11], 1);
        if (s.cr6.lt) {
            r[3] = 1;
            return;
        }
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        Compare(r[11], 3);
        r[3] = s.cr6.lt ? 0 : m.ReadU8(Address(r[1] - 16u));
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies deps, Registers &s) {
    Control c{m, deps, s};
    switch (e) {
    case 0x82bde948u:
        c.Write();
        return true;
    case 0x82bde9b8u:
        c.Register();
        return true;
    case 0x82b9cb70u:
        c.Endian();
        return true;
    default:
        return false;
    }
}
} // namespace lo::semantic::gpu::serialization_control61
