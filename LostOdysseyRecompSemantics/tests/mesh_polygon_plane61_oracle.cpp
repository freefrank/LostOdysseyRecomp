#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/mesh_polygon_plane61.h"
#include "lo_semantics/recovery_abi.h"
#include <cmath>
namespace polygon_plane_oracle {
using Registers = mesh_polygon_plane61::Registers;
constexpr GuestAddress Output = 0x30000, Indices = 0x31000, Positions = 0x32000;
constexpr std::array<test::Region, 2> Regions{{{0, 0x120000}, {0x82000000, 0x10000}}};
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
    const unsigned count = mode < 3 ? mode + 3 : mode == 3 ? 0 : 5;
    auto seed = [&](test::GuestWindow &w) {
        w.Fill(0);
        auto m = w.Memory();
        constexpr float pts[]{0, 0, 2, 2, 0, 2, 3, 1, 2, 2, 3, 2, 0, 2, 2};
        for (unsigned i = 0; i < 15; ++i)
            m.WriteU32(Positions + 4 * i, std::bit_cast<std::uint32_t>(pts[i]));
        for (unsigned i = 0; i < 5; ++i)
            m.WriteU8(Indices + i, i);
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
    s.r[3] = mode == 4 ? count : Output;
    s.r[4] = mode == 4 ? Indices : count;
    s.r[5] = Indices;
    s.r[6] = Positions;
    auto om = before.Memory();
    memory = &om;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (mode == 4)
        __imp__sub_82BC3880(c, before.Bytes());
    else
        __imp__sub_82BD9390(c, before.Bytes());
    auto csr = PPCFPSCRRegister{}.getcsr();
    memory = nullptr;
    auto m = after.Memory();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    (void)mesh_polygon_plane61::Apply(mode == 4 ? 0x82bc3880u : 0x82bd9390u, m, native, s);
    if (crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || csr != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("polygon plane Full72/RAM/CSR mode " + std::to_string(mode));
    if (s.r[3] != (mode == 3 ? 0u : 1u))
        throw std::runtime_error("polygon plane result");
    if (mode < 3) {
        constexpr float want[]{0, 0, 1, -2};
        for (unsigned i = 0; i < 4; ++i)
            if (std::abs(std::bit_cast<float>(m.ReadU32(Output + 4 * i)) - want[i]) > 1e-6f)
                throw std::runtime_error("polygon plane expected");
    }
    if (mode == 4)
        for (unsigned i = 0; i < 5; ++i)
            if (m.ReadU8(Indices + i) != 4 - i)
                throw std::runtime_error("polygon reverse");
}
} // namespace polygon_plane_oracle
void PolygonPlaneSave(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *polygon_plane_oracle::memory;
    for (unsigned i = 26; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
    m.WriteU32(std::uint32_t(s.r[1] - 8), std::uint32_t(s.r[12]));
}
void PolygonPlaneRestore(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *polygon_plane_oracle::memory;
    for (unsigned i = 26; i < 32; ++i)
        s.r[i] = recovery_abi::ReadU64(m, std::uint32_t(s.r[1] - 16 - 8 * (31 - i)));
    s.r[12] = m.ReadU32(std::uint32_t(s.r[1] - 8));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}
void PolygonPlaneSaveFp(PPCContext &c, std::uint8_t *) {
    c.fpscr.disableFlushMode();
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *polygon_plane_oracle::memory;
    for (unsigned i = 21; i < 32; ++i)
        recovery_abi::WriteU64(m, std::uint32_t(s.r[12] - 8 * (32 - i)), s.fpr_bits[i]);
}
void PolygonPlaneRestoreFp(PPCContext &c, std::uint8_t *) {
    c.fpscr.disableFlushMode();
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *polygon_plane_oracle::memory;
    for (unsigned i = 21; i < 32; ++i)
        s.fpr_bits[i] = recovery_abi::ReadU64(m, std::uint32_t(s.r[12] - 8 * (32 - i)));
    crt_full_oracle::ToPpc(c, s);
}

int main() {
    try {
        for (unsigned i = 0; i < 5; ++i)
            polygon_plane_oracle::Check(i);
        std::puts("PASS mesh-polygon-plane61 5 complete-original-body cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
