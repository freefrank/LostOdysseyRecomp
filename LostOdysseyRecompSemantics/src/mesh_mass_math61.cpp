#include "lo_semantics/mesh_mass_math61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::mesh_mass_math61 {
namespace {
using recovery_abi::Address;
constexpr std::uint64_t Constants = 0xffffffff82000000ull;
struct MassMath {
    GuestMemory &m;
    float_triplet_transfer::NativeServices &native;
    Registers &s;
    double F(unsigned i) { return std::bit_cast<double>(s.fpr_bits[i]); }
    void Set(unsigned i, double v) { s.fpr_bits[i] = std::bit_cast<std::uint64_t>(v); }
    void Single(unsigned i, double v) { Set(i, double(float(v))); }
    void Gradual() {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            native.SetHostFpControl(s.cached_fp_control);
        }
    }
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Load(unsigned i, std::uint64_t p) {
        Gradual();
        Set(i, double(std::bit_cast<float>(Word(p))));
    }
    void Double(unsigned i, std::uint64_t p) {
        Gradual();
        s.fpr_bits[i] = recovery_abi::ReadU64(m, Address(p));
    }
    void Store(unsigned i, std::uint64_t p) {
        recovery_abi::WriteU64(m, Address(p), s.fpr_bits[i]);
    }
    void Neg(unsigned i) { s.fpr_bits[i] ^= 0x8000000000000000ull; }
    void Compare(double a, double b) {
        bool u = std::isnan(a) || std::isnan(b);
        s.cr6 = {std::uint8_t(!u && a < b), std::uint8_t(!u && a > b), std::uint8_t(!u && a == b),
                 std::uint8_t(u)};
    }
    static std::uint64_t Product(std::uint64_t a, std::uint64_t b) {
        return std::uint64_t(std::int64_t(std::int32_t(Address(a))) *
                             std::int64_t(std::int32_t(Address(b))));
    }
    void SaveFloats(unsigned first) {
        Gradual();
        for (unsigned i = first; i < 32; ++i)
            Store(i, s.r[12] - 8 * (32 - i));
    }
    void RestoreFloats(unsigned first) {
        Gradual();
        for (unsigned i = first; i < 32; ++i)
            Double(i, s.r[12] - 8 * (32 - i));
    }
    void ProjectedMoments() {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = 0x82bcce50u;
        for (unsigned i = 29; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        m.WriteU32(Address(r[1] - 8), Address(r[12]));
        r[12] = r[1] - 32;
        s.lr = 0x82bcce58u;
        SaveFloats(15);
        r[9] = Word(r[3] + 100);
        r[11] = Constants;
        r[29] = 0xffffffff820d0000ull;
        r[10] = Word(r[3] + 96);
        r[5] = Address(r[9]) << 2;
        r[8] = Word(r[3] + 80);
        r[31] = 0xffffffff82050000ull;
        r[7] = Word(r[3] + 72);
        r[9] = Constants;
        r[30] = 0xffffffffaaaa0000ull;
        Double(0, r[11] + 4072);
        r[6] = Address(r[10]) << 2;
        Double(5, r[29] + 27320);
        Store(0, r[3] + 184);
        r[11] = 0;
        Double(6, r[31] + 5168);
        r[10] = r[4] + 32;
        Double(1, r[9] + 4112);
        r[30] |= 43691;
        for (unsigned offset = 176;; offset -= 8) {
            Store(0, r[3] + offset);
            if (offset == 112)
                break;
        }
        do {
            // Integrate each directed projected edge. Cubic polynomial terms
            // also supply the mixed moments needed by the face integrator.
            r[9] = Word(r[10]);
            ++r[11];
            Double(25, r[3] + 112);
            r[10] += 4;
            r[9] = Product(r[7], r[9]);
            Double(23, r[3] + 120);
            Double(21, r[3] + 160);
            Double(22, r[3] + 136);
            Double(19, r[3] + 144);
            Double(18, r[3] + 168);
            Double(20, r[3] + 184);
            Double(17, r[3] + 176);
            r[9] += r[8];
            auto count = Address(r[11]);
            s.cr6 = {std::uint8_t(count < 3), std::uint8_t(count > 3), std::uint8_t(count == 3),
                     s.xer_so};
            Load(11, r[5] + r[9]);
            Load(12, r[6] + r[9]);
            r[9] = (std::uint64_t(Address(r[11])) * Address(r[30])) >> 32;
            Set(9, F(12) * F(12));
            Set(4, F(11) * F(11));
            Set(3, F(9) * F(12));
            Set(8, F(4) * F(11));
            r[9] = Address(r[9]) >> 1;
            r[31] = Address(r[9]) << 1;
            r[9] += r[31];
            r[9] = r[11] - r[9];
            r[9] += 8;
            r[9] = Address(r[9]) << 2;
            r[9] = Word(r[9] + r[4]);
            r[9] = Product(r[9], r[7]);
            r[9] += r[8];
            Load(0, r[6] + r[9]);
            Set(28, F(0) * F(12));
            Load(13, r[5] + r[9]);
            Set(10, F(13) - F(11));
            Set(31, F(0) + F(12));
            Set(2, F(0) * F(0));
            Set(29, F(13) + F(11));
            Set(30, F(13) * F(13));
            Set(7, F(0) - F(12));
            Set(28, F(28) * F(1));
            Set(24, F(31) * F(10) + F(25));
            Store(24, r[3] + 112);
            Set(26, F(31) * F(0) + F(9));
            Set(29, F(29) * F(13) + F(4));
            Set(27, F(30) * F(13));
            Set(30, F(30) * F(11));
            Set(4, F(4) * F(13));
            Set(25, F(2) * F(6) + F(28));
            Set(28, F(9) * F(6) + F(28));
            Set(31, F(29) * F(13) + F(8));
            Set(9, F(25) + F(9));
            Set(25, F(26) * F(0) + F(3));
            Set(26, F(26) * F(10) + F(23));
            Store(26, r[3] + 120);
            Set(28, F(28) + F(2));
            Set(2, F(2) * F(0));
            Set(15, F(31) * F(13));
            Set(16, F(9) * F(12));
            Set(23, F(25) * F(0));
            Set(25, F(25) * F(10) + F(22));
            Double(22, r[3] + 128);
            Store(25, r[3] + 136);
            Set(2, F(2) * F(5) + F(16));
            Set(23, F(3) * F(12) + F(23));
            Set(3, F(3) * F(5));
            Set(16, F(8) * F(11) + F(15));
            Set(23, F(23) * F(10) + F(21));
            Double(21, r[3] + 152);
            Set(3, F(28) * F(0) + F(3));
            Store(23, r[3] + 160);
            Set(28, F(28) * F(11));
            Set(3, F(3) * F(11));
            Set(28, F(9) * F(13) + F(28));
            Set(9, F(31) * F(7) + F(21));
            Set(11, F(29) * F(7) + F(22));
            Set(31, F(30) * F(1));
            Set(2, F(2) * F(13) + F(3));
            Set(3, F(28) * F(10) + F(19));
            Set(13, F(16) * F(7) + F(20));
            Set(10, F(2) * F(10) + F(18));
            Set(2, F(30) * F(6));
            Set(2, F(4) * F(1) + F(2));
            Store(11, r[3] + 128);
            Set(4, F(4) * F(6) + F(31));
            Store(9, r[3] + 152);
            Store(13, r[3] + 184);
            Store(3, r[3] + 144);
            Store(10, r[3] + 168);
            Set(2, F(27) * F(5) + F(2));
            Set(4, F(8) * F(5) + F(4));
            Set(8, F(2) + F(8));
            Set(4, F(4) + F(27));
            Set(0, F(8) * F(0));
            Set(0, F(4) * F(12) + F(0));
            Set(0, F(0) * F(7) + F(17));
            Store(0, r[3] + 176);
        } while (s.cr6.lt);
        // Normalize projected monomials by their degree-dependent divisors.
        r[11] = Constants;
        Double(12, r[11] + 3952);
        r[11] = 0xffffffff820d0000ull;
        Set(8, F(24) * F(12));
        Store(8, r[3] + 112);
        Double(12, r[11] + 27312);
        r[11] = 0xffffffff820d0000ull;
        Set(7, F(26) * F(12));
        Store(7, r[3] + 120);
        Double(12, r[11] + 27304);
        r[11] = 0xffffffff82050000ull;
        Set(8, F(25) * F(12));
        Store(8, r[3] + 136);
        Double(12, r[11] - 32624);
        r[11] = 0xffffffff820d0000ull;
        Set(7, F(23) * F(12));
        Store(7, r[3] + 160);
        Double(12, r[11] + 27296);
        r[11] = 0xffffffff820d0000ull;
        Set(11, F(11) * F(12));
        Store(11, r[3] + 128);
        Double(12, r[11] + 27288);
        r[11] = 0xffffffff820d0000ull;
        Set(9, F(9) * F(12));
        Store(9, r[3] + 152);
        Double(12, r[11] + 27280);
        r[11] = 0xffffffff820d0000ull;
        Set(12, F(13) * F(12));
        Store(12, r[3] + 184);
        Double(13, r[11] + 27272);
        r[11] = 0xffffffff820d0000ull;
        Set(11, F(3) * F(13));
        Store(11, r[3] + 144);
        Double(13, r[11] + 27264);
        r[11] = 0xffffffff820d0000ull;
        Set(12, F(10) * F(13));
        Store(12, r[3] + 168);
        Double(13, r[11] + 27256);
        Set(0, F(0) * F(13));
        Store(0, r[3] + 176);
        r[12] = r[1] - 32;
        s.lr = 0x82bcd0f4u;
        RestoreFloats(15);
        for (unsigned i = 29; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
    void FaceMoments() {
        auto &r = s.r;
        r[12] = s.lr;
        m.WriteU32(Address(r[1] - 8), Address(r[12]));
        r[12] = r[1] - 8;
        s.lr = 0x82bcd108u;
        SaveFloats(23);
        auto old = r[1];
        r[1] -= 160;
        m.WriteU32(Address(r[1]), Address(old));
        s.lr = 0x82bcd110u;
        ProjectedMoments();
        // Recover the suppressed coordinate from the face plane. Reciprocal
        // normal powers convert projected monomials into surface moments.
        r[11] = Word(r[3] + 104);
        r[10] = Word(r[3] + 96);
        Double(13, r[4] + 24);
        r[9] = Address(r[11]) << 3;
        Double(12, r[3] + 112);
        Double(10, r[3] + 120);
        r[10] = Address(r[10]) << 3;
        Set(30, F(12) * F(13));
        Double(8, r[3] + 128);
        r[11] = Word(r[3] + 100);
        Double(6, r[3] + 152);
        Double(7, r[3] + 136);
        Double(11, r[9] + r[4]);
        r[9] = Constants;
        r[11] = Address(r[11]) << 3;
        Double(4, r[3] + 160);
        Double(3, r[3] + 184);
        Double(5, r[3] + 144);
        Double(1, r[3] + 176);
        Double(9, r[9] + 4112);
        r[9] = Constants;
        Double(31, r[3] + 168);
        Double(0, r[9] + 3880);
        r[9] = 0xffffffff82050000ull;
        Set(0, F(0) / F(11));
        Double(2, r[9] + 5168);
        Set(12, F(10) * F(0));
        Store(12, r[3] + 192);
        Set(12, F(8) * F(0));
        Store(12, r[3] + 200);
        Double(12, r[10] + r[4]);
        Set(29, F(0) * F(0));
        Set(12, F(10) * F(12));
        Double(28, r[11] + r[4]);
        Set(11, F(7) * F(0));
        Store(11, r[3] + 216);
        Set(27, F(6) * F(0));
        Store(27, r[3] + 224);
        Set(27, F(4) * F(0));
        Set(26, F(3) * F(0));
        Set(12, F(28) * F(8) + F(12));
        Set(28, F(29) * F(0));
        Set(12, F(12) + F(30));
        Set(12, F(12) * F(29));
        Neg(12);
        Store(12, r[3] + 208);
        Double(12, r[11] + r[4]);
        Double(11, r[10] + r[4]);
        Set(25, F(12) * F(12));
        Store(27, r[3] + 240);
        Set(27, F(11) * F(10));
        Store(26, r[3] + 248);
        Set(26, F(11) * F(11));
        Set(11, F(11) * F(12));
        Set(12, F(12) * F(8) + F(27));
        Set(27, F(26) * F(7));
        Set(11, F(11) * F(5));
        Set(12, F(12) * F(9) + F(30));
        Set(12, F(12) * F(13) + F(27));
        Set(12, F(25) * F(6) + F(12));
        Set(12, F(11) * F(9) + F(12));
        Set(12, F(12) * F(28));
        Store(12, r[3] + 232);
        Double(11, r[10] + r[4]);
        Set(25, F(11) * F(10));
        Double(12, r[11] + r[4]);
        Set(27, F(12) * F(12));
        Set(24, F(11) * F(12));
        Set(26, F(11) * F(11));
        Set(8, F(12) * F(8) + F(25));
        Set(25, F(27) * F(6));
        Set(23, F(26) * F(12));
        Set(12, F(27) * F(12));
        Set(8, F(8) * F(2) + F(30));
        Set(30, F(24) * F(5));
        Set(24, F(27) * F(11));
        Set(11, F(26) * F(11));
        Set(30, F(30) * F(9) + F(25));
        Set(27, F(24) * F(1));
        Set(25, F(28) * F(0));
        Set(0, F(31) * F(0));
        Store(0, r[3] + 264);
        Set(0, F(26) * F(7) + F(30));
        Set(30, F(23) * F(31) + F(27));
        Set(6, F(6) * F(13));
        Set(10, F(10) * F(13));
        Set(0, F(0) * F(2));
        Set(2, F(30) * F(2));
        Set(0, F(8) * F(13) + F(0));
        Set(0, F(0) * F(13) + F(2));
        Set(0, F(11) * F(4) + F(0));
        Set(0, F(12) * F(3) + F(0));
        Set(0, F(0) * F(25));
        Neg(0);
        Store(0, r[3] + 256);
        Double(0, r[10] + r[4]);
        Set(0, F(1) * F(0) + F(6));
        Double(12, r[11] + r[4]);
        Set(0, F(12) * F(3) + F(0));
        Set(0, F(0) * F(29));
        Neg(0);
        Store(0, r[3] + 272);
        Double(12, r[10] + r[4]);
        Set(11, F(12) * F(7));
        Double(0, r[11] + r[4]);
        Set(7, F(12) * F(12));
        Set(8, F(0) * F(0));
        Set(12, F(12) * F(0));
        Set(0, F(0) * F(5) + F(11));
        Set(11, F(8) * F(1));
        Set(12, F(12) * F(31));
        Set(0, F(0) * F(9) + F(10));
        Set(0, F(0) * F(13) + F(11));
        Set(0, F(7) * F(4) + F(0));
        Set(0, F(12) * F(9) + F(0));
        Set(0, F(0) * F(28));
        Store(0, r[3] + 280);
        r[1] += 160;
        r[12] = r[1] - 8;
        s.lr = 0x82bcd2f0u;
        RestoreFloats(23);
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
    void IntegerCompare(std::uint64_t a, std::uint64_t b = 0) {
        auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    void WordStore(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    void SingleStore(unsigned i, std::uint64_t p) {
        m.WriteU32(Address(p), std::bit_cast<std::uint32_t>(float(F(i))));
    }
    void AccumulateFace() {
        auto &r = s.r;
        const unsigned axes[]{Word(r[3] + 96), Word(r[3] + 100), Word(r[3] + 104)};
        unsigned xSlot = axes[0] == 0 ? 0 : axes[1] == 0 ? 1 : 2;
        Double(0, r[3] + 192 + 8 * xSlot);
        Double(13, r[1] + 80);
        Double(12, r[3] + 288);
        Set(0, F(0) * F(13) + F(12));
        Store(0, r[3] + 288);
        // Divergence-theorem contributions are grouped by selected projection
        // axes, then normalized after the final triangle. Preserve sum order.
        for (unsigned order = 0; order < 3; ++order)
            for (unsigned i = 0; i < 3; ++i) {
                Double(0, r[3] + 216 + 24 * order + 8 * i);
                Double(13, r[1] + 80 + 8 * axes[i]);
                Double(12, r[3] + 296 + 24 * order + 8 * axes[i]);
                Set(0, F(13) * F(0) + F(12));
                Store(0, r[3] + 296 + 24 * order + 8 * axes[i]);
            }
        // These volatile register values survive into the final tensor writer.
        r[5] = r[1] + 80;
        r[6] = r[1] + 80;
        r[7] = axes[1] * 8u;
        r[26] = axes[1] * 8u;
        ++r[30];
    }
    void FinalizeVolume() {
        auto &r = s.r;
        r[11] = Constants;
        Double(12, r[3] + 296);
        Double(6, r[3] + 320);
        Double(5, r[3] + 328);
        Double(4, r[3] + 336);
        Double(11, r[3] + 304);
        Load(13, r[11] + 3664);
        r[11] = Constants;
        Double(3, r[3] + 344);
        Double(2, r[3] + 360);
        Double(7, r[3] + 288);
        Compare(F(7), F(31));
        Double(0, r[11] + 3952);
        r[11] = 0xffffffff820d0000ull;
        Set(10, F(12) * F(0));
        Double(12, r[3] + 312);
        Set(8, F(12) * F(0));
        Store(10, r[3] + 296);
        Set(9, F(11) * F(0));
        Store(8, r[3] + 312);
        Set(3, F(3) * F(0));
        Store(9, r[3] + 304);
        Double(12, r[11] + 27328);
        Set(2, F(2) * F(0));
        Set(6, F(6) * F(12));
        Store(3, r[3] + 344);
        Set(5, F(5) * F(12));
        Store(6, r[3] + 320);
        Set(12, F(4) * F(12));
        Double(4, r[3] + 352);
        Set(4, F(4) * F(0));
        Store(5, r[3] + 328);
        Store(12, r[3] + 336);
        Set(11, F(13));
        Store(4, r[3] + 352);
        Set(0, F(13));
        Store(2, r[3] + 360);
        if (!s.cr6.eq) {
            r[11] = Constants;
            Double(0, r[11] + 3880);
            Set(0, F(0) / F(7));
            Set(13, F(0) * F(10));
            Set(12, F(0) * F(9));
            Set(0, F(0) * F(8));
            Single(13, F(13));
            Single(11, F(12));
            Single(0, F(0));
        }
        Gradual();
        SingleStore(13, r[28]);
        r[10] = r[28] + 24;
        SingleStore(11, r[28] + 4);
        r[11] = r[1] + 80;
        SingleStore(0, r[28] + 8);
        r[9] = 9;
        Double(0, r[3] + 56);
        Double(10, r[3] + 344);
        Double(13, r[3] + 336);
        Set(10, F(0) * F(10));
        Double(12, r[3] + 328);
        Double(11, r[3] + 320);
        Set(7, F(13) + F(12));
        Double(9, r[3] + 352);
        Set(6, F(13) + F(11));
        Double(8, r[3] + 360);
        Set(5, F(12) + F(11));
        Set(9, F(0) * F(9));
        Set(8, F(0) * F(8));
        s.fpr_bits[13] = s.fpr_bits[10] ^ 0x8000000000000000ull;
        Store(13, r[1] + 104);
        Store(13, r[1] + 88);
        Set(10, F(7) * F(0));
        Store(10, r[1] + 80);
        Set(10, F(6) * F(0));
        Store(10, r[1] + 112);
        Set(0, F(5) * F(0));
        Store(0, r[1] + 144);
        s.fpr_bits[12] = s.fpr_bits[9] ^ 0x8000000000000000ull;
        Store(12, r[1] + 136);
        s.fpr_bits[11] = s.fpr_bits[8] ^ 0x8000000000000000ull;
        Store(12, r[1] + 120);
        Store(11, r[1] + 96);
        Store(11, r[1] + 128);
        s.ctr = r[9];
        do {
            r[9] = recovery_abi::ReadU64(m, Address(r[11]));
            r[11] += 8;
            recovery_abi::WriteU64(m, Address(r[10]), r[9]);
            r[10] += 8;
            --s.ctr;
        } while (Address(s.ctr));
        r[4] = r[28] + 96;
        s.lr = 0x82bcd88cu;
        Inertia();
        Double(0, r[3] + 48);
        Store(0, r[28] + 16);
        r[3] = 1;
    }
    void VolumeBody() {
        auto &r = s.r;
        r[11] = 0xffffffff832e0000ull;
        r[28] = r[4];
        r[11] = Word(r[11] - 2744);
        IntegerCompare(r[11]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        r[11] = Constants;
        r[31] = Word(r[3] + 84);
        r[27] = 0;
        r[30] = r[27];
        Double(31, r[11] + 4072);
        r[11] = Word(r[3] + 68);
        Store(31, r[3] + 360);
        Store(31, r[3] + 352);
        IntegerCompare(r[11]);
        for (unsigned offset = 344;; offset -= 8) {
            Store(31, r[3] + offset);
            if (offset == 288)
                break;
        }
        if (s.cr6.gt) {
            r[11] = 0x55550000u;
            r[29] = r[11] | 21846u;
            do {
                r[9] = Word(r[3] + 88);
                r[11] = r[9] & 2u;
                IntegerCompare(r[11]);
                if (!s.cr6.eq) {
                    r[11] = m.ReadU16(Address(r[31]));
                    r[10] = m.ReadU16(Address(r[31] + 4));
                    WordStore(r[1] + 112, r[11]);
                    r[11] = m.ReadU16(Address(r[31] + 2));
                } else {
                    r[11] = Word(r[31]);
                    r[10] = Word(r[31] + 8);
                    WordStore(r[1] + 112, r[11]);
                    r[11] = Word(r[31] + 4);
                }
                r[9] &= 1u;
                WordStore(r[1] + 120, r[10]);
                WordStore(r[1] + 116, r[11]);
                IntegerCompare(r[9]);
                if (!s.cr6.eq) {
                    WordStore(r[1] + 116, r[10]);
                    WordStore(r[1] + 120, r[11]);
                }
                r[5] = r[1] + 112;
                r[4] = r[1] + 80;
                s.lr = 0x82bcd4e0u;
                TrianglePlane();
                Double(13, r[1] + 88);
                Double(0, r[1] + 80);
                s.fpr_bits[12] = s.fpr_bits[13] & 0x7fffffffffffffffull;
                s.fpr_bits[0] &= 0x7fffffffffffffffull;
                Double(13, r[1] + 96);
                s.fpr_bits[13] &= 0x7fffffffffffffffull;
                Compare(F(0), F(12));
                bool xLargest = false;
                if (s.cr6.gt) {
                    Compare(F(0), F(13));
                    xLargest = s.cr6.gt;
                }
                if (xLargest)
                    WordStore(r[3] + 104, r[27]);
                else {
                    Gradual();
                    Compare(F(12), F(13));
                    r[11] = s.cr6.gt ? 1 : 2;
                    WordStore(r[3] + 104, r[11]);
                }
                // Dominant normal axis keeps reciprocal projection well scaled.
                r[11] = Word(r[3] + 104);
                r[4] = r[1] + 80;
                auto a = (Address(r[11]) + 1) % 3, b = (a + 1) % 3;
                WordStore(r[3] + 96, a);
                WordStore(r[3] + 100, b);
                s.lr = 0x82bcd570u;
                FaceMoments();
                AccumulateFace();
                r[11] = Word(r[3] + 76);
                r[10] = Word(r[3] + 68);
                r[31] += r[11];
                IntegerCompare(r[30], r[10]);
            } while (s.cr6.lt);
        }
        FinalizeVolume();
    }
    void Volume() {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = 0x82bcd408u;
        for (unsigned i = 26; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        WordStore(r[1] - 8, r[12]);
        Gradual();
        Store(31, r[1] - 64);
        auto old = r[1];
        r[1] -= 224;
        WordStore(r[1], old);
        VolumeBody();
        r[1] += 224;
        Double(31, r[1] - 64);
        for (unsigned i = 26; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
    void DescriptorVolume() {
        auto &r = s.r;
        r[12] = s.lr;
        WordStore(r[1] - 8, r[12]);
        auto old = r[1];
        r[1] -= 464;
        WordStore(r[1], old);
        r[9] = Constants;
        Gradual();
        Store(1, r[1] + 136);
        r[11] = r[3];
        r[10] = r[1] + 144;
        Double(0, r[9] + 4072);
        r[9] = 7;
        Store(0, r[1] + 128);
        s.ctr = r[9];
        do {
            r[9] = Word(r[11]);
            r[11] += 4;
            WordStore(r[10], r[9]);
            r[10] += 4;
            --s.ctr;
        } while (Address(s.ctr));
        r[4] = r[5];
        r[3] = r[1] + 80;
        s.lr = 0x82bcd8f4u;
        Volume();
        r[1] += 464;
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
    void TrianglePlane() {
        auto &r = s.r;
        r[9] = Word(r[3] + 72);
        r[11] = Word(r[5] + 4);
        r[7] = Word(r[5]);
        r[8] = Word(r[5] + 8);
        r[11] = Product(r[11], r[9]);
        r[10] = Word(r[3] + 80);
        r[6] = Product(r[7], r[9]);
        r[8] = Product(r[8], r[9]);
        r[11] += r[10];
        r[9] = r[6] + r[10];
        r[8] += r[10];
        // Edges are first->second and second->third. Keep binary32 rounding.
        Load(11, r[11]);
        Load(0, r[9]);
        Load(7, r[11] + 4);
        Single(0, F(11) - F(0));
        Load(13, r[9] + 4);
        Load(6, r[11] + 8);
        Single(11, F(7) - F(13));
        Load(12, r[9] + 8);
        Load(7, r[11]);
        Single(13, F(6) - F(12));
        Load(10, r[8]);
        Load(6, r[11] + 4);
        Single(12, F(10) - F(7));
        Load(9, r[8] + 4);
        Load(7, r[11] + 8);
        Single(10, F(9) - F(6));
        Load(8, r[8] + 8);
        r[11] = Constants;
        Single(9, F(8) - F(7));
        Single(7, F(12) * F(11));
        Single(6, F(10) * F(13));
        Single(8, F(9) * F(0));
        Single(13, F(13) * F(12) - F(8));
        Single(12, F(10) * F(0) - F(7));
        Load(10, r[11] + 3664);
        Single(0, F(9) * F(11) - F(6));
        Single(11, F(13) * F(13));
        Single(11, F(12) * F(12) + F(11));
        Single(11, F(0) * F(0) + F(11));
        Single(11, std::sqrt(F(11)));
        Compare(F(11), F(10));
        if (!s.cr6.eq) {
            r[11] = Constants;
            Load(10, r[11] + 30596);
            Single(11, F(10) / F(11));
            Single(0, F(11) * F(0));
            Single(13, F(11) * F(13));
            Single(12, F(11) * F(12));
        }
        r[11] = Word(r[3] + 72);
        r[11] = Product(r[11], r[7]);
        r[11] += r[10];
        Load(11, r[11] + 4);
        Single(11, F(11) * F(13));
        Load(10, r[11] + 8);
        Load(9, r[11]);
        Store(13, r[4] + 8);
        Store(0, r[4]);
        Store(12, r[4] + 16);
        Single(13, F(10) * F(12) + F(11));
        Single(0, -(F(9) * F(0) + F(13)));
        Store(0, r[4] + 24);
    }
    void Inertia() {
        auto &r = s.r;
        r[11] = Constants;
        Double(12, r[3] + 288);
        Double(0, r[3] + 56);
        r[10] = r[4];
        Load(13, r[11] + 3664);
        r[11] = Constants;
        Set(11, F(13));
        Set(10, F(13));
        Double(9, r[11] + 4072);
        Compare(F(12), F(9));
        Set(9, F(0) * F(12));
        Store(9, r[3] + 48);
        if (!s.cr6.eq) {
            r[11] = Constants;
            Double(11, r[3] + 304);
            Double(10, r[3] + 312);
            Double(13, r[11] + 3880);
            Set(13, F(13) / F(12));
            Double(12, r[3] + 296);
            Set(11, F(11) * F(13));
            Set(10, F(10) * F(13));
            Set(12, F(12) * F(13));
            Single(11, F(11));
            Single(10, F(10));
            Single(13, F(12));
        }
        // Origin tensor from integrated second moments and density.
        Double(4, r[3] + 360);
        Single(12, F(9));
        Set(4, F(4) * F(0));
        Double(6, r[3] + 344);
        Double(5, r[3] + 352);
        Set(6, F(6) * F(0));
        Set(5, F(5) * F(0));
        Double(9, r[3] + 336);
        Double(8, r[3] + 328);
        r[9] = r[1] - 160;
        Double(7, r[3] + 320);
        r[11] = r[1] - 80;
        r[8] = 9;
        Neg(4);
        Store(4, r[1] - 64);
        Store(4, r[1] - 32);
        Set(4, F(8) + F(9));
        Neg(6);
        Store(6, r[1] - 56);
        Neg(5);
        Store(6, r[1] - 72);
        Set(9, F(7) + F(9));
        Store(5, r[1] - 24);
        Set(8, F(7) + F(8));
        Store(5, r[1] - 40);
        Single(6, F(12) * F(13));
        Single(5, F(12) * F(11));
        Single(12, F(12) * F(10));
        Set(7, F(4) * F(0));
        Store(7, r[1] - 80);
        Set(9, F(9) * F(0));
        Store(9, r[1] - 48);
        Set(0, F(8) * F(0));
        Store(0, r[1] - 16);
        Single(0, F(13) * F(6));
        Single(9, F(11) * F(5));
        Single(8, F(10) * F(12));
        s.ctr = r[8];
        do {
            r[8] = recovery_abi::ReadU64(m, Address(r[11]));
            r[11] += 8;
            recovery_abi::WriteU64(m, Address(r[9]), r[8]);
            r[9] += 8;
            --s.ctr;
        } while (Address(s.ctr));
        // Shift the symmetric tensor to the centroid; binary32 products feed
        // binary64 matrix additions, matching the original mixed precision.
        Gradual();
        Single(11, F(6) * F(11));
        r[11] = r[1] - 160;
        Single(12, F(13) * F(12));
        Double(13, r[1] - 104);
        Single(6, F(8) + F(9));
        r[9] = 9;
        Single(8, F(0) + F(8));
        Single(9, F(0) + F(9));
        Double(0, r[1] - 136);
        Single(10, F(5) * F(10));
        Set(0, F(11) + F(0));
        Double(11, r[1] - 144);
        Set(12, F(12) + F(11));
        Store(0, r[1] - 136);
        Set(11, F(7) - F(6));
        Store(11, r[1] - 160);
        Double(11, r[1] - 128);
        Set(11, F(11) - F(8));
        Store(11, r[1] - 128);
        Double(11, r[1] - 96);
        Set(13, F(10) + F(13));
        Set(11, F(11) - F(9));
        Store(13, r[1] - 104);
        Store(12, r[1] - 144);
        Store(11, r[1] - 96);
        Store(0, r[1] - 152);
        Store(13, r[1] - 120);
        Store(12, r[1] - 112);
        s.ctr = r[9];
        do {
            r[9] = recovery_abi::ReadU64(m, Address(r[11]));
            r[11] += 8;
            recovery_abi::WriteU64(m, Address(r[10]), r[9]);
            r[10] += 8;
            --s.ctr;
        } while (Address(s.ctr));
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, float_triplet_transfer::NativeServices &native,
           Registers &s) {
    MassMath math{m, native, s};
    switch (e) {
    case 0x82bcce48u:
        math.ProjectedMoments();
        return true;
    case 0x82bcd0f8u:
        math.FaceMoments();
        return true;
    case 0x82bcd400u:
        math.Volume();
        return true;
    case 0x82bcd8a8u:
        math.DescriptorVolume();
        return true;
    case 0x82bcd300u:
        math.TrianglePlane();
        return true;
    case 0x82bccca8u:
        math.Inertia();
        return true;
    default:
        return false;
    }
}
} // namespace lo::semantic::gpu::mesh_mass_math61
