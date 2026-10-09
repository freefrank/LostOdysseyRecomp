#include "lo_semantics/power_log61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::power_log61 {
namespace {
using recovery_abi::Address;
struct Log {
    GuestMemory &m;
    float_triplet_transfer::NativeServices &fp;
    Registers &s;
    double F(unsigned i) { return std::bit_cast<double>(s.fpr_bits[i]); }
    void V(unsigned i, double v) { s.fpr_bits[i] = std::bit_cast<std::uint64_t>(v); }
    void Mode() {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            fp.SetHostFpControl(s.cached_fp_control);
        }
    }
    void Load(unsigned i, std::uint64_t p) {
        Mode();
        s.fpr_bits[i] = recovery_abi::ReadU64(m, Address(p));
    }
    void Save(unsigned i, std::uint64_t p) {
        Mode();
        recovery_abi::WriteU64(m, Address(p), s.fpr_bits[i]);
    }
    void Compare(double a, double b) {
        s.cr6 = {std::uint8_t(a < b), std::uint8_t(a > b), std::uint8_t(a == b),
                 std::uint8_t(std::isnan(a) || std::isnan(b))};
    }
    void NaN() {
        s.r[11] = 0xffffffff83210000ull;
        Load(0, s.r[11] + 21768);
        s.fpr_bits[1] = s.fpr_bits[0] ^ 0x8000000000000000ull;
    }
    void Run() {
        auto &r = s.r;
        r[11] = 0xffffffff82000000ull;
        Save(1, r[1] + 16);
        Load(8, r[11] + 3880);
        Compare(F(1), F(8));
        if (s.cr6.eq) {
            r[11] = 0xffffffff82000000ull;
            Load(1, r[11] + 4072);
            return;
        }
        r[11] = m.ReadU16(Address(r[1] + 16));
        r[10] = r[11] & 0x7ff0u;
        s.cr6 = {std::uint8_t(r[10] < 32752), std::uint8_t(r[10] > 32752),
                 std::uint8_t(r[10] == 32752), s.xer_so};
        if (s.cr6.eq) {
            r[11] = 0xffffffff820d0000ull;
            Load(0, r[11] + 12224);
            Compare(F(1), F(0));
            if (s.cr6.gt)
                return;
            NaN();
            return;
        }
        r[9] = 0xffffffff82000000ull;
        Load(0, r[9] + 4072);
        Compare(F(1), F(0));
        if (!s.cr6.gt) {
            Compare(F(1), F(0));
            if (!s.cr6.eq) {
                NaN();
                return;
            }
            r[11] = 0xffffffff83210000ull;
            Load(0, r[11] + 21760);
            s.fpr_bits[1] = s.fpr_bits[0] ^ 0x8000000000000000ull;
            return;
        }
        r[9] = 0xffffffff82000000ull;
        Load(0, r[9] + 3736);
        Compare(F(1), F(0));
        if (s.cr6.lt) {
            r[11] = 0xffffffff820d0000ull;
            Load(0, r[11] + 12216);
            V(1, F(1) * F(0));
            Save(1, r[1] + 16);
            r[11] = m.ReadU16(Address(r[1] + 16));
            r[10] = ((r[11] >> 4) & 0x7ff) - std::uint64_t(1075);
        } else
            r[10] = ((r[10] >> 4) & 0xfff) - std::uint64_t(1022);
        r[11] &= 32783;
        s.cr0 = {0, std::uint8_t(r[11] != 0), std::uint8_t(r[11] == 0), s.xer_so};
        Save(1, r[1] - 16);
        r[9] = 0xffffffff82000000ull;
        r[11] |= 16352;
        m.WriteU16(Address(r[1] - 16), std::uint16_t(r[11]));
        r[11] = 0xffffffff820d2f68ull;
        Load(13, r[11]);
        Load(0, r[1] - 16);
        Compare(F(0), F(13));
        Load(13, r[9] + 3952);
        // Range reduction around one keeps the rational argument small.
        if (s.cr6.gt) {
            V(12, F(0) - F(13));
            V(11, F(0) + F(8));
            V(0, F(12) - F(13));
            V(13, F(11) * F(13));
        } else {
            V(0, F(0) - F(13));
            --r[10];
            V(12, F(0) + F(8));
            V(13, F(12) * F(13));
        }
        r[9] = std::uint64_t(std::int64_t(std::bit_cast<std::int32_t>(Address(r[10]))));
        V(13, F(0) / F(13));
        r[10] = 0xffffffff82000000ull;
        Load(10, r[11] + 8);
        recovery_abi::WriteU64(m, Address(r[1] - 16), r[9]);
        Load(11, r[10] + 3744);
        r[10] = 0xffffffff82000000ull;
        V(0, F(13) * F(13));
        Load(9, r[10] + 3688);
        r[10] = 0xffffffff82000000ull;
        V(7, F(0) - F(9));
        Load(9, r[10] + 3696);
        Load(12, r[1] - 16);
        V(12, double(std::bit_cast<std::int64_t>(s.fpr_bits[12])));
        V(6, F(12) * F(9));
        Load(9, r[11] + 40);
        V(9, -(F(0) * F(11) - F(9)));
        Load(11, r[11] + 64);
        r[11] = 0xffffffff82000000ull;
        V(7, F(7) * F(0) + F(11));
        Load(11, r[11] + 3704);
        r[11] = 0xffffffff82000000ull;
        V(9, F(9) * F(0) - F(11));
        Load(11, r[11] + 3672);
        V(11, F(7) * F(0) - F(11));
        V(0, F(9) * F(0));
        V(0, F(0) / F(11));
        V(0, F(0) + F(8));
        V(0, F(0) * F(13) - F(6));
        V(1, F(12) * F(10) + F(0));
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, float_triplet_transfer::NativeServices &fp,
           Registers &s) {
    if (e != 0x82301a68u)
        return false;
    Log{m, fp, s}.Run();
    return true;
}
} // namespace lo::semantic::gpu::power_log61
