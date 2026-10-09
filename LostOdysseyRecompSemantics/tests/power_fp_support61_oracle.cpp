#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/power_fp_support61.h"
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <limits>
namespace power_support_oracle {
using Registers = power_fp_support61::Registers;
constexpr std::array<test::Region, 3> Regions{
    {{0, 0x120000}, {0x82000000, 0x10000}, {0x83215000, 0x1000}}};
std::array<unsigned char, 1320> constants{};
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
        WriteU64(m, 0x82000f28, std::bit_cast<std::uint64_t>(1.));
        for (unsigned i = 0; i < 40; ++i)
            m.WriteU8(0x83215500 + i, constants[1280 + i]);
        WriteU64(m, 0x82000fe8, 0);
        WriteU64(m, 0x82000f70, std::bit_cast<std::uint64_t>(.5));
    }
    const double inf = std::numeric_limits<double>::infinity();
    const double bases[]{2., .5, 1., 1., inf, -inf, -inf, -inf};
    const double exponents[]{inf, inf, inf, -inf, -3., 3., -3., 0.};
    const GuestAddress entry = mode < 12 ? entries[mode] : 0x82b7e6d8u;
    Registers s{};
    for (unsigned i = 0; i < 32; ++i) {
        s.r[i] = 0x1122334400000000ull + i;
        s.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    s.r[1] = 0x8877665500080000ull;
    s.r[4] = mode == 7 ? std::uint64_t(std::int64_t(-21)) : 0x30000;
    s.fpr_bits[1] = std::bit_cast<std::uint64_t>(mode < 12 ? values[mode] : bases[mode - 12]);
    s.lr = 0x9988776681234567ull;
    s.xer_so = 1;
    s.cached_fp_control = 0x9fc0;
    if (mode >= 12) {
        s.fpr_bits[2] = std::bit_cast<std::uint64_t>(exponents[mode - 12]);
        s.r[5] = 0x30000;
    }
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    switch (entry) {
    case 0x82b7e6d8:
        __imp__sub_82B7E6D8(c, before.Bytes());
        break;
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
    (void)power_fp_support61::Apply(entry, m, native, s);
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
    if (mode >= 12) {
        auto actual = std::bit_cast<double>(ReadU64(m, 0x30000));
        if (mode == 15) {
            if (s.r[3] != 1 || !std::isnan(actual))
                throw std::runtime_error("indeterminate infinite power");
        } else {
            double expected = std::pow(bases[mode - 12], exponents[mode - 12]);
            if (s.r[3] || actual != expected ||
                (actual == 0 && std::signbit(actual) != std::signbit(expected)))
                throw std::runtime_error("infinite power result");
        }
        return;
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
        auto path = std::getenv("LO_POWER_CONSTANTS");
        if (!path)
            throw std::runtime_error("LO_POWER_CONSTANTS required");
        std::ifstream f(path, std::ios::binary);
        f.read(reinterpret_cast<char *>(power_support_oracle::constants.data()), 1320);
        if (f.gcount() != 1320)
            throw std::runtime_error("power bundle size");
        for (unsigned i = 0; i < 20; ++i)
            power_support_oracle::Check(i);
        std::puts("PASS power-fp-support61 20 original local-chain / independent helper cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
