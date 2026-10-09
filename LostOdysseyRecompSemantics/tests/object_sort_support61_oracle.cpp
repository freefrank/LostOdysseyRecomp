// Append to private pinned originals; no original instruction bodies live here.
#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/object_sort_support61.h"
#include <bit>

namespace sort_support61_oracle {
using Registers = object_sort_support61::Registers;
constexpr GuestAddress Object = 0x30000u;
constexpr std::array<test::Region, 3> Regions{{{0u, 0x120000u},
    {0x82000000u, 0x1000u}, {0x821ba000u, 0x1000u}}};
struct RestoreHost {
    std::uint32_t control = PPCFPSCRRegister{}.getcsr();
    ~RestoreHost() { PPCFPSCRRegister{}.setcsr(control); }
};
struct Guest final : crt_close_recursive_buffer_context::GuestServices {
    void CallIndirect(GuestAddress, GuestMemory&, Registers&) override {
        throw std::runtime_error("unexpected initialization/retained-payload callback");
    }
};
struct Native final : float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t control) override {
        PPCFPSCRRegister{}.setcsr(control);
    }
};

void Check(GuestAddress entry) {
    RestoreHost restore;
    test::GuestWindow original(Regions), recovered(Regions);
    const auto seed = [](test::GuestWindow& window) {
        window.Fill(0xa5u);
        auto memory = window.Memory();
        memory.WriteU32(0x821baa74u, std::bit_cast<std::uint32_t>(0.75f));
        memory.WriteU32(0x82000e50u, std::bit_cast<std::uint32_t>(1.0f));
        memory.WriteU32(Object + 12u, std::bit_cast<std::uint32_t>(0.5f));
    };
    seed(original); seed(recovered);
    auto memory = recovered.Memory();
    Registers initial{};
    for (unsigned i = 0; i < 32u; ++i) {
        initial.r[i] = 0x1122334400000000ull + i;
        initial.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    initial.r[1] = 0x8877665500080000ull;
    initial.r[3] = 0xaabbccdd00030000ull;
    initial.lr = 0x9988776681234567ull;
    initial.cached_fp_control = 0x9fc0u;
    initial.xer_so = 1u;
    PPCContext context{}; crt_full_oracle::ToPpc(context, initial);
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    switch (entry) {
    case 0x82bd2a08u: __imp__sub_82BD2A08(context, original.Bytes()); break;
    case 0x82bd2c08u: __imp__sub_82BD2C08(context, original.Bytes()); break;
    case 0x82bd2c50u: __imp__sub_82BD2C50(context, original.Bytes()); break;
    default: throw std::runtime_error("unknown support case");
    }
    const auto expected_host = PPCFPSCRRegister{}.getcsr();
    auto state = initial; Guest guest; Native native;
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if (!object_sort_support61::Apply(entry, memory, {guest, native}, state) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context)) !=
            crt_full_oracle::Snapshot(state) ||
        !original.EqualCommitted(recovered) ||
        expected_host != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("object-sort support Full72/RAM/FP-control mismatch");
    if (state.r[3] != initial.r[3] || state.r[1] != initial.r[1])
        throw std::runtime_error("object identity or stack changed");
    if (entry == 0x82bd2a08u &&
        (memory.ReadU32(Object) != 0u || memory.ReadU32(Object + 4u) != 0u ||
         memory.ReadU32(Object + 8u) != 0u ||
         memory.ReadU32(Object + 12u) != std::bit_cast<std::uint32_t>(0.75f)))
        throw std::runtime_error("float-state initialization outcome absent");
    if (entry == 0x82bd2c08u &&
        (memory.ReadU32(Object) != 0u || memory.ReadU32(Object + 4u) != 0u ||
         memory.ReadU32(Object + 8u) != 0xa5a5a5a5u || state.lr != 0x81234567u))
        throw std::runtime_error("tail cleanup/retained payload outcome absent");
    if (entry == 0x82bd2c50u &&
        (memory.ReadU32(Object) != 0x80000000u || memory.ReadU32(Object + 4u) != 0u ||
         memory.ReadU32(Object + 8u) != 0u || memory.ReadU32(Object + 12u) != 0u ||
         memory.ReadU32(Object + 16u) != 0u || memory.ReadU8(Object + 20u) != 1u ||
         memory.ReadU8(Object + 21u) != 0xa5u))
        throw std::runtime_error("sentinel initialization extent/outcome absent");
}
}

void OriginalObjectSortSupport61Indirect(std::uint32_t, PPCContext&, std::uint8_t*) {
    throw std::runtime_error("unexpected original support callback");
}

int main() {
    try {
        for (auto entry : {0x82bd2a08u, 0x82bd2c08u, 0x82bd2c50u})
            sort_support61_oracle::Check(entry);
        std::puts("PASS object-sort-support61 3 focused PPC cases"); return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what()); return 1;
    }
}
