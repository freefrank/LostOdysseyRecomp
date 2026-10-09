// Original BB06D8 plus the three closed visit-record leaves are pinned.
#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/object_grid_transform61.h"
#include <bit>

namespace grid_transform61_oracle {
using Full = object_grid_transform61::Registers;
constexpr GuestAddress Grid = 0x30000u, Words = 0x32000u, Metadata = 0x33000u;
constexpr std::array<test::Region, 1> GridRegions{{{0u, 0x120000u}}};
GuestMemory* original_memory = nullptr;
struct Native final : object_grid_transform61::NativeServices {
    void SetHostFpControl(std::uint32_t value) override { PPCFPSCRRegister{}.setcsr(value); }
};
struct RestoreHost {
    std::uint32_t value = PPCFPSCRRegister{}.getcsr();
    ~RestoreHost() { PPCFPSCRRegister{}.setcsr(value); }
};
void Seed(test::GuestWindow& window, unsigned dimension, int unmarked_corner) {
    window.Fill(0xa5u); auto memory = window.Memory();
    memory.WriteU32(Grid + 88u, dimension);
    memory.WriteU32(Grid + 92u, dimension * dimension);
    memory.WriteU32(Grid + 108u, Words);
    memory.WriteU32(Metadata + 4u, 0x12345678u);
    constexpr std::array<float, 3> origin{10.0f,20.0f,-5.0f};
    constexpr std::array<float, 3> bias{0.2f,0.3f,-0.4f};
    constexpr std::array<float, 3> scale{0.75f,1.25f,1.5f};
    for (unsigned axis = 0u; axis < 3u; ++axis) {
        memory.WriteU32(Grid + 28u + axis * 4u, std::bit_cast<std::uint32_t>(origin[axis]));
        memory.WriteU32(Grid + 40u + axis * 4u, std::bit_cast<std::uint32_t>(bias[axis]));
        memory.WriteU32(Grid + 76u + axis * 4u, std::bit_cast<std::uint32_t>(scale[axis]));
    }
    for (unsigned i = 0u; i < dimension * dimension * dimension; ++i)
        memory.WriteU32(Words + 4u * i, unmarked_corner >= 0 && int(i) != unmarked_corner ? 0x80000007u : 0x123u);
}
void Check(unsigned dimension, int marked) {
    RestoreHost restore;
    test::GuestWindow original(GridRegions), recovered(GridRegions);
    Seed(original, dimension, marked); Seed(recovered, dimension, marked);
    auto expected_memory = original.Memory(), memory = recovered.Memory();
    Full initial{};
    for (unsigned i = 0u; i < 32u; ++i) {
        initial.r[i] = 0x1122334400000000ull + i;
        initial.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    initial.r[1] = 0x8877665500080000ull;
    initial.r[3] = 0xaabbccdd00000000ull | Grid;
    initial.r[4] = 0x7766554400000000ull | Metadata;
    initial.lr = 0x9988776681234567ull;
    initial.cached_fp_control = 0x9fc0u; initial.xer_so = 1u;
    PPCContext context{}; crt_full_oracle::ToPpc(context, initial);
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    original_memory = &expected_memory;
    __imp__sub_82BB06D8(context, original.Bytes());
    original_memory = nullptr;
    const auto expected_host = PPCFPSCRRegister{}.getcsr();
    auto state = initial; Native native;
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if (!object_grid_transform61::Apply(0x82bb06d8u, memory, native, state))
        throw std::runtime_error("missing grid transform entry");
    const auto expected = crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context));
    const auto actual = crt_full_oracle::Snapshot(state);
    for (unsigned i = 0u; i < expected.size(); ++i)
        if (expected[i] != actual[i]) {
            std::fprintf(stderr, "grid-transform dimension %u marker %d Full72 %u expected %016llx actual %016llx\n",
                dimension, marked, i, static_cast<unsigned long long>(expected[i]), static_cast<unsigned long long>(actual[i]));
            throw std::runtime_error("grid transform register mismatch");
        }
    if (!original.EqualCommitted(recovered)) throw std::runtime_error("grid transform RAM mismatch");
    if (expected_host != PPCFPSCRRegister{}.getcsr()) throw std::runtime_error("grid transform FP-control mismatch");
    const auto count = dimension * dimension * dimension;
    const auto qualified = marked == 0 ? 1u : count;
    if (state.r[3] != qualified || state.r[1] != initial.r[1] || state.lr != 0x81234567u)
        throw std::runtime_error("grid transform count/frame outcome mismatch");
    for (unsigned i = 0u; i < count; ++i) {
        const auto value = marked >= 0 && int(i) != marked ? 0xffffffffu : 0x123u;
        if (memory.ReadU32(Words + 4u * i) != value)
            throw std::runtime_error("grid transform marked propagation mismatch");
    }
    const auto frame = Address(initial.r[1] - 608u);
    if (memory.ReadU32(frame + 224u) != 0x820d6db8u || memory.ReadU32(frame + 280u) != 0xffffffffu)
        throw std::runtime_error("grid transform visit-record lifetime mismatch");
    if (marked >= 0 && memory.ReadU32(frame + 228u) != 0x12345678u)
        throw std::runtime_error("grid transform metadata copy absent");
}
}

void OriginalGridTransform61Save(unsigned first, PPCContext& context, std::uint8_t*) {
    const auto state = crt_full_oracle::FromPpc(context);
    auto& memory = *grid_transform61_oracle::original_memory;
    for (unsigned i = first; i < 32u; ++i)
        recovery_abi::WriteU64(memory, Address(state.r[1] - 16u - 8u * (31u - i)), state.r[i]);
    memory.WriteU32(Address(state.r[1] - 8u), Address(state.r[12]));
}
void OriginalGridTransform61Restore(unsigned first, PPCContext& context, std::uint8_t*) {
    auto state = crt_full_oracle::FromPpc(context);
    auto& memory = *grid_transform61_oracle::original_memory;
    for (unsigned i = first; i < 32u; ++i)
        state.r[i] = recovery_abi::ReadU64(memory, Address(state.r[1] - 16u - 8u * (31u - i)));
    state.r[12] = memory.ReadU32(Address(state.r[1] - 8u)); state.lr = state.r[12];
    crt_full_oracle::ToPpc(context, state);
}
int main() {
    try {
        grid_transform61_oracle::Check(0u, -1);
        grid_transform61_oracle::Check(2u, -1);
        grid_transform61_oracle::Check(2u, 0);
        grid_transform61_oracle::Check(2u, 7);
        std::puts("PASS object-grid-transform61 4 focused original-PPC cases"); return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what()); return 1;
    }
}
