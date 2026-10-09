#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/mesh_geometry_math61.h"
#include <cmath>
#include <cstdlib>
#include <fstream>
namespace mesh_math_oracle {
using Registers = mesh_geometry_math61::Registers;
constexpr GuestAddress Input = 0x30000, Output = 0x31000, Constants = 0x83214e88;
constexpr std::array<test::Region, 3> Regions{
    {{0, 0x120000}, {0x82000000, 0x10000}, {0x83214000, 0x1000}}};
std::array<unsigned char, 184> constants{};
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
        float points[]{0, 0, 2, 3, 0, 2, 0, 4, 2};
        if (mode == 1) {
            points[6] = 6;
            points[7] = 0;
        }
        for (unsigned i = 0; i < 9; ++i)
            m.WriteU32(Input + 4 * i, std::bit_cast<std::uint32_t>(points[i]));
        m.WriteU32(0x82000e50, 0);
        m.WriteU32(0x82007784, std::bit_cast<std::uint32_t>(1.f));
        for (unsigned i = 0; i < constants.size(); ++i)
            m.WriteU8(Constants + i, constants[i]);
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
    s.r[3] = Output;
    s.r[4] = Input;
    s.r[5] = Input + 12;
    s.r[6] = Input + 24;
    constexpr double angles[][2]{{0.1, 2.0}, {2.0, -1.0}, {-2.0, 1.0},
                                 {1.0, 0.0}, {0.0, -0.0}, {-0.0, -0.0}};
    if (mode >= 2) {
        s.fpr_bits[1] = std::bit_cast<std::uint64_t>(angles[mode - 2][0]);
        s.fpr_bits[2] = std::bit_cast<std::uint64_t>(angles[mode - 2][1]);
    }
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (mode < 2)
        __imp__sub_82BD92C0(c, before.Bytes());
    else
        __imp__sub_822DA388(c, before.Bytes());
    auto csr = PPCFPSCRRegister{}.getcsr();
    auto m = after.Memory();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (!mesh_geometry_math61::Apply(mode < 2 ? 0x82bd92c0u : 0x822da388u, m, native, s) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || csr != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("mesh math Full72/RAM/host mismatch mode " + std::to_string(mode));
    if (mode < 2) {
        float want[]{0, 0, mode ? 0.f : 1.f, mode ? 0.f : -2.f};
        for (unsigned i = 0; i < 4; ++i)
            if (std::bit_cast<float>(m.ReadU32(Output + 4 * i)) != want[i])
                throw std::runtime_error("triangle plane value");
    } else {
        auto result = std::bit_cast<double>(s.fpr_bits[1]);
        auto expected = std::atan2(angles[mode - 2][0], angles[mode - 2][1]);
        if (std::abs(result - expected) > 1e-12 || std::signbit(result) != std::signbit(expected))
            throw std::runtime_error("guest atan2 value");
    }
}
} // namespace mesh_math_oracle
int main() {
    try {
        const char *path = std::getenv("LO_MESH_MATH_CONSTANTS");
        if (!path)
            throw std::runtime_error(
                "LO_MESH_MATH_CONSTANTS must name a private 184-byte block from guest 83214E88");
        std::ifstream f(path, std::ios::binary);
        f.read(reinterpret_cast<char *>(mesh_math_oracle::constants.data()),
               mesh_math_oracle::constants.size());
        if (f.gcount() != 184 || f.peek() != std::char_traits<char>::eof())
            throw std::runtime_error("private math constant block size");
        for (unsigned i = 0; i < 8; ++i)
            mesh_math_oracle::Check(i);
        std::puts("PASS mesh-geometry-math61 8 original-body cases with private guest constants");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
