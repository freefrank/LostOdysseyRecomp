#include "lo_semantics/tree_mesh_lifetime61.h"
#include "lo_semantics/grid_transform_buffer61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::tree_mesh_lifetime61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
struct Lifetime {
    GuestMemory& memory; Dependencies deps; Registers& state;
    std::uint32_t Word(std::uint64_t p) { return memory.ReadU32(Address(p)); }
    void Store(std::uint64_t p, std::uint64_t v) { memory.WriteU32(Address(p), Address(v)); }
    void CompareZero(std::uint64_t v) { const auto x=Address(v); state.cr6={0u,std::uint8_t(x!=0u),std::uint8_t(x==0u),state.xer_so}; }
    void Enter(unsigned first,unsigned frame,GuestAddress continuation=0) {
        auto& r=state.r;r[12]=state.lr;if(continuation)state.lr=continuation;
        Store(r[1]-8u,r[12]);
        for(unsigned i=first;i<32;++i)WriteU64(memory,Address(r[1]-16u-8u*(31u-i)),r[i]);
        const auto stack=r[1];r[1]-=frame;Store(r[1],stack);
    }
    void Leave(unsigned first,unsigned frame) {
        auto& r=state.r;r[1]+=frame;r[12]=Word(r[1]-8u);state.lr=r[12];
        for(unsigned i=first;i<32;++i)r[i]=ReadU64(memory,Address(r[1]-16u-8u*(31u-i)));
    }
    void Release(GuestAddress continuation) {
        state.lr=continuation;(void)owned_tree_reorder_support61::Apply(0x82bd20f0u,memory,deps,state);
    }
    void Indirect(GuestAddress continuation) {
        state.ctr=state.r[11];state.lr=continuation;deps.guest.CallIndirect(Address(state.ctr)&~3u,memory,state);
    }
    void Attach() {
        Enter(29,112,0x82bd2208u);auto& r=state.r;r[30]=r[4];r[31]=r[3];r[3]=r[30];r[29]=r[5];
        state.lr=0x82bd2220u;(void)grid_transform_buffer61::Apply(0x82bd17f0u,memory,state);
        r[11]=Address(r[3])&255u;CompareZero(r[11]);
        if(state.cr6.eq)r[3]=0;
        else {
            r[3]=r[31];Release(0x82bd2240u);r[11]=Word(r[31]);r[4]=r[29];Store(r[31]+4u,r[30]);
            r[3]=r[31];r[11]=Word(r[11]+28u);Indirect(0x82bd225cu);
        }
        Leave(29,112);
    }
    void Destroy(bool deleting) {
        Enter(deleting?30:31,deleting?112:96);auto& r=state.r;r[11]=0xffffffff820d0000ull;r[31]=r[3];r[11]+=27700u;
        if(deleting)r[30]=r[4];Store(r[31],r[11]);Release(deleting?0x82bd2824u:0x82bd228cu);
        r[3]=r[31];state.lr=deleting?0x82bd282cu:0x82bd2294u;
        (void)transform_owner_routes61::Apply(0x82bd1770u,memory,deps,state);
        if(deleting) {
            r[11]=Address(r[30])&1u;CompareZero(r[11]);
            if(!state.cr6.eq) {
                state.lr=0x82bd283cu;(void)crt_close_recursive_buffer_context::Apply(0x82bd0798u,memory,deps.guest,state);
                r[11]=Word(r[3]);r[4]=r[31];r[11]=Word(r[11]+12u);Indirect(0x82bd2850u);
            }
            r[3]=r[31];
        }
        Leave(deleting?30:31,deleting?112:96);
    }
};
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies deps,Registers& state) {
    Lifetime operation{memory,deps,state};
    switch(entry) {
    case 0x82bd2200u:operation.Attach();return true;
    case 0x82bd2268u:operation.Destroy(false);return true;
    case 0x82bd27f8u:operation.Destroy(true);return true;
    default:return false;
    }
}
}
