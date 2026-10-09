// Appended to genuine private bodies by the focused oracle runner.
#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/object_sort_lifecycle61.h"
#include <bit>

namespace object_sort_lifecycle61_oracle {
using Registers = object_sort_lifecycle61::Registers;
constexpr GuestAddress Object = 0x30000u, Payload = 0x32000u;
constexpr GuestAddress Service = 0x33000u, Table = 0x34000u, Target = 0x2a00u;
constexpr GuestAddress ServiceGlobal = 0x832df548u, ObjectVtable = 0x820d60e4u;
constexpr std::array<test::Region, 3> Regions{{{0u, 0x120000u},
    {0x82000000u, 0x1000u}, {0x832df000u, 0x1000u}}};
struct RestoreHost {
    std::uint32_t control = PPCFPSCRRegister{}.getcsr();
    ~RestoreHost() { PPCFPSCRRegister{}.setcsr(control); }
};
struct Guest final : crt_close_recursive_buffer_context::GuestServices {
    std::vector<std::array<std::uint64_t, 72>> events;
    void CallIndirect(GuestAddress target, GuestMemory& memory, Registers& state) override {
        if (target != Target || state.ctr != (Target | 3u) ||
            state.r[3] != Service || state.r[4] != Payload ||
            state.lr != 0x82bae1e4u || state.r[1] != 0x887766550007ffa0ull ||
            memory.ReadU32(Object) != ObjectVtable)
            throw std::runtime_error("wrong object cleanup service/frame");
        events.push_back(crt_full_oracle::Snapshot(state));
        memory.WriteU32(Payload, 0xfeed1234u);
        state.r[31] += 0x100u;
        state.r[3] = 0x1122334455667788ull;
        state.r[8] ^= 0xabcdef1234567890ull;
        state.fpr_bits[7] ^= 0x180u;
        state.cr1.gt ^= 1u; state.cr7.eq ^= 1u; state.xer_ca ^= 1u;
        state.cached_fp_control = 0x1f80u;
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
        memory.WriteU32(Object + 108u, mode == 2u ? Payload : 0u);
        memory.WriteU32(0x82000e0cu, std::bit_cast<std::uint32_t>(-4.5f));
        memory.WriteU32(0x82000d64u, std::bit_cast<std::uint32_t>(8.25f));
        memory.WriteU32(0x82000e50u, std::bit_cast<std::uint32_t>(1.0f));
        memory.WriteU32(ServiceGlobal, Service); memory.WriteU32(Service, Table);
        memory.WriteU32(Table + 20u, Target | 3u);
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
    if (mode == 0u) __imp__sub_82BB25D0(context, original.Bytes());
    else __imp__sub_82BAE1A0(context, original.Bytes());
    const auto expected_host = PPCFPSCRRegister{}.getcsr();
    active_guest = nullptr; active_memory = nullptr;
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if (!object_sort_lifecycle61::Apply(mode == 0u ? 0x82bb25d0u : 0x82bae1a0u,
        recovered_memory, {actual, native}, state))
        throw std::runtime_error("missing object lifecycle entry");
    if (crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context)) != crt_full_oracle::Snapshot(state) ||
        !original.EqualCommitted(recovered) || expected.events != actual.events ||
        expected_host != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("object lifecycle Full72/RAM/callback/FP mismatch");
    if (state.r[1] != initial.r[1] || state.r[31] != initial.r[31] ||
        state.lr != (mode == 0u ? initial.lr : 0x81234567u) ||
        recovered_memory.ReadU32(Object) != ObjectVtable ||
        actual.events.size() != (mode == 2u ? 1u : 0u) ||
        native.controls != (mode == 0u ? std::vector<std::uint32_t>{0x1f80u} : std::vector<std::uint32_t>{}))
        throw std::runtime_error("object lifecycle state outcome absent");
    if (mode == 0u) {
        for (unsigned offset = 4u; offset < 16u; offset += 4u)
            if (recovered_memory.ReadU32(Object + offset) != std::bit_cast<std::uint32_t>(-4.5f))
                throw std::runtime_error("first triplet initialization absent");
        for (unsigned offset = 16u; offset < 28u; offset += 4u)
            if (recovered_memory.ReadU32(Object + offset) != std::bit_cast<std::uint32_t>(8.25f))
                throw std::runtime_error("second triplet initialization absent");
        for (const unsigned offset : {88u, 92u, 104u, 108u, 112u})
            if (recovered_memory.ReadU32(Object + offset) != 0u)
                throw std::runtime_error("object zero initialization absent");
        if (recovered_memory.ReadU32(Object + 96u) != 0x3f800000u ||
            recovered_memory.ReadU32(Object + 100u) != 0x3f800000u || state.r[3] != initial.r[3])
            throw std::runtime_error("object scalar initialization absent");
    } else if (mode == 2u) {
        if (recovered_memory.ReadU32(Object + 108u) != Payload ||
            recovered_memory.ReadU32(Object + 0x100u + 108u) != 0u ||
            recovered_memory.ReadU32(Payload) != 0xfeed1234u || state.r[3] != 0x1122334455667788ull)
            throw std::runtime_error("mutable cleanup outcome absent");
    } else if (state.r[3] != initial.r[3] || recovered_memory.ReadU32(Object + 108u) != 0u) {
        throw std::runtime_error("empty cleanup outcome absent");
    }
}
}
void OriginalObjectSortLifecycle61Indirect(GuestAddress target, PPCContext& context, std::uint8_t*) {
    auto state = crt_full_oracle::FromPpc(context);
    object_sort_lifecycle61_oracle::active_guest->CallIndirect(target,
        *object_sort_lifecycle61_oracle::active_memory, state);
    crt_full_oracle::ToPpc(context, state);
}
int main() {
    try {
        for (unsigned mode = 0; mode < 3u; ++mode) object_sort_lifecycle61_oracle::Check(mode);
        std::puts("PASS object-sort-lifecycle61 3 focused PPC cases"); return 0;
    } catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
