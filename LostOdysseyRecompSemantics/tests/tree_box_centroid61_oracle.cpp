#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/tree_box_centroid61.h"
#include <bit>
namespace box_centroid_oracle {
using Registers = tree_box_centroid61::Registers;
constexpr GuestAddress Builder = 0x30000u, Boxes = 0x31000u, Output = 0x32000u;
constexpr std::array<test::Region, 2> Regions{{{0u, 0x120000u}, {0x8201f000u, 0x1000u}}};
struct Native final : float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t value) override { PPCFPSCRRegister{}.setcsr(value); }
};
void Check(unsigned mode) {
    struct Restore {
        std::uint32_t csr = PPCFPSCRRegister{}.getcsr();
        ~Restore() { PPCFPSCRRegister{}.setcsr(csr); }
    } restore;
    test::GuestWindow before(Regions), after(Regions);
    const auto seed = [](test::GuestWindow& window) {
        window.Fill(0xa5u);
        auto memory = window.Memory();
        memory.WriteU32(Builder + 72u, Boxes);
        constexpr std::array<float, 12> values{-8.f, 2.f, -6.f, 4.f, 10.f, 2.f,
                                               -2.f, -4.f, 8.f, 6.f, 2.f, 12.f};
        for (unsigned i = 0; i < values.size(); ++i)
            memory.WriteU32(Boxes + 4u * i, std::bit_cast<std::uint32_t>(values[i]));
        memory.WriteU32(0x8201f9f0u, std::bit_cast<std::uint32_t>(0.5f));
    };
    seed(before); seed(after);
    Registers state{};
    for (unsigned i = 0; i < 32; ++i) {
        state.r[i] = 0x1122334400000000ull + i;
        state.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    state.r[1] = 0x8877665500080000ull;
    state.r[3] = 0xaabbccdd00000000ull | Builder;
    state.r[4] = mode == 0 ? 1u : 0u;
    state.r[5] = mode == 0 ? 1u : mode == 1 ? Output : Boxes + 4u;
    state.lr = 0x9988776681234567ull;
    state.cached_fp_control = 0x9fc0u;
    state.xer_so = 1;
    const auto initial = state;
    PPCContext context{};
    crt_full_oracle::ToPpc(context, state);
    PPCFPSCRRegister{}.setcsr(state.cached_fp_control);
    if (mode == 0) __imp__sub_82BD8848(context, before.Bytes());
    else __imp__sub_82BD8888(context, before.Bytes());
    const auto expectedHost = PPCFPSCRRegister{}.getcsr();
    auto memory = after.Memory(); Native native;
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    const auto entry = mode == 0 ? 0x82bd8848u : 0x82bd8888u;
    if (!tree_box_centroid61::Apply(entry, memory, native, state) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context)) != crt_full_oracle::Snapshot(state) ||
        !before.EqualCommitted(after) || expectedHost != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("box centroid Full72/RAM/host mismatch");
    if (mode == 0) {
        if (std::bit_cast<double>(state.fpr_bits[1]) != -1.)
            throw std::runtime_error("axis centroid value");
    } else {
        constexpr std::array<float, 3> expected{-2.f, 6.f, -2.f};
        for (unsigned i = 0; i < expected.size(); ++i)
            if (memory.ReadU32(Address(initial.r[5]) + 4u * i) != std::bit_cast<std::uint32_t>(expected[i]))
                throw std::runtime_error("triplet centroid/overlap value");
    }
}
}
int main() {
    try {
        for (unsigned mode = 0; mode < 3; ++mode) box_centroid_oracle::Check(mode);
        std::puts("PASS tree-box-centroid61 3 actual-body cases"); return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what()); return 1;
    }
}
