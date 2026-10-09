#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/mesh_mass_math61.h"
#include "lo_semantics/recovery_abi.h"
#include <cmath>
namespace mass_math_oracle {
using Registers = mesh_mass_math61::Registers;
constexpr GuestAddress State = 0x30000, Output = 0x31000, Indices = 0x32000, Positions = 0x33000;
constexpr std::array<test::Region, 2> Regions{{{0, 0x120000}, {0x82000000, 0x10000}}};
struct Native final : float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t v) override { PPCFPSCRRegister{}.setcsr(v); }
} native;
void Check(unsigned mode) {
    struct Restore {
        std::uint32_t csr = PPCFPSCRRegister{}.getcsr();
        ~Restore() { PPCFPSCRRegister{}.setcsr(csr); }
    } restore;
    test::GuestWindow before(Regions), after(Regions);
    auto seed = [&](test::GuestWindow &w) {
        w.Fill(0);
        auto m = w.Memory();
        auto dbl = [&](GuestAddress p, double v) {
            recovery_abi::WriteU64(m, p, std::bit_cast<std::uint64_t>(v));
        };
        m.WriteU32(0x82000e50, 0);
        m.WriteU32(0x82007784, std::bit_cast<std::uint32_t>(1.f));
        dbl(0x82000fe8, 0);
        dbl(0x82000f28, 1);
        m.WriteU32(State + 72, 16);
        m.WriteU32(State + 80, Positions);
        const float points[]{0, 0, 2, 3, 0, 2, 0, 4, 2};
        for (unsigned i = 0; i < 3; ++i) {
            m.WriteU32(Indices + 4 * i, i);
            for (unsigned j = 0; j < 3; ++j)
                m.WriteU32(Positions + 16 * i + 4 * j,
                           std::bit_cast<std::uint32_t>(points[3 * i + j]));
        }
        if (mode == 1) {
            m.WriteU32(Positions + 32, std::bit_cast<std::uint32_t>(6.f));
            m.WriteU32(Positions + 36, 0);
        }
        dbl(State + 56, 2);
        dbl(State + 288, mode == 3 ? 0 : 8);
        for (unsigned i = 0; i < 3; ++i) {
            double c = i + 1;
            dbl(State + 296 + 8 * i, mode == 3 ? 0 : 8 * c);
            dbl(State + 320 + 8 * i, mode == 3 ? 0 : 8 * (c * c + 1. / 3));
        }
        for (unsigned i = 0; i < 3; ++i) {
            constexpr double products[]{2, 6, 3};
            dbl(State + 344 + 8 * i, mode == 3 ? 0 : 8 * products[i]);
        }
    };
    seed(before);
    seed(after);
    Registers s{};
    for (unsigned i = 0; i < 32; ++i) {
        s.r[i] = 0x1122334400000000ull + i;
        s.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    s.r[1] = 0x8877665500080000ull;
    s.lr = 0x9988776681234567ull;
    s.xer_so = 1;
    s.cached_fp_control = 0x9fc0;
    s.r[3] = State;
    s.r[4] = Output;
    s.r[5] = Indices;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (mode < 2)
        __imp__sub_82BCD300(c, before.Bytes());
    else
        __imp__sub_82BCCCA8(c, before.Bytes());
    auto csr = PPCFPSCRRegister{}.getcsr();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    auto m = after.Memory();
    (void)mesh_mass_math61::Apply(mode < 2 ? 0x82bcd300u : 0x82bccca8u, m, native, s);
    if (crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || csr != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("mass math Full72/RAM/CSR mode " + std::to_string(mode));
    auto dbl = [&](GuestAddress p) { return std::bit_cast<double>(recovery_abi::ReadU64(m, p)); };
    if (mode < 2) {
        double plane[]{0, 0, mode == 0 ? 1. : 0., mode == 0 ? -2. : 0.};
        for (unsigned i = 0; i < 4; ++i)
            if (dbl(Output + 8 * i) != plane[i])
                throw std::runtime_error("strided plane");
    } else {
        if (dbl(State + 48) != (mode == 2 ? 16. : 0.))
            throw std::runtime_error("density scaled mass");
        for (unsigned i = 0; i < 9; ++i) {
            double expected = mode == 2 && i % 4 == 0 ? 32. / 3 : 0;
            if (std::abs(dbl(Output + 8 * i) - expected) > 1e-12)
                throw std::runtime_error("translated cube inertia");
        }
    }
}
} // namespace mass_math_oracle
int main() {
    try {
        for (unsigned i = 0; i < 4; ++i)
            mass_math_oracle::Check(i);
        std::puts("PASS mesh-mass-math61 4 original plane/inertia cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
