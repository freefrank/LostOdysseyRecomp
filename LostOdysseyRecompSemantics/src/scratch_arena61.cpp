#include "lo_semantics/scratch_arena61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::scratch_arena61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
struct Arena {
    GuestMemory &m;
    GuestServices &guest;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Store(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    void Compare(std::uint64_t a, std::uint64_t b = 0) {
        auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    void Enter(unsigned first, unsigned frame, GuestAddress continuation = 0) {
        auto &r = s.r;
        r[12] = s.lr;
        if (continuation)
            s.lr = continuation;
        Store(r[1] - 8u, r[12]);
        for (unsigned i = first; i < 32; ++i)
            WriteU64(m, Address(r[1] - 16u - 8u * (31u - i)), r[i]);
        const auto stack = r[1];
        r[1] -= frame;
        Store(r[1], stack);
    }
    void Leave(unsigned first, unsigned frame) {
        auto &r = s.r;
        r[1] += frame;
        for (unsigned i = first; i < 32; ++i)
            r[i] = ReadU64(m, Address(r[1] - 16u - 8u * (31u - i)));
        r[12] = Word(r[1] - 8u);
        s.lr = r[12];
    }
    void Call(GuestAddress continuation) {
        s.ctr = s.r[11];
        s.lr = continuation;
        guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void Diagnostic(GuestAddress first, GuestAddress second, GuestAddress final, unsigned line,
                    unsigned message) {
        auto &r = s.r;
        r[11] = Word(r[3]);
        r[11] = Word(r[11] + 132u);
        Call(first);
        r[11] = Word(r[3]);
        r[11] = Word(r[11] + 8u);
        Call(second);
        r[11] = 0xffffffff820d0000ull;
        r[7] = line;
        r[6] = r[11] + 29152u;
        r[11] = 0xffffffff820d0000ull;
        r[4] = 4;
        r[5] = r[11] + message;
        r[11] = Word(r[3]);
        r[11] = Word(r[11]);
        Call(final);
    }
    void Reserve() {
        Enter(29, 112, 0x82bde630u);
        auto &r = s.r;
        r[11] = 0xffffffff832e0000ull;
        r[31] = r[11] - 2676u;
        r[10] = Word(r[31] - 24u);
        r[11] = Word(r[31] - 28u);
        Compare(r[10], r[11]);
        if (s.cr6.gt) {
            r[3] = Word(r[31] - 4u);
            Diagnostic(0x82bde660u, 0x82bde670u, 0x82bde698u, 343, 29100);
            r[3] = 0;
            Leave(29, 112);
            return;
        }
        r[10] = r[11] + r[3];
        r[11] = Word(r[31] - 20u);
        Compare(r[10], r[11]);
        if (s.cr6.gt) {
            r[29] = 0xffffffff83210000ull;
            r[4] = Word(r[31] - 32u);
            Compare(r[4]);
            r[11] = Word(r[29] + 26588u);
            r[11] += r[3];
            r[3] = Word(r[31]);
            r[30] = r[11] - 1u;
            r[11] = Word(r[3]);
            if (s.cr6.eq) {
                r[11] = Word(r[11] + 12u);
                r[4] = r[30];
                Call(0x82bde6e8u);
            } else {
                r[11] = Word(r[11] + 16u);
                r[5] = r[30];
                Call(0x82bde6fcu);
            }
            r[11] = Word(r[29] + 26588u);
            r[10] = r[3];
            --r[11];
            r[11] = Address(r[11]);
            Store(r[31] - 32u, r[10]);
            r[10] += r[11];
            r[11] = r[10] & ~r[11];
            Store(r[31] - 28u, r[11]);
            Store(r[31] - 24u, r[11]);
            r[11] += r[30];
            Store(r[31] - 20u, r[11]);
        }
        r[3] = 1;
        Leave(29, 112);
    }
    void Counting() {
        Enter(32, 96);
        auto &r = s.r;
        r[11] = Address(r[3]) & 255u;
        Compare(r[11]);
        r[11] = 0xffffffff832e0000ull;
        r[11] -= 2724u;
        if (!s.cr6.eq) {
            r[10] = m.ReadU8(Address(r[11] + 1u));
            Compare(r[10]);
            if (!s.cr6.eq) {
                r[3] = Word(r[11] + 44u);
                Diagnostic(0x82bde778u, 0x82bde788u, 0x82bde7b0u, 388, 29188);
                Leave(32, 96);
                return;
            }
            r[10] = 1;
            m.WriteU8(Address(r[11]), std::uint8_t(r[10]));
            r[10] = 0;
            Store(r[11] + 8u, r[10]);
            Store(r[11] + 4u, r[10]);
        } else {
            r[10] = 0;
            r[9] = Word(r[11] + 12u);
            m.WriteU8(Address(r[11]), std::uint8_t(r[10]));
            r[10] = Word(r[11] + 8u);
            Compare(r[10], r[9]);
            if (s.cr6.gt)
                Store(r[11] + 12u, r[10]);
        }
        Leave(32, 96);
    }
    void Scoped() {
        Enter(28, 128, 0x82bde818u);
        auto &r = s.r;
        r[11] = Address(r[3]) & 255u;
        r[30] = r[4];
        Compare(r[11]);
        r[29] = r[5];
        r[28] = r[6];
        r[11] = 0xffffffff832e0000ull;
        if (!s.cr6.eq) {
            r[31] = r[11] - 2724u;
            r[11] = m.ReadU8(Address(r[31] + 1u));
            Compare(r[11]);
            if (s.cr6.eq) {
                r[11] = m.ReadU8(Address(r[31]));
                Compare(r[11]);
                if (!s.cr6.eq) {
                    r[3] = Word(r[31] + 44u);
                    Diagnostic(0x82bde868u, 0x82bde878u, 0x82bde8a0u, 418, 29224);
                    Leave(28, 128);
                    return;
                }
                r[3] = Word(r[31] + 48u);
                r[4] = r[30];
                r[11] = Word(r[3]);
                r[11] = Word(r[11] + 12u);
                Call(0x82bde8c0u);
                r[11] = r[3];
                r[3] = r[29];
                Store(r[31] + 32u, r[11]);
                Store(r[31] + 36u, r[11]);
                r[11] += r[30];
                Store(r[31] + 40u, r[11]);
                s.lr = 0x82bde8dcu;
                Reserve();
                r[11] = 0xffffffff83210000ull;
                r[11] += 26588u;
                r[10] = Word(r[11]);
                Store(r[11], r[28]);
                Store(r[11] + 4u, r[10]);
                r[10] = 1;
                m.WriteU8(Address(r[31] + 1u), std::uint8_t(r[10]));
            }
        } else {
            r[11] -= 2724u;
            r[10] = m.ReadU8(Address(r[11] + 1u));
            Compare(r[10]);
            if (!s.cr6.eq) {
                r[10] = 0;
                m.WriteU8(Address(r[11] + 1u), std::uint8_t(r[10]));
                Store(r[11] + 32u, r[10]);
                Store(r[11] + 36u, r[10]);
                Store(r[11] + 40u, r[10]);
                r[10] = Word(r[11] + 20u);
                Store(r[11] + 24u, r[10]);
                r[11] = 0xffffffff83210000ull;
                r[11] += 26588u;
                r[10] = Word(r[11] + 4u);
                Store(r[11], r[10]);
            }
        }
        Leave(28, 128);
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, GuestServices &guest, Registers &s) {
    Arena a{m, guest, s};
    switch (e) {
    case 0x82bde628u:
        a.Reserve();
        return true;
    case 0x82bde738u:
        a.Counting();
        return true;
    case 0x82bde810u:
        a.Scoped();
        return true;
    default:
        return false;
    }
}
} // namespace lo::semantic::gpu::scratch_arena61
