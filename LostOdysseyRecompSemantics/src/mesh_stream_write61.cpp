#include "lo_semantics/mesh_stream_write61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::mesh_stream_write61 {
namespace {
using recovery_abi::Address;
struct Stream {
    GuestMemory &m;
    Dependencies deps;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Store(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    void Compare(std::uint64_t v) {
        auto x = Address(v);
        s.cr6 = {0u, std::uint8_t(x != 0), std::uint8_t(x == 0), s.xer_so};
    }
    void Enter(unsigned first, unsigned frame, GuestAddress continuation = 0) {
        auto &r = s.r;
        r[12] = s.lr;
        if (continuation)
            s.lr = continuation;
        Store(r[1] - 8, r[12]);
        for (unsigned i = first; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        auto stack = r[1];
        r[1] -= frame;
        Store(r[1], stack);
    }
    void Leave(unsigned first, unsigned frame) {
        auto &r = s.r;
        r[1] += frame;
        for (unsigned i = first; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
    void Gradual() {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            deps.fp.SetHostFpControl(s.cached_fp_control);
        }
    }
    void Load(unsigned f, std::uint64_t p) {
        Gradual();
        s.fpr_bits[f] = std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(Word(p))));
    }
    void Save(unsigned f, std::uint64_t p) {
        Gradual();
        Store(p, std::bit_cast<std::uint32_t>(
                     static_cast<float>(std::bit_cast<double>(s.fpr_bits[f]))));
    }
    void Reverse(std::uint64_t source, std::uint64_t destination) {
        for (unsigned i = 0; i < 4; ++i) {
            s.r[11] = m.ReadU8(Address(source + 3 - i));
            m.WriteU8(Address(destination + i), std::uint8_t(s.r[11]));
        }
    }
    void Call(GuestAddress continuation) {
        s.ctr = s.r[11];
        s.lr = continuation;
        deps.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void Scalar() {
        Enter(32, 96);
        auto &r = s.r;
        r[11] = Address(r[4]) & 255u;
        Save(1, r[1] + 116);
        Compare(r[11]);
        if (!s.cr6.eq) {
            Reverse(r[1] + 116, r[1] + 80);
            r[11] = Word(r[1] + 80);
            Store(r[1] + 116, r[11]);
        }
        r[11] = Word(r[5]);
        Load(1, r[1] + 116);
        r[3] = r[5];
        r[11] = Word(r[11] + 40);
        Call(0x82badaccu);
        Leave(32, 96);
    }
    void Span() {
        Enter(28, 128, 0x82badcd8u);
        auto &r = s.r;
        r[31] = r[4];
        r[30] = r[3];
        r[28] = r[6];
        Compare(r[31]);
        if (!s.cr6.eq) {
            r[29] = Address(r[5]) & 255u;
            do {
                Load(0, r[30]);
                --r[31];
                Save(0, r[1] + 80);
                r[30] += 4;
                Compare(r[29]);
                if (!s.cr6.eq) {
                    Reverse(r[1] + 80, r[1] + 84);
                    r[11] = Word(r[1] + 84);
                    Store(r[1] + 80, r[11]);
                }
                r[11] = Word(r[28]);
                Load(1, r[1] + 80);
                r[3] = r[28];
                r[11] = Word(r[11] + 40);
                Call(0x82badd4cu);
                Compare(r[31]);
            } while (!s.cr6.eq);
        }
        Leave(28, 128);
    }
    void Byte(std::uint64_t value, GuestAddress continuation) {
        s.r[11] = Word(s.r[3]);
        s.r[4] = value;
        s.r[11] = Word(s.r[11] + 28);
        Call(continuation);
    }
    void Header() {
        Enter(24, 160, 0x82badd68u);
        auto &r = s.r;
        r[24] = r[7];
        r[30] = Address(r[8]) & 255u;
        r[28] = r[3];
        r[27] = r[4];
        r[26] = r[5];
        r[25] = r[6];
        Store(r[1] + 212, r[24]);
        r[31] = r[9];
        r[29] = 0;
        Compare(r[30]);
        if (!s.cr6.eq)
            r[29] = 1;
        r[3] = r[31];
        Byte(78, 0x82baddb4u);
        Byte(88, 0x82baddc8u);
        Byte(83, 0x82badddcu);
        Byte(r[29], 0x82baddf0u);
        r[3] = r[31];
        Byte(r[28], 0x82bade08u);
        Byte(r[27], 0x82bade1cu);
        Byte(r[26], 0x82bade30u);
        Byte(r[25], 0x82bade44u);
        r[4] = r[24];
        Compare(r[30]);
        if (!s.cr6.eq) {
            Reverse(r[1] + 212, r[1] + 80);
            r[4] = Word(r[1] + 80);
        }
        r[11] = Word(r[31]);
        r[3] = r[31];
        r[11] = Word(r[11] + 36);
        Call(0x82bade88u);
        r[3] = 1;
        Leave(24, 160);
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    Stream w{m, d, s};
    switch (e) {
    case 0x82bada70u:
        w.Scalar();
        return true;
    case 0x82badcd0u:
        w.Span();
        return true;
    case 0x82badd60u:
        w.Header();
        return true;
    default:
        return false;
    }
}
} // namespace lo::semantic::gpu::mesh_stream_write61
