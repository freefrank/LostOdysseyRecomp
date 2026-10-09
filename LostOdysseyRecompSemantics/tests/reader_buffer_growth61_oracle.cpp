// Append to genuine selected PPC bodies: BD0798, B7A0B0, BD2870.
#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/reader_buffer_growth61.h"
#include <bit>

namespace reader_growth61_oracle {
using Registers = reader_buffer_growth61::Registers;
constexpr GuestAddress Reader = 0x30000u, Old = 0x32000u, Fresh = 0x33000u;
constexpr GuestAddress Global = 0x832df554u, Table = 0x83216624u, Vtable = 0x34000u;
constexpr GuestAddress Allocate = 0x2a00u, Dispose = 0x2b00u, Threshold = 0x82000e50u;
constexpr std::array<test::Region, 4> Regions{{{0u, 0x120000u},
    {0x82000000u, 0x1000u}, {0x83216000u, 0x1000u}, {0x832df000u, 0x1000u}}};
struct RestoreHost {
    std::uint32_t control = PPCFPSCRRegister{}.getcsr();
    ~RestoreHost() { PPCFPSCRRegister{}.setcsr(control); }
};
struct Guest final : crt_close_recursive_buffer_context::GuestServices {
    unsigned mode = 0;
    std::vector<std::array<std::uint64_t, 72>> events;
    void CallIndirect(GuestAddress target, GuestMemory& memory, Registers& state) override {
        events.push_back(crt_full_oracle::Snapshot(state));
        if (target == Allocate) {
            if (state.r[4] != (mode == 1u ? 12u : 32u) || state.r[5] != 64u ||
                state.lr != 0x82bd2918u)
                throw std::runtime_error("wrong capacity/alignment/allocation boundary");
            state.r[3] = mode == 2u ? 0u : Fresh;
        } else if (target == Dispose) {
            if (state.r[4] != Old || state.lr != 0x82bd2960u ||
                memory.ReadU32(Fresh) != 0x12345678u ||
                memory.ReadU32(Fresh + 4u) != 0x90abcdefu)
                throw std::runtime_error("missing copy before disposal");
            // Disposal's mutable state is used by the final payload store.
            state.r[30] = Fresh + 16u;
            state.r[31] += 0x100u;
            memory.WriteU32(Old, 0xdeadbeefu);
        } else throw std::runtime_error("unexpected growth callback");
        state.r[8] ^= 0xabcdef1234567890ull;
        state.fpr_bits[7] ^= 0x180u;
        state.cr1.gt ^= 1u;
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
        memory.WriteU32(Reader, mode == 1u ? 0u : 4u);
        memory.WriteU32(Reader + 4u, mode == 1u ? 0u : 2u);
        memory.WriteU32(Reader + 8u, mode == 1u ? 0u : Old);
        memory.WriteU32(Reader + 12u, std::bit_cast<std::uint32_t>(mode == 0u ? 1.0f : 2.0f));
        memory.WriteU32(Threshold, std::bit_cast<std::uint32_t>(1.0f));
        memory.WriteU32(Global, 0u); memory.WriteU32(Table, Vtable);
        memory.WriteU32(Vtable, Allocate | 3u);
        memory.WriteU32(Vtable + 12u, Dispose | 3u);
        memory.WriteU32(Old, 0x12345678u); memory.WriteU32(Old + 4u, 0x90abcdefu);
    };
    seed(original); seed(recovered);
    auto original_memory = original.Memory(), recovered_memory = recovered.Memory();
    Registers initial{};
    for (unsigned i = 0; i < 32u; ++i) {
        initial.r[i] = 0x1122334400000000ull + i;
        initial.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    initial.r[1] = 0x8877665500080000ull;
    initial.r[3] = 0xaabbccdd00030000ull; initial.r[4] = 3u;
    initial.lr = 0x9988776681234567ull; initial.cached_fp_control = 0x9fc0u;
    initial.xer_so = 1u;
    PPCContext context{}; crt_full_oracle::ToPpc(context, initial);
    auto state = initial; Guest expected, actual; Native native;
    expected.mode = actual.mode = mode;
    active_guest = &expected; active_memory = &original_memory;
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    __imp__sub_82BD2870(context, original.Bytes());
    const auto expected_host = PPCFPSCRRegister{}.getcsr();
    active_guest = nullptr; active_memory = nullptr;
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if (!reader_buffer_growth61::Apply(0x82bd2870u, recovered_memory, {actual, native}, state))
        throw std::runtime_error("missing growth entry");
    if (crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context)) != crt_full_oracle::Snapshot(state) ||
        !original.EqualCommitted(recovered) || expected.events != actual.events ||
        expected_host != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("growth Full72/RAM/callback/host-control mismatch");
    if (state.r[3] != ((mode == 0u || mode == 2u) ? 0u : 1u) ||
        state.r[1] != initial.r[1] || state.lr != 0x81234567u ||
        state.r[30] != initial.r[30] || state.r[31] != initial.r[31] ||
        recovered_memory.ReadU32(Reader) != (mode == 0u ? 4u : mode == 1u ? 3u : 8u) ||
        actual.events.size() != (mode == 0u ? 0u : mode == 3u ? 2u : 1u) ||
        native.controls != std::vector<std::uint32_t>{0x1f80u} ||
        (mode == 1u && recovered_memory.ReadU32(Reader + 8u) != Fresh) ||
        (mode == 2u && recovered_memory.ReadU32(Reader + 8u) != Old) ||
        (mode == 3u && recovered_memory.ReadU32(Reader + 0x108u) != Fresh + 16u))
        throw std::runtime_error("growth outcome/ownership/restore mismatch");
}
}
void OriginalReaderGrowth61Indirect(GuestAddress target, PPCContext& context, std::uint8_t*) {
    auto state = crt_full_oracle::FromPpc(context);
    reader_growth61_oracle::active_guest->CallIndirect(target, *reader_growth61_oracle::active_memory, state);
    crt_full_oracle::ToPpc(context, state);
}
int main() {
    try {
        for (unsigned mode = 0; mode < 4u; ++mode) reader_growth61_oracle::Check(mode);
        std::puts("PASS reader-buffer-growth61 4 focused PPC cases"); return 0;
    } catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
