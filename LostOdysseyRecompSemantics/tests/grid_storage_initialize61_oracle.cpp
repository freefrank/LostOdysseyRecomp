#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/grid_storage_initialize61.h"
#include <bit>

namespace grid_initialize61_oracle {
using Registers=grid_storage_initialize61::Registers;
constexpr GuestAddress Object=0x30000u, Bounds=0x31000u, Payload=0x32000u;
constexpr GuestAddress Allocator=0x33000u, Vtable=0x34000u, Target=0x2a00u;
constexpr std::array<test::Region,4> Regions{{{0u,0x120000u},
    {0x82007000u,0x1000u},{0x8201f000u,0x1000u},{0x832df000u,0x1000u}}};
struct RestoreHost {
    std::uint32_t control=PPCFPSCRRegister{}.getcsr();
    ~RestoreHost(){PPCFPSCRRegister{}.setcsr(control);}
};
struct Guest final:crt_close_recursive_buffer_context::GuestServices {
    unsigned mode=0u;
    std::vector<std::array<std::uint64_t,72>> events;
    void CallIndirect(GuestAddress target,GuestMemory&,Registers& s) override {
        events.push_back(crt_full_oracle::Snapshot(s));
        if(target!=Target || s.r[3]!=Allocator || s.r[4]!=108u || s.r[5]!=286u || s.lr!=0x82bb203cu)
            throw std::runtime_error("wrong grid allocator arguments");
        s.r[3]=mode==1u ? 0u : Payload;
        if(mode==2u) {s.r[30]=3u;s.r[29]=0x1122334400000055ull;s.r[31]+=0x100u;}
        s.r[8]^=0xabcdef1234567890ull;s.fpr_bits[7]^=0x180u;s.cr1.gt^=1u;
    }
};
struct Native final:float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t value) override {PPCFPSCRRegister{}.setcsr(value);}
};
Guest* active_guest=nullptr;
GuestMemory* active_memory=nullptr;
void Check(unsigned mode) {
    RestoreHost restore;
    test::GuestWindow original(Regions),recovered(Regions);
    const auto seed=[](test::GuestWindow& window) {
        window.Fill(0xa5u);auto memory=window.Memory();
        constexpr std::array<float,6> bounds{{-2.f,-4.f,-8.f,6.f,12.f,24.f}};
        for(unsigned i=0u;i<bounds.size();++i) memory.WriteU32(Bounds+i*4u,std::bit_cast<std::uint32_t>(bounds[i]));
        memory.WriteU32(0x82007784u,std::bit_cast<std::uint32_t>(1.f));
        memory.WriteU32(0x8201f9f0u,std::bit_cast<std::uint32_t>(0.5f));
        memory.WriteU32(0x832df548u,Allocator);memory.WriteU32(Allocator,Vtable);
        memory.WriteU32(Vtable+8u,Target|3u);
    };
    seed(original);seed(recovered);
    auto original_memory=original.Memory(),recovered_memory=recovered.Memory();
    Registers initial{};
    for(unsigned i=0u;i<32u;++i) {initial.r[i]=0x1122334400000000ull+i;initial.fpr_bits[i]=0x3ff0000000000000ull+i;}
    initial.r[1]=0x8877665500080000ull;initial.r[3]=Object;initial.r[4]=3u;initial.r[5]=Bounds;
    initial.lr=0x9988776681234567ull;initial.cached_fp_control=0x9fc0u;initial.xer_so=1u;
    PPCContext context{};crt_full_oracle::ToPpc(context,initial);
    auto state=initial;Guest expected,actual;Native native;expected.mode=actual.mode=mode;
    active_guest=&expected;active_memory=&original_memory;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    __imp__sub_82BB1E58(context,original.Bytes());const auto expected_host=PPCFPSCRRegister{}.getcsr();
    active_guest=nullptr;active_memory=nullptr;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if(!grid_storage_initialize61::Apply(0x82bb1e58u,recovered_memory,{actual,native},state))
        throw std::runtime_error("missing grid initialize entry");
    if(crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context))!=crt_full_oracle::Snapshot(state) ||
        !original.EqualCommitted(recovered) || expected.events!=actual.events || expected_host!=PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("grid initialize Full72/RAM/callback/host-control mismatch");
    const auto output=Object+(mode==2u?0x100u:0u);
    if(state.r[3]!=(mode==1u?0u:1u) || recovered_memory.ReadU32(output+108u)!=(mode==1u?0u:Payload) ||
        state.r[1]!=initial.r[1] || state.lr!=0x81234567u || actual.events.size()!=1u ||
        recovered_memory.ReadU32(Object+88u)!=3u || recovered_memory.ReadU32(Object+92u)!=9u ||
        recovered_memory.ReadU32(Object+104u)!=27u)
        throw std::runtime_error("grid initialization outcome mismatch");
    constexpr std::array<float,15> values{{2.f,4.f,8.f,4.f,8.f,16.f,8.f,16.f,32.f,
        0.25f,0.125f,0.0625f,4.f,8.f,16.f}};
    for(unsigned i=0u;i<15u;++i)
        if(recovered_memory.ReadU32(Object+28u+i*4u)!=std::bit_cast<std::uint32_t>(values[i]))
            throw std::runtime_error("grid bounds/coordinate scale mismatch");
    const unsigned filled=mode==0u?27u:mode==2u?3u:0u;
    for(unsigned i=0u;i<filled;++i)
        if(recovered_memory.ReadU32(Payload+i*4u)!=(mode==2u?0x55u:0xffffffffu))
            throw std::runtime_error("grid sentinel initialization mismatch");
    if(recovered_memory.ReadU32(Payload+filled*4u)!=0xa5a5a5a5u)
        throw std::runtime_error("grid fill exceeded live count");
}
}
void OriginalGridInitialize61Save(unsigned first,PPCContext& context,std::uint8_t*) {
    auto& memory=*grid_initialize61_oracle::active_memory;const auto registers=crt_full_oracle::Gprs(context);
    for(unsigned i=first;i<=31u;++i) WriteU64(memory,Address(context.r1.u64-16u-8u*(31u-i)),registers[i]->u64);
    memory.WriteU32(Address(context.r1.u64-8u),context.r12.u32);
}
void OriginalGridInitialize61Restore(unsigned first,PPCContext& context,std::uint8_t*) {
    auto& memory=*grid_initialize61_oracle::active_memory;const auto registers=crt_full_oracle::Gprs(context);
    for(unsigned i=first;i<=31u;++i) registers[i]->u64=ReadU64(memory,Address(context.r1.u64-16u-8u*(31u-i)));
    context.r12.u64=memory.ReadU32(Address(context.r1.u64-8u));context.lr=context.r12.u64;
}
void OriginalGridInitialize61Indirect(GuestAddress target,PPCContext& context,std::uint8_t*) {
    auto state=crt_full_oracle::FromPpc(context);
    grid_initialize61_oracle::active_guest->CallIndirect(target,*grid_initialize61_oracle::active_memory,state);
    crt_full_oracle::ToPpc(context,state);
}
int main() {
    try {
        for(unsigned mode=0u;mode<3u;++mode) grid_initialize61_oracle::Check(mode);
        std::puts("PASS grid-storage-initialize61 3 focused PPC cases");return 0;
    } catch(const std::exception& error) {std::fprintf(stderr,"%s\n",error.what());return 1;}
}
