#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/power_math61.h"
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <limits>
namespace power_oracle {
using Registers = power_math61::Registers;
constexpr std::array<test::Region, 5> Regions{{{0, 0x120000},
                                               {0x82000000, 0x2000},
                                               {0x820d2000, 0x2000},
                                               {0x83214000, 0x2000},
                                               {0x82218000, 0x1000}}};
std::array<unsigned char, 1440> constants{};
GuestMemory *memory = nullptr;
struct Native final : float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t v) override { PPCFPSCRRegister{}.setcsr(v); }
};
void Check(unsigned mode) {
    const double inf = std::numeric_limits<double>::infinity();
    const double base[]{2., 3., -2., 2., .5, 1.0625, 0., -0., inf, -2., 2., 2.};
    const double exponent[]{-22., 5., 3., .5, -2., .7, -3., 3., -2., .5, 0., 1025.};
    struct Restore {
        std::uint32_t csr = PPCFPSCRRegister{}.getcsr();
        ~Restore() { PPCFPSCRRegister{}.setcsr(csr); }
    } restore;
    test::GuestWindow before(Regions), after(Regions);
    for (auto *w : {&before, &after}) {
        w->Fill(0xa5);
        auto m = w->Memory();
        unsigned offset = 0;
        for (auto region : std::array<test::Region, 5>{{{0x82000e00, 768},
                                                        {0x820d2f68, 512},
                                                        {0x83215500, 40},
                                                        {0x822181a0, 112},
                                                        {0x83214fc0, 8}}})
            for (unsigned i = 0; i < region.size; ++i)
                m.WriteU8(region.base + i, constants[offset++]);
    }
    Registers s{};
    for (unsigned i = 0; i < 32; ++i) {
        s.r[i] = 0x1122334400000000ull + i;
        s.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    s.r[1] = 0x8877665500080000ull;
    s.fpr_bits[1] = std::bit_cast<std::uint64_t>(base[mode]);
    s.fpr_bits[2] = std::bit_cast<std::uint64_t>(exponent[mode]);
    s.lr = 0x9988776681234567ull;
    s.xer_so = 1;
    s.cached_fp_control = 0x9fc0;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    auto om = before.Memory();
    memory = &om;
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    __imp__sub_82B7E860(c, before.Bytes());
    auto host = PPCFPSCRRegister{}.getcsr();
    auto m = after.Memory();
    Native native;
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    (void)power_math61::Apply(0x82b7e860u, m, native, s);
    auto a = crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),
         b = crt_full_oracle::Snapshot(s);
    if (a != b || !before.EqualCommitted(after) || host != PPCFPSCRRegister{}.getcsr()) {
        std::fprintf(stderr, "pow%u Full%d RAM%d CSR%d actual%.17g\n", mode, a == b,
                     before.EqualCommitted(after), host == PPCFPSCRRegister{}.getcsr(),
                     std::bit_cast<double>(s.fpr_bits[1]));
        for (unsigned i = 0; i < a.size(); ++i)
            if (a[i] != b[i])
                std::fprintf(stderr, "field%u %llx/%llx\n", i, (unsigned long long)a[i],
                             (unsigned long long)b[i]);
        for (auto region : Regions)
            for (unsigned i = 0; i < region.size; ++i)
                if (before.Bytes()[region.base + i] != after.Bytes()[region.base + i]) {
                    std::fprintf(stderr, "RAM %08llx %02x/%02x\n",
                                 (unsigned long long)(region.base + i),
                                 before.Bytes()[region.base + i], after.Bytes()[region.base + i]);
                    break;
                }
        throw std::runtime_error("power original graph mismatch");
    }
    auto actual = std::bit_cast<double>(s.fpr_bits[1]),
         expected = std::pow(base[mode], exponent[mode]);
    if (std::isnan(expected)) {
        if (!std::isnan(actual))
            throw std::runtime_error("power domain");
    } else if (std::isinf(expected)) {
        if (actual != expected)
            throw std::runtime_error("power infinity");
    } else if (std::abs(actual - expected) > 1e-12 * std::max(1., std::abs(expected)) ||
               (expected == 0 && std::signbit(actual) != std::signbit(expected)))
        throw std::runtime_error("power value");
    if (mode == 0 && actual != std::ldexp(1., -22))
        throw std::runtime_error("exact cooking tolerance");
}
} // namespace power_oracle
void PowerSave(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    for (unsigned i = first; i < 32; ++i)
        WriteU64(*power_oracle::memory, Address(s.r[12] - 8 * (32 - i)), s.fpr_bits[i]);
}
void PowerRestore(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    for (unsigned i = first; i < 32; ++i)
        s.fpr_bits[i] = ReadU64(*power_oracle::memory, Address(s.r[12] - 8 * (32 - i)));
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        auto path = std::getenv("LO_POWER_CONSTANTS");
        if (!path)
            throw std::runtime_error("LO_POWER_CONSTANTS required");
        std::ifstream f(path, std::ios::binary);
        f.read(reinterpret_cast<char *>(power_oracle::constants.data()), 1440);
        if (f.gcount() != 1440)
            throw std::runtime_error("power bundle size");
        for (unsigned mode = 0; mode < 12; ++mode)
            power_oracle::Check(mode);
        std::puts("PASS power-math61 12 complete original graph / independent power cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
