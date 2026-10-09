#include "lo_semantics/owned_tree_plane_query61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::owned_tree_plane_query61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
void Compare(Registers& s,std::uint64_t a,std::uint64_t b){const auto x=Address(a),y=Address(b);s.cr6={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),s.xer_so};}
struct Math {
    GuestMemory& m;Dependencies d;Registers& s;
    void ScalarMode(){if(s.cached_fp_control&0x8040u){s.cached_fp_control&=~0x8040u;d.fp.SetHostFpControl(s.cached_fp_control);}}
    double Get(unsigned n)const{return std::bit_cast<double>(s.fpr_bits[n]);}
    void Single(unsigned n,double value){ScalarMode();s.fpr_bits[n]=std::bit_cast<std::uint64_t>(double(float(value)));}
    void Load(unsigned n,std::uint64_t address){ScalarMode();s.fpr_bits[n]=std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(m.ReadU32(Address(address)))));}
    void Copy(unsigned to,unsigned from){s.fpr_bits[to]=s.fpr_bits[from];}
    void Abs(unsigned to,unsigned from){s.fpr_bits[to]=s.fpr_bits[from]&0x7fffffffffffffffull;}
    void FloatCompare(unsigned a,unsigned b){const auto x=Get(a),y=Get(b);s.cr6={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),std::uint8_t(std::isnan(x)||std::isnan(y))};}
};
void Enter(GuestMemory& m,Dependencies d,Registers& s){
    auto& r=s.r;r[12]=s.lr;s.lr=0x82bda8c0u;
    for(unsigned n=27u;n<32u;++n)WriteU64(m,Address(r[1]-8u*(33u-n)),r[n]);
    m.WriteU32(Address(r[1]-8u),Address(r[12]));Math f{m,d,s};f.ScalarMode();WriteU64(m,Address(r[1]-56u),s.fpr_bits[31]);
    const auto caller=r[1];r[1]-=144u;m.WriteU32(Address(r[1]),Address(caller));r[11]=0xffffffff82020000ull;
    r[30]=m.ReadU32(Address(r[1]+80u));r[31]=r[3];r[29]=r[4];r[28]=r[6];r[27]=r[7];f.Load(31,r[11]-1552u);
}
void Leave(GuestMemory& m,Dependencies d,Registers& s){
    auto& r=s.r;r[1]+=144u;Math{m,d,s}.ScalarMode();s.fpr_bits[31]=ReadU64(m,Address(r[1]-56u));
    for(unsigned n=27u;n<32u;++n)r[n]=ReadU64(m,Address(r[1]-8u*(33u-n)));
    r[12]=m.ReadU32(Address(r[1]-8u));s.lr=r[12];
}
bool Classify(GuestMemory& m,Dependencies d,Registers& s){
    auto& r=s.r;Math f{m,d,s};Compare(s,r[5],1u);r[11]=1u;r[9]=0u;
    f.Load(11,r[31]+12u);f.Load(0,r[31]);f.Single(0,f.Get(11)+f.Get(0));
    f.Load(10,r[31]+16u);f.Load(11,r[31]+20u);f.Load(13,r[31]+4u);f.Copy(7,10);f.Copy(6,11);f.Load(12,r[31]+8u);
    f.Single(13,f.Get(10)+f.Get(13));f.Single(12,f.Get(11)+f.Get(12));
    f.Single(11,f.Get(0)*f.Get(31));f.Single(10,f.Get(13)*f.Get(31));f.Single(9,f.Get(12)*f.Get(31));
    f.Load(8,r[31]+12u);f.Single(8,f.Get(8)-f.Get(11));f.Single(7,f.Get(7)-f.Get(10));f.Single(6,f.Get(6)-f.Get(9));
    if(!s.cr6.lt){
        r[10]=r[29]+4u;
        do{
            r[8]=r[11]&r[5];Compare(s,r[8],0u);
            if(!s.cr6.eq){
                f.Load(0,r[10]+4u);f.Load(13,r[10]-4u);f.Load(12,r[10]);f.Load(5,r[10]+8u);
                // Preserve the original single-rounding stages and Z/X/Y
                // accumulation order for distance and projection radius.
                f.Single(4,f.Get(0)*f.Get(9));f.Abs(0,0);f.Abs(3,13);
                f.Single(13,f.Get(13)*f.Get(11)+f.Get(4));f.Single(0,f.Get(0)*f.Get(6));f.Abs(4,12);
                f.Single(13,f.Get(12)*f.Get(10)+f.Get(13));f.Single(12,f.Get(3)*f.Get(8)+f.Get(0));
                f.Single(0,f.Get(13)+f.Get(5));f.Single(13,f.Get(4)*f.Get(7)+f.Get(12));
                f.FloatCompare(13,0);if(s.cr6.lt){r[11]=0u;return false;}
                s.fpr_bits[13]^=0x8000000000000000ull;f.FloatCompare(13,0);if(s.cr6.lt)r[9]|=r[11];
            }
            r[11]=(std::uint64_t(Address(r[11]))<<1u)&0xfffffffeu;r[10]+=16u;Compare(s,r[11],r[5]);
        }while(!s.cr6.gt);
    }
    r[30]=r[9];r[11]=1u;return true;
}
void Query(GuestMemory& m,Dependencies d,Registers& s){
    Enter(m,d,s);auto& r=s.r;
    for(;;){
        (void)Classify(m,d,s);r[11]=Address(r[11])&0xffu;Compare(s,r[11],0u);if(s.cr6.eq)break;
        Compare(s,r[30],0u);
        if(s.cr6.eq)r[5]=0u;
        else{
            r[11]=m.ReadU32(Address(r[31]+24u));r[10]=Address(r[11])&0xfffffffeu;Compare(s,r[10],0u);
            if(!s.cr6.eq){
                r[7]=r[27];r[6]=r[28];r[5]=r[30];r[4]=r[29];r[3]=Address(r[11])&0xfffffffeu;s.lr=0x82bda9f0u;
                Query(m,d,s);
                r[11]=m.ReadU32(Address(r[31]+24u));r[11]=Address(r[11])&0xfffffffeu;Compare(s,r[11],0u);
                r[31]=r[11]+40u;if(s.cr6.eq)r[31]=0u;r[5]=r[30];continue;
            }
            r[5]=1u;
        }
        r[3]=m.ReadU32(Address(r[31]+36u));r[6]=r[27];r[4]=m.ReadU32(Address(r[31]+32u));s.ctr=r[28];s.lr=0x82bdaa38u;
        d.guest.CallIndirect(Address(s.ctr)&~3u,m,s);break;
    }
    Leave(m,d,s);
}
}
bool Apply(GuestAddress entry,GuestMemory& m,Dependencies d,Registers& s){if(entry!=0x82bda8b8u)return false;Query(m,d,s);return true;}
}
