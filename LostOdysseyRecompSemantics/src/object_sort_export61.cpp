#include "lo_semantics/object_sort_export61.h"
#include "lo_semantics/crt_close_upper61.h"
#include "lo_semantics/crt_close_next61.h"
#include "lo_semantics/crt_reader_cleanup_callers_context.h"
#include "lo_semantics/crt_reader_object_chain61.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::object_sort_export61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
void Compare(Registers& s,std::uint64_t a,std::uint64_t b) {
    const auto x=Address(a),y=Address(b);
    s.cr6={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),s.xer_so};
}
void Call(GuestAddress entry,GuestAddress continuation,GuestMemory& m,Dependencies d,Registers& s) {
    s.lr=continuation;(void)ApplyAcceptedLower(entry,m,d,s);
}
void Enter(GuestMemory& m,Registers& s,bool copy) {
    auto& r=s.r;r[12]=s.lr;m.WriteU32(Address(r[1]-8u),Address(r[12]));
    if(copy)WriteU64(m,Address(r[1]-24u),r[30]);
    WriteU64(m,Address(r[1]-16u),r[31]);const auto caller=r[1];r[1]-=copy?144u:128u;m.WriteU32(Address(r[1]),Address(caller));
    r[31]=r[3];if(copy)r[30]=r[4];r[11]=m.ReadU32(Address(r[31]+184u));Compare(s,r[11],0u);
}
void Leave(GuestMemory& m,Registers& s,bool copy) {
    auto& r=s.r;r[1]+=copy?144u:128u;r[12]=m.ReadU32(Address(r[1]-8u));s.lr=r[12];
    if(copy)r[30]=ReadU64(m,Address(r[1]-24u));
    r[31]=ReadU64(m,Address(r[1]-16u));
}
void Export(bool copy,GuestMemory& m,Dependencies d,Registers& s) {
    Enter(m,s,copy);auto& r=s.r;
    if(s.cr6.eq){r[3]=0u;Leave(m,s,copy);return;}
    r[5]=0u;r[4]=4096u;r[3]=r[1]+80u;
    Call(0x82bd0cd0u,copy?0x82b9dfd8u:0x82b9df60u,m,d,s);
    r[5]=r[1]+80u;r[4]=0u;r[3]=m.ReadU32(Address(r[31]+184u));
    Call(0x82bafec0u,copy?0x82b9dfe8u:0x82b9df70u,m,d,s);
    r[3]=r[1]+80u;if(copy)r[31]=m.ReadU32(Address(r[30]));
    Call(0x82bd0900u,copy?0x82b9dff4u:0x82b9df78u,m,d,s);
    if(!copy){
        r[31]=r[3];r[3]=r[1]+80u;Call(0x82bd0df0u,0x82b9df84u,m,d,s);r[3]=r[31];
    }else{
        Compare(s,r[31],r[3]);r[3]=r[1]+80u;
        if(!s.cr6.eq){Call(0x82bd0df0u,0x82b9e004u,m,d,s);r[3]=0u;}
        else{
            r[4]=0u;Call(0x82bd0ea8u,0x82b9e014u,m,d,s);
            r[4]=r[3];r[5]=r[31];r[3]=m.ReadU32(Address(r[30]+4u));
            Call(0x82b7a0b0u,0x82b9e024u,m,d,s);
            r[3]=r[1]+80u;Call(0x82bd0df0u,0x82b9e02cu,m,d,s);r[3]=1u;
        }
    }
    Leave(m,s,copy);
}
}
bool ApplyAcceptedLower(GuestAddress entry,GuestMemory& m,Dependencies d,Registers& s) {
    switch(entry){
    case 0x82bd0cd0u:return crt_close_upper61::Apply(entry,m,d.engine.sort,s);
    case 0x82bafec0u:return object_sort_dispatch61::Apply(entry,m,d,s);
    case 0x82bd0900u:return crt_close_next61::Apply(entry,m,d.engine.sort.guest,s);
    case 0x82bd0df0u:return crt_reader_cleanup_callers_context::Apply(entry,m,d.engine.sort.guest,s);
    case 0x82bd0ea8u:return crt_reader_object_chain61::Apply(entry,m,d.engine.sort,s);
    case 0x82b7a0b0u:return crt_copy_full_context::Apply(entry,m,s);
    default:return false;
    }
}
bool Apply(GuestAddress entry,GuestMemory& m,Dependencies d,Registers& s) {
    if(entry!=0x82b9df18u&&entry!=0x82b9dfa0u)return false;
    Export(entry==0x82b9dfa0u,m,d,s);return true;
}
}
