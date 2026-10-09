// Appended to the two pinned bodies by semantic_recovery.py.
#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/crt_reader_sort_float61.h"
#include <bit>

namespace sort_float61_oracle {
using Registers = crt_reader_sort_float61::Registers;
constexpr GuestAddress Reader = 0x30000u, Payload = 0x32000u;
constexpr GuestAddress TableGlobal = 0x832df554u, Table = 0x83216624u;
constexpr GuestAddress Vtable = 0x34000u, Target = 0x2a00u;
constexpr GuestAddress Threshold = 0x82000e50u;
constexpr std::array<test::Region, 4> Regions{{{0u, 0x120000u},
    {0x82000000u, 0x1000u}, {0x83216000u, 0x1000u}, {0x832df000u, 0x1000u}}};
struct RestoreHost {
    std::uint32_t control = PPCFPSCRRegister{}.getcsr();
    ~RestoreHost() { PPCFPSCRRegister{}.setcsr(control); }
};
struct Guest final : crt_close_recursive_buffer_context::GuestServices {
    std::vector<std::array<std::uint64_t, 72>> events;
    void CallIndirect(GuestAddress target, GuestMemory& memory, Registers& state) override {
        if (target != Target || state.ctr != (Target | 3u))
            throw std::runtime_error("wrong cleanup table slot/alignment");
        events.push_back(crt_full_oracle::Snapshot(state));
        if (state.r[3] != 0xffffffff83216624ull || state.r[4] != Payload ||
            state.lr != 0x82bd2a7cu || state.r[1] != 0x887766550007ff90ull)
            throw std::runtime_error("wrong cleanup receiver/payload/frame");
        memory.WriteU32(Payload, 0xfeed1234u);
        // Exercise mutable call boundaries, including the two live registers
        // used by the reset stores. The saved caller r30/r31 still restore.
        state.r[31] += 0x100u;
        state.r[30] = 0x1122334400000055ull;
        state.r[8] ^= 0xabcdef1234567890ull;
        state.fpr_bits[0] = 0x4010000000000000ull;
        state.fpr_bits[13] = 0x4020000000000000ull;
        state.fpr_bits[7] ^= 0x180u;
        state.cr1.gt ^= 1u; state.cr7.eq ^= 1u; state.xer_ca ^= 1u;
        state.cached_fp_control = 0x9fc0u;
        PPCFPSCRRegister{}.setcsr(state.cached_fp_control);
    }
};
struct Native final : float_triplet_transfer::NativeServices {
    std::vector<std::uint32_t> controls;
    void SetHostFpControl(std::uint32_t value) override {
        controls.push_back(value); PPCFPSCRRegister{}.setcsr(value);
    }
};
Guest* active_guest = nullptr;
GuestMemory* active_memory = nullptr;
void Check(unsigned mode) {
    RestoreHost restore;
    test::GuestWindow original(Regions), recovered(Regions);
    const auto seed = [mode](test::GuestWindow& window) {
        window.Fill(0xa5u); auto memory = window.Memory();
        memory.WriteU32(Reader, 11u); memory.WriteU32(Reader + 4u, 22u);
        memory.WriteU32(Reader + 8u, mode == 1u ? 0u : Payload);
        memory.WriteU32(Reader + 12u, std::bit_cast<std::uint32_t>(mode == 0u ? 0.5f : 1.0f));
        memory.WriteU32(Threshold, std::bit_cast<std::uint32_t>(1.0f));
        memory.WriteU32(TableGlobal, 0u); memory.WriteU32(Table, Vtable);
        memory.WriteU32(Vtable + 12u, Target | 3u);
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
    initial.lr = 0x9988776681234567ull; initial.cached_fp_control = 0x9fc0u;
    initial.xer_so = 1u;
    PPCContext context{}; crt_full_oracle::ToPpc(context, initial);
    auto state = initial; Guest expected, actual; Native native;
    active_guest = &expected; active_memory = &original_memory;
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    __imp__sub_82BD2A28(context, original.Bytes());
    const auto expected_host = PPCFPSCRRegister{}.getcsr();
    active_guest = nullptr; active_memory = nullptr;
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if (!crt_reader_sort_float61::Apply(0x82bd2a28u, recovered_memory, {actual, native}, state))
        throw std::runtime_error("missing cleanup entry");
    if (crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context)) != crt_full_oracle::Snapshot(state) ||
        !original.EqualCommitted(recovered) || expected.events != actual.events ||
        expected_host != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("cleanup Full72/RAM/callback/host-control mismatch");
    const auto output = mode == 2u ? Reader + 0x100u : Reader;
    const auto reset_value = mode == 2u ? 0x55u : 0u;
    if (state.r[3] != initial.r[3] + (mode == 2u ? 0x100u : 0u) ||
        state.r[1] != initial.r[1] || state.lr != 0x81234567u ||
        state.r[30] != initial.r[30] || state.r[31] != initial.r[31] ||
        recovered_memory.ReadU32(output) != reset_value ||
        recovered_memory.ReadU32(output + 4u) != reset_value ||
        recovered_memory.ReadU32(Reader + 8u) != (mode == 1u ? 0u : Payload) ||
        actual.events.size() != (mode == 2u ? 1u : 0u) ||
        native.controls != std::vector<std::uint32_t>{0x1f80u} ||
        (mode == 2u && (recovered_memory.ReadU32(output + 8u) != reset_value ||
            recovered_memory.ReadU32(Payload) != 0xfeed1234u)))
        throw std::runtime_error("cleanup branch/live-state/restore outcome absent");
}
}
void OriginalSortFloat61Indirect(GuestAddress target, PPCContext& context, std::uint8_t*) {
    auto state = crt_full_oracle::FromPpc(context);
    sort_float61_oracle::active_guest->CallIndirect(target, *sort_float61_oracle::active_memory, state);
    crt_full_oracle::ToPpc(context, state);
}
int main() {
    try {
        for (unsigned mode = 0; mode < 3u; ++mode) sort_float61_oracle::Check(mode);
        std::puts("PASS crt-reader-sort-float61 3 focused PPC cases"); return 0;
    } catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
