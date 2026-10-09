#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/grid_transform_routes61.h"
namespace grid_transform61_oracle {
constexpr GuestAddress Object=0x30000u;
constexpr std::array<test::Region,1> Regions{{{0u,0x120000u}}};
void Check() {
    test::GuestWindow original(Regions),recovered(Regions);original.Fill(0xa5u);recovered.Fill(0xa5u);
    auto memory=recovered.Memory();grid_transform_routes61::Registers state{};
    for(unsigned i=0u;i<32u;++i){state.r[i]=0x1122334400000000ull+i;state.fpr_bits[i]=0x3ff0000000000000ull+i;}
    state.r[1]=0x8877665500080000ull;state.r[3]=0xaabbccdd00000000ull|Object;
    state.lr=0x9988776681234567ull;state.cached_fp_control=0x9fc0u;state.xer_so=1u;
    const auto initial=state;PPCContext context{};crt_full_oracle::ToPpc(context,state);
    __imp__sub_82BD7950(context,original.Bytes());
    if(!grid_transform_routes61::Apply(0x82bd7950u,memory,state) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context))!=crt_full_oracle::Snapshot(state) ||
        !original.EqualCommitted(recovered))throw std::runtime_error("transform ctor Full72/RAM mismatch");
    if(state.r[3]!=initial.r[3] || state.r[1]!=initial.r[1] || state.r[31]!=initial.r[31] ||
        state.lr!=0x81234567u || memory.ReadU32(Object)!=0x820d6dc0u ||
        state.r[10]!=0xffffffff820d6bb8ull || memory.ReadU32(Object+20u)!=0xa5a5a5a5u)
        throw std::runtime_error("transform ctor ownership/frame/table mismatch");
    for(unsigned offset=4u;offset<20u;offset+=4u)
        if(memory.ReadU32(Object+offset)!=0u)throw std::runtime_error("transform ctor state field not reset");
}
}
int main() {
    try {grid_transform61_oracle::Check();std::puts("PASS grid-transform-routes61 constructor");return 0;}
    catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}
}
