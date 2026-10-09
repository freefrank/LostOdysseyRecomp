#include "lo_semantics/transform_owner_build61.h"
#include "lo_semantics/recovery_abi.h"
namespace lo::semantic::gpu::transform_owner_build61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
void Compare(Registers& s,std::uint64_t a,std::uint64_t b=0u) {
    const auto x=Address(a),y=Address(b);s.cr6={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),s.xer_so};
}
bool LowByteSuccess(Registers& s){s.r[11]=Address(s.r[3])&255u;Compare(s,s.r[11]);return !s.cr6.eq;}
void BuildBody(GuestMemory& m,Dependencies deps,Registers& s) {
    auto& r=s.r;r[30]=r[4];r[29]=r[3];r[3]=m.ReadU32(Address(r[30]));Compare(s,r[3]);
    if(s.cr6.eq){r[3]=0u;return;}
    s.lr=0x82bd7a84u;(void)grid_transform_buffer61::Apply(0x82bd17f0u,m,s);
    if(!LowByteSuccess(s)){r[3]=0u;return;}
    r[28]=r[30]+4u;r[11]=m.ReadU32(Address(r[28]));Compare(s,r[11],1u);
    if(!s.cr6.eq) {
        r[11]=0xffffffff820d0000ull;r[5]=146u;r[4]=r[11]+28192u;r[11]=0xffffffff820d0000ull;r[3]=r[11]+28128u;
        s.lr=0x82bd7ab8u;(void)diagnostic_format_routes61::Apply(0x82b9d328u,m,deps.diagnostics,s);return;
    }
    r[3]=m.ReadU32(Address(r[30]));s.lr=0x82bd7ac8u;(void)grid_transform_buffer61::Apply(0x82bd1830u,m,s);
    r[3]=r[29];s.lr=0x82bd7ad0u;(void)transform_owner_routes61::Apply(0x82bd1558u,m,deps.owner,s);
    r[11]=m.ReadU32(Address(r[30]));m.WriteU32(Address(r[29]+4u),Address(r[11]));
    r[11]=m.ReadU32(Address(r[30]));r[27]=m.ReadU32(Address(r[11]+8u));Compare(s,r[27],1u);
    if(s.cr6.eq) {r[11]=m.ReadU32(Address(r[29]+8u));r[3]=1u;r[11]|=4u;m.WriteU32(Address(r[29]+8u),Address(r[11]));return;}
    s.lr=0x82bd7b04u;(void)crt_close_recursive_buffer_context::Apply(0x82bd0798u,m,deps.owner.guest,s);
    r[11]=m.ReadU32(Address(r[3]));r[5]=24u;r[4]=28u;r[11]=m.ReadU32(Address(r[11]));s.ctr=r[11];s.lr=0x82bd7b1cu;
    deps.owner.guest.CallIndirect(Address(s.ctr)&~3u,m,s);r[31]=0u;Compare(s,r[3]);
    if(!s.cr6.eq){s.lr=0x82bd7b2cu;(void)grid_transform_buffer61::Apply(0x82bdac60u,m,s);}
    else r[3]=r[31];
    Compare(s,r[3]);m.WriteU32(Address(r[29]+12u),Address(r[3]));if(s.cr6.eq){r[3]=0u;return;}
    r[11]=0xffffffff820d0000ull;r[10]=m.ReadU32(Address(r[30]));m.WriteU32(Address(r[1]+140u),Address(r[31]));
    r[9]=5u;r[11]+=27732u;m.WriteU32(Address(r[1]+108u),Address(r[31]));m.WriteU32(Address(r[1]+144u),Address(r[31]));
    m.WriteU32(Address(r[1]+148u),Address(r[31]));m.WriteU32(Address(r[1]+152u),Address(r[10]));r[10]=r[1]+84u;
    m.WriteU32(Address(r[1]+80u),Address(r[11]));r[11]=r[28];s.ctr=r[9];
    do {r[9]=m.ReadU32(Address(r[11]));r[11]+=4u;m.WriteU32(Address(r[10]),Address(r[9]));r[10]+=4u;--s.ctr;}while(Address(s.ctr)!=0u);
    r[4]=r[1]+80u;m.WriteU32(Address(r[1]+104u),Address(r[27]));s.lr=0x82bd7b94u;
    (void)owned_tree_construct61::Apply(0x82bdad18u,m,{deps.owner.guest,deps.owner.fp},s);
    if(!LowByteSuccess(s)){r[3]=0u;return;}
    r[11]=0xffffffff820d0000ull;r[5]=m.ReadU8(Address(r[30]+25u));r[3]=r[29];r[4]=m.ReadU8(Address(r[30]+24u));
    r[11]+=25268u;m.WriteU32(Address(r[1]+80u),Address(r[11]));s.lr=0x82bd7bbcu;
    (void)transform_owner_initialize61::Apply(0x82bd12f8u,m,deps.owner,s);
    if(!LowByteSuccess(s)){r[3]=0u;return;}
    r[3]=m.ReadU32(Address(r[29]+16u));r[4]=m.ReadU32(Address(r[29]+12u));r[11]=m.ReadU32(Address(r[3]));
    r[11]=m.ReadU32(Address(r[11]+4u));s.ctr=r[11];s.lr=0x82bd7be0u;deps.owner.guest.CallIndirect(Address(s.ctr)&~3u,m,s);
    if(!LowByteSuccess(s)){r[3]=0u;return;}
    r[11]=m.ReadU8(Address(r[30]+26u));Compare(s,r[11]);
    if(s.cr6.eq) {
        r[30]=m.ReadU32(Address(r[29]+12u));Compare(s,r[30]);
        if(!s.cr6.eq) {
            r[3]=r[30];s.lr=0x82bd7c0cu;(void)manager_release_context61::Apply(0x82bdb260u,m,deps.owner,s);
            s.lr=0x82bd7c10u;(void)crt_close_recursive_buffer_context::Apply(0x82bd0798u,m,deps.owner.guest,s);
            r[11]=m.ReadU32(Address(r[3]));r[4]=r[30];r[11]=m.ReadU32(Address(r[11]+12u));
            s.ctr=r[11];s.lr=0x82bd7c24u;deps.owner.guest.CallIndirect(Address(s.ctr)&~3u,m,s);
            m.WriteU32(Address(r[29]+12u),Address(r[31]));
        }
    }
    r[3]=1u;
}
}
bool Apply(GuestAddress entry,GuestMemory& m,Dependencies deps,Registers& s) {
    if(entry!=0x82bd7a60u)return false;
    auto& r=s.r;r[12]=s.lr;s.lr=0x82bd7a68u;
    for(unsigned i=27u;i<=31u;++i)WriteU64(m,Address(r[1]-16u-8u*(31u-i)),r[i]);
    m.WriteU32(Address(r[1]-8u),Address(r[12]));const auto caller_sp=r[1];r[1]-=208u;m.WriteU32(Address(r[1]),Address(caller_sp));
    BuildBody(m,deps,s);r[1]+=208u;
    for(unsigned i=27u;i<=31u;++i)r[i]=ReadU64(m,Address(r[1]-16u-8u*(31u-i)));
    r[12]=m.ReadU32(Address(r[1]-8u));s.lr=r[12];return true;
}
}
