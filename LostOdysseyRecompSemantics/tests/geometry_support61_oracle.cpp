#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/geometry_support61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>

namespace geometry_support61_oracle {
using Registers = geometry_support61::Registers;
constexpr GuestAddress Object = 0x30000u;
constexpr std::array<test::Region, 2> Regions{{{0u, 0x120000u}, {0x82000000u, 0x1000u}}};
struct Native final : float_triplet_transfer::NativeServices {
    std::vector<std::uint32_t> controls;
    void SetHostFpControl(std::uint32_t value) override {
        controls.push_back(value); PPCFPSCRRegister{}.setcsr(value);
    }
};
struct RestoreHost {
    std::uint32_t control = PPCFPSCRRegister{}.getcsr();
    ~RestoreHost() { PPCFPSCRRegister{}.setcsr(control); }
};
void Check(unsigned mode) {
    RestoreHost restore;
    test::GuestWindow original(Regions), recovered(Regions);
    const auto seed = [](test::GuestWindow& window) {
        window.Fill(0xa5u); auto memory = window.Memory();
        memory.WriteU32(0x82000e0cu, std::bit_cast<std::uint32_t>(-4.5f));
        memory.WriteU32(0x82000e50u, std::bit_cast<std::uint32_t>(1.25f));
    };
    seed(original); seed(recovered);
    auto recovered_memory = recovered.Memory();
    Registers initial{};
    for (unsigned i = 0u; i < 32u; ++i) {
        initial.r[i] = 0x1122334400000000ull + i;
        initial.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    initial.r[1] = 0x8877665500080000ull; initial.r[3] = 0xaabbccdd00000000ull | Object;
    initial.lr = 0x9988776681234567ull; initial.cached_fp_control = 0x9fc0u; initial.xer_so = 1u;
    PPCContext context{}; crt_full_oracle::ToPpc(context, initial);
    auto state = initial; Native native;
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if (mode == 0u) __imp__sub_82BD43F8(context, original.Bytes());
    else if (mode == 1u) __imp__sub_82BD3D80(context, original.Bytes());
    else __imp__sub_82BD4438(context, original.Bytes());
    const auto expected_host = PPCFPSCRRegister{}.getcsr();
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    const GuestAddress entry = mode == 0u ? 0x82bd43f8u : mode == 1u ? 0x82bd3d80u : 0x82bd4438u;
    if (!geometry_support61::Apply(entry, recovered_memory, native, state))
        throw std::runtime_error("missing geometry support entry");
    if (crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context)) != crt_full_oracle::Snapshot(state) ||
        !original.EqualCommitted(recovered) || expected_host != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("geometry support Full72/RAM/FP mismatch");
    if (state.r[3] != initial.r[3] || state.r[1] != initial.r[1] || state.r[31] != initial.r[31] ||
        state.lr != (mode == 2u ? initial.lr : 0x81234567u) ||
        recovered_memory.ReadU32(Object) != (mode == 2u ? 0x820d6f00u : 0x820d6c90u))
        throw std::runtime_error("geometry vtable/frame outcome mismatch");
    if (mode != 2u) {
        for (const unsigned offset : {4u, 8u, 12u, 92u, 96u, 100u, 104u})
            if (recovered_memory.ReadU32(Object + offset) != 0u)
                throw std::runtime_error("geometry field initialization absent");
        if (recovered_memory.ReadU32(Object + 132u) != std::bit_cast<std::uint32_t>(-4.5f) ||
            recovered_memory.ReadU32(Object + 136u) != std::bit_cast<std::uint32_t>(1.25f) ||
            recovered_memory.ReadU8(Object + 140u) != 0u || recovered_memory.ReadU8(Object + 141u) != 1u ||
            native.controls != std::vector<std::uint32_t>{0x1f80u})
            throw std::runtime_error("geometry float/flags outcome mismatch");
    } else if (recovered_memory.ReadU32(Object + 4u) != 0xa5a5a5a5u || !native.controls.empty()) {
        throw std::runtime_error("geometry teardown modified unrelated fields");
    }
}
}
int main() {
    try {
        for (unsigned mode = 0u; mode < 3u; ++mode) geometry_support61_oracle::Check(mode);
        std::puts("PASS geometry-support61 3 focused PPC cases"); return 0;
    } catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
