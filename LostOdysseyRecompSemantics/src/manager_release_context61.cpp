#include "lo_semantics/manager_release_context61.h"
#include "lo_semantics/manager_init_context.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::manager_release_context61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
using InitRegisters=manager_init_context::Registers;
void ToInit(const Registers& full,InitRegisters& init) {
    init.r=full.r;init.lr=full.lr;init.ctr=full.ctr;
    init.f0_bits=full.fpr_bits[0];init.f1_bits=full.fpr_bits[1];init.f13_bits=full.fpr_bits[13];
    init.f30_bits=full.fpr_bits[30];init.f31_bits=full.fpr_bits[31];
    init.cached_fp_control=full.cached_fp_control;init.xer_so=full.xer_so;init.xer_ca=full.xer_ca;
    init.cr6={full.cr6.lt,full.cr6.gt,full.cr6.eq,full.cr6.so};
}
void FromInit(Registers& full,const InitRegisters& init) {
    full.r=init.r;full.lr=init.lr;full.ctr=init.ctr;
    full.fpr_bits[0]=init.f0_bits;full.fpr_bits[1]=init.f1_bits;full.fpr_bits[13]=init.f13_bits;
    full.fpr_bits[30]=init.f30_bits;full.fpr_bits[31]=init.f31_bits;
    full.cached_fp_control=init.cached_fp_control;full.xer_so=init.xer_so;full.xer_ca=init.xer_ca;
    full.cr6={init.cr6.lt,init.cr6.gt,init.cr6.eq,init.cr6.un};
}
class InitBoundary final:public manager_init_context::PpcBoundaryServices {
public:
    InitBoundary(GuestServices& guest,Registers& full):guest_(guest),full_(full){}
    void CallDirect(GuestAddress target,GuestMemory& memory,InitRegisters& init)override {
        FromInit(full_,init);guest_.CallDirect(target,memory,full_);ToInit(full_,init);
    }
    void CallVirtual(GuestAddress target,GuestMemory& memory,InitRegisters& init)override {
        FromInit(full_,init);guest_.CallIndirect(target,memory,full_);ToInit(full_,init);
    }
private:
    GuestServices& guest_;Registers& full_;
};
void CompareZero(Registers& state,std::uint64_t value) {
    const auto word=Address(value);
    state.cr6={0u,std::uint8_t(word!=0u),std::uint8_t(word==0u),state.xer_so};
}
void Release(GuestMemory& memory,Dependencies deps,Registers& state) {
    auto& r=state.r;r[12]=state.lr;memory.WriteU32(Address(r[1]-8u),Address(r[12]));
    WriteU64(memory,Address(r[1]-24u),r[30]);WriteU64(memory,Address(r[1]-16u),r[31]);
    const auto caller_sp=r[1];r[1]-=112u;memory.WriteU32(Address(r[1]),Address(caller_sp));
    r[30]=r[3];r[31]=0xffffffff83310000ull;r[3]=memory.ReadU32(Address(r[31]-18936u));
    CompareZero(state,r[3]);
    if(state.cr6.eq) {
        state.lr=0x823f336cu;
        InitRegisters init{};ToInit(state,init);InitBoundary boundary(deps.guest,state);
        manager_init_context::Apply(memory,boundary,init);FromInit(state,init);
        r[3]=memory.ReadU32(Address(r[31]-18936u));
    }
    r[11]=memory.ReadU32(Address(r[3]));r[4]=r[30];r[11]=memory.ReadU32(Address(r[11]+12u));
    state.ctr=r[11];state.lr=0x823f3384u;deps.guest.CallIndirect(Address(state.ctr)&~3u,memory,state);
    r[1]+=112u;r[12]=memory.ReadU32(Address(r[1]-8u));state.lr=r[12];
    r[30]=ReadU64(memory,Address(r[1]-24u));r[31]=ReadU64(memory,Address(r[1]-16u));
}
void CleanupOwner(GuestMemory& memory,Dependencies deps,Registers& state) {
    auto& r=state.r;r[12]=state.lr;memory.WriteU32(Address(r[1]-8u),Address(r[12]));
    WriteU64(memory,Address(r[1]-16u),r[31]);const auto caller_sp=r[1];r[1]-=96u;
    memory.WriteU32(Address(r[1]),Address(caller_sp));r[31]=r[3];state.lr=0x82bdb278u;
    (void)owned_tree_cleanup61::Apply(0x82bdac88u,memory,{deps.guest,deps.fp},state);
    r[3]=memory.ReadU32(Address(r[31]+8u));CompareZero(state,r[3]);
    if(!state.cr6.eq) {
        state.lr=0x82bdb288u;Release(memory,deps,state);
        r[11]=0u;memory.WriteU32(Address(r[31]+8u),Address(r[11]));
    }
    r[11]=0u;memory.WriteU32(Address(r[31]+12u),Address(r[11]));
    r[1]+=96u;r[12]=memory.ReadU32(Address(r[1]-8u));state.lr=r[12];r[31]=ReadU64(memory,Address(r[1]-16u));
}
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies deps,Registers& state) {
    switch(entry) {
    case 0x823f3340u:case 0x82388b58u:Release(memory,deps,state);return true;
    case 0x82bdb260u:CleanupOwner(memory,deps,state);return true;
    default:return false;
    }
}
}
