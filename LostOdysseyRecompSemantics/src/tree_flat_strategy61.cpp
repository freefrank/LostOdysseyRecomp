#include "lo_semantics/tree_flat_strategy61.h"
#include "lo_semantics/tree_flatten36_61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::tree_flat_strategy61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
struct Bind {
    GuestMemory& memory;Dependencies deps;Registers& state;
    std::uint32_t Word(std::uint64_t p){return memory.ReadU32(Address(p));}
    void Store(std::uint64_t p,std::uint64_t v){memory.WriteU32(Address(p),Address(v));}
    void Compare(std::uint64_t a,std::uint64_t b=0){auto x=Address(a),y=Address(b);state.cr6={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),state.xer_so};}
    void Allocator(GuestAddress continuation){state.lr=continuation;(void)crt_close_recursive_buffer_context::Apply(0x82bd0798u,memory,deps.guest,state);}
    void Indirect(GuestAddress continuation){state.ctr=state.r[11];state.lr=continuation;deps.guest.CallIndirect(Address(state.ctr)&~3u,memory,state);}
    bool Storage(){
        auto& r=state.r;r[31]=Word(r[29]+8u);Store(r[29]+4u,r[11]);Compare(r[31]);
        if(!state.cr6.eq){
            Allocator(0x82bdbdf4u);r[11]=Word(r[3]);r[4]=r[31]-4u;r[11]=Word(r[11]+12u);Indirect(0x82bdbe08u);
            r[11]=0;Store(r[29]+8u,r[11]);
        }
        // Retain the original overflow sentinel passed to the allocator;
        // do not add a new error return or roll back the owner count.
        r[11]=119275520u;r[31]=Word(r[29]+4u);r[11]|=29127u;Compare(r[31],r[11]);
        if(!state.cr6.gt){
            r[11]=(r[31]<<3u)&0xfffffff8u;r[10]=std::uint64_t(-5);r[11]+=r[31];
            r[11]=(r[11]<<2u)&0xfffffffcu;Compare(r[11],r[10]);r[30]=r[11]+4u;
            if(state.cr6.gt)r[30]=~std::uint64_t(0);
        }else r[30]=~std::uint64_t(0);
        Allocator(0x82bdbe48u);r[11]=Word(r[3]);r[5]=30;r[4]=r[30];r[11]=Word(r[11]);Indirect(0x82bdbe60u);
        Compare(r[3]);if(!state.cr6.eq){r[11]=r[3]+4u;Store(r[3],r[31]);}else r[11]=0;
        Compare(r[11]);Store(r[29]+8u,r[11]);return !state.cr6.eq;
    }
    bool Run(){
        auto& r=state.r;r[28]=r[4];r[29]=r[3];Compare(r[28]);if(state.cr6.eq)return false;
        r[10]=Word(r[28]+4u);r[11]=Word(r[28]+16u);r[10]=Word(r[10]+36u);
        r[10]=(r[10]<<1u)&0xfffffffeu;--r[10];Compare(r[11],r[10]);if(!state.cr6.eq)return false;
        r[10]=Word(r[29]+4u);Compare(r[10],r[11]);if(!state.cr6.eq&&!Storage())return false;
        r[11]=1;r[6]=Word(r[28]+4u);r[5]=r[1]+80u;r[3]=Word(r[29]+8u);r[4]=0;Store(r[1]+80u,r[11]);
        state.lr=0x82bdbea0u;(void)tree_flatten36_61::Apply(0x82bdbc18u,memory,deps.fp,state);return true;
    }
    void Execute(){
        auto& r=state.r;r[12]=state.lr;state.lr=0x82bdbd98u;
        for(unsigned i=28;i<32;++i)WriteU64(memory,Address(r[1]-16u-8u*(31u-i)),r[i]);
        Store(r[1]-8u,r[12]);const auto stack=r[1];r[1]-=128u;Store(r[1],stack);
        const bool success=Run();r[3]=success?1u:0u;r[1]+=128u;
        for(unsigned i=28;i<32;++i)r[i]=ReadU64(memory,Address(r[1]-16u-8u*(31u-i)));
        r[12]=Word(r[1]-8u);state.lr=r[12];
    }
};
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies deps,Registers& state){
    if(entry!=0x82bdbd90u)return false;
    Bind{memory,deps,state}.Execute();return true;
}
}
