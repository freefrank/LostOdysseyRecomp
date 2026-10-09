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
    void Swap(std::uint64_t p, unsigned width) {
        auto &r = s.r;
        for (unsigned i = 0; i < width / 2; ++i) {
            r[11] = m.ReadU8(Address(p + i));
            r[10] = m.ReadU8(Address(p + width - 1 - i));
            m.WriteU8(Address(p + width - 1 - i), std::uint8_t(r[11]));
            m.WriteU8(Address(p + i), std::uint8_t(r[10]));
        }
    }
    void Scalar(bool inplace = false) {
        Enter(32, 96);
        auto &r = s.r;
        r[11] = Address(r[4]) & 255u;
        Save(1, r[1] + 116);
        Compare(r[11]);
        if (!s.cr6.eq) {
            if (inplace)
                Swap(r[1] + 116, 4);
            else {
                Reverse(r[1] + 116, r[1] + 80);
                r[11] = Word(r[1] + 80);
                Store(r[1] + 116, r[11]);
            }
        }
        r[11] = Word(r[5]);
        Load(1, r[1] + 116);
        r[3] = r[5];
        r[11] = Word(r[11] + 40);
        Call(inplace ? 0x82bd7ec4u : 0x82badaccu);
        Leave(32, 96);
    }
    void Span(bool inplace = false) {
        Enter(28, 128, inplace ? 0x82bd7ff8u : 0x82badcd8u);
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
                    if (inplace)
                        Swap(r[1] + 80, 4);
                    else {
                        Reverse(r[1] + 80, r[1] + 84);
                        r[11] = Word(r[1] + 84);
                        Store(r[1] + 80, r[11]);
                    }
                }
                r[11] = Word(r[28]);
                Load(1, r[1] + 80);
                r[3] = r[28];
                r[11] = Word(r[11] + 40);
                Call(inplace ? 0x82bd8064u : 0x82badd4cu);
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
    void WordScalar(bool half = false) {
        Enter(32, 96);
        auto &r = s.r;
        r[11] = Address(r[4]) & 255u;
        const auto slot = r[1] + (half ? 118u : 116u);
        if (half)
            m.WriteU16(Address(slot), std::uint16_t(r[3]));
        else
            Store(slot, r[3]);
        Compare(r[11]);
        if (!s.cr6.eq)
            Swap(slot, half ? 2 : 4);
        r[11] = Word(r[5]);
        r[3] = r[5];
        r[4] = half ? m.ReadU16(Address(slot)) : Word(slot);
        r[11] = Word(r[11] + (half ? 32 : 36));
        Call(half ? 0x82bd7d44u : 0x82bd7e04u);
        Leave(32, 96);
    }
    void Forward(GuestAddress entry, unsigned slot) {
        Enter(31, 96);
        auto &r = s.r;
        r[31] = r[3];
        r[3] = Word(r[31] + 4);
        r[11] = Word(r[3]);
        r[11] = Word(r[11] + slot);
        Call(entry + 40);
        r[3] = r[31];
        Leave(31, 96);
    }
    void Header(bool ice) {
        const auto delta = ice ? 0x2a318u : 0u;
        Enter(24, 160, 0x82badd68u + delta);
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
        Byte(ice ? 73 : 78, 0x82baddb4u + delta);
        Byte(ice ? 67 : 88, 0x82baddc8u + delta);
        Byte(ice ? 69 : 83, 0x82badddcu + delta);
        Byte(r[29], 0x82baddf0u + delta);
        r[3] = r[31];
        Byte(r[28], 0x82bade08u + delta);
        Byte(r[27], 0x82bade1cu + delta);
        Byte(r[26], 0x82bade30u + delta);
        Byte(r[25], 0x82bade44u + delta);
        if (ice) {
            Store(r[1] + 80, r[24]);
            Compare(r[30]);
            if (!s.cr6.eq) {
                r[10] = m.ReadU8(Address(r[1] + 82));
                r[11] = m.ReadU8(Address(r[1] + 81));
                m.WriteU8(Address(r[1] + 81), std::uint8_t(r[10]));
                r[10] = m.ReadU8(Address(r[1] + 215));
                m.WriteU8(Address(r[1] + 82), std::uint8_t(r[11]));
                m.WriteU8(Address(r[1] + 80), std::uint8_t(r[10]));
                r[10] = m.ReadU8(Address(r[1] + 212));
                m.WriteU8(Address(r[1] + 83), std::uint8_t(r[10]));
            }
            r[11] = Word(r[31]);
            r[3] = r[31];
            r[4] = Word(r[1] + 80);
        } else {
            r[4] = r[24];
            Compare(r[30]);
            if (!s.cr6.eq) {
                Reverse(r[1] + 212, r[1] + 80);
                r[4] = Word(r[1] + 80);
            }
            r[11] = Word(r[31]);
            r[3] = r[31];
        }
        r[11] = Word(r[11] + 36);
        Call(ice ? 0x82bd81a0u : 0x82bade88u);
        r[3] = 1;
        Leave(24, 160);
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    Stream w{m, d, s};
    switch (e) {
    case 0x82b9e528u:
        w.Forward(e, 28);
        return true;
    case 0x82b9e568u:
        w.Forward(e, 32);
        return true;
    case 0x82b9e5a8u:
        w.Forward(e, 36);
        return true;
    case 0x82b9e5e8u:
        w.Forward(e, 40);
        return true;
    case 0x82b9e628u:
        w.Forward(e, 44);
        return true;
    case 0x82b9e668u:
        w.Forward(e, 48);
        return true;

    case 0x82bada70u:
        w.Scalar();
        return true;
    case 0x82badcd0u:
        w.Span();
        return true;
    case 0x82badd60u:
        w.Header(false);
        return true;
    case 0x82bd8078u:
        w.Header(true);
        return true;
    case 0x82bd7db0u:
        w.WordScalar();
        return true;
    case 0x82bd7d00u:
        w.WordScalar(true);
        return true;
    case 0x82bd7e70u:
        w.Scalar(true);
        return true;
    case 0x82bd7ff0u:
        w.Span(true);
        return true;
    default:
        return false;
    }
}
} // namespace lo::semantic::gpu::mesh_stream_write61
