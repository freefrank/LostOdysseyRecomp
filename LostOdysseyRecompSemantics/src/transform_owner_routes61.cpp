#include "lo_semantics/transform_owner_routes61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/pointer_fields.h"
#include <array>
namespace lo::semantic::gpu::transform_owner_routes61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
void CompareZero(Registers& s,std::uint64_t value) {
    const auto word=Address(value);s.cr6={0u,std::uint8_t(word!=0u),std::uint8_t(word==0u),s.xer_so};
}
void Cleanup(GuestMemory& memory,Dependencies deps,Registers& s) {
    auto& r=s.r;r[12]=s.lr;s.lr=0x82bd1560u;
    for(unsigned i=29u;i<=31u;++i)WriteU64(memory,Address(r[1]-16u-8u*(31u-i)),r[i]);
    memory.WriteU32(Address(r[1]-8u),Address(r[12]));const auto caller_sp=r[1];r[1]-=112u;
    memory.WriteU32(Address(r[1]),Address(caller_sp));r[31]=r[3];r[29]=0u;
    r[30]=memory.ReadU32(Address(r[31]+12u));CompareZero(s,r[30]);
    if(!s.cr6.eq) {
        r[3]=r[30];s.lr=0x82bd1580u;(void)manager_release_context61::Apply(0x82bdb260u,memory,deps,s);
        s.lr=0x82bd1584u;(void)crt_close_recursive_buffer_context::Apply(0x82bd0798u,memory,deps.guest,s);
        r[11]=memory.ReadU32(Address(r[3]));r[4]=r[30];r[11]=memory.ReadU32(Address(r[11]+12u));
        s.ctr=r[11];s.lr=0x82bd1598u;deps.guest.CallIndirect(Address(s.ctr)&~3u,memory,s);
        memory.WriteU32(Address(r[31]+12u),Address(r[29]));
    }
    r[3]=memory.ReadU32(Address(r[31]+16u));CompareZero(s,r[3]);
    if(!s.cr6.eq) {
        r[11]=memory.ReadU32(Address(r[3]));r[4]=1u;r[11]=memory.ReadU32(Address(r[11]));
        s.ctr=r[11];s.lr=0x82bd15bcu;deps.guest.CallIndirect(Address(s.ctr)&~3u,memory,s);
        memory.WriteU32(Address(r[31]+16u),Address(r[29]));
    }
    r[1]+=112u;
    for(unsigned i=29u;i<=31u;++i)r[i]=ReadU64(memory,Address(r[1]-16u-8u*(31u-i)));
    r[12]=memory.ReadU32(Address(r[1]-8u));s.lr=r[12];
}
void BaseCleanup(GuestMemory& memory,Dependencies deps,Registers& s) {
    s.r[11]=0xffffffff820d6bb8ull;memory.WriteU32(Address(s.r[3]),Address(s.r[11]));
    Cleanup(memory,deps,s); // true tail call: retain incoming LR and SP
}
void DerivedCleanup(GuestMemory& memory,Dependencies deps,Registers& s) {
    auto& r=s.r;r[12]=s.lr;memory.WriteU32(Address(r[1]-8u),Address(r[12]));
    WriteU64(memory,Address(r[1]-16u),r[31]);const auto caller_sp=r[1];r[1]-=96u;
    memory.WriteU32(Address(r[1]),Address(caller_sp));r[11]=0xffffffff820d0000ull;r[31]=r[3];r[11]+=28096u;
    memory.WriteU32(Address(r[31]),Address(r[11]));s.lr=0x82bd7a44u;Cleanup(memory,deps,s);
    r[3]=r[31];s.lr=0x82bd7a4cu;BaseCleanup(memory,deps,s);
    r[1]+=96u;r[12]=memory.ReadU32(Address(r[1]-8u));s.lr=r[12];r[31]=ReadU64(memory,Address(r[1]-16u));
}
void Construct(GuestMemory& memory,Registers& s,std::uint32_t displacement) {
    const std::uint64_t table=0xffffffff820d0000ull+displacement;
    PointerFieldRegisters fields{};fields.r3=s.r[3];
    const std::array assignments{ConstantFieldAssignment{PointerFieldRegister::R11,table},ConstantFieldAssignment{PointerFieldRegister::R10,0u}};
    const std::array writes{ConstantFieldWrite{4,PointerFieldWidth::Word,0u},
        ConstantFieldWrite{0,PointerFieldWidth::Word,Address(table)},ConstantFieldWrite{8,PointerFieldWidth::Word,0u}};
    InitializeConstantFields(memory,fields,PointerFieldRegister::R3,assignments,writes);s.r[10]=fields.r10;s.r[11]=fields.r11;
}
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies deps,Registers& state) {
    switch(entry) {
    case 0x82bd1558u:Cleanup(memory,deps,state);return true;
    case 0x82bd1770u:BaseCleanup(memory,deps,state);return true;
    case 0x82bd7a20u:DerivedCleanup(memory,deps,state);return true;
    case 0x82bdb310u:Construct(memory,state,28284u);return true;
    case 0x82bdb610u:Construct(memory,state,28316u);return true;
    case 0x82bdbd58u:Construct(memory,state,28348u);return true;
    case 0x82bdc1d0u:Construct(memory,state,28380u);return true;
    default:return false;
    }
}
}
