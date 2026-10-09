#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/grid_neighbor_update61.h"
#include "lo_semantics/recovery_abi.h"

namespace grid_neighbor_update61_oracle {
using Registers = grid_neighbor_update61::Registers;
constexpr GuestAddress Object = 0x30000u, Buffer = 0x32000u;
constexpr std::array<test::Region, 1> Regions{{{0u, 0x120000u}}};
GuestMemory* active_memory = nullptr;
void Check(unsigned mode) {
    const unsigned side = mode == 0u ? 0u : 2u;
    std::array<std::uint32_t, 8> words{};
    for (unsigned i = 0; i < words.size(); ++i) words[i] = 0x80000100u + i;
    if (mode == 2u) words[7] &= ~0x80000000u;
    if (mode == 3u) words[1] &= ~0x80000000u;
    test::GuestWindow original(Regions), recovered(Regions);
    const auto seed = [&](test::GuestWindow& window) {
        window.Fill(0xa5u); auto memory = window.Memory();
        memory.WriteU32(Object + 88u, side);
        memory.WriteU32(Object + 92u, side * side);
        memory.WriteU32(Object + 108u, Buffer);
        for (unsigned i = 0; i < words.size(); ++i) memory.WriteU32(Buffer + 4u * i, words[i]);
    };
    seed(original); seed(recovered);
    auto original_memory = original.Memory(), recovered_memory = recovered.Memory();
    Registers initial{};
    for (unsigned i = 0; i < 32u; ++i) {
        initial.r[i] = 0x1122334400000000ull + i;
        initial.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    initial.r[1] = 0x8877665500080000ull;
    initial.r[3] = 0xaabbccdd00030000ull;
    initial.lr = 0x9988776681234567ull;
    initial.xer_so = 1u; initial.cached_fp_control = 0x1f80u;
    PPCContext context{}; crt_full_oracle::ToPpc(context, initial);
    auto state = initial;
    active_memory = &original_memory;
    __imp__sub_82BB23C0(context, original.Bytes());
    active_memory = nullptr;
    if (!grid_neighbor_update61::Apply(0x82bb23c0u, recovered_memory, state))
        throw std::runtime_error("missing grid neighbor entry");
    if (crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context)) != crt_full_oracle::Snapshot(state) ||
        !original.EqualCommitted(recovered))
        throw std::runtime_error("grid neighbor Full72/RAM mismatch");
    if (state.r[3] != 1u || state.r[1] != initial.r[1] || state.lr != 0x81234567u)
        throw std::runtime_error("grid neighbor return/frame mismatch");
    for (unsigned i = 25u; i <= 31u; ++i)
        if (state.r[i] != initial.r[i] || recovery_abi::ReadU64(recovered_memory,
            recovery_abi::Address(initial.r[1] - 8u * (33u - i))) != initial.r[i])
            throw std::runtime_error("grid neighbor saved register mismatch");
    // Independently describe the four fixture outcomes, without reimplementing
    // the corner walker: far-corner blocker rejects every cell of the 2-cube;
    // an x-neighbor blocker rejects only the first two cells.
    for (unsigned i = 0; i < words.size(); ++i) {
        const bool marked = mode == 1u || (mode == 3u && i >= 2u);
        const auto expected = words[i] | (marked ? 0x40000000u : 0u);
        if (recovered_memory.ReadU32(Buffer + 4u * i) != expected)
            throw std::runtime_error("grid neighbor marker outcome mismatch");
    }
}
}
void OriginalGridNeighbor61Save(PPCContext& context) {
    const auto regs = crt_full_oracle::Gprs(context);
    auto& memory = *grid_neighbor_update61_oracle::active_memory;
    for (unsigned i = 25u; i <= 31u; ++i)
        recovery_abi::WriteU64(memory, recovery_abi::Address(context.r1.u64 - 8u * (33u - i)), regs[i]->u64);
    memory.WriteU32(recovery_abi::Address(context.r1.u64 - 8u), context.r12.u32);
}
void OriginalGridNeighbor61Restore(PPCContext& context) {
    const auto regs = crt_full_oracle::Gprs(context);
    auto& memory = *grid_neighbor_update61_oracle::active_memory;
    for (unsigned i = 25u; i <= 31u; ++i)
        regs[i]->u64 = recovery_abi::ReadU64(memory, recovery_abi::Address(context.r1.u64 - 8u * (33u - i)));
    context.r12.u64 = memory.ReadU32(recovery_abi::Address(context.r1.u64 - 8u));
    context.lr = context.r12.u64;
}
int main() {
    try {
        for (unsigned mode = 0; mode < 4u; ++mode) grid_neighbor_update61_oracle::Check(mode);
        std::puts("PASS grid-neighbor-update61 4 focused PPC cases"); return 0;
    } catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
