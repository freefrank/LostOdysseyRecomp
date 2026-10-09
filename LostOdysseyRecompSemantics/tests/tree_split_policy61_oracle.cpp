#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/tree_split_policy61.h"
#include <bit>
namespace split_policy_oracle {
using Registers = tree_split_policy61::Registers;
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
        memory.WriteU32(Builder + 4u, 8u);
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
    state.r[4] = 0;
    state.r[6] = Boxes;
    state.r[7] = 1;
    state.r[5] = 6u + mode;
    state.lr = 0x9988776681234567ull;
    state.cached_fp_control = 0x9fc0u;
    state.xer_so = 1;
    const auto initial = state;
    PPCContext context{};
    crt_full_oracle::ToPpc(context, state);
    PPCFPSCRRegister{}.setcsr(state.cached_fp_control);
    if (mode == 0) __imp__sub_82BB3B60(context, before.Bytes());
    else __imp__sub_82BB3B88(context, before.Bytes());
    const auto expectedHost = PPCFPSCRRegister{}.getcsr();
    auto memory = after.Memory(); Native native;
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    const auto entry = mode == 0 ? 0x82bb3b60u : 0x82bb3b88u;
    if (!tree_split_policy61::Apply(entry, memory, native, state) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context)) != crt_full_oracle::Snapshot(state) ||
        !before.EqualCommitted(after) || expectedHost != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("split policy Full72/RAM/host mismatch");
    if (mode == 0) {
        if (std::bit_cast<double>(state.fpr_bits[1]) != 6.)
            throw std::runtime_error("midpoint value");
    } else if (state.r[3] != (mode == 3 ? 1u : 0u) || state.xer_ca != (mode == 3 ? 0u : 1u)) {
        throw std::runtime_error("unsigned threshold/carry result");
    }
}
}
int main() {
    try {
        for (unsigned mode = 0; mode < 4; ++mode) split_policy_oracle::Check(mode);
        std::puts("PASS tree-split-policy61 4 actual-body cases"); return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what()); return 1;
    }
}
