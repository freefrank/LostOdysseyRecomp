#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/mesh_mass_math61.h"
#include "lo_semantics/recovery_abi.h"
#include <cmath>
#include <cstdlib>
#include <fstream>
namespace mass_math_oracle {
using Registers = mesh_mass_math61::Registers;
constexpr GuestAddress State = 0x30000, Output = 0x31000, Indices = 0x32000, Positions = 0x33000;
std::array<unsigned char, 128> constants{};
GuestMemory *memory = nullptr;
constexpr std::array<test::Region, 6> Regions{{{0, 0x120000},
                                               {0x82000000, 0x10000},
                                               {0x820d6000, 0x1000},
                                               {0x82051000, 0x1000},
                                               {0x82048000, 0x1000},
                                               {0x832df000, 0x1000}}};
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
        for (unsigned i = 0; i < 72; ++i)
            m.WriteU8(0x820d6a78 + i, constants[i]);
        constexpr GuestAddress extra[]{0x82000fe8, 0x82001010, 0x82051430, 0x82000f70,
                                       0x82048090, 0x82000f28, 0x820d6ac0};
        for (unsigned i = 0; i < 7; ++i)
            for (unsigned j = 0; j < 8; ++j)
                m.WriteU8(extra[i] + j, constants[72 + 8 * i + j]);
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
        if (mode >= 4) {
            m.WriteU32(State + 96, 0);
            m.WriteU32(State + 100, 1);
            m.WriteU32(State + 104, 2);
            dbl(Output + 16, (mode & 1) ? -1 : 1);
            dbl(Output + 24, (mode & 1) ? 2 : -2);
            for (unsigned i = 0; i < 3; ++i)
                m.WriteU32(Output + 32 + 4 * i, (mode & 1) == 0 ? i : 2 - i);
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
        if (mode >= 8) {
            constexpr float xyz[]{0, 1, 2, 2, 1, 2, 2, 3, 2, 0, 3, 2,
                                  0, 1, 4, 2, 1, 4, 2, 3, 4, 0, 3, 4};
            constexpr unsigned indices[]{0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4,
                                         3, 7, 6, 3, 6, 2, 0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5};
            for (unsigned i = 0; i < 8; ++i)
                for (unsigned j = 0; j < 3; ++j)
                    m.WriteU32(Positions + 16 * i + 4 * j,
                               std::bit_cast<std::uint32_t>(xyz[3 * i + j]));
            for (unsigned i = 0; i < 36; ++i) {
                unsigned v = indices[i];
                if (mode == 9) {
                    unsigned j = i % 3;
                    v = indices[i - j + (j == 1 ? 2 : j == 2 ? 1 : 0)];
                    m.WriteU16(Indices + 2 * i, v);
                } else
                    m.WriteU32(Indices + 4 * i, v);
            }
            m.WriteU32(State + 64, 8);
            m.WriteU32(State + 68, 12);
            m.WriteU32(State + 76, mode == 9 ? 6 : 12);
            m.WriteU32(State + 84, Indices);
            m.WriteU32(State + 88, mode == 9 ? 3 : 0);
            m.WriteU32(0x832df548, mode == 10 ? 0 : 1);
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
    if (mode == 9) {
        s.r[3] = State + 64;
        s.r[5] = Output;
        s.fpr_bits[1] = std::bit_cast<std::uint64_t>(2.);
    }
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    auto om = before.Memory();
    memory = &om;
    if (mode == 9)
        __imp__sub_82BCD8A8(c, before.Bytes());
    else if (mode >= 8)
        __imp__sub_82BCD400(c, before.Bytes());
    else if (mode >= 6)
        __imp__sub_82BCD0F8(c, before.Bytes());
    else if (mode >= 4)
        __imp__sub_82BCCE48(c, before.Bytes());
    else if (mode < 2)
        __imp__sub_82BCD300(c, before.Bytes());
    else
        __imp__sub_82BCCCA8(c, before.Bytes());
    memory = nullptr;
    auto csr = PPCFPSCRRegister{}.getcsr();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    auto m = after.Memory();
    (void)mesh_mass_math61::Apply(mode == 9   ? 0x82bcd8a8u
                                  : mode >= 8 ? 0x82bcd400u
                                  : mode >= 6 ? 0x82bcd0f8u
                                  : mode >= 4 ? 0x82bcce48u
                                  : mode < 2  ? 0x82bcd300u
                                              : 0x82bccca8u,
                                  m, native, s);
    if (crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || csr != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("mass math Full72/RAM/CSR mode " + std::to_string(mode));
    auto dbl = [&](GuestAddress p) { return std::bit_cast<double>(recovery_abi::ReadU64(m, p)); };
    if (mode >= 8) {
        if (mode == 10) {
            if (s.r[3] != 0)
                throw std::runtime_error("disabled mass gate");
            return;
        }
        if (s.r[3] != 1 || std::abs(dbl(Output + 16) - 16) > 1e-12)
            throw std::runtime_error("integrated cube mass");
        double center[]{1, 2, 3};
        for (unsigned i = 0; i < 3; ++i)
            if (std::bit_cast<float>(m.ReadU32(Output + 4 * i)) != center[i])
                throw std::runtime_error("volume centroid");
        for (unsigned row = 0; row < 3; ++row)
            for (unsigned col = 0; col < 3; ++col) {
                double centered = row == col ? 32. / 3 : 0;
                double origin = centered + (row == col ? 16 * (14 - center[row] * center[row])
                                                       : -16 * center[row] * center[col]);
                if (std::abs(dbl(Output + 24 + 8 * (3 * row + col)) - origin) > 1e-10 ||
                    std::abs(dbl(Output + 96 + 8 * (3 * row + col)) - centered) > 1e-10)
                    throw std::runtime_error("integrated cube inertia");
            }
        return;
    }
    if (mode >= 6) {
        constexpr double face[]{6, 8, 12, 9, 16, 24, 16.2, 38.4, 48, 7.2, 32, 24};
        for (unsigned i = 0; i < 12; ++i)
            if (std::abs(dbl(State + 192 + 8 * i) - face[i]) > 1e-12)
                throw std::runtime_error("face monomial " + std::to_string(i));
        return;
    }
    if (mode >= 4) {
        constexpr double moments[]{6, 6, 8, 9, 6, 16, 16.2, 7.2, 9.6, 38.4};
        for (unsigned i = 0; i < 10; ++i)
            if (std::abs(dbl(State + 112 + 8 * i) - ((mode & 1) == 0 ? 1 : -1) * moments[i]) >
                1e-12)
                throw std::runtime_error("projected triangle monomial " + std::to_string(i));
        return;
    }
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
void MassSaveGpr(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *mass_math_oracle::memory;
    for (unsigned i = first; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
    m.WriteU32(std::uint32_t(s.r[1] - 8), std::uint32_t(s.r[12]));
}
void MassRestoreGpr(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *mass_math_oracle::memory;
    for (unsigned i = first; i < 32; ++i)
        s.r[i] = recovery_abi::ReadU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)));
    s.r[12] = m.ReadU32(std::uint32_t(s.r[1] - 8));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}
void MassSaveFpr(unsigned first, PPCContext &c, std::uint8_t *) {
    c.fpscr.disableFlushMode();
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *mass_math_oracle::memory;
    for (unsigned i = first; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[12] - 8 * (32 - i)), s.fpr_bits[i]);
}
void MassRestoreFpr(unsigned first, PPCContext &c, std::uint8_t *) {
    c.fpscr.disableFlushMode();
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *mass_math_oracle::memory;
    for (unsigned i = first; i < 32; ++i)
        s.fpr_bits[i] = recovery_abi::ReadU64(m, std::uint32_t(s.r[12] - 8 * (32 - i)));
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        const char *path = std::getenv("LO_MASS_CONSTANTS");
        if (!path)
            throw std::runtime_error("private mass constants required");
        std::ifstream f(path, std::ios::binary);
        f.read(reinterpret_cast<char *>(mass_math_oracle::constants.data()), 128);
        if (f.gcount() != 128)
            throw std::runtime_error("private mass constant size");
        for (unsigned i = 0; i < 11; ++i)
            mass_math_oracle::Check(i);
        std::puts("PASS mesh-mass-math61 11 original mass math and complete volume chain cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
