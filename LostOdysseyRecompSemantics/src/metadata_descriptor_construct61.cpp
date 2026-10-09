#include "lo_semantics/metadata_descriptor_construct61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::metadata_descriptor_construct61 {
namespace {
using recovery_abi::Address;using recovery_abi::ReadU64;using recovery_abi::WriteU64;
void ClearWords(GuestMemory& m,Registers& s,unsigned first,unsigned last){
    for(unsigned offset=first;offset<=last;offset+=4u)m.WriteU32(Address(s.r[3]+offset),Address(s.r[11]));
}
void Base(GuestMemory& m,Registers& s){
    auto& r=s.r;r[11]=0xffffffff82190000ull;r[12]=67108864u;r[10]=r[11]-10504u;r[12]|=128u;r[11]=0u;r[12]<<=32u;
    r[9]=~std::uint64_t(0u);m.WriteU32(Address(r[3]),Address(r[10]));r[10]=~std::uint64_t(0u);r[12]|=16384u;
    m.WriteU32(Address(r[3]+16u),Address(r[11]));r[8]|=r[12];m.WriteU32(Address(r[3]+24u),Address(r[11]));
    m.WriteU32(Address(r[3]+4u),Address(r[9]));m.WriteU32(Address(r[3]+32u),Address(r[10]));r[10]=0xffffffff83310000ull;
    m.WriteU32(Address(r[3]+28u),Address(r[11]));WriteU64(m,Address(r[3]+8u),r[8]);m.WriteU32(Address(r[3]+44u),Address(r[11]));
    r[10]=m.ReadU32(Address(r[10]+24280u));m.WriteU32(Address(r[3]+48u),Address(r[11]));
    const auto gate=std::bit_cast<std::int32_t>(Address(r[10]));s.cr6={std::uint8_t(gate<0),std::uint8_t(gate>0),std::uint8_t(gate==0),s.xer_so};
    m.WriteU32(Address(r[3]+52u),Address(r[11]));m.WriteU32(Address(r[3]+56u),Address(r[11]));
    m.WriteU32(Address(r[3]+44u),Address(r[6]));m.WriteU32(Address(r[3]+40u),Address(r[7]));
    if(s.cr6.eq){r[10]=0xffffffff83310000ull;r[9]=m.ReadU32(Address(r[10]+24304u));
        m.WriteU32(Address(r[3]+32u),Address(r[9]));m.WriteU32(Address(r[10]+24304u),Address(r[3]));}
    r[10]=0xffffffff82190000ull;m.WriteU32(Address(r[3]+64u),Address(r[11]));m.WriteU32(Address(r[3]+68u),Address(r[11]));
    r[10]+=5584u;m.WriteU32(Address(r[3]+72u),Address(r[11]));m.WriteU32(Address(r[3]+76u),Address(r[11]));
    m.WriteU32(Address(r[3]+80u),Address(r[5]));m.WriteU32(Address(r[3]),Address(r[10]));r[10]=1u;
    ClearWords(m,s,84u,100u);m.WriteU32(Address(r[3]+104u),Address(r[10]));ClearWords(m,s,108u,136u);
}
void Derived(GuestMemory& m,Registers& s){
    auto& r=s.r;r[12]=s.lr;m.WriteU32(Address(r[1]-8u),Address(r[12]));WriteU64(m,Address(r[1]-16u),r[31]);
    const auto caller_sp=r[1];r[1]-=112u;m.WriteU32(Address(r[1]),Address(caller_sp));r[31]=r[6];
    m.WriteU8(Address(r[1]+167u),std::uint8_t(r[7]));r[6]=r[8];r[8]=ReadU64(m,Address(r[1]+192u));r[7]=r[9];
    m.WriteU32(Address(r[1]+188u),Address(r[10]));r[4]=0u;s.lr=0x82410a58u;Base(m,s);
    r[11]=0xffffffff82000000ull;r[10]=8u;r[7]=m.ReadU32(Address(r[1]+220u));r[11]+=20832u;r[9]=r[31]|128u;
    m.WriteU32(Address(r[1]+84u),Address(r[11]));r[11]=0u;
    WriteU64(m,Address(r[3]+140u),r[11]);WriteU64(m,Address(r[3]+148u),r[11]);m.WriteU32(Address(r[3]+156u),Address(r[11]));
    m.WriteU16(Address(r[3]+160u),std::uint16_t(r[11]));m.WriteU32(Address(r[3]+164u),Address(r[11]));m.WriteU32(Address(r[1]+80u),Address(r[11]));
    r[11]=Address(r[11]);m.WriteU32(Address(r[3]+180u),Address(r[10]));m.WriteU32(Address(r[3]+168u),Address(r[11]));r[11]=0u;
    ClearWords(m,s,172u,176u);m.WriteU32(Address(r[3]+184u),Address(r[9]));r[9]=m.ReadU8(Address(r[1]+167u));
    m.WriteU32(Address(r[3]+188u),Address(r[11]));m.WriteU32(Address(r[3]+196u),Address(r[11]));m.WriteU8(Address(r[3]+192u),std::uint8_t(r[9]));
    r[9]=m.ReadU32(Address(r[1]+204u));r[8]=m.ReadU32(Address(r[1]+84u));m.WriteU32(Address(r[3]),Address(r[8]));
    ClearWords(m,s,208u,252u);r[8]=m.ReadU32(Address(r[1]+212u));ClearWords(m,s,256u,276u);
    m.WriteU32(Address(r[3]+284u),Address(r[9]));r[9]=1u;m.WriteU32(Address(r[3]+280u),Address(r[11]));
    m.WriteU32(Address(r[3]+288u),Address(r[8]));m.WriteU32(Address(r[3]+292u),Address(r[7]));ClearWords(m,s,296u,308u);
    m.WriteU32(Address(r[3]+312u),Address(r[10]));ClearWords(m,s,316u,328u);m.WriteU32(Address(r[3]+332u),Address(r[10]));
    ClearWords(m,s,336u,344u);m.WriteU32(Address(r[3]+348u),Address(r[9]));ClearWords(m,s,352u,372u);
    r[11]=m.ReadU32(Address(r[1]+188u));m.WriteU32(Address(r[3]+200u),Address(r[11]));
    r[1]+=112u;r[12]=m.ReadU32(Address(r[1]-8u));s.lr=r[12];r[31]=ReadU64(m,Address(r[1]-16u));
}
}
bool Apply(GuestAddress entry,GuestMemory& m,Registers& s){
    switch(entry){case 0x8240cc58u:Base(m,s);return true;case 0x82410a28u:Derived(m,s);return true;default:return false;}
}
}
