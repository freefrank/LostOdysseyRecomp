#include "lo_semantics/owned_tree_construct61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::owned_tree_construct61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
void Compare(Registers& s,std::uint64_t a,std::uint64_t b=0u) {
    const auto x=Address(a),y=Address(b);s.cr6={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),s.xer_so};
}
void CompareSignedZero(Registers& s,std::uint64_t value) {
    const auto x=std::bit_cast<std::int32_t>(Address(value));
    s.cr6={std::uint8_t(x<0),std::uint8_t(x>0),std::uint8_t(x==0),s.xer_so};
}
void Construct(GuestMemory& m,Dependencies deps,Registers& s) {
    auto& r=s.r;r[12]=s.lr;s.lr=0x82bdad20u;
    for(unsigned i=26u;i<=31u;++i)WriteU64(m,Address(r[1]-16u-8u*(31u-i)),r[i]);
    m.WriteU32(Address(r[1]-8u),Address(r[12]));const auto caller_sp=r[1];r[1]-=144u;
    m.WriteU32(Address(r[1]),Address(caller_sp));r[31]=r[4];r[30]=r[3];Compare(s,r[31]);
    bool success=false;
    if(!s.cr6.eq) {
        r[11]=m.ReadU32(Address(r[31]+24u));Compare(s,r[11]);
        if(!s.cr6.eq) {
            s.lr=0x82bdad44u;(void)owned_tree_cleanup61::Apply(0x82bdac88u,m,{deps.guest,deps.fp},s);
            r[26]=1u;r[27]=0u;m.WriteU32(Address(r[31]+64u),Address(r[26]));m.WriteU32(Address(r[31]+68u),Address(r[27]));
            s.lr=0x82bdad58u;(void)crt_close_recursive_buffer_context::Apply(0x82bd0798u,m,deps.guest,s);
            r[11]=m.ReadU32(Address(r[31]+24u));r[5]=61u;r[4]=(r[11]<<2u)&0xfffffffcu;
            r[11]=m.ReadU32(Address(r[3]));r[11]=m.ReadU32(Address(r[11]));s.ctr=r[11];s.lr=0x82bdad74u;
            deps.guest.CallIndirect(Address(s.ctr)&~3u,m,s);Compare(s,r[3]);m.WriteU32(Address(r[30]),Address(r[3]));
            if(!s.cr6.eq) {
                r[10]=m.ReadU32(Address(r[31]+24u));r[11]=r[27];Compare(s,r[10]);
                if(s.cr6.gt) {
                    r[10]=r[27];
                    do {
                        r[8]=m.ReadU32(Address(r[30]));r[9]=r[11];++r[11];
                        m.WriteU32(Address(r[10]+r[8]),Address(r[9]));r[10]+=4u;
                        r[9]=m.ReadU32(Address(r[31]+24u));Compare(s,r[11],r[9]);
                    } while(s.cr6.lt);
                }
                r[11]=m.ReadU32(Address(r[31]+24u));r[10]=107347968u;r[11]=(r[11]<<1u)&0xfffffffeu;
                r[10]|=26214u;r[29]=r[11]-1u;Compare(s,r[29],r[10]);
                if(s.cr6.gt)r[28]=~std::uint64_t(0u);
                else {
                    r[11]=(r[29]<<2u)&0xfffffffcu;r[10]=std::uint64_t(-5ll);r[11]+=r[29];
                    r[11]=(r[11]<<3u)&0xfffffff8u;Compare(s,r[11],r[10]);r[28]=r[11]+4u;
                    if(s.cr6.gt)r[28]=~std::uint64_t(0u);
                }
                s.lr=0x82bdadf4u;(void)crt_close_recursive_buffer_context::Apply(0x82bd0798u,m,deps.guest,s);
                r[11]=m.ReadU32(Address(r[3]));r[5]=26u;r[4]=r[28];r[11]=m.ReadU32(Address(r[11]));
                s.ctr=r[11];s.lr=0x82bdae0cu;deps.guest.CallIndirect(Address(s.ctr)&~3u,m,s);Compare(s,r[3]);
                if(!s.cr6.eq) {
                    r[10]=r[29]-1u;m.WriteU32(Address(r[3]),Address(r[29]));r[9]=r[3]+4u;CompareSignedZero(s,r[10]);
                    if(!s.cr6.lt) {
                        r[11]=r[9]+28u;
                        do {
                            --r[10];m.WriteU32(Address(r[11]-4u),Address(r[27]));m.WriteU32(Address(r[11]),Address(r[27]));
                            m.WriteU32(Address(r[11]+4u),Address(r[27]));CompareSignedZero(s,r[10]);
                            m.WriteU32(Address(r[11]+8u),Address(r[27]));r[11]+=40u;
                        } while(!s.cr6.lt);
                    }
                    r[11]=r[9];
                } else r[11]=r[27];
                r[9]=0xffffffff832e0000ull;m.WriteU32(Address(r[30]+4u),Address(r[11]));r[10]=r[27];
                m.WriteU32(Address(r[31]+28u),Address(r[11]));r[11]=m.ReadU32(Address(r[30]+4u));r[4]=r[31];
                m.WriteU32(Address(r[9]-2728u),Address(r[10]));r[10]=m.ReadU32(Address(r[30]));m.WriteU32(Address(r[11]+32u),Address(r[10]));
                r[11]=m.ReadU32(Address(r[30]+4u));r[10]=m.ReadU32(Address(r[31]+24u));m.WriteU32(Address(r[11]+36u),Address(r[10]));
                m.WriteU8(Address(r[31]+56u),static_cast<std::uint8_t>(r[26]));r[3]=m.ReadU32(Address(r[30]+4u));
                s.lr=0x82bdae94u;(void)owned_tree_expand61::Apply(0x82bdaa48u,m,deps,s);
                r[11]=m.ReadU32(Address(r[31]+64u));r[3]=1u;m.WriteU32(Address(r[30]+16u),Address(r[11]));
                r[11]=m.ReadU32(Address(r[31]+60u));m.WriteU32(Address(r[30]+20u),Address(r[11]));success=true;
            }
        }
    }
    if(!success)r[3]=0u;
    r[1]+=144u;
    for(unsigned i=26u;i<=31u;++i)r[i]=ReadU64(m,Address(r[1]-16u-8u*(31u-i)));
    r[12]=m.ReadU32(Address(r[1]-8u));s.lr=r[12];
}
}
bool Apply(GuestAddress entry,GuestMemory& m,Dependencies deps,Registers& s) {
    if(entry!=0x82bdad18u)return false;
    Construct(m,deps,s);return true;
}
}
