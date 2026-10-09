#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/mesh_bounds_math61.h"
#include "lo_semantics/recovery_abi.h"
#include <cmath>
namespace bounds_oracle {
using Registers = mesh_bounds_math61::Registers;
constexpr GuestAddress Output = 0x30000, Points = 0x31000;
constexpr std::array<test::Region, 3> Regions{
    {{0, 0x120000}, {0x82000000, 0x10000}, {0x8201f000, 0x1000}}};
GuestMemory *original_memory = nullptr;
struct Native final : float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t v) override { PPCFPSCRRegister{}.setcsr(v); }
};
void Check(unsigned count) {
    struct Restore {
        std::uint32_t csr = PPCFPSCRRegister{}.getcsr();
        ~Restore() { PPCFPSCRRegister{}.setcsr(csr); }
    } restore;
    test::GuestWindow before(Regions), after(Regions);
    const auto seed = [&](test::GuestWindow &w) {
        w.Fill(0xa5);
        auto m = w.Memory();
        constexpr float points[]{-2, 1, 0, 6, 1, 0, 2, 5, 0, 2, -3, 0, 2, 1, 8, 1, 1, 0, 2, 1, -4};
        for (unsigned i = 0; i < 21; ++i)
            m.WriteU32(Points + 4 * i, std::bit_cast<std::uint32_t>(points[i]));
        m.WriteU32(0x82000d64, 0xff7fffff);
        m.WriteU32(0x82000e0c, 0x7f7fffff);
        m.WriteU32(0x82007784, std::bit_cast<std::uint32_t>(1.f));
        m.WriteU32(0x8201f9f0, std::bit_cast<std::uint32_t>(.5f));
    };
    seed(before);
    seed(after);
    Registers s{};
    for (unsigned i = 0; i < 32; ++i) {
        s.r[i] = 0x1122334400000000ull + i;
        s.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    s.r[1] = 0x8877665500080000ull;
    s.r[3] = Output;
    s.r[4] = count;
    s.r[5] = count ? Points : 0;
    s.lr = 0x9988776681234567ull;
    s.xer_so = 1;
    s.cached_fp_control = 0x9fc0;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    auto om = before.Memory();
    original_memory = &om;
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    __imp__sub_82BC9040(c, before.Bytes());
    auto host = PPCFPSCRRegister{}.getcsr();
    Native native;
    auto m = after.Memory();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    (void)mesh_bounds_math61::Apply(0x82bc9040u, m, native, s);
    auto a = crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),
         b = crt_full_oracle::Snapshot(s);
    if (a != b || !before.EqualCommitted(after) || host != PPCFPSCRRegister{}.getcsr()) {
        std::fprintf(stderr, "sphere count%u Full%d RAM%d CSR%d\n", count, a == b,
                     before.EqualCommitted(after), host == PPCFPSCRRegister{}.getcsr());
        for (unsigned i = 0; i < a.size(); ++i)
            if (a[i] != b[i])
                std::fprintf(stderr, "field%u %llx/%llx\n", i, (unsigned long long)a[i],
                             (unsigned long long)b[i]);
        throw std::runtime_error("sphere original mismatch");
    }
    if (s.r[3] != (count ? 1u : 0u))
        throw std::runtime_error("sphere result");
    auto value = [&](GuestAddress p) { return std::bit_cast<float>(m.ReadU32(p)); };
    if (count) {
        auto radius = value(Output + 12);
        for (unsigned i = 0; i < count; ++i) {
            double d = 0;
            for (unsigned axis = 0; axis < 3; ++axis) {
                double delta = value(Points + 12 * i + 4 * axis) - value(Output + 4 * axis);
                d += delta * delta;
            }
            if (std::sqrt(d) > radius + 1e-5)
                throw std::runtime_error("sphere enclosure");
        }
        if (count == 1 && (value(Output) != -2 || value(Output + 4) != 1 ||
                           value(Output + 8) != 0 || radius != 0))
            throw std::runtime_error("single point sphere");
        if (count == 4 &&
            (value(Output) != 2 || value(Output + 4) != 1 || value(Output + 8) != 0 || radius != 4))
            throw std::runtime_error("planar four point sphere");
    }
}
} // namespace bounds_oracle
void SphereSave(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    for (unsigned i = 19; i < 32; ++i)
        WriteU64(*bounds_oracle::original_memory, Address(s.r[12] - 8 * (32 - i)), s.fpr_bits[i]);
}
void SphereRestore(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    for (unsigned i = 19; i < 32; ++i)
        s.fpr_bits[i] = ReadU64(*bounds_oracle::original_memory, Address(s.r[12] - 8 * (32 - i)));
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        for (unsigned n : {0u, 1u, 3u, 4u, 7u})
            bounds_oracle::Check(n);
        std::puts("PASS mesh-bounds-math61 5 original leaf / independent sphere cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
