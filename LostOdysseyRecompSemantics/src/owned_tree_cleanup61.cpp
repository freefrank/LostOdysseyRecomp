#include "lo_semantics/owned_tree_cleanup61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/object_sort_support61.h"
#include <bit>
namespace lo::semantic::gpu::owned_tree_cleanup61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
void Compare(Registers& s,std::uint64_t a,std::uint64_t b) {
    const auto x=Address(a),y=Address(b);
    s.cr6={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),s.xer_so};
}
void CompareRemaining(Registers& s) {
    const auto remaining=std::bit_cast<std::int32_t>(Address(s.r[30]));
    s.cr6={std::uint8_t(remaining<0),std::uint8_t(remaining>0),std::uint8_t(remaining==0),s.xer_so};
}
void Dispose(GuestMemory& memory,GuestServices& guest,Registers& s,
    unsigned allocation_register,GuestAddress table_return,GuestAddress dispose_return) {
    auto& r=s.r;s.lr=table_return;
    (void)crt_close_recursive_buffer_context::Apply(0x82bd0798u,memory,guest,s);
    r[11]=memory.ReadU32(Address(r[3]));r[4]=r[allocation_register];
    r[11]=memory.ReadU32(Address(r[11]+12u));s.ctr=r[11];s.lr=dispose_return;
    guest.CallIndirect(Address(s.ctr)&~3u,memory,s);
}
void Cleanup(GuestMemory& memory,GuestServices& guest,Registers& s) {
    auto& r=s.r;r[12]=s.lr;s.lr=0x82bd9748u;
    for(unsigned i=27u;i<=31u;++i)WriteU64(memory,Address(r[1]-16u-8u*(31u-i)),r[i]);
    memory.WriteU32(Address(r[1]-8u),Address(r[12]));const auto caller_sp=r[1];r[1]-=128u;
    memory.WriteU32(Address(r[1]),Address(caller_sp));r[27]=r[4];r[31]=r[3];
    r[11]=r[27]&2u;Compare(s,r[11],0u);
    if(!s.cr6.eq) {
        r[28]=r[31]-4u;r[11]=memory.ReadU32(Address(r[28]));
        r[10]=(r[11]<<2u)&0xfffffffcu;r[30]=r[11]-1u;r[11]+=r[10];CompareRemaining(s);
        r[11]=(r[11]<<3u)&0xfffffff8u;r[11]+=r[31];
        if(!s.cr6.lt) {
            // Point at each node's child field while moving backwards over
            // 40-byte elements. Reload all live state after recursive disposal.
            r[31]=r[11]+24u;r[29]=0u;
            do {
                r[31]-=40u;r[11]=memory.ReadU32(Address(r[31]));r[10]=r[11]&1u;
                r[3]=r[11]&0xfffffffeu;Compare(s,r[10],0u);
                if(s.cr6.eq) {
                    Compare(s,r[3],0u);
                    if(!s.cr6.eq){r[4]=3u;s.lr=0x82bd97b4u;Cleanup(memory,guest,s);}
                }
                --r[30];memory.WriteU32(Address(r[31]+8u),Address(r[29]));
                memory.WriteU32(Address(r[31]+12u),Address(r[29]));CompareRemaining(s);
            }while(!s.cr6.lt);
        }
        r[11]=r[27]&1u;Compare(s,r[11],0u);
        if(!s.cr6.eq)Dispose(memory,guest,s,28u,0x82bd97d8u,0x82bd97ecu);
        r[3]=r[28];
    } else {
        r[11]=memory.ReadU32(Address(r[31]+24u));r[10]=r[11]&1u;r[3]=r[11]&0xfffffffeu;
        Compare(s,r[10],0u);
        if(s.cr6.eq) {
            Compare(s,r[3],0u);
            if(!s.cr6.eq){r[4]=3u;s.lr=0x82bd981cu;Cleanup(memory,guest,s);}
        }
        r[29]=0u;r[11]=r[27]&1u;Compare(s,r[11],0u);
        memory.WriteU32(Address(r[31]+32u),Address(r[29]));memory.WriteU32(Address(r[31]+36u),Address(r[29]));
        if(!s.cr6.eq)Dispose(memory,guest,s,31u,0x82bd9838u,0x82bd984cu);
        r[3]=r[31];
    }
    r[1]+=128u;
    for(unsigned i=27u;i<=31u;++i)r[i]=ReadU64(memory,Address(r[1]-16u-8u*(31u-i)));
    r[12]=memory.ReadU32(Address(r[1]-8u));s.lr=r[12];
}
void CleanupOwner(GuestMemory& memory,Dependencies deps,Registers& s) {
    auto& r=s.r;r[12]=s.lr;s.lr=0x82bdac90u;
    for(unsigned i=29u;i<=31u;++i)WriteU64(memory,Address(r[1]-16u-8u*(31u-i)),r[i]);
    memory.WriteU32(Address(r[1]-8u),Address(r[12]));const auto caller_sp=r[1];r[1]-=112u;
    memory.WriteU32(Address(r[1]),Address(caller_sp));r[31]=r[3];r[29]=0u;
    r[30]=memory.ReadU32(Address(r[31]+24u));Compare(s,r[30],0u);
    if(!s.cr6.eq) {
        r[3]=r[30];s.lr=0x82bdacb0u;
        (void)object_sort_support61::Apply(0x82bd2c08u,memory,{deps.guest,deps.fp},s);
        Dispose(memory,deps.guest,s,30u,0x82bdacb4u,0x82bdacc8u);
        memory.WriteU32(Address(r[31]+24u),Address(r[29]));
    }
    r[3]=memory.ReadU32(Address(r[31]+4u));Compare(s,r[3],0u);
    if(!s.cr6.eq) {
        r[4]=3u;s.lr=0x82bdace0u;Cleanup(memory,deps.guest,s);
        memory.WriteU32(Address(r[31]+4u),Address(r[29]));
    }
    r[11]=memory.ReadU32(Address(r[31]));Compare(s,r[11],0u);
    if(!s.cr6.eq) {
        // The raw pointer is reloaded after resolving the shared allocator.
        s.lr=0x82bdacf4u;
        (void)crt_close_recursive_buffer_context::Apply(0x82bd0798u,memory,deps.guest,s);
        r[11]=memory.ReadU32(Address(r[3]));r[4]=memory.ReadU32(Address(r[31]));
        r[11]=memory.ReadU32(Address(r[11]+12u));s.ctr=r[11];s.lr=0x82bdad08u;
        deps.guest.CallIndirect(Address(s.ctr)&~3u,memory,s);
        memory.WriteU32(Address(r[31]),Address(r[29]));
    }
    r[1]+=112u;
    for(unsigned i=29u;i<=31u;++i)r[i]=ReadU64(memory,Address(r[1]-16u-8u*(31u-i)));
    r[12]=memory.ReadU32(Address(r[1]-8u));s.lr=r[12];
}
}
bool Apply(GuestAddress entry,GuestMemory& memory,GuestServices& guest,Registers& state) {
    if(entry!=0x82bd9740u)return false;
    Cleanup(memory,guest,state);return true;
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies deps,Registers& state) {
    if(entry==0x82bd9740u){Cleanup(memory,deps.guest,state);return true;}
    if(entry!=0x82bdac88u)return false;
    CleanupOwner(memory,deps,state);return true;
}
}
