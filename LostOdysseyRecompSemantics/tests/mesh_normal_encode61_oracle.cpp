#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/mesh_normal_encode61.h"
#include "lo_semantics/recovery_abi.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
namespace normal_oracle {
using Registers = mesh_normal_encode61::Registers;
constexpr GuestAddress Output = 0x30000, Constants = 0x83214e08;
constexpr std::array<test::Region, 5> Regions{{{0, 0x120000},
                                               {0x82000000, 0x10000},
                                               {0x820d6000, 0x1000},
                                               {0x82218000, 0x1000},
                                               {0x83214000, 0x1000}}};
std::array<unsigned char, 144> constants{};
GuestMemory *memory = nullptr;
struct Native final : float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t v) override { PPCFPSCRRegister{}.setcsr(v); }
} native;
void Check(unsigned mode) {
    struct Restore {
        std::uint32_t csr = PPCFPSCRRegister{}.getcsr();
        ~Restore() { PPCFPSCRRegister{}.setcsr(csr); }
    } restore;
    test::GuestWindow before(Regions), after(Regions);
    auto seed = [](test::GuestWindow &w) {
        w.Fill(0);
        auto m = w.Memory();
        for (unsigned i = 0; i < 128; ++i)
            m.WriteU8(Constants + i, constants[i]);
        constexpr GuestAddress extra[]{0x820d60a8, 0x820d6454, 0x82000e40, 0x822181c4};
        for (unsigned i = 0; i < 4; ++i)
            for (unsigned j = 0; j < 4; ++j)
                m.WriteU8(extra[i] + j, constants[128 + 4 * i + j]);
        m.WriteU32(0x82000e50, 0);
        m.WriteU32(0x82007784, std::bit_cast<std::uint32_t>(1.f));
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
    s.r[6] = Output;
    s.r[7] = Output + 4;
    s.r[8] = Output + 8;
    s.r[9] = Output + 12;
    s.r[10] = 8;
    constexpr double vectors[][3]{{0, 0, 1}, {0.6, -0.8, 0}, {-1. / 3, 2. / 3, -2. / 3}};
    if (mode < 3)
        for (unsigned i = 0; i < 3; ++i)
            s.fpr_bits[i + 1] = std::bit_cast<std::uint64_t>(vectors[mode][i]);
    else
        s.fpr_bits[1] = std::bit_cast<std::uint64_t>(-0.25);
    auto om = before.Memory();
    memory = &om;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (mode < 3)
        __imp__sub_82BB8FA0(c, before.Bytes());
    else
        __imp__sub_82325048(c, before.Bytes());
    auto csr = PPCFPSCRRegister{}.getcsr();
    memory = nullptr;
    auto m = after.Memory();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (!mesh_normal_encode61::Apply(mode < 3 ? 0x82bb8fa0u : 0x82325048u, m, native, s) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || csr != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("normal encoding Full72/RAM/host mismatch mode " +
                                 std::to_string(mode));
    if (mode == 3) {
        if (std::abs(std::bit_cast<double>(s.fpr_bits[1]) - std::asin(-0.25)) > 1e-12)
            throw std::runtime_error("guest arcsine");
        return;
    }
    unsigned sign = 0, order = 0;
    double v[3];
    for (unsigned i = 0; i < 3; ++i) {
        v[i] = std::abs(vectors[mode][i]);
        if (vectors[mode][i] < 0)
            sign |= 1u << i;
    }
    if (v[0] > v[1])
        order |= 1;
    if (v[1] > v[2])
        order |= 2;
    if (v[2] > v[0])
        order |= 4;
    if (m.ReadU32(Output) != order || m.ReadU32(Output + 4) != sign)
        throw std::runtime_error("normal sign/order masks");
    // The source retains min(|x|,|y|) for the second angle; the first
    // uses min(max(|x|,|y|),|z|), not a sorted pair of the smaller values.
    const double second = std::min(v[0], v[1]);
    const double first = std::min(std::max(v[0], v[1]), v[2]);
    double scale = 1020.0 / std::acos(-1.0);
    int a = int(std::asin(first) * scale),
        b = int(std::asin(second / std::sqrt(1 - first * first)) * scale);
    if (std::abs(int(m.ReadU32(Output + 8)) - a) > 1 ||
        std::abs(int(m.ReadU32(Output + 12)) - b) > 1)
        throw std::runtime_error("normal quantized angles");
}
} // namespace normal_oracle
void NormalSave(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *normal_oracle::memory;
    for (unsigned i = 28; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
    m.WriteU32(std::uint32_t(s.r[1] - 8), std::uint32_t(s.r[12]));
}
void NormalRestore(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *normal_oracle::memory;
    for (unsigned i = 28; i < 32; ++i)
        s.r[i] = recovery_abi::ReadU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)));
    s.r[12] = m.ReadU32(std::uint32_t(s.r[1] - 8));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}
void NormalSaveFp(PPCContext &c, std::uint8_t *) {
    c.fpscr.disableFlushMode();
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *normal_oracle::memory;
    for (unsigned i = 25; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[12] - 8 * (32 - i)), s.fpr_bits[i]);
}
void NormalRestoreFp(PPCContext &c, std::uint8_t *) {
    c.fpscr.disableFlushMode();
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *normal_oracle::memory;
    for (unsigned i = 25; i < 32; ++i)
        s.fpr_bits[i] = recovery_abi::ReadU64(m, std::uint32_t(s.r[12] - 8 * (32 - i)));
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        const char *path = std::getenv("LO_NORMAL_ENCODING_CONSTANTS");
        if (!path)
            throw std::runtime_error(
                "LO_NORMAL_ENCODING_CONSTANTS private 144-byte bundle required");
        std::ifstream f(path, std::ios::binary);
        f.read(reinterpret_cast<char *>(normal_oracle::constants.data()), 144);
        if (f.gcount() != 144)
            throw std::runtime_error("private normal constant bundle size");
        for (unsigned i = 0; i < 4; ++i)
            normal_oracle::Check(i);
        std::puts("PASS mesh-normal-encode61 4 original encoder/arcsine cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
