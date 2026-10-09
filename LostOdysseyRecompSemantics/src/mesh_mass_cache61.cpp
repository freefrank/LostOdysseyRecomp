#include "lo_semantics/mesh_mass_cache61.h"
#include "lo_semantics/legacy_fp_classification.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::mesh_mass_cache61 {
void Classify(GuestMemory &m, float_triplet_transfer::NativeServices &fp, Registers &s) {
    struct Native final : legacy_fp_classification::HostFpServices {
        float_triplet_transfer::NativeServices &fp;
        explicit Native(float_triplet_transfer::NativeServices &v) : fp(v) {}
        void SetHostFpControl(std::uint32_t v) override { fp.SetHostFpControl(v); }
    } native(fp);
    legacy_fp_classification::Registers c;
    c.integer.r = s.r;
    c.integer.sp = s.r[1];
    c.integer.lr = s.lr;
    c.integer.ctr = s.ctr;
    c.integer.xer_so = s.xer_so;
    c.integer.xer_ca = s.xer_ca;
    c.integer.cr0 = {s.cr0.lt, s.cr0.gt, s.cr0.eq, s.cr0.so};
    c.integer.cr6 = {s.cr6.lt, s.cr6.gt, s.cr6.eq, s.cr6.so};
    c.f0_bits = s.fpr_bits[0];
    c.f1_bits = s.fpr_bits[1];
    c.cached_fp_control = s.cached_fp_control;
    (void)legacy_fp_classification::Apply(0x82b7dfc0u, m, native, c);
    s.r = c.integer.r;
    s.r[1] = c.integer.sp;
    s.lr = c.integer.lr;
    s.ctr = c.integer.ctr;
    s.xer_so = c.integer.xer_so;
    s.xer_ca = c.integer.xer_ca;
    s.cr0 = {c.integer.cr0.lt, c.integer.cr0.gt, c.integer.cr0.eq, c.integer.cr0.so};
    s.cr6 = {c.integer.cr6.lt, c.integer.cr6.gt, c.integer.cr6.eq, c.integer.cr6.so};
    s.fpr_bits[0] = c.f0_bits;
    s.fpr_bits[1] = c.f1_bits;
    s.cached_fp_control = c.cached_fp_control;
}
namespace {
using recovery_abi::Address;
struct Cache {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Store(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    double F(unsigned i) { return std::bit_cast<double>(s.fpr_bits[i]); }
    void Single(unsigned i, double v) {
        s.fpr_bits[i] = std::bit_cast<std::uint64_t>(double(float(v)));
    }
    void Gradual() {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            d.fp.SetHostFpControl(s.cached_fp_control);
        }
    }
    void Load(unsigned i, std::uint64_t p) {
        Gradual();
        s.fpr_bits[i] = std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(Word(p))));
    }
    void Double(unsigned i, std::uint64_t p) {
        Gradual();
        s.fpr_bits[i] = recovery_abi::ReadU64(m, Address(p));
    }
    void FloatStore(unsigned i, std::uint64_t p) {
        Store(p, std::bit_cast<std::uint32_t>(float(F(i))));
    }
    void Compare(std::uint64_t v) {
        auto x = Address(v);
        s.cr6 = {0, std::uint8_t(x != 0), std::uint8_t(x == 0), s.xer_so};
    }
    void FloatCompare(double a, double b) {
        bool u = std::isnan(a) || std::isnan(b);
        s.cr6 = {std::uint8_t(!u && a < b), std::uint8_t(!u && a > b), std::uint8_t(!u && a == b),
                 std::uint8_t(u)};
    }
    bool ClassifierAccepts(GuestAddress continuation) {
        s.lr = continuation;
        Classify(m, d.fp, s);
        s.r[11] = s.r[3] & 519u;
        Compare(s.r[11]);
        s.cr0 = s.cr6;
        return s.cr6.eq;
    }
    bool AcceptArray(std::uint64_t base, unsigned count, GuestAddress continuation) {
        bool valid = true;
        for (unsigned i = 0; i < count; ++i) {
            Load(1, base + 4 * i);
            if (!ClassifierAccepts(continuation + 20 * i)) {
                valid = false;
                break;
            }
        }
        s.r[11] = valid ? 1 : 0;
        s.r[11] &= 255u;
        Compare(s.r[11]);
        return !s.cr6.eq;
    }
    void Body() {
        auto &r = s.r;
        r[30] = r[3];
        r[11] = 0xffffffff82000000ull;
        r[29] = r[30] + 292;
        Load(0, r[11] + 3664);
        Load(13, r[29]);
        FloatCompare(F(13), F(0));
        if (!s.cr6.lt) {
            r[3] = r[29];
            return;
        }
        r[11] = Word(r[30] + 168);
        r[5] = r[1] + 112;
        r[10] = Word(r[30] + 164);
        r[3] = r[1] + 80;
        Store(r[1] + 80, r[11]);
        r[11] = Word(r[30] + 160);
        Store(r[1] + 100, r[10]);
        Store(r[1] + 84, r[11]);
        r[11] = 0;
        Store(r[1] + 104, r[11]);
        r[11] = Word(r[30] + 172);
        Store(r[1] + 96, r[11]);
        r[11] = 12;
        Store(r[1] + 88, r[11]);
        Store(r[1] + 92, r[11]);
        r[11] = 0xffffffff82000000ull;
        Load(1, r[11] + 30596);
        s.lr = 0x82b9f488u;
        (void)mesh_mass_math61::Apply(0x82bcd8a8u, m, d.fp, s);
        r[11] = r[3] & 255u;
        Compare(r[11]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        // Owner cache intentionally holds the origin tensor, not the second
        // centroid tensor emitted by the lower mass-property pipeline.
        r[11] = r[1] + 160;
        r[10] = r[30] + 308;
        r[9] = 3;
        do {
            Double(0, r[11] - 24);
            --r[9];
            Double(13, r[11]);
            Single(0, F(0));
            Double(12, r[11] + 24);
            Single(13, F(13));
            Single(12, F(12));
            FloatStore(0, r[10] - 12);
            FloatStore(13, r[10]);
            r[11] += 8;
            FloatStore(12, r[10] + 12);
            Compare(r[9]);
            r[10] += 4;
        } while (!s.cr6.eq);
        r[31] = r[30] + 296;
        Load(0, r[1] + 112);
        Load(13, r[1] + 116);
        Load(12, r[1] + 120);
        FloatStore(0, r[30] + 332);
        FloatStore(13, r[30] + 336);
        FloatStore(12, r[30] + 340);
        if (!AcceptArray(r[31], 9, 0x82b9f508u) || !AcceptArray(r[30] + 332, 3, 0x82b9f5d0u)) {
            r[3] = 0;
            return;
        }
        Double(0, r[1] + 128);
        Single(1, F(0));
        if (!ClassifierAccepts(0x82b9f624u)) {
            r[3] = 0;
            return;
        }
        r[11] = 0xffffffff82000000ull;
        Double(0, r[1] + 128);
        Double(13, r[11] + 4072);
        FloatCompare(F(0), F(13));
        if (s.cr6.lt) {
            r[11] = 0xffffffff820d0000ull;
            r[6] = 0;
            r[7] = r[11] + 24352;
            r[11] = 0xffffffff820d0000ull;
            r[5] = 417;
            r[4] = r[11] + 23768;
            r[3] = 206;
            s.lr = 0x82b9f664u;
            (void)diagnostic_format_routes61::Apply(0x82b9c298u, m, d.diagnostics, s);
            Load(13, r[31]);
            s.fpr_bits[13] ^= 0x8000000000000000ull;
            FloatStore(13, r[31]);
            for (unsigned i = 1; i < 9; ++i) {
                Load(14 - i, r[31] + 4 * i);
                s.fpr_bits[14 - i] ^= 0x8000000000000000ull;
            }
            Double(0, r[1] + 128);
            FloatStore(13, r[31] + 4);
            s.fpr_bits[0] ^= 0x8000000000000000ull;
            for (unsigned i = 2; i < 9; ++i)
                FloatStore(14 - i, r[31] + 4 * i);
        }
        Gradual();
        Single(0, F(0));
        FloatStore(0, r[29]);
        r[3] = r[29];
    }
    void Run() {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = 0x82b9f420u;
        for (unsigned i = 29; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        Store(r[1] - 8, r[12]);
        auto old = r[1];
        r[1] -= 320;
        Store(r[1], old);
        Body();
        r[1] += 320;
        for (unsigned i = 29; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    if (e != 0x82b9f418u)
        return false;
    Cache{m, d, s}.Run();
    return true;
}
} // namespace lo::semantic::gpu::mesh_mass_cache61
