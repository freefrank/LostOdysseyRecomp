#include "lo_semantics/mesh_indexed_remap61.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/reader_buffer_growth61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::mesh_indexed_remap61 {
namespace {
using recovery_abi::Address;
struct Remap {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Word(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    static std::uint32_t Shift(std::uint64_t v, unsigned n) { return Address(v) << n; }
    void Compare(std::uint64_t a, std::uint64_t b = 0) {
        auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    void Enter(unsigned first, unsigned frame, GuestAddress cont) {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = cont;
        for (unsigned i = first; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        Word(r[1] - 8, r[12]);
        auto old = r[1];
        r[1] -= frame;
        Word(r[1], old);
    }
    void Leave(unsigned first, unsigned frame) {
        auto &r = s.r;
        r[1] += frame;
        for (unsigned i = first; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
    void Lower(GuestAddress e, GuestAddress cont) {
        s.lr = cont;
        switch (e) {
        case 0x82bd0798u:
            (void)crt_close_recursive_buffer_context::Apply(e, m, d.sort.guest, s);
            break;
        case 0x82b7bc40u:
            crt_reader_chain61::ApplySupport_B7BC40(m, d.sort.accepted, s);
            break;
        case 0x82bd2870u:
            (void)reader_buffer_growth61::Apply(e, m, {d.sort.guest, d.fp}, s);
            break;
        case 0x82bbf3f8u:
            Corner();
            break;
        }
    }
    void Call(GuestAddress c) {
        s.ctr = s.r[11];
        s.lr = c;
        d.sort.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void Grow(GuestAddress c) {
        auto &r = s.r;
        if (s.cr6.eq) {
            r[4] = 1;
            r[3] = r[31];
            Lower(0x82bd2870u, c);
        }
    }
    void Corner() {
        Enter(24, 160, 0x82bbf400u);
        auto &r = s.r;
        r[30] = r[3];
        r[24] = Shift(r[4], 2);
        r[29] = r[5];
        r[25] = r[6];
        r[11] = Word(r[30] + 8);
        r[10] = Word(r[24] + r[11]);
        auto v = std::int32_t(r[10]);
        s.cr6 = {std::uint8_t(v < -1), std::uint8_t(v > -1), std::uint8_t(v == -1), s.xer_so};
        if (s.cr6.eq) {
            r[11] = Shift(r[4], 1);
            r[31] = Word(r[30] + 20);
            r[10] = Word(r[30]);
            r[11] += r[4];
            r[11] = Shift(r[11], 2);
            r[9] = Word(r[31]);
            r[11] += r[10];
            r[10] = Word(r[31] + 4);
            Compare(r[10], r[9]);
            r[28] = Word(r[11]);
            r[27] = Word(r[11] + 4);
            r[26] = Word(r[11] + 8);
            constexpr GuestAddress continuations[]{0x82bbf464u, 0x82bbf498u, 0x82bbf4ccu,
                                                   0x82bbf504u};
            for (unsigned i = 0; i < 4; ++i) {
                Grow(continuations[i]);
                r[11] = Word(r[31] + 4);
                r[10] = Word(r[31] + 8);
                r[11] = Shift(r[11], 2);
                Word(r[11] + r[10], r[i == 0 ? 28 : i == 1 ? 27 : i == 2 ? 26 : 28]);
                r[11] = Word(r[31] + 4);
                if (i < 3)
                    r[10] = Word(r[31]);
                ++r[11];
                if (i < 3)
                    Compare(r[11], r[10]);
                Word(r[31] + 4, r[11]);
                if (i == 2)
                    r[28] = Word(r[30] + 12);
            }
            r[11] = Word(r[30] + 16);
            r[9] = Word(r[30] + 4);
            r[10] = Shift(r[11], 1);
            r[8] = Word(r[29]);
            r[11] += r[10];
            r[11] = Shift(r[11], 2);
            r[11] += r[25];
            r[11] = Shift(r[11], 2);
            Word(r[11] + r[9], r[8]);
            r[11] = Word(r[30] + 8);
            r[10] = Word(r[29]);
            Word(r[24] + r[11], r[10]);
            r[11] = Word(r[29]);
            ++r[11];
            Word(r[29], r[11]);
        } else {
            r[11] = Word(r[30] + 16);
            r[8] = Word(r[30] + 4);
            r[9] = Shift(r[11], 1);
            r[11] += r[9];
            r[11] = Shift(r[11], 2);
            r[11] += r[25];
            r[11] = Shift(r[11], 2);
            Word(r[11] + r[8], r[10]);
        }
        Leave(24, 160);
    }
    void Batch() {
        Enter(23, 192, 0x82bbfeb0u);
        auto &r = s.r;
        r[28] = r[3];
        r[29] = r[4];
        r[24] = r[5];
        r[30] = r[6];
        Lower(0x82bd0798u, 0x82bbfec8u);
        r[11] = Word(r[3]);
        r[5] = 1;
        r[10] = Word(r[28] + 228);
        r[4] = Shift(r[10], 2);
        r[11] = Word(r[11]);
        Call(0x82bbfee4u);
        r[11] = Word(r[28] + 228);
        r[4] = 255;
        r[5] = Shift(r[11], 2);
        r[25] = r[3];
        Lower(0x82b7bc40u, 0x82bbfef8u);
        r[11] = m.ReadU8(Address(r[28] + 288));
        Compare(r[11]);
        if (!s.cr6.eq) {
            r[11] = 0;
            Word(r[28] + 232, r[11]);
        }
        r[11] = Word(r[28] + 252);
        r[31] = r[28] + 232;
        Word(r[1] + 88, r[25]);
        Compare(r[24]);
        Word(r[1] + 100, r[30]);
        Word(r[1] + 80, r[11]);
        r[11] = Word(r[28] + 248);
        r[23] = Word(r[31]);
        Word(r[1] + 84, r[11]);
        if (!s.cr6.eq) {
            r[30] = r[24];
            do {
                r[11] = Word(r[29]);
                r[6] = 0;
                r[10] = Word(r[28] + 248);
                r[5] = r[31];
                r[9] = Shift(r[11], 1);
                r[3] = r[1] + 80;
                Word(r[1] + 96, r[11]);
                r[11] += r[9];
                r[11] = Shift(r[11], 4);
                r[11] += r[10];
                r[10] = Word(r[11] + 28);
                r[27] = Word(r[11] + 16);
                r[26] = Word(r[11] + 20);
                r[4] = Word(r[11] + 12);
                Word(r[1] + 92, r[10]);
                Lower(0x82bbf3f8u, 0x82bbff78u);
                r[6] = 1;
                r[5] = r[31];
                r[4] = r[27];
                r[3] = r[1] + 80;
                Lower(0x82bbf3f8u, 0x82bbff8cu);
                r[6] = 2;
                r[5] = r[31];
                r[4] = r[26];
                r[3] = r[1] + 80;
                Lower(0x82bbf3f8u, 0x82bbffa0u);
                --r[30];
                r[29] += 4;
                Compare(r[30]);
            } while (!s.cr6.eq);
        }
        Compare(r[25]);
        if (!s.cr6.eq) {
            Lower(0x82bd0798u, 0x82bbffbcu);
            r[11] = Word(r[3]);
            r[4] = r[25];
            r[11] = Word(r[11] + 12);
            Call(0x82bbffd0u);
        }
        r[30] = r[28] + 176;
        r[11] = Word(r[31]);
        r[31] = r[11] - r[23];
        r[11] = Word(r[30]);
        r[10] = Word(r[30] + 4);
        Compare(r[10], r[11]);
        if (s.cr6.eq) {
            r[4] = 1;
            r[3] = r[30];
            Lower(0x82bd2870u, 0x82bbfff8u);
        }
        r[11] = Word(r[30] + 4);
        r[10] = Word(r[30] + 8);
        r[11] = Shift(r[11], 2);
        Word(r[11] + r[10], r[24]);
        r[11] = Word(r[30] + 4);
        r[10] = Word(r[30]);
        ++r[11];
        Compare(r[11], r[10]);
        Word(r[30] + 4, r[11]);
        if (s.cr6.eq) {
            r[4] = 1;
            r[3] = r[30];
            Lower(0x82bd2870u, 0x82bc002cu);
        }
        r[11] = Word(r[30] + 4);
        r[3] = r[31];
        r[10] = Word(r[30] + 8);
        r[11] = Shift(r[11], 2);
        Word(r[11] + r[10], r[31]);
        r[11] = Word(r[30] + 4);
        ++r[11];
        Word(r[30] + 4, r[11]);
        Leave(23, 192);
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    Remap r{m, d, s};
    switch (e) {
    case 0x82bbf3f8u:
        r.Corner();
        break;
    case 0x82bbfea8u:
        r.Batch();
        break;
    default:
        return false;
    }
    return true;
}
} // namespace lo::semantic::gpu::mesh_indexed_remap61
