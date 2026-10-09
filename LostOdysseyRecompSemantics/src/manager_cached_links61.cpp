#include "lo_semantics/manager_cached_links61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::manager_cached_links61 {
namespace {
using recovery_abi::Address;using recovery_abi::ReadU64;using recovery_abi::WriteU64;
void SignedCompare(Registers& s,std::uint64_t a,std::uint64_t b=0u){
    const auto x=std::bit_cast<std::int32_t>(Address(a)),y=std::bit_cast<std::int32_t>(Address(b));
    s.cr6={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),s.xer_so};
}
void ZeroCompare(Registers& s,std::uint64_t value){const auto x=Address(value);s.cr6={0u,std::uint8_t(x!=0u),std::uint8_t(x==0u),s.xer_so};}
void ClearCachedTargets(GuestMemory& m,Registers& s){
    auto& r=s.r;r[3]=r[5];SignedCompare(s,r[10]);if(!s.cr6.gt)return;
    r[4]=r[5];r[6]=~std::uint64_t(0u);
    do {
        r[11]=m.ReadU32(Address(r[28]));r[8]=r[5];r[9]=m.ReadU32(Address(r[4]+r[11]));
        r[11]=m.ReadU32(Address(r[9]+188u));SignedCompare(s,r[11]);
        if(s.cr6.gt){
            r[10]=r[5];
            do {
                r[11]=m.ReadU32(Address(r[9]+184u));r[11]+=r[10];r[7]=m.ReadU32(Address(r[11]+28u));ZeroCompare(s,r[7]);
                if(!s.cr6.eq){
                    r[12]=1u;r[7]=ReadU64(m,Address(r[7]+8u));r[12]<<=58u;r[7]&=r[12];
                    s.cr6={0u,std::uint8_t(r[7]!=0u),std::uint8_t(r[7]==0u),s.xer_so};
                    if(s.cr6.eq)m.WriteU32(Address(r[11]+28u),Address(r[5]));
                }
                m.WriteU32(Address(r[11]+32u),Address(r[5]));++r[8];m.WriteU32(Address(r[11]+36u),Address(r[6]));r[10]+=40u;
                r[11]=m.ReadU32(Address(r[9]+188u));SignedCompare(s,r[8],r[11]);
            }while(s.cr6.lt);
            r[10]=m.ReadU32(Address(r[28]+4u));
        }
        ++r[3];r[4]+=4u;SignedCompare(s,r[3],r[10]);
    }while(s.cr6.lt);
}
void DetachOwnedLinks(GuestMemory& m,Registers& s){
    auto& r=s.r;r[30]=r[5];SignedCompare(s,r[10]);if(!s.cr6.gt)return;
    r[31]=r[5];r[3]=~std::uint64_t(0u);
    do {
        r[11]=m.ReadU32(Address(r[28]));r[4]=r[5];r[6]=m.ReadU32(Address(r[31]+r[11]));
        r[11]=m.ReadU32(Address(r[6]+200u));SignedCompare(s,r[11]);
        if(s.cr6.gt){
            r[7]=r[5];
            do {
                r[11]=m.ReadU32(Address(r[6]+196u));r[8]=r[11]+r[7];r[11]=m.ReadU32(Address(r[8]+48u));ZeroCompare(s,r[11]);
                if(!s.cr6.eq){
                    r[10]=m.ReadU32(Address(r[8]+76u));r[10]&=1u;ZeroCompare(s,r[10]);
                    if(!s.cr6.eq){
                        r[10]=m.ReadU32(Address(r[11]+28u));ZeroCompare(s,r[10]);
                        if(!s.cr6.eq){
                            r[27]=m.ReadU32(Address(r[11]+32u));r[9]=m.ReadU32(Address(r[10]+196u));
                            r[10]=r[27]*108u;r[10]+=r[9];m.WriteU32(Address(r[10]+48u),Address(r[5]));
                        }
                        m.WriteU32(Address(r[11]+28u),Address(r[5]));m.WriteU32(Address(r[11]+32u),Address(r[3]));
                        m.WriteU32(Address(r[8]+48u),Address(r[5]));
                    }
                }
                r[11]=m.ReadU32(Address(r[6]+200u));++r[4];r[7]+=108u;SignedCompare(s,r[4],r[11]);
            }while(s.cr6.lt);
            r[10]=m.ReadU32(Address(r[28]+4u));
        }
        ++r[30];r[31]+=4u;SignedCompare(s,r[30],r[10]);
    }while(s.cr6.lt);
}
}
bool Apply(GuestAddress entry,GuestMemory& m,Registers& s){
    if(entry!=0x824002f0u)return false;
    auto& r=s.r;r[12]=s.lr;s.lr=0x824002f8u;
    for(unsigned i=27u;i<=31u;++i)WriteU64(m,Address(r[1]-16u-8u*(31u-i)),r[i]);
    m.WriteU32(Address(r[1]-8u),Address(r[12]));r[31]=0xffffffff83310000ull;r[5]=0u;
    r[11]=m.ReadU32(Address(r[31]+24296u));SignedCompare(s,r[11]);r[11]=0xffffffff83370000ull;r[28]=r[11]-28404u;
    r[10]=m.ReadU32(Address(r[28]+4u));if(!s.cr6.eq)ClearCachedTargets(m,s);
    r[11]=r[5];r[29]=0xffffffff83310000ull;m.WriteU32(Address(r[31]+24296u),Address(r[11]));
    r[11]=m.ReadU32(Address(r[29]+24300u));SignedCompare(s,r[11]);if(!s.cr6.eq)DetachOwnedLinks(m,s);
    r[11]=r[5];m.WriteU32(Address(r[29]+24300u),Address(r[11]));
    for(unsigned i=27u;i<=31u;++i)r[i]=ReadU64(m,Address(r[1]-16u-8u*(31u-i)));
    r[12]=m.ReadU32(Address(r[1]-8u));s.lr=r[12];return true;
}
}
