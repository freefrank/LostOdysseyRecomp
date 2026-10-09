#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/geometry_primitives61.h"
#include <cmath>
namespace primitives_oracle {
using Registers = geometry_primitives61::Registers;
constexpr GuestAddress Input = 0x30000, Output = 0x31000;
constexpr std::array<test::Region, 3> Regions{
    {{0, 0x120000}, {0x82000000, 0x10000}, {0x8201f000, 0x1000}}};
struct Native final : float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t v) override { PPCFPSCRRegister{}.setcsr(v); }
};
void Check(unsigned mode) {
    struct Restore {
        std::uint32_t csr = PPCFPSCRRegister{}.getcsr();
        ~Restore() { PPCFPSCRRegister{}.setcsr(csr); }
    } restore;
    test::GuestWindow before(Regions), after(Regions);
    const auto seed = [&](test::GuestWindow &w) {
        w.Fill(0xa5);
        auto m = w.Memory();
        constexpr float box[]{-2, -4, -6, 4, 2, 2}, triangle[]{0, 0, 0, 3, 0, 0, 0, 4, 0},
            outer[]{-3, -5, -7, 5, 3, 3};
        for (unsigned i = 0; i < (mode < 4 ? 6u : 9u); ++i)
            m.WriteU32(Input + 4 * i,
                       std::bit_cast<std::uint32_t>(mode < 4 ? box[i] : triangle[i]));
        for (unsigned i = 0; i < 6; ++i)
            m.WriteU32(Output + 4 * i, std::bit_cast<std::uint32_t>(outer[i]));
        if (mode == 2)
            m.WriteU32(Output + 20, std::bit_cast<std::uint32_t>(1.f));
        m.WriteU32(0x8201f9f0, std::bit_cast<std::uint32_t>(0.5f));
        m.WriteU32(0x82000f20, std::bit_cast<std::uint32_t>(1.f / 3.f));
        m.WriteU32(0x82007784, std::bit_cast<std::uint32_t>(1.f));
        m.WriteU32(0x82000e50, 0);
    };
    seed(before);
    seed(after);
    Registers s{};
    for (unsigned i = 0; i < 32; ++i) {
        s.r[i] = 0x1122334400000000ull + i;
        s.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    s.r[1] = 0x8877665500080000ull;
    s.r[3] = Input;
    s.r[4] = Output;
    s.r[5] = mode == 7 ? 1 : 0;
    s.fpr_bits[1] = std::bit_cast<std::uint64_t>(mode == 7 ? 1. : 0.5);
    s.lr = 0x9988776681234567ull;
    s.xer_so = 1;
    s.cached_fp_control = 0x9fc0;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    constexpr GuestAddress entries[]{0x82bddf08, 0x82bddfc8, 0x82bddfc8, 0x82bde040,
                                     0x82bde108, 0x82bde140, 0x82bde1b8, 0x82bde1b8};
    switch (mode) {
    case 0:
        __imp__sub_82BDDF08(c, before.Bytes());
        break;
    case 1:
    case 2:
        __imp__sub_82BDDFC8(c, before.Bytes());
        break;
    case 3:
        __imp__sub_82BDE040(c, before.Bytes());
        break;
    case 4:
        __imp__sub_82BDE108(c, before.Bytes());
        break;
    case 5:
        __imp__sub_82BDE140(c, before.Bytes());
        break;
    default:
        __imp__sub_82BDE1B8(c, before.Bytes());
        break;
    }
    auto host = PPCFPSCRRegister{}.getcsr();
    auto m = after.Memory();
    Native native;
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (!geometry_primitives61::Apply(entries[mode], m, native, s) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || host != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("geometry primitive Full72/RAM/host mismatch");
    auto value = [&](GuestAddress p) { return std::bit_cast<float>(m.ReadU32(p)); };
    if (mode == 0) {
        constexpr float cube[]{-3, -5, -6, 5, 3, 2};
        for (unsigned i = 0; i < 6; ++i)
            if (value(Output + 4 * i) != cube[i])
                throw std::runtime_error("centered cube result");
        if (std::bit_cast<double>(s.fpr_bits[1]) != 4.)
            throw std::runtime_error("cube radius");
    }
    if ((mode == 1 || mode == 2) && s.r[3] != (mode == 1 ? 1u : 0u))
        throw std::runtime_error("box containment");
    if (mode == 3) {
        constexpr float corners[]{-2, -4, -6, 4, -4, -6, 4, 2, -6, -2, 2, -6,
                                  -2, -4, 2,  4, -4, 2,  4, 2, 2,  -2, 2, 2};
        for (unsigned i = 0; i < 24; ++i)
            if (value(Output + 4 * i) != corners[i])
                throw std::runtime_error("box corners order");
    }
    if (mode == 4 &&
        (value(Input + 16) != 4.f || value(Input + 24) != 3.f || value(Input + 12) != 0.f))
        throw std::runtime_error("triangle winding swap");
    if (mode == 5 && std::bit_cast<double>(s.fpr_bits[1]) != 6.)
        throw std::runtime_error("triangle area");
    if (mode >= 6) {
        constexpr float original[3][3]{{0, 0, 0}, {3, 0, 0}, {0, 4, 0}};
        for (unsigned n = 0; n < 3; ++n) {
            double distance = 0;
            for (unsigned axis = 0; axis < 3; ++axis) {
                const auto delta = value(Input + 12 * n + 4 * axis) - original[n][axis];
                distance += delta * delta;
                if (mode == 6) {
                    const double center = axis == 0 ? 1. : axis == 1 ? 4. / 3. : 0.;
                    if (std::abs(delta - 0.5 * (original[n][axis] - center)) > 1e-6)
                        throw std::runtime_error("relative triangle expansion");
                }
            }
            if (mode == 7 && std::abs(std::sqrt(distance) - 1.) > 1e-6)
                throw std::runtime_error("normalized triangle expansion");
        }
    }
}
} // namespace primitives_oracle
int main() {
    try {
        for (unsigned mode = 0; mode < 8; ++mode)
            primitives_oracle::Check(mode);
        std::puts("PASS geometry-primitives61 8 original-body cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
