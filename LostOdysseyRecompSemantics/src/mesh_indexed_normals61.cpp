#include "lo_semantics/mesh_indexed_normals61.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::mesh_indexed_normals61 {
namespace {
using recovery_abi::Address;
struct Normals {
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
    void FCompare(double a, double b) {
        s.cr6 = {std::uint8_t(a < b), std::uint8_t(a > b), std::uint8_t(a == b),
                 std::uint8_t(std::isnan(a) || std::isnan(b))};
    }
    void Gradual() {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            d.fp.SetHostFpControl(s.cached_fp_control);
        }
    }
    double F(unsigned i) { return std::bit_cast<double>(s.fpr_bits[i]); }
    void Single(unsigned i, double v) {
        s.fpr_bits[i] = std::bit_cast<std::uint64_t>(double(float(v)));
    }
    void Load(unsigned i, std::uint64_t p) {
        Gradual();
        s.fpr_bits[i] = std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(Word(p))));
    }
    void Store(unsigned i, std::uint64_t p) { Word(p, std::bit_cast<std::uint32_t>(float(F(i)))); }
    void Lower(GuestAddress e, GuestAddress c) {
        s.lr = c;
        switch (e) {
        case 0x82bd0798u:
            (void)crt_close_recursive_buffer_context::Apply(e, m, d.sort.guest, s);
            break;
        case 0x82b7bc40u:
            crt_reader_chain61::ApplySupport_B7BC40(m, d.sort.accepted, s);
            break;
        case 0x82bb3c00u:
            (void)mesh_indexed_channels61::Apply(e, m, d, s);
            break;
        }
    }
    void Call(GuestAddress c) {
        s.ctr = s.r[11];
        s.lr = c;
        d.sort.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void FaceNormal() {
        auto &r = s.r;
        r[10] = Word(r[30] - 24);
        r[4] = r[30] - 8;
        r[9] = Word(r[30] - 20);
        r[7] = Shift(r[10], 1);
        r[8] = Word(r[30] - 28);
        r[11] = Word(r[31] + 252);
        r[10] += r[7];
        r[7] = Shift(r[9], 1);
        r[6] = Shift(r[10], 2);
        r[10] = Shift(r[8], 1);
        r[9] += r[7];
        r[10] += r[8];
        r[9] = Shift(r[9], 2);
        r[7] = Shift(r[10], 2);
        r[8] = Word(r[6] + r[11]);
        r[10] = Word(r[9] + r[11]);
        r[11] = Word(r[7] + r[11]);
        r[7] = Shift(r[10], 1);
        r[9] = Shift(r[11], 1);
        r[10] += r[7];
        r[11] += r[9];
        r[10] = Shift(r[10], 2);
        r[11] = Shift(r[11], 2);
        r[10] += r[29];
        r[9] = r[11] + r[29];
        r[11] = Shift(r[8], 1);
        r[11] += r[8];
        Load(0, r[10]);
        r[11] = Shift(r[11], 2);
        Load(13, r[10] + 4);
        Load(12, r[10] + 8);
        r[11] += r[29];
        Load(11, r[9]);
        Load(10, r[9] + 4);
        Load(9, r[9] + 8);
        Load(8, r[11]);
        Load(7, r[11] + 4);
        Single(0, F(0) - F(8));
        Load(8, r[11] + 8);
        Single(13, F(13) - F(7));
        Load(7, r[11]);
        Single(12, F(12) - F(8));
        Load(8, r[11] + 4);
        Single(11, F(11) - F(7));
        Load(7, r[11] + 8);
        Single(10, F(10) - F(8));
        Single(9, F(9) - F(7));
        Single(6, F(13) * F(11));
        Single(8, F(10) * F(12));
        Single(7, F(9) * F(0));
        Single(0, F(10) * F(0) - F(6));
        Store(0, r[30]);
        Single(13, F(9) * F(13) - F(8));
        Store(13, r[4]);
        Single(12, F(12) * F(11) - F(7));
        Store(12, r[30] - 4);
        s.fpr_bits[0] = s.fpr_bits[13];
        s.fpr_bits[13] = s.fpr_bits[12];
        Load(12, r[30]);
        Single(11, F(13) * F(13));
        Single(11, F(0) * F(0) + F(11));
        Single(11, F(12) * F(12) + F(11));
        FCompare(F(11), F(31));
        if (!s.cr6.eq) {
            Single(11, std::sqrt(F(11)));
            Single(11, F(30) / F(11));
            Single(0, F(0) * F(11));
            Store(0, r[4]);
            Single(0, F(13) * F(11));
            Store(0, r[30] - 4);
            Single(0, F(12) * F(11));
            Store(0, r[30]);
        }
        r[11] = m.ReadU8(Address(r[31] + 283));
        Compare(r[11]);
        if (!s.cr6.eq) {
            r[3] = r[31] + 144;
            Lower(0x82bb3c00u, 0x82bbeee4u);
        }
    }
    bool AllocateCounts() {
        auto &r = s.r;
        for (unsigned i = 0; i < 2; ++i) {
            Lower(0x82bd0798u, i ? 0x82bbef28u : 0x82bbeefcu);
            r[11] = Word(r[31] + 212);
            r[5] = 0;
            r[4] = Shift(r[11], 2);
            r[11] = Word(r[3]);
            r[11] = Word(r[11]);
            Call(i ? 0x82bbef44u : 0x82bbef18u);
            Compare(r[3]);
            Word(r[31] + 264 + 4 * i, r[3]);
            if (s.cr6.eq)
                return false;
        }
        for (unsigned i = 0; i < 2; ++i) {
            r[11] = Word(r[31] + 212);
            r[4] = 0;
            r[3] = Word(r[31] + 264 + 4 * i);
            r[5] = Shift(r[11], 2);
            Lower(0x82b7bc40u, i ? 0x82bbef78u : 0x82bbef64u);
        }
        return true;
    }
    void Prefix(bool final) {
        auto &r = s.r;
        if (final) {
            r[11] = Word(r[31] + 268);
            r[9] = 0;
            r[10] = 1;
            Word(r[11], r[9]);
            r[11] = Word(r[31] + 212);
        } else {
            r[11] = Word(r[31] + 212);
            r[10] = 1;
        }
        Compare(r[11], 1);
        if (!s.cr6.gt)
            return;
        r[11] = 4;
        do {
            r[9] = Word(r[31] + 268);
            ++r[10];
            r[8] = Word(r[31] + 264);
            r[9] += r[11];
            r[8] += r[11];
            r[11] += 4;
            r[7] = Word(r[9] - 4);
            r[8] = Word(r[8] - 4);
            r[8] += r[7];
            Word(r[9], r[8]);
            r[9] = Word(r[31] + 212);
            Compare(r[10], r[9]);
        } while (s.cr6.lt);
    }
    void CountIncidence() {
        auto &r = s.r;
        r[11] = Word(r[31] + 224);
        r[10] = 0;
        Compare(r[11]);
        if (!s.cr6.gt)
            return;
        r[11] = r[27] + 16;
        do {
            for (unsigned corner = 0; corner < 3; ++corner) {
                r[9] = Word(r[11] + int(4 * corner) - 4);
                if (corner == 0)
                    ++r[10];
                if (corner == 2)
                    r[11] += 48;
                r[6] = Word(r[31] + 252);
                r[7] = Shift(r[9], 1);
                r[8] = Word(r[31] + 264);
                r[9] += r[7];
                r[9] = Shift(r[9], 2);
                r[9] = Word(r[9] + r[6]);
                r[9] = Shift(r[9], 2);
                r[7] = Word(r[9] + r[8]);
                ++r[7];
                Word(r[9] + r[8], r[7]);
            }
            r[9] = Word(r[31] + 224);
            Compare(r[10], r[9]);
        } while (s.cr6.lt);
    }
    bool Scatter() {
        auto &r = s.r;
        Lower(0x82bd0798u, 0x82bbf070u);
        r[11] = Word(r[31] + 224);
        r[9] = Word(r[3]);
        r[5] = 0;
        r[10] = Shift(r[11], 1);
        r[11] += r[10];
        r[4] = Shift(r[11], 2);
        r[11] = Word(r[9]);
        Call(0x82bbf094u);
        Compare(r[3]);
        Word(r[31] + 272, r[3]);
        if (s.cr6.eq)
            return false;
        r[10] = Word(r[31] + 224);
        r[11] = 0;
        Compare(r[10]);
        if (s.cr6.gt) {
            r[10] = r[27] + 16;
            do {
                r[8] = Word(r[10] - 4);
                r[4] = r[11];
                r[7] = Word(r[10]);
                r[3] = r[11];
                r[5] = Shift(r[8], 1);
                r[6] = Word(r[10] + 4);
                r[9] = Word(r[31] + 252);
                r[30] = r[11];
                r[5] += r[8];
                r[29] = Word(r[31] + 268);
                r[8] = Shift(r[7], 1);
                r[28] = Word(r[31] + 272);
                r[5] = Shift(r[5], 2);
                r[8] += r[7];
                r[7] = Shift(r[6], 1);
                r[27] = Shift(r[8], 2);
                r[6] += r[7];
                r[8] = Word(r[5] + r[9]);
                ++r[11];
                r[6] = Shift(r[6], 2);
                r[10] += 48;
                r[7] = Word(r[27] + r[9]);
                r[6] = Word(r[6] + r[9]);
                r[9] = Shift(r[8], 2);
                r[8] = Shift(r[7], 2);
                r[7] = Shift(r[6], 2);
                r[6] = Word(r[9] + r[29]);
                r[6] = Shift(r[6], 2);
                Word(r[6] + r[28], r[4]);
                r[6] = Word(r[31] + 268);
                r[5] = Word(r[9] + r[6]);
                ++r[5];
                Word(r[9] + r[6], r[5]);
                r[9] = Word(r[31] + 268);
                r[6] = Word(r[31] + 272);
                r[9] = Word(r[8] + r[9]);
                r[9] = Shift(r[9], 2);
                Word(r[9] + r[6], r[3]);
                r[9] = Word(r[31] + 268);
                r[6] = Word(r[8] + r[9]);
                ++r[6];
                Word(r[8] + r[9], r[6]);
                r[9] = Word(r[31] + 268);
                r[8] = Word(r[31] + 272);
                r[9] = Word(r[7] + r[9]);
                r[9] = Shift(r[9], 2);
                Word(r[9] + r[8], r[30]);
                r[9] = Word(r[31] + 268);
                r[8] = Word(r[7] + r[9]);
                ++r[8];
                Word(r[7] + r[9], r[8]);
                r[9] = Word(r[31] + 224);
                Compare(r[11], r[9]);
            } while (s.cr6.lt);
        }
        return true;
    }
    bool Body() {
        auto &r = s.r;
        r[31] = r[3];
        r[11] = m.ReadU8(Address(r[31] + 283));
        Compare(r[11]);
        if (s.cr6.eq) {
            r[11] = m.ReadU8(Address(r[31] + 282));
            Compare(r[11]);
            if (s.cr6.eq)
                return true;
        }
        for (unsigned offset : {212u, 228u, 252u}) {
            r[11] = Word(r[31] + offset);
            Compare(r[11]);
            if (s.cr6.eq)
                return false;
        }
        r[27] = Word(r[31] + 248);
        Compare(r[27]);
        if (s.cr6.eq)
            return false;
        r[29] = Word(r[31] + 236);
        Compare(r[29]);
        if (s.cr6.eq)
            return false;
        r[11] = Word(r[31] + 224);
        r[28] = 0;
        Compare(r[11]);
        if (s.cr6.gt) {
            r[10] = 0xffffffff82000000ull;
            r[11] = 0xffffffff82000000ull;
            r[30] = r[27] + 40;
            Load(30, r[10] + 30596);
            Load(31, r[11] + 3664);
            do {
                FaceNormal();
                r[11] = Word(r[31] + 224);
                ++r[28];
                r[30] += 48;
                Compare(r[28], r[11]);
            } while (s.cr6.lt);
        }
        if (!AllocateCounts())
            return false;
        CountIncidence();
        Prefix(false);
        if (!Scatter())
            return false;
        Prefix(true);
        return true;
    }
    void Run() {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = 0x82bbed28u;
        for (unsigned i = 27; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        Word(r[1] - 8, r[12]);
        Gradual();
        recovery_abi::WriteU64(m, Address(r[1] - 64), s.fpr_bits[30]);
        recovery_abi::WriteU64(m, Address(r[1] - 56), s.fpr_bits[31]);
        auto old = r[1];
        r[1] -= 144;
        Word(r[1], old);
        r[3] = Body() ? 1 : 0;
        r[1] += 144;
        Gradual();
        s.fpr_bits[30] = recovery_abi::ReadU64(m, Address(r[1] - 64));
        s.fpr_bits[31] = recovery_abi::ReadU64(m, Address(r[1] - 56));
        for (unsigned i = 27; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    if (e != 0x82bbed20u)
        return false;
    Normals{m, d, s}.Run();
    return true;
}
} // namespace lo::semantic::gpu::mesh_indexed_normals61
