#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/object_sort_reader61.h"
#include <bit>

namespace object_reader61_oracle {
using Registers = object_sort_reader61::Registers;
constexpr GuestAddress Output = 0x30000u, Reader = 0x30100u, Node = 0x30200u;
constexpr GuestAddress Bytes = 0x31000u, Old = 0x32000u, Fresh = 0x33000u;
constexpr GuestAddress Vtable = 0x34000u, Allocate = 0x2a00u, Dispose = 0x2b00u;
constexpr GuestAddress Cursor = 0x83216184u, Histogram = 0x832dc448u;
constexpr std::array<test::Region, 5> Regions{{{0u, 0x120000u},
    {0x82000000u, 0x1000u}, {0x82baf000u, 0x1000u},
    {0x83216000u, 0x1000u}, {0x832dc000u, 0x4000u}}};
struct RestoreHost {
    std::uint32_t control = PPCFPSCRRegister{}.getcsr();
    ~RestoreHost() { PPCFPSCRRegister{}.setcsr(control); }
};
struct Guest final : crt_close_recursive_buffer_context::GuestServices {
    std::vector<std::array<std::uint64_t, 72>> events;
    void CallIndirect(GuestAddress target, GuestMemory& memory, Registers& state) override {
        events.push_back(crt_full_oracle::Snapshot(state));
        if (target == Allocate) {
            if (state.r[4] != 8u || state.r[5] != 64u)
                throw std::runtime_error("wrong decoder growth request");
            state.r[3] = Fresh;
        } else if (target == Dispose) {
            if (state.r[4] != Old || memory.ReadU32(Fresh) != 10569u)
                throw std::runtime_error("decoder copy before disposal missing");
            memory.WriteU32(Old, 0xdeadbeefu);
        } else throw std::runtime_error("unexpected decoder callback");
        state.r[8] ^= 0xabcdef1234567890ull; state.fpr_bits[7] ^= 0x180u;
        state.cr1.gt ^= 1u;
    }
};
struct Native final : float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t value) override { PPCFPSCRRegister{}.setcsr(value); }
};
Guest* active_guest = nullptr;
GuestMemory* active_memory = nullptr;
void Check(unsigned mode) {
    RestoreHost restore;
    std::vector<unsigned> bits;
    const auto append = [&](std::uint32_t value, unsigned width) {
        for (unsigned n = width; n != 0u; --n) bits.push_back((value >> (n - 1u)) & 1u);
    };
    const unsigned count = mode == 0u ? 0u : mode == 1u ? 26u : mode == 2u ? 6u : 2u;
    append(count, 32u);
    if (mode == 1u) for (unsigned op = 0u; op < 26u; ++op) append(op, 5u);
    if (mode == 2u) {
        append(26u,5u); append(2u,5u);
        append(27u,5u); append(3u,5u);
        append(28u,5u); append(4u,5u);
        append(29u,5u); append(5u,5u); append(6u,5u);
        append(30u,5u); append(7u,5u); append(8u,5u);
        append(31u,5u); append(9u,5u); append(10u,5u); append(11u,5u);
    }
    if (mode == 3u) { append(0u,5u); append(1u,5u); }
    test::GuestWindow original(Regions), recovered(Regions);
    const auto seed = [&](test::GuestWindow& window) {
        window.Fill(0xa5u); auto memory = window.Memory();
        memory.WriteU32(Output, mode == 3u ? 1u : 64u); memory.WriteU32(Output + 4u, 7u);
        memory.WriteU32(Output + 8u, Old); memory.WriteU32(Output + 12u, std::bit_cast<std::uint32_t>(2.0f));
        memory.WriteU32(Reader, Node); memory.WriteU8(Reader + 24u, 0u);
        memory.WriteU32(Node, Bytes); memory.WriteU32(Node + 4u, 0u);
        for (unsigned i = 0u; i < (bits.size() + 7u) / 8u; ++i) {
            std::uint8_t byte = 0u;
            for (unsigned j = 0u; j < 8u; ++j)
                if (8u*i+j < bits.size()) byte |= static_cast<std::uint8_t>(bits[8u*i+j] << (7u-j));
            memory.WriteU8(Bytes + i, byte);
        }
        for (unsigned axis = 0u; axis < 3u; ++axis) memory.WriteU32(Cursor + axis*4u, 10u);
        for (unsigned op = 0u; op < 32u; ++op) {
            memory.WriteU32(Histogram + op*4u, 0u);
            memory.WriteU32(0x82baf77cu + op*4u, 0x82001000u + op*4u);
        }
        memory.WriteU32(0x82000e50u, std::bit_cast<std::uint32_t>(1.0f));
        memory.WriteU32(0x832df554u, 0u); memory.WriteU32(0x83216624u, Vtable);
        memory.WriteU32(Vtable, Allocate | 3u); memory.WriteU32(Vtable + 12u, Dispose | 3u);
    };
    seed(original); seed(recovered);
    auto original_memory = original.Memory(), recovered_memory = recovered.Memory();
    Registers initial{};
    for (unsigned i = 0u; i < 32u; ++i) {
        initial.r[i] = 0x1122334400000000ull + i;
        initial.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    initial.r[1] = 0x8877665500080000ull; initial.r[3] = Output;
    initial.r[4] = Reader; initial.r[5] = 32u;
    initial.lr = 0x9988776681234567ull; initial.cached_fp_control = 0x9fc0u; initial.xer_so = 1u;
    PPCContext context{}; crt_full_oracle::ToPpc(context, initial);
    auto state = initial; Guest expected, actual; Native native;
    active_guest = &expected; active_memory = &original_memory;
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    __imp__sub_82BAF600(context, original.Bytes());
    const auto expected_host = PPCFPSCRRegister{}.getcsr();
    active_guest = nullptr; active_memory = nullptr;
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if (!object_sort_reader61::Apply(0x82baf600u, recovered_memory, {actual,native}, state))
        throw std::runtime_error("missing object reader entry");
    if (crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context)) != crt_full_oracle::Snapshot(state) ||
        !original.EqualCommitted(recovered) || expected.events != actual.events ||
        expected_host != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("object reader Full72/RAM/callback/host-control mismatch");
    if (state.r[3] != count || state.r[1] != initial.r[1] || state.lr != 0x81234567u ||
        recovered_memory.ReadU32(Output + 4u) != count ||
        recovered_memory.ReadU32(Node + 4u) != (bits.size()+7u)/8u ||
        actual.events.size() != (mode == 3u ? 2u : 0u))
        throw std::runtime_error("object reader count/stream/growth outcome mismatch");
    if (mode == 2u) {
        constexpr std::array<std::uint32_t,6> values{{10562u,10338u,4194u,4293u,8391u,11593u}};
        for (unsigned i = 0u; i < values.size(); ++i)
            if (recovered_memory.ReadU32(Old + i*4u) != values[i])
                throw std::runtime_error("explicit coordinate flatten mismatch");
    }
    if (mode == 3u && (recovered_memory.ReadU32(Fresh) != 10569u ||
        recovered_memory.ReadU32(Fresh + 4u) != 10570u))
        throw std::runtime_error("neighbor decode across growth mismatch");
}
}
// Synthetic ABI save/restore support for the selected original entry.
void OriginalObjectReader61Save(unsigned first, PPCContext& context, std::uint8_t*) {
    auto& memory = *object_reader61_oracle::active_memory;
    const auto registers = crt_full_oracle::Gprs(context);
    for (unsigned i = first; i <= 31u; ++i)
        WriteU64(memory, Address(context.r1.u64 - 16u - 8u * (31u - i)), registers[i]->u64);
    memory.WriteU32(Address(context.r1.u64 - 8u), context.r12.u32);
}
void OriginalObjectReader61Restore(unsigned first, PPCContext& context, std::uint8_t*) {
    auto& memory = *object_reader61_oracle::active_memory;
    const auto registers = crt_full_oracle::Gprs(context);
    for (unsigned i = first; i <= 31u; ++i)
        registers[i]->u64 = ReadU64(memory, Address(context.r1.u64 - 16u - 8u * (31u - i)));
    context.r12.u64 = memory.ReadU32(Address(context.r1.u64 - 8u));
    context.lr = context.r12.u64;
}
void OriginalObjectReader61Indirect(GuestAddress target, PPCContext& context, std::uint8_t*) {
    auto state = crt_full_oracle::FromPpc(context);
    object_reader61_oracle::active_guest->CallIndirect(target, *object_reader61_oracle::active_memory, state);
    crt_full_oracle::ToPpc(context, state);
}
int main() {
    try {
        for (unsigned mode = 0u; mode < 4u; ++mode) object_reader61_oracle::Check(mode);
        std::puts("PASS object-sort-reader61 4 focused PPC cases"); return 0;
    } catch (const std::exception& error) { std::fprintf(stderr,"%s\n",error.what()); return 1; }
}
