#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/power_fp_support61.h"
#include <cmath>
#include <limits>
namespace power_support_oracle {
using Registers = power_fp_support61::Registers;
constexpr std::array<test::Region, 2> Regions{{{0, 0x120000}, {0x82000000, 0x10000}}};
struct Native final : float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t v) override { PPCFPSCRRegister{}.setcsr(v); }
};
void Check(unsigned mode) {
    constexpr GuestAddress entries[]{0x82b7e668, 0x82b7e668, 0x82b7e668, 0x82b7e668,
                                     0x82b7e668, 0x82b822f0, 0x82b822f0, 0x82b822c8,
                                     0x82b823c8, 0x82b823c8, 0x82b823c8, 0x82b823c8};
    const double values[]{0.,
                          3.,
                          -22.,
                          .5,
                          std::numeric_limits<double>::denorm_min(),
                          2.,
                          -.125,
                          1.5,
                          2.,
                          0.,
                          std::numeric_limits<double>::denorm_min(),
                          -std::numeric_limits<double>::denorm_min() * 7};
    struct Restore {
        std::uint32_t csr = PPCFPSCRRegister{}.getcsr();
        ~Restore() { PPCFPSCRRegister{}.setcsr(csr); }
    } restore;
    test::GuestWindow before(Regions), after(Regions);
    for (auto *w : {&before, &after}) {
        w->Fill(0xa5);
        auto m = w->Memory();
        WriteU64(m, 0x82000fe8, 0);
        WriteU64(m, 0x82000f70, std::bit_cast<std::uint64_t>(.5));
    }
    Registers s{};
    for (unsigned i = 0; i < 32; ++i) {
        s.r[i] = 0x1122334400000000ull + i;
        s.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    s.r[1] = 0x8877665500080000ull;
    s.r[4] = mode == 7 ? std::uint64_t(std::int64_t(-21)) : 0x30000;
    s.fpr_bits[1] = std::bit_cast<std::uint64_t>(values[mode]);
    s.lr = 0x9988776681234567ull;
    s.xer_so = 1;
    s.cached_fp_control = 0x9fc0;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    switch (entries[mode]) {
    case 0x82b7e668:
        __imp__sub_82B7E668(c, before.Bytes());
        break;
    case 0x82b822f0:
        __imp__sub_82B822F0(c, before.Bytes());
        break;
    case 0x82b822c8:
        __imp__sub_82B822C8(c, before.Bytes());
        break;
    default:
        __imp__sub_82B823C8(c, before.Bytes());
    }
    auto host = PPCFPSCRRegister{}.getcsr();
    auto m = after.Memory();
    Native native;
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    (void)power_fp_support61::Apply(entries[mode], m, native, s);
    auto a = crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),
         b = crt_full_oracle::Snapshot(s);
    if (a != b || !before.EqualCommitted(after) || host != PPCFPSCRRegister{}.getcsr()) {
        std::fprintf(stderr, "power helper%u Full%d RAM%d CSR%d\n", mode, a == b,
                     before.EqualCommitted(after), host == PPCFPSCRRegister{}.getcsr());
        for (unsigned i = 0; i < a.size(); ++i)
            if (a[i] != b[i])
                std::fprintf(stderr, "field%u %llx/%llx\n", i, (unsigned long long)a[i],
                             (unsigned long long)b[i]);
        throw std::runtime_error("power helper state mismatch");
    }
    if (mode < 5) {
        constexpr unsigned result[]{2, 1, 2, 0, 0};
        if (s.r[3] != result[mode])
            throw std::runtime_error("integer parity");
    } else if (mode < 7) {
        auto exponent = mode == 5 ? 2 : -2;
        if (std::bit_cast<std::int64_t>(s.r[3]) != exponent)
            throw std::runtime_error("exponent");
    } else if (mode == 7) {
        if (std::bit_cast<double>(s.fpr_bits[1]) != std::ldexp(.75, -21))
            throw std::runtime_error("replace exponent");
    } else {
        int exponent;
        auto fraction = std::frexp(values[mode], &exponent);
        if (std::bit_cast<double>(s.fpr_bits[1]) != fraction ||
            std::bit_cast<std::int32_t>(m.ReadU32(0x30000)) != exponent)
            throw std::runtime_error("decomposition");
    }
}
} // namespace power_support_oracle
int main() {
    try {
        for (unsigned i = 0; i < 12; ++i)
            power_support_oracle::Check(i);
        std::puts("PASS power-fp-support61 12 original leaf / independent parity and decomposition "
                  "cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
