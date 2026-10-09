#include "object_sort_engine61_oracle_fixture.h"
#include "lo_semantics/object_sort_dispatch61.h"
#include "lo_semantics/crt_close_upper61.h"
#include "lo_semantics/crt_close_buffer_callers_context.h"
#include "lo_semantics/crt_reader_cleanup_callers_context.h"
#include "lo_semantics/crt_reader_units61.h"

namespace object_sort_dispatch61_oracle {
using namespace sort_engine61_oracle;
using EngineEnvironment = sort_engine61_oracle::Environment;
constexpr GuestAddress Object = 0x36000u, Cells = 0x37000u;
constexpr GuestAddress GlobalService = 0x38000u, GlobalTable = 0x39000u;
constexpr std::array<test::Region, 5> Regions{{{0u, 0x120000u},
    {0x82000000u, 0x1000u}, {0x821ba000u, 0x1000u},
    {0x83216000u, 0x1000u}, {0x832dc000u, 0x4000u}}};
struct Output final : crt_close_block_output_context::ErrorOutputServices {
    void CallOutput(GuestMemory&, Full&) override {
        throw std::runtime_error("file output is outside dispatch fixture scope");
    }
};
EngineEnvironment* active = nullptr;
Output* active_output = nullptr;
void Lower(GuestAddress entry, EngineEnvironment& env, Output& output, Full& state) {
    auto& memory = env.stream.memory;
    const auto deps = env.Deps();
    switch (entry) {
    case 0x82bae200u: (void)object_sort_engine61::Apply(entry, memory, deps, state); break;
    case 0x82bd0cd0u: (void)crt_close_upper61::Apply(entry, memory, deps.sort, state); break;
    case 0x82bd0df0u: (void)crt_reader_cleanup_callers_context::Apply(entry, memory, env.guest, state); break;
    case 0x82bd1050u: (void)crt_reader_units61::Apply(entry, memory, env.guest, state); break;
    case 0x82bd1200u:
        (void)crt_close_buffer_callers_context::Apply(entry, memory,
            {env.guest, {deps.sort.accepted, output}}, state); break;
    default: EngineLower(entry, env, state); break;
    }
}
void Check(unsigned mode) {
    RestoreHost restore;
    const std::vector<std::uint32_t> words = mode == 0u ? std::vector<std::uint32_t>{} :
        mode == 1u ? std::vector<std::uint32_t>{0x80000000u, 0x80000000u, 0x40000002u, 0xffffffffu} :
                     std::vector<std::uint32_t>{0u, 1u, 2u, 3u};
    test::GuestWindow original(Regions), recovered(Regions);
    const auto seed = [&](test::GuestWindow& window) {
        Seed(window, {}, {9u, 8u, 7u}, 2u);
        auto memory = window.Memory();
        memory.WriteU32(Object + 88u, 2u);
        memory.WriteU32(Object + 104u, static_cast<std::uint32_t>(words.size()));
        memory.WriteU32(Object + 108u, Cells);
        for (unsigned i = 0u; i < words.size(); ++i) memory.WriteU32(Cells + i * 4u, words[i]);
        memory.WriteU32(0x832df548u, GlobalService);
        memory.WriteU32(GlobalService, GlobalTable);
        memory.WriteU32(GlobalTable + 8u, AllocateTarget | 1u);
        memory.WriteU32(GlobalTable + 20u, FreeTarget | 3u);
    };
    seed(original); seed(recovered);
    EngineEnvironment expected(original), actual(recovered); Output expected_output, actual_output;
    auto initial = Initial(2u);
    initial.r[3] = 0xaabbccdd00000000ull | Object;
    initial.r[4] = 0u;
    initial.r[5] = mode == 0u ? 0u : Writer;
    PPCContext context{}; crt_full_oracle::ToPpc(context, initial);
    auto state = initial;
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    active = &expected; active_output = &expected_output;
    __imp__sub_82BAFEC0(context, original.Bytes());
    const auto expected_host = PPCFPSCRRegister{}.getcsr();
    active = nullptr; active_output = nullptr;
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if (!object_sort_dispatch61::Apply(0x82bafec0u, actual.stream.memory,
        {actual.Deps(), actual_output}, state))
        throw std::runtime_error("missing object sort dispatch entry");
    if (crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context)) != crt_full_oracle::Snapshot(state) ||
        !original.EqualCommitted(recovered) || expected.guest.events != actual.guest.events ||
        expected_host != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("object dispatch Full72/RAM/callback/FP mismatch");
    if (state.r[3] != 1u || state.r[1] != initial.r[1] || state.lr != 0x81234567u)
        throw std::runtime_error("object dispatch return/frame mismatch");
    for (unsigned i = 18u; i <= 31u; ++i)
        if (state.r[i] != initial.r[i]) throw std::runtime_error("object dispatch saved register mismatch");
    if (mode != 0u) {
        if (actual.stream.memory.ReadU32(Data) != 0x504d4150u ||
            actual.stream.memory.ReadU32(Node + 4u) <= 12u)
            throw std::runtime_error("PMAP header or grouped output absent");
    }
    if (actual.guest.events.empty()) throw std::runtime_error("dispatch allocation events absent");
}
}
void OriginalObjectSortDispatch61Lower(std::uint32_t entry, PPCContext& context, std::uint8_t*) {
    auto state = crt_full_oracle::FromPpc(context);
    object_sort_dispatch61_oracle::Lower(entry, *object_sort_dispatch61_oracle::active,
        *object_sort_dispatch61_oracle::active_output, state);
    crt_full_oracle::ToPpc(context, state);
}
void OriginalObjectSortDispatch61Indirect(std::uint32_t target, PPCContext& context, std::uint8_t*) {
    auto state = crt_full_oracle::FromPpc(context);
    auto& env = *object_sort_dispatch61_oracle::active;
    env.guest.CallIndirect(target, env.stream.memory, state);
    crt_full_oracle::ToPpc(context, state);
}
void OriginalObjectSortDispatch61Save(PPCContext& context) {
    auto& memory = object_sort_dispatch61_oracle::active->stream.memory;
    const auto regs = crt_full_oracle::Gprs(context);
    for (unsigned i = 18u; i <= 31u; ++i)
        recovery_abi::WriteU64(memory, recovery_abi::Address(context.r1.u64 - 8u * (33u - i)), regs[i]->u64);
    memory.WriteU32(recovery_abi::Address(context.r1.u64 - 8u), context.r12.u32);
}
void OriginalObjectSortDispatch61Restore(PPCContext& context) {
    auto& memory = object_sort_dispatch61_oracle::active->stream.memory;
    const auto regs = crt_full_oracle::Gprs(context);
    for (unsigned i = 18u; i <= 31u; ++i)
        regs[i]->u64 = recovery_abi::ReadU64(memory, recovery_abi::Address(context.r1.u64 - 8u * (33u - i)));
    context.r12.u64 = memory.ReadU32(recovery_abi::Address(context.r1.u64 - 8u)); context.lr = context.r12.u64;
}
int main() {
    try {
        for (unsigned mode = 0u; mode < 3u; ++mode) object_sort_dispatch61_oracle::Check(mode);
        std::puts("PASS object-sort-dispatch61 3 focused PPC upper cases with accepted lowers"); return 0;
    } catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
