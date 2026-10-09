#include "lo_semantics/owned_tree_visit61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::owned_tree_visit61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
void Compare(Registers& s,std::uint64_t a,std::uint64_t b) {
    const auto x=Address(a),y=Address(b);s.cr6={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),s.xer_so};
}
void Enter(unsigned first,GuestAddress continuation,GuestMemory& m,Registers& s) {
    auto& r=s.r;r[12]=s.lr;s.lr=continuation;
    for(unsigned i=first;i<32u;++i)WriteU64(m,Address(r[1]-8u*(33u-i)),r[i]);
    m.WriteU32(Address(r[1]-8u),Address(r[12]));const auto caller=r[1];r[1]-=128u;m.WriteU32(Address(r[1]),Address(caller));
}
void Leave(unsigned first,GuestMemory& m,Registers& s) {
    auto& r=s.r;r[1]+=128u;for(unsigned i=first;i<32u;++i)r[i]=ReadU64(m,Address(r[1]-8u*(33u-i)));
    r[12]=m.ReadU32(Address(r[1]-8u));s.lr=r[12];
}
bool Visit(GuestAddress continuation,std::uint64_t target,GuestMemory& m,GuestServices& guest,Registers& s) {
    s.ctr=target;s.lr=continuation;guest.CallIndirect(Address(s.ctr)&~3u,m,s);
    s.r[11]=Address(s.r[3])&0xffu;Compare(s,s.r[11],0u);return !s.cr6.eq;
}
void DepthFirst(GuestMemory& m,GuestServices& guest,Registers& s) {
    Enter(27u,0x82bda0b0u,m,s);auto& r=s.r;
    r[28]=r[3];r[30]=r[4];r[31]=r[5];r[29]=r[6];r[27]=r[7];Compare(s,r[28],0u);
    if(!s.cr6.eq){
        r[11]=m.ReadU32(Address(r[31]));++r[11];m.WriteU32(Address(r[31]),Address(r[11]));
        r[10]=m.ReadU32(Address(r[30]));Compare(s,r[11],r[10]);if(s.cr6.gt)m.WriteU32(Address(r[30]),Address(r[11]));
        Compare(s,r[29],0u);bool descend=true;
        if(!s.cr6.eq){r[4]=m.ReadU32(Address(r[31]));r[5]=r[27];r[3]=r[28];descend=Visit(0x82bda108u,r[29],m,guest,s);}
        if(descend){
            r[11]=m.ReadU32(Address(r[28]+24u));r[10]=Address(r[11])&0xfffffffeu;Compare(s,r[10],0u);
            if(!s.cr6.eq){
                r[7]=r[27];r[6]=r[29];r[5]=r[31];r[4]=r[30];r[3]=Address(r[11])&0xfffffffeu;s.lr=0x82bda13cu;
                DepthFirst(m,guest,s);r[11]=m.ReadU32(Address(r[31]));--r[11];m.WriteU32(Address(r[31]),Address(r[11]));
            }
            r[11]=m.ReadU32(Address(r[28]+24u));r[11]=Address(r[11])&0xfffffffeu;Compare(s,r[11],0u);
            if(!s.cr6.eq){
                r[10]=r[11]+40u;Compare(s,r[10],0u);
                if(!s.cr6.eq){
                    Compare(s,r[11],0u);r[3]=r[11]+40u;if(s.cr6.eq)r[3]=0u;
                    r[7]=r[27];r[6]=r[29];r[5]=r[31];r[4]=r[30];s.lr=0x82bda188u;
                    DepthFirst(m,guest,s);r[11]=m.ReadU32(Address(r[31]));--r[11];m.WriteU32(Address(r[31]),Address(r[11]));
                }
            }
        }
    }
    Leave(27u,m,s);
}
void ChildrenFirst(GuestMemory& m,GuestServices& guest,Registers& s) {
    Enter(28u,0x82bda1a8u,m,s);auto& r=s.r;r[29]=r[4];r[28]=r[5];
    for(;;){
        r[11]=m.ReadU32(Address(r[3]+24u));r[31]=Address(r[11])&0xfffffffeu;Compare(s,r[31],0u);
        r[30]=r[31]+40u;if(s.cr6.eq)r[30]=0u;
        if(!s.cr6.eq){r[5]=r[28];r[4]=0u;r[3]=r[31];if(!Visit(0x82bda1e4u,r[29],m,guest,s))break;}
        Compare(s,r[30],0u);
        if(!s.cr6.eq){r[5]=r[28];r[4]=0u;r[3]=r[30];if(!Visit(0x82bda20cu,r[29],m,guest,s))break;}
        Compare(s,r[31],0u);
        if(!s.cr6.eq){r[5]=r[28];r[4]=r[29];r[3]=r[31];s.lr=0x82bda230u;ChildrenFirst(m,guest,s);}
        Compare(s,r[30],0u);if(s.cr6.eq)break;r[3]=r[30];
    }
    Leave(28u,m,s);
}
}
bool Apply(GuestAddress entry,GuestMemory& m,GuestServices& guest,Registers& s) {
    switch(entry){case 0x82bda0a8u:DepthFirst(m,guest,s);return true;case 0x82bda1a0u:ChildrenFirst(m,guest,s);return true;default:return false;}
}
}
