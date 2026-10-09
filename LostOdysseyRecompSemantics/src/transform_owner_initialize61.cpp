#include "lo_semantics/transform_owner_initialize61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::transform_owner_initialize61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
void CompareZero(Registers& s,std::uint64_t value) {
    const auto word=Address(value);s.cr6={0u,std::uint8_t(word!=0u),std::uint8_t(word==0u),s.xer_so};
}
void Initialize(GuestMemory& m,Dependencies deps,Registers& s) {
    auto& r=s.r;r[12]=s.lr;s.lr=0x82bd1300u;
    for(unsigned i=29u;i<=31u;++i)WriteU64(m,Address(r[1]-16u-8u*(31u-i)),r[i]);
    m.WriteU32(Address(r[1]-8u),Address(r[12]));const auto caller_sp=r[1];r[1]-=112u;
    m.WriteU32(Address(r[1]),Address(caller_sp));r[31]=r[3];r[30]=r[4];r[29]=r[5];
    r[3]=m.ReadU32(Address(r[31]+16u));CompareZero(s,r[3]);
    if(!s.cr6.eq) {
        r[11]=m.ReadU32(Address(r[3]));r[4]=1u;r[11]=m.ReadU32(Address(r[11]));
        s.ctr=r[11];s.lr=0x82bd1330u;deps.guest.CallIndirect(Address(s.ctr)&~3u,m,s);
        r[11]=0u;m.WriteU32(Address(r[31]+16u),Address(r[11]));
    }
    r[11]=Address(r[30])&255u;CompareZero(s,r[11]);r[11]=m.ReadU32(Address(r[31]+8u));
    if(!s.cr6.eq)r[11]|=2u;
    else r[11]=(r[11]|(r[11]<<32u))&0xfffffffffffffffdull;
    m.WriteU32(Address(r[31]+8u),Address(r[11]));
    r[11]=Address(r[29])&255u;CompareZero(s,r[11]);r[11]=m.ReadU32(Address(r[31]+8u));
    r[11]=s.cr6.eq ? r[11]&0xfffffffeu : r[11]|1u;
    m.WriteU32(Address(r[31]+8u),Address(r[11]));r[11]=Address(r[11]);
    r[10]=r[11]&2u;r[11]&=1u;CompareZero(s,r[10]);
    const bool first_mode=!s.cr6.eq;CompareZero(s,r[11]);const bool wide=!s.cr6.eq;
    // Only control-flow constants survive calls; all guest values stay in s.
    const GuestAddress allocator_lr=first_mode ? (wide?0x82bd1398u:0x82bd13c4u) : (wide?0x82bd13f8u:0x82bd1424u);
    const GuestAddress allocation_lr=first_mode ? (wide?0x82bd13b0u:0x82bd13dcu) : (wide?0x82bd1410u:0x82bd143cu);
    const GuestAddress constructor=first_mode ? (wide?0x82bdb610u:0x82bdb310u) : (wide?0x82bdc1d0u:0x82bdbd58u);
    s.lr=allocator_lr;(void)crt_close_recursive_buffer_context::Apply(0x82bd0798u,m,deps.guest,s);
    r[11]=m.ReadU32(Address(r[3]));r[5]=0u;r[4]=wide?36u:12u;
    r[11]=m.ReadU32(Address(r[11]));s.ctr=r[11];s.lr=allocation_lr;
    deps.guest.CallIndirect(Address(s.ctr)&~3u,m,s);CompareZero(s,r[3]);
    if(!s.cr6.eq) {
        s.lr=allocation_lr+12u;(void)transform_owner_routes61::Apply(constructor,m,deps,s);
    } else r[3]=0u;
    r[11]=Address(r[3]);m.WriteU32(Address(r[31]+16u),Address(r[3]));
    r[11]=r[11]==0u?1u:0u;r[3]=r[11]^1u;
    r[1]+=112u;
    for(unsigned i=29u;i<=31u;++i)r[i]=ReadU64(m,Address(r[1]-16u-8u*(31u-i)));
    r[12]=m.ReadU32(Address(r[1]-8u));s.lr=r[12];
}
}
bool Apply(GuestAddress entry,GuestMemory& m,Dependencies deps,Registers& state) {
    if(entry!=0x82bd12f8u)return false;
    Initialize(m,deps,state);return true;
}
}
