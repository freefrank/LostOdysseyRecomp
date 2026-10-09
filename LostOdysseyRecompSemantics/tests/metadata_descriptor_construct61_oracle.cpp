#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/metadata_descriptor_construct61.h"
namespace descriptor_construct61_oracle {
using Registers=metadata_descriptor_construct61::Registers;
constexpr GuestAddress Object=0x30000u,Previous=0x31000u,Gate=0x83315ed8u,Head=0x83315ef0u;
constexpr std::array<test::Region,2> Regions{{{0u,0x120000u},{0x83315000u,0x1000u}}};
void Check(unsigned mode){
    test::GuestWindow before(Regions),after(Regions);Registers initial{};
    for(unsigned i=0u;i<32u;++i){initial.r[i]=0x1122334400000000ull+i;initial.fpr_bits[i]=0x3ff0000000000000ull+i;}
    initial.r[1]=0x8877665500080000ull;initial.r[3]=0xaabbccdd00000000ull|Object;initial.r[8]=0x8000000000000020ull;
    initial.lr=0x9988776681234567ull;initial.cached_fp_control=0x9fc0u;initial.xer_so=1u;
    const auto seed=[&](test::GuestWindow& w){w.Fill(0xa5u);auto m=w.Memory();m.WriteU32(Gate,mode==1u?0xffffffffu:0u);m.WriteU32(Head,Previous);
        WriteU64(m,Address(initial.r[1]+80u),0x8000000000000100ull);m.WriteU32(Address(initial.r[1]+92u),0x44445555u);
        m.WriteU32(Address(initial.r[1]+100u),0x66667777u);m.WriteU32(Address(initial.r[1]+108u),0x88889999u);};
    seed(before);seed(after);auto state=initial;PPCContext c{};crt_full_oracle::ToPpc(c,initial);
    if(mode==2u)__imp__sub_82410A28(c,before.Bytes());else __imp__sub_8240CC58(c,before.Bytes());
    auto m=after.Memory();const GuestAddress entry=mode==2u?0x82410a28u:0x8240cc58u;
    if(!metadata_descriptor_construct61::Apply(entry,m,state)||crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c))!=crt_full_oracle::Snapshot(state)||!before.EqualCommitted(after))
        throw std::runtime_error("descriptor construct Full72/RAM mismatch");
    if(m.ReadU32(Head)!=(mode==1u?Previous:Object)||m.ReadU32(Object+32u)!=(mode==1u?0xffffffffu:Previous)||
        state.r[3]!=initial.r[3]||state.r[1]!=initial.r[1]||m.ReadU32(Object+20u)!=0xa5a5a5a5u||m.ReadU32(Object+60u)!=0xa5a5a5a5u)
        throw std::runtime_error("descriptor pending-list/borrowed storage mismatch");
    const auto flags=(mode==2u?0x8000000000000100ull:initial.r[8])|0x0400008000004000ull;
    if(ReadU64(m,Object+8u)!=flags||m.ReadU32(Object+80u)!=Address(initial.r[5])||m.ReadU32(Object+104u)!=1u||
        m.ReadU32(Object+44u)!=Address(initial.r[mode==2u?8u:6u])||m.ReadU32(Object+40u)!=Address(initial.r[mode==2u?9u:7u]))
        throw std::runtime_error("descriptor base fields mismatch");
    if(mode==2u){
        if(state.lr!=Address(initial.lr)||state.r[31]!=initial.r[31]||m.ReadU32(Object)!=0x82005160u||
            m.ReadU32(Object+184u)!=(Address(initial.r[6])|128u)||m.ReadU8(Object+192u)!=std::uint8_t(initial.r[7])||
            m.ReadU32(Object+200u)!=Address(initial.r[10])||m.ReadU32(Object+284u)!=0x44445555u||
            m.ReadU32(Object+288u)!=0x66667777u||m.ReadU32(Object+292u)!=0x88889999u||
            m.ReadU32(Object+312u)!=8u||m.ReadU32(Object+332u)!=8u||m.ReadU32(Object+348u)!=1u||
            m.ReadU16(Object+162u)!=0xa5a5u||m.ReadU32(Object+204u)!=0xa5a5a5a5u)
            throw std::runtime_error("descriptor derived stack arguments/field boundaries mismatch");
    }else if(state.lr!=initial.lr||m.ReadU32(Object)!=0x821915d0u)throw std::runtime_error("descriptor base table/LR mismatch");
}
}
int main(){try{for(unsigned mode=0u;mode<3u;++mode)descriptor_construct61_oracle::Check(mode);std::puts("PASS metadata-descriptor-construct61 3 actual-body cases");return 0;}
catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
