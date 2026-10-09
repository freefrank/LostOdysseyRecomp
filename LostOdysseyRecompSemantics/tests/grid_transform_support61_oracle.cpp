// Appended to six private original leaf bodies; no lower stubs are required.
#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/grid_transform_support61.h"
#include <bit>
#include <cmath>

namespace transform_support61_oracle {
using Full = grid_transform_support61::Registers;
constexpr GuestAddress Object = 0x30000u, Source = 0x31000u;
constexpr std::array<test::Region, 2> LeafRegions{{{0u, 0x120000u}, {0x82000000u, 0x3000u}}};
struct Native final : grid_transform_support61::NativeServices {
    void SetHostFpControl(std::uint32_t value) override { PPCFPSCRRegister{}.setcsr(value); }
};
struct RestoreHost {
    std::uint32_t value = PPCFPSCRRegister{}.getcsr();
    ~RestoreHost() { PPCFPSCRRegister{}.setcsr(value); }
};
struct LeafCase { GuestAddress entry; double input = 2.25; bool source = true; };
void Check(LeafCase selected) {
    RestoreHost restore;
    test::GuestWindow original(LeafRegions), recovered(LeafRegions);
    const auto seed = [](test::GuestWindow& window) {
        window.Fill(0xa5u); auto memory = window.Memory();
        memory.WriteU32(Source + 4u, 0x12345678u);
        memory.WriteU32(0x82000e50u, std::bit_cast<std::uint32_t>(1.0f));
        recovery_abi::WriteU64(memory, 0x820029c0u, std::bit_cast<std::uint64_t>(0x1p52));
        recovery_abi::WriteU64(memory, 0x82000f28u, std::bit_cast<std::uint64_t>(1.0));
    };
    seed(original); seed(recovered);
    auto memory = recovered.Memory();
    Full initial{};
    for (unsigned i = 0u; i < 32u; ++i) {
        initial.r[i] = 0x1122334400000000ull + i;
        initial.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    initial.r[1] = 0x8877665500080000ull;
    initial.r[3] = 0xaabbccdd00000000ull | Object;
    initial.r[5] = selected.source ? (0x7766554400000000ull | Source) : 0x7766554400000000ull;
    initial.lr = 0x9988776681234567ull;
    initial.cached_fp_control = 0x9fc0u; initial.xer_so = 1u;
    initial.fpr_bits[1] = std::bit_cast<std::uint64_t>(selected.input);
    PPCContext context{}; crt_full_oracle::ToPpc(context, initial);
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    switch (selected.entry) {
    case 0x822c5128u: __imp__sub_822C5128(context, original.Bytes()); break;
    case 0x82f2b308u: __imp__sub_82F2B308(context, original.Bytes()); break;
    case 0x82bd1278u: __imp__sub_82BD1278(context, original.Bytes()); break;
    case 0x82bd78c0u: __imp__sub_82BD78C0(context, original.Bytes()); break;
    case 0x82bd78d8u: __imp__sub_82BD78D8(context, original.Bytes()); break;
    case 0x82bd78e8u: __imp__sub_82BD78E8(context, original.Bytes()); break;
    default: throw std::runtime_error("unknown transform support case");
    }
    const auto expected_host = PPCFPSCRRegister{}.getcsr();
    auto state = initial; Native native;
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if (!grid_transform_support61::Apply(selected.entry, memory, native, state))
        throw std::runtime_error("missing transform support entry");
    const auto expected = crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context));
    const auto actual = crt_full_oracle::Snapshot(state);
    for (unsigned i = 0u; i < expected.size(); ++i)
        if (expected[i] != actual[i]) {
            std::fprintf(stderr, "%08x Full72 index %u expected %016llx actual %016llx\n", selected.entry, i,
                static_cast<unsigned long long>(expected[i]), static_cast<unsigned long long>(actual[i]));
            throw std::runtime_error("transform support register mismatch");
        }
    if (!original.EqualCommitted(recovered) || expected_host != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("transform support RAM/FP-control mismatch");
    if (state.lr != initial.lr || state.r[1] != initial.r[1])
        throw std::runtime_error("leaf unexpectedly changed LR/stack");
    switch (selected.entry) {
    case 0x822c5128u:
        if (state.fpr_bits[1] != std::bit_cast<std::uint64_t>(std::ceil(selected.input)))
            throw std::runtime_error("finite ceil/signed-zero outcome absent");
        break;
    case 0x82f2b308u:
        for (unsigned offset = 0u; offset < 24u; offset += 4u)
            if (memory.ReadU32(Object + offset) != 0u)
                throw std::runtime_error("six-word clear outcome absent");
        if (memory.ReadU8(Object + 24u) != 0xa5u)
            throw std::runtime_error("six-word clear extent mismatch");
        break;
    case 0x82bd1278u:
        if (memory.ReadU32(Object + 8u) != 34u || memory.ReadU32(Object + 16u) != 0xffffffffu ||
            memory.ReadU32(Object + 24u) != 0x01010000u || memory.ReadU8(Object + 28u) != 0xa5u)
            throw std::runtime_error("format defaults/extent mismatch");
        break;
    case 0x82bd78c0u:
        if (memory.ReadU32(Object + 56u) != 0xffffffffu)
            throw std::runtime_error("visit sentinel missing");
        [[fallthrough]];
    case 0x82bd78d8u:
        if (memory.ReadU32(Object) != 0x820d6db8u || memory.ReadU32(Object + 4u) != 0xa5a5a5a5u)
            throw std::runtime_error("visit type/payload preservation mismatch");
        break;
    case 0x82bd78e8u:
        if (state.r[3] != 0u || memory.ReadU32(Object + 4u) != (selected.source ? 0x12345678u : 0xa5a5a5a5u))
            throw std::runtime_error("optional visit copy outcome absent");
        break;
    }
}
}

int main() {
    using transform_support61_oracle::LeafCase;
    try {
        for (const auto selected : std::array{LeafCase{0x822c5128u, 2.25},
            LeafCase{0x822c5128u, -2.25}, LeafCase{0x822c5128u, -0.0},
            LeafCase{0x82f2b308u}, LeafCase{0x82bd1278u}, LeafCase{0x82bd78c0u},
            LeafCase{0x82bd78d8u}, LeafCase{0x82bd78e8u}, LeafCase{0x82bd78e8u, 2.25, false}})
            transform_support61_oracle::Check(selected);
        std::puts("PASS grid-transform-support61 9 focused original-PPC cases"); return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what()); return 1;
    }
}
