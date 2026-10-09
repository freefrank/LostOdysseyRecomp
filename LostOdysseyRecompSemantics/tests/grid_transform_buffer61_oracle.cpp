// Appended to the three original leaf bodies; no lower stubs are required.
#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/grid_transform_buffer61.h"

namespace transform_buffer61_oracle {
constexpr GuestAddress Descriptor = 0x30000u, Triplets = 0x31000u;
constexpr std::array<test::Region, 1> Regions{{{0u, 0x120000u}}};
void Check(unsigned mode)
{
    test::GuestWindow original(Regions), recovered(Regions);
    const auto seed = [mode](test::GuestWindow& window) {
        window.Fill(0xa5u); auto memory = window.Memory();
        memory.WriteU32(Descriptor + 8u, 5u);
        memory.WriteU32(Descriptor + 12u, mode == 1u ? 0u : 12u);
        memory.WriteU32(Descriptor + 16u, Triplets);
        // Intentionally outside committed RAM: the leaf compares addresses
        // without reading vertex storage. Addition also crosses the word limit.
        memory.WriteU32(Descriptor + 20u, 0xfffffff8u);
        constexpr std::array<std::array<std::uint32_t, 3>, 5> rows{{
            {{0u,1u,2u}}, {{3u,3u,4u}}, {{5u,6u,6u}}, {{7u,8u,7u}},
            {{0u,0x40000000u,2u}}}};
        for (unsigned i = 0u; i < rows.size(); ++i)
            for (unsigned j = 0u; j < 3u; ++j)
                memory.WriteU32(Triplets + i * 12u + j * 4u, rows[i][j]);
    };
    seed(original); seed(recovered);
    grid_transform_buffer61::Registers initial{};
    for (unsigned i = 0u; i < 32u; ++i) {
        initial.r[i] = 0x1122334400000000ull + i;
        initial.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    initial.r[1] = 0x8877665500080000ull;
    initial.r[3] = 0xaabbccdd00000000ull | Descriptor;
    initial.lr = 0x9988776681234567ull;
    initial.cached_fp_control = 0x9fc0u; initial.xer_so = 1u;
    PPCContext context{}; crt_full_oracle::ToPpc(context, initial);
    const auto host_control = PPCFPSCRRegister{}.getcsr();
    const GuestAddress entry = mode < 2u ? 0x82bd17f0u :
        mode == 2u ? 0x82bd1830u : 0x82bdac60u;
    switch (entry) {
    case 0x82bd17f0u: __imp__sub_82BD17F0(context, original.Bytes()); break;
    case 0x82bd1830u: __imp__sub_82BD1830(context, original.Bytes()); break;
    case 0x82bdac60u: __imp__sub_82BDAC60(context, original.Bytes()); break;
    }
    const auto original_host = PPCFPSCRRegister{}.getcsr();
    auto state = initial; auto memory = recovered.Memory();
    if (!grid_transform_buffer61::Apply(entry, memory, state) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context)) != crt_full_oracle::Snapshot(state) ||
        !original.EqualCommitted(recovered) || original_host != host_control ||
        PPCFPSCRRegister{}.getcsr() != host_control)
        throw std::runtime_error("transform buffer Full72/RAM/host-control mismatch");
    if (state.r[1] != initial.r[1] || state.lr != initial.lr)
        throw std::runtime_error("transform buffer leaf frame changed");
    if (mode < 2u && state.r[3] != (mode == 0u ? 1u : 0u))
        throw std::runtime_error("descriptor presence predicate mismatch");
    if (mode == 2u && (state.r[3] != 4u || state.r[4] != 0u ||
        state.r[11] != Triplets + 60u))
        throw std::runtime_error("repeated wrapped address count mismatch");
    if (mode == 3u) {
        for (unsigned offset = 0u; offset <= 24u; offset += 4u)
            if (memory.ReadU32(Descriptor + offset) != 0u)
                throw std::runtime_error("owner slot clear missing");
        if (state.r[3] != initial.r[3] || memory.ReadU32(Descriptor + 28u) != 0xa5a5a5a5u)
            throw std::runtime_error("owner clear extent/return mismatch");
    }
}
}
int main()
{
    try {
        for (unsigned mode = 0u; mode < 4u; ++mode) transform_buffer61_oracle::Check(mode);
        std::puts("PASS grid-transform-buffer61 4 focused PPC cases"); return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what()); return 1;
    }
}
