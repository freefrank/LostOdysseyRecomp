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
