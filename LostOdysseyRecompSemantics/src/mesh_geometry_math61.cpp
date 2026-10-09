#include "lo_semantics/mesh_geometry_math61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::mesh_geometry_math61 {
namespace {
using recovery_abi::Address;
struct Math {
    GuestMemory &m;
    float_triplet_transfer::NativeServices &native;
    Registers &s;
    double F(unsigned i) { return std::bit_cast<double>(s.fpr_bits[i]); }
    void Set(unsigned i, double x) { s.fpr_bits[i] = std::bit_cast<std::uint64_t>(x); }
    void Single(unsigned i, double x) { Set(i, double(float(x))); }
    void Gradual() {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            native.SetHostFpControl(s.cached_fp_control);
        }
    }
    void Load(unsigned i, std::uint64_t p) {
        Gradual();
        Set(i, double(std::bit_cast<float>(m.ReadU32(Address(p)))));
    }
    void Double(unsigned i, std::uint64_t p) {
        Gradual();
        s.fpr_bits[i] = recovery_abi::ReadU64(m, Address(p));
    }
    void Store(unsigned i, std::uint64_t p) {
        m.WriteU32(Address(p), std::bit_cast<std::uint32_t>(float(F(i))));
    }
    void Compare(double a, double b) {
        bool u = std::isnan(a) || std::isnan(b);
        s.cr6 = {std::uint8_t(!u && a < b), std::uint8_t(!u && a > b), std::uint8_t(!u && a == b),
                 std::uint8_t(u)};
    }
    void Sign(unsigned reg) {
        auto v = Address(s.r[reg]) & 0x80000000u;
        s.r[reg] = v;
        s.cr0 = {std::uint8_t(v != 0), 0u, std::uint8_t(v == 0), s.xer_so};
    }
    void Neg(unsigned i) { s.fpr_bits[i] ^= 0x8000000000000000ull; }
    void Plane() {
        auto &r = s.r;
        Load(0, r[4]);
        r[11] = 0xffffffff82000000ull;
        Load(13, r[4] + 4);
        Load(11, r[4] + 8);
        Load(9, r[5] + 4);
        Load(8, r[5] + 8);
        Single(9, F(9) - F(13));
        Load(10, r[6]);
        Single(8, F(8) - F(11));
        Load(6, r[6] + 8);
        Single(10, F(10) - F(0));
        Load(12, r[5]);
        Single(11, F(6) - F(11));
        Single(12, F(12) - F(0));
        Load(7, r[6] + 4);
        Single(0, F(7) - F(13));
        Single(13, F(10) * F(9));
        Single(7, F(11) * F(12));
        Single(6, F(0) * F(8));
        Single(0, F(12) * F(0) - F(13));
        Store(0, r[3] + 8);
        Single(13, F(10) * F(8) - F(7));
        Load(10, r[11] + 3664);
        Single(12, F(11) * F(9) - F(6));
        Store(13, r[3] + 4);
        Store(12, r[3]);
        Single(11, F(13) * F(13));
        Single(11, F(0) * F(0) + F(11));
        Single(11, F(12) * F(12) + F(11));
        Compare(F(11), F(10));
        if (!s.cr6.eq) {
            Single(10, std::sqrt(F(11)));
            r[11] = 0xffffffff82000000ull;
            Load(11, r[11] + 30596);
            Single(11, F(11) / F(10));
            Single(12, F(12) * F(11));
            Store(12, r[3]);
            Single(13, F(13) * F(11));
            Store(13, r[3] + 4);
            Single(0, F(0) * F(11));
            Store(0, r[3] + 8);
        }
        Load(0, r[4]);
        Load(13, r[3]);
        Single(0, F(0) * F(13));
        Load(13, r[4] + 8);
        Load(12, r[3] + 8);
        Load(11, r[4] + 4);
        Load(10, r[3] + 4);
        Single(0, F(13) * F(12) + F(0));
        Single(0, -(F(11) * F(10) + F(0)));
        Store(0, r[3] + 12);
    }
    void FinishAngle() {
        auto &r = s.r;
        Double(13, r[11] + 16);
        r[11] = m.ReadU32(Address(r[1] + 16));
        Set(13, F(13) - F(0));
        Set(1, F(2) >= 0.0 ? F(0) : F(13));
        Sign(11);
        if (!s.cr0.eq)
            Neg(1);
    }
    void Angle() {
        auto &r = s.r;
        r[11] = 0xffffffff83210000ull;
        Gradual();
        recovery_abi::WriteU64(m, Address(r[1] + 16), s.fpr_bits[1]);
        recovery_abi::WriteU64(m, Address(r[1] + 24), s.fpr_bits[2]);
        r[11] += 20104;
        Load(0, r[11] + 168);
        Compare(F(2), F(0));
        if (s.cr6.eq) {
            Compare(F(1), F(0));
            if (s.cr6.eq) {
                r[10] = m.ReadU32(Address(r[1] + 24));
                Sign(10);
                if (s.cr0.eq)
                    return;
                r[10] = m.ReadU32(Address(r[1] + 16));
                Sign(10);
                if (!s.cr0.eq) {
                    Double(0, r[11] + 16);
                    s.fpr_bits[1] = s.fpr_bits[0];
                    Neg(1);
                } else
                    Double(1, r[11] + 16);
                return;
            }
            Double(0, r[11] + 8);
            FinishAngle();
            return;
        }
        Gradual();
        s.fpr_bits[12] = s.fpr_bits[2] & 0x7fffffffffffffffull;
        r[10] = 0;
        s.fpr_bits[13] = s.fpr_bits[1] & 0x7fffffffffffffffull;
        Set(0, F(12));
        Compare(F(13), F(0));
        if (s.cr6.gt) {
            Set(0, F(13));
            r[10] = 2;
            Set(13, F(12));
        }
        Set(13, F(13) / F(0));
        Double(0, r[11] + 24);
        Compare(F(13), F(0));
        if (s.cr6.gt) {
            Double(0, r[11] + 40);
            ++r[10];
            Load(12, r[11] + 176);
            Set(11, F(0) + F(13));
            Set(0, F(0) * F(13) - F(12));
            Set(13, F(0) / F(11));
        }
        Set(0, F(13) * F(13));
        Double(11, r[11] + 112);
        Double(12, r[11] + 80);
        auto index = std::bit_cast<std::int32_t>(Address(r[10]));
        s.cr6 = {std::uint8_t(index < 1), std::uint8_t(index > 1), std::uint8_t(index == 1),
                 s.xer_so};
        Set(10, F(11) + F(0));
        Double(11, r[11] + 72);
        Set(11, F(12) * F(0) + F(11));
        Double(12, r[11] + 104);
        Set(10, F(10) * F(0) + F(12));
        Double(12, r[11] + 64);
        Set(11, F(11) * F(0) + F(12));
        Double(12, r[11] + 96);
        Set(10, F(10) * F(0) + F(12));
        Double(12, r[11] + 56);
        Set(11, F(11) * F(0) + F(12));
        Double(12, r[11] + 88);
        Set(12, F(10) * F(0) + F(12));
        Set(0, F(11) * F(0));
        Set(0, F(0) * F(13));
        Set(0, F(0) / F(12));
        Set(0, F(0) + F(13));
        if (s.cr6.gt)
            Neg(0);
        r[10] = Address(r[10]) << 3;
        r[9] = r[11] + 128;
        Double(13, r[10] + r[9]);
        Set(0, F(13) + F(0));
        FinishAngle();
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, float_triplet_transfer::NativeServices &n,
           Registers &s) {
    Math g{m, n, s};
    switch (e) {
    case 0x82bd92c0u:
        g.Plane();
        return true;
    case 0x822da388u:
        g.Angle();
        return true;
    default:
        return false;
    }
}
} // namespace lo::semantic::gpu::mesh_geometry_math61
