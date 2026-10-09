// Appended to the original BD9858 body, with ABI save/restore shims only.
#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/owned_tree_storage61.h"
#include <bit>

namespace tree_storage61_oracle {
using Registers = owned_tree_storage61::Registers;
constexpr GuestAddress Node = 0x30000u, Items = 0x31000u, Alternate = 0x32000u;
constexpr GuestAddress Service = 0x33000u, Table = 0x34000u;
constexpr GuestAddress ThresholdCall = 0x2a00u, ScoreCall = 0x2b00u;
constexpr std::uint64_t Opaque = 0xaabbccdd12345678ull;
constexpr std::array<test::Region, 1> Regions{{{0u, 0x120000u}}};
struct RestoreHost {
    std::uint32_t value = PPCFPSCRRegister{}.getcsr();
    ~RestoreHost() { PPCFPSCRRegister{}.setcsr(value); }
};
struct Guest final : crt_close_recursive_buffer_context::GuestServices {
    explicit Guest(unsigned selected) : mode(selected) {}
    unsigned mode, scores = 0u;
    std::vector<std::array<std::uint64_t, 72>> events;
    void CallIndirect(GuestAddress target, GuestMemory& memory, Registers& state) override {
        events.push_back(crt_full_oracle::Snapshot(state));
        if (Address(state.r[3]) != Service || state.ctr != (target | 3u))
            throw std::runtime_error("partition service receiver/target mismatch");
        if (target == ThresholdCall) {
            const auto count = mode == 0u ? 0u : mode == 1u ? 4u : 2u;
            if (state.lr != 0x82bd9898u || state.r[4] != Items || state.r[5] != count ||
                Address(state.r[6]) != Node || state.r[7] != Opaque)
                throw std::runtime_error("partition threshold arguments mismatch");
            state.fpr_bits[1] = std::bit_cast<std::uint64_t>(2.0);
            if (mode == 2u) memory.WriteU32(Node + 32u, Alternate);
        } else if (target == ScoreCall) {
            if (state.lr != 0x82bd98d8u || state.r[5] != Opaque)
                throw std::runtime_error("partition score arguments mismatch");
            state.fpr_bits[1] = std::bit_cast<std::uint64_t>(double(state.r[4]));
            // This live threshold must replace the initially selected value.
            if (mode == 2u && scores == 0u)
                state.fpr_bits[31] = std::bit_cast<std::uint64_t>(3.0);
            ++scores;
        } else throw std::runtime_error("unexpected partition service target");
        state.r[8] ^= 0x1020304050607080ull;
        state.fpr_bits[7] ^= 0x80u; state.cr0.eq ^= 1u; state.cr1.gt ^= 1u; state.cr7.lt ^= 1u;
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
void Check(unsigned mode)
{
    RestoreHost restore;
    const unsigned count = mode == 0u ? 0u : mode == 1u ? 4u : 2u;
    test::GuestWindow original(Regions), recovered(Regions);
    const auto seed = [count](test::GuestWindow& window) {
        window.Fill(0xa5u); auto memory = window.Memory();
        memory.WriteU32(Node + 32u, Items); memory.WriteU32(Node + 36u, count);
        constexpr std::array<std::uint32_t, 4> values{{1u,4u,2u,5u}};
        for (unsigned i = 0u; i < values.size(); ++i) {
            memory.WriteU32(Items + 4u*i, values[i]);
            memory.WriteU32(Alternate + 4u*i, values[i]);
        }
        memory.WriteU32(Service, Table);
        memory.WriteU32(Table + 8u, ThresholdCall | 3u);
        memory.WriteU32(Table + 12u, ScoreCall | 3u);
    };
    seed(original); seed(recovered);
    Registers initial{};
    for (unsigned i = 0u; i < 32u; ++i) {
        initial.r[i] = 0x1122334400000000ull + i;
        initial.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    initial.r[1] = 0x8877665500080000ull;
    initial.r[3] = 0xaabbccdd00000000ull | Node; initial.r[4] = Opaque;
    initial.r[5] = 0x7766554400000000ull | Service;
    initial.lr = 0x9988776681234567ull; initial.cached_fp_control = 0x9fc0u; initial.xer_so = 1u;
    PPCContext context{}; crt_full_oracle::ToPpc(context, initial);
    auto original_memory = original.Memory(), recovered_memory = recovered.Memory();
    Guest expected(mode), actual(mode); Native native;
    active_guest = &expected; active_memory = &original_memory;
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    __imp__sub_82BD9858(context, original.Bytes());
    const auto expected_host = PPCFPSCRRegister{}.getcsr();
    active_guest = nullptr; active_memory = nullptr;
    auto state = initial;
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if (!owned_tree_storage61::Apply(0x82bd9858u, recovered_memory, {actual,native}, state) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context)) != crt_full_oracle::Snapshot(state) ||
        !original.EqualCommitted(recovered) || expected.events != actual.events ||
        expected_host != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("partition Full72/RAM/callback/host-control mismatch");
    if (state.r[3] != (mode == 0u ? 0u : mode == 1u ? 2u : 1u) ||
        actual.scores != count || actual.events.size() != count + 1u ||
        state.r[1] != initial.r[1] || state.lr != 0x81234567u ||
        state.fpr_bits[31] != initial.fpr_bits[31] ||
        native.controls != std::vector<std::uint32_t>(count + 2u, 0x1f80u))
        throw std::runtime_error("partition count/FP/frame outcome mismatch");
    for (unsigned i = 25u; i <= 31u; ++i)
        if (state.r[i] != initial.r[i]) throw std::runtime_error("partition saved register mismatch");
    if (mode == 1u) {
        constexpr std::array<std::uint32_t,4> values{{4u,5u,2u,1u}};
        for (unsigned i = 0u; i < values.size(); ++i)
            if (recovered_memory.ReadU32(Items + 4u*i) != values[i])
                throw std::runtime_error("partition mixed-score order mismatch");
    }
    if (mode == 2u && (recovered_memory.ReadU32(Node + 32u) != Alternate ||
        recovered_memory.ReadU32(Alternate) != 4u || recovered_memory.ReadU32(Alternate + 4u) != 1u ||
        recovered_memory.ReadU32(Items) != 1u || recovered_memory.ReadU32(Items + 4u) != 4u))
        throw std::runtime_error("partition live array/threshold mismatch");
}
}
void OriginalTreeStorage61Save(unsigned first, PPCContext& context, std::uint8_t*) {
    const auto registers = crt_full_oracle::Gprs(context);
    for (unsigned i = first; i <= 31u; ++i)
        WriteU64(*tree_storage61_oracle::active_memory,
            Address(context.r1.u64 - 16u - 8u * (31u - i)), registers[i]->u64);
    tree_storage61_oracle::active_memory->WriteU32(Address(context.r1.u64 - 8u), context.r12.u32);
}
void OriginalTreeStorage61Restore(unsigned first, PPCContext& context, std::uint8_t*) {
    const auto registers = crt_full_oracle::Gprs(context);
    for (unsigned i = first; i <= 31u; ++i)
        registers[i]->u64 = ReadU64(*tree_storage61_oracle::active_memory,
            Address(context.r1.u64 - 16u - 8u * (31u - i)));
    context.r12.u64 = tree_storage61_oracle::active_memory->ReadU32(Address(context.r1.u64 - 8u));
    context.lr = context.r12.u64;
}
void OriginalTreeStorage61Indirect(GuestAddress target, PPCContext& context, std::uint8_t*) {
    auto state = crt_full_oracle::FromPpc(context);
    tree_storage61_oracle::active_guest->CallIndirect(target, *tree_storage61_oracle::active_memory, state);
    crt_full_oracle::ToPpc(context, state);
}
int main() {
    try {
        for (unsigned mode = 0u; mode < 3u; ++mode) tree_storage61_oracle::Check(mode);
        std::puts("PASS owned-tree-storage61 3 focused PPC cases"); return 0;
    } catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
