#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/manager_cached_links61.h"
namespace manager_cached_links61_oracle {
using Registers=manager_cached_links61::Registers;
constexpr GuestAddress List=0x30000u,A=0x31000u,B=0x32000u,Cache=0x33000u,Links=0x34000u,OtherLinks=0x35000u;
constexpr GuestAddress Plain=0x36000u,Retained=0x36100u,Linked=0x36200u,Unowned=0x36300u,Inactive=0x36400u;
constexpr GuestAddress DirtyCache=0x83315ee8u,DirtyLinks=0x83315eecu,GlobalList=0x8336910cu;
constexpr std::array<test::Region,3> Regions{{{0u,0x120000u},{0x83315000u,0x1000u},{0x83369000u,0x1000u}}};
GuestMemory* active_memory=nullptr;
void Seed(test::GuestWindow& w,unsigned mode){
    w.Fill(0xa5u);auto m=w.Memory();m.WriteU32(GlobalList,List);m.WriteU32(GlobalList+4u,2u);
    m.WriteU32(DirtyCache,mode==0u?0u:1u);m.WriteU32(DirtyLinks,mode==2u?1u:0u);m.WriteU32(List,A);m.WriteU32(List+4u,B);
    m.WriteU32(A+184u,Cache);m.WriteU32(A+188u,3u);m.WriteU32(B+188u,0u);
    m.WriteU32(Cache+28u,Plain);m.WriteU32(Cache+40u+28u,Retained);m.WriteU32(Cache+80u+28u,0u);
    WriteU64(m,Plain+8u,0u);WriteU64(m,Retained+8u,1ull<<58u);
    m.WriteU32(A+196u,Links);m.WriteU32(A+200u,4u);m.WriteU32(B+196u,OtherLinks);m.WriteU32(B+200u,2u);
    m.WriteU32(Links+48u,Linked);m.WriteU32(Links+76u,1u);m.WriteU32(Linked+28u,B);m.WriteU32(Linked+32u,1u);
    m.WriteU32(Links+108u+48u,Unowned);m.WriteU32(Links+108u+76u,1u);m.WriteU32(Unowned+28u,0u);m.WriteU32(Unowned+32u,55u);
    m.WriteU32(Links+216u+48u,Inactive);m.WriteU32(Links+216u+76u,0u);m.WriteU32(Inactive+28u,A);m.WriteU32(Inactive+32u,2u);
    m.WriteU32(Links+324u+48u,0u);m.WriteU32(OtherLinks+48u,0u);m.WriteU32(OtherLinks+108u+48u,Linked);
}
void Check(unsigned mode){
    test::GuestWindow before(Regions),after(Regions);Seed(before,mode);Seed(after,mode);auto om=before.Memory(),m=after.Memory();Registers s{};
    for(unsigned i=0u;i<32u;++i){s.r[i]=0x1122334400000000ull+i;s.fpr_bits[i]=0x3ff0000000000000ull+i;}
    s.r[1]=0x8877665500080000ull;s.lr=0x9988776681234567ull;s.cached_fp_control=0x9fc0u;s.xer_so=1u;s.xer_ca=1u;
    const auto initial=s;PPCContext c{};crt_full_oracle::ToPpc(c,s);active_memory=&om;__imp__sub_824002F0(c,before.Bytes());active_memory=nullptr;
    if(!manager_cached_links61::Apply(0x824002f0u,m,s)||crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c))!=crt_full_oracle::Snapshot(s)||!before.EqualCommitted(after))
        throw std::runtime_error("manager cached links Full72/RAM mismatch");
    if(m.ReadU32(DirtyCache)!=0u||m.ReadU32(DirtyLinks)!=0u||s.r[1]!=initial.r[1]||s.lr!=Address(initial.lr))
        throw std::runtime_error("manager dirty flags/frame mismatch");
    if(mode!=0u){
        if(m.ReadU32(Cache+28u)!=0u||m.ReadU32(Cache+68u)!=Retained||m.ReadU32(Cache+108u)!=0u)
            throw std::runtime_error("cached target bit58 retention mismatch");
        for(unsigned i=0u;i<3u;++i)if(m.ReadU32(Cache+i*40u+32u)!=0u||m.ReadU32(Cache+i*40u+36u)!=0xffffffffu)
            throw std::runtime_error("cached state reset mismatch");
    }
    if(mode==2u){
        if(m.ReadU32(Links+48u)!=0u||m.ReadU32(Links+156u)!=0u||m.ReadU32(OtherLinks+156u)!=0u||
            m.ReadU32(Linked+28u)!=0u||m.ReadU32(Linked+32u)!=0xffffffffu||m.ReadU32(Unowned+32u)!=0xffffffffu||
            m.ReadU32(Links+264u)!=Inactive||m.ReadU32(Inactive+28u)!=A||m.ReadU32(Inactive+32u)!=2u)
            throw std::runtime_error("bidirectional detach/inactive retention mismatch");
    }
}
}
void OriginalManagerCachedLinks61Save(unsigned first,PPCContext& c,std::uint8_t*){
    auto& m=*manager_cached_links61_oracle::active_memory;const auto r=crt_full_oracle::Gprs(c);
    for(unsigned i=first;i<=31u;++i)WriteU64(m,Address(c.r1.u64-16u-8u*(31u-i)),r[i]->u64);
    m.WriteU32(Address(c.r1.u64-8u),c.r12.u32);
}
void OriginalManagerCachedLinks61Restore(unsigned first,PPCContext& c,std::uint8_t*){
    auto& m=*manager_cached_links61_oracle::active_memory;const auto r=crt_full_oracle::Gprs(c);
    for(unsigned i=first;i<=31u;++i)r[i]->u64=ReadU64(m,Address(c.r1.u64-16u-8u*(31u-i)));
    c.r12.u64=m.ReadU32(Address(c.r1.u64-8u));c.lr=c.r12.u64;
}
int main(){try{for(unsigned mode=0u;mode<3u;++mode)manager_cached_links61_oracle::Check(mode);std::puts("PASS manager-cached-links61 3 actual-body cases");return 0;}
catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
