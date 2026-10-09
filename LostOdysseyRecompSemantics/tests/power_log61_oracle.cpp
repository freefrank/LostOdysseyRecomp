#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/power_log61.h"
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <limits>
namespace power_log_oracle {
using Registers = power_log61::Registers;
constexpr std::array<test::Region, 4> Regions{
    {{0, 0x120000}, {0x82000000, 0x2000}, {0x820d2000, 0x2000}, {0x83215000, 0x1000}}};
std::array<unsigned char, 1296> constants{};
struct Native final : float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t v) override { PPCFPSCRRegister{}.setcsr(v); }
};
void Check(unsigned mode) {
    const double values[]{1.,
                          2.,
                          .75,
                          std::numeric_limits<double>::denorm_min(),
                          0.,
                          -1.,
                          std::numeric_limits<double>::infinity()};
    struct Restore {
        std::uint32_t csr = PPCFPSCRRegister{}.getcsr();
        ~Restore() { PPCFPSCRRegister{}.setcsr(csr); }
    } restore;
    test::GuestWindow before(Regions), after(Regions);
    for (auto *w : {&before, &after}) {
        w->Fill(0xa5);
        auto m = w->Memory();
        unsigned offset = 0;
        for (auto region :
             std::array<test::Region, 3>{{{0x82000e00, 768}, {0x820d2f68, 512}, {0x83215500, 16}}})
            for (unsigned i = 0; i < region.size; ++i)
                m.WriteU8(region.base + i, constants[offset++]);
    }
    Registers s{};
    for (unsigned i = 0; i < 32; ++i) {
        s.r[i] = 0x1122334400000000ull + i;
        s.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    s.r[1] = 0x8877665500080000ull;
    s.r[4] = 0x30000;
    s.fpr_bits[1] = std::bit_cast<std::uint64_t>(values[mode]);
    s.lr = 0x9988776681234567ull;
    s.xer_so = 1;
    s.cached_fp_control = 0x9fc0;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    __imp__sub_82301A68(c, before.Bytes());
    auto host = PPCFPSCRRegister{}.getcsr();
    auto m = after.Memory();
    Native native;
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    (void)power_log61::Apply(0x82301a68u, m, native, s);
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
    const auto actual = std::bit_cast<double>(s.fpr_bits[1]);
    if (mode == 5) {
        if (!std::isnan(actual))
            throw std::runtime_error("log negative domain");
    } else {
        auto expected = std::log(values[mode]);
        if (std::isinf(expected)
                ? actual != expected
                : std::abs(actual - expected) > 1e-12 * std::max(1., std::abs(expected))) {
            std::fprintf(stderr, "mode%u actual %.17g expected %.17g\n", mode, actual, expected);
            throw std::runtime_error("log value");
        }
    }
}
} // namespace power_log_oracle
int main() {
    try {
        auto path = std::getenv("LO_POWER_CONSTANTS");
        if (!path)
            throw std::runtime_error("LO_POWER_CONSTANTS required");
        std::ifstream f(path, std::ios::binary);
        f.read(reinterpret_cast<char *>(power_log_oracle::constants.data()), 1296);
        if (f.gcount() != 1296)
            throw std::runtime_error("power bundle size");
        for (unsigned i = 0; i < 7; ++i)
            power_log_oracle::Check(i);
        std::puts("PASS power-log61 7 original leaf / independent natural logarithm cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
