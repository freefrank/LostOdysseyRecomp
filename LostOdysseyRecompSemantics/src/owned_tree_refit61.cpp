#include "lo_semantics/owned_tree_refit61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::owned_tree_refit61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
void Compare(Registers& s,std::uint64_t a,std::uint64_t b) {
    const auto x=Address(a),y=Address(b);s.cr6={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),s.xer_so};
}
std::uint64_t Shift(std::uint64_t value,unsigned amount) {return (std::uint64_t(Address(value))<<amount)&std::uint64_t(std::uint32_t(0xffffffffu<<amount));}
struct Bounds {
    GuestMemory& m;NativeServices& native;Registers& s;
    void ScalarMode(){if(s.cached_fp_control&0x8040u){s.cached_fp_control&=~0x8040u;native.SetHostFpControl(s.cached_fp_control);}}
    double Get(unsigned n)const{return std::bit_cast<double>(s.fpr_bits[n]);}
    void Load(unsigned n,std::uint64_t address){ScalarMode();s.fpr_bits[n]=std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(m.ReadU32(Address(address)))));}
    void Store(unsigned n,std::uint64_t address){ScalarMode();m.WriteU32(Address(address),std::bit_cast<std::uint32_t>(float(Get(n))));}
    void CompareFloat(unsigned a,unsigned b){const auto x=Get(a),y=Get(b);s.cr6={std::uint8_t(x<y),std::uint8_t(x>y),std::uint8_t(x==y),std::uint8_t(std::isnan(x)||std::isnan(y))};}
    void Copy(unsigned to,unsigned from){s.fpr_bits[to]=s.fpr_bits[from];}
};
void RefitNode(unsigned box_base,GuestMemory& m,NativeServices& native,Registers& s) {
    auto& r=s.r;Bounds f{m,native,s};
    r[10]=m.ReadU32(Address(r[11]+24u));r[9]=Address(r[10])&0xfffffffeu;
    Compare(s,r[9],0u);
    if(!s.cr6.eq){
        r[9]=Address(r[10])&0xfffffffeu;Compare(s,r[9],0u);r[10]=r[9]+40u;if(s.cr6.eq)r[10]=0u;
        // Comparisons deliberately select the second child on ties/unordered.
        for(unsigned axis=0;axis<6u;++axis){
            f.Load(0,r[9]+axis*4u);f.Load(13,r[10]+axis*4u);f.CompareFloat(0,13);
            if(!(axis<3u?s.cr6.lt:s.cr6.gt))f.Copy(0,13);
            f.Store(0,r[11]+axis*4u);
        }
        return;
    }
    r[8]=m.ReadU32(Address(r[11]+36u));r[9]=m.ReadU32(Address(r[11]+32u));Compare(s,r[8],0u);
    if(s.cr6.eq){for(unsigned axis=0;axis<6u;++axis)f.Store(axis<3u?7u:6u,r[11]+axis*4u);return;}
    r[10]=m.ReadU32(Address(r[9]));Compare(s,r[8],1u);r[7]=Shift(r[10],1u);r[10]+=r[7];r[10]=Shift(r[10],3u);r[10]+=r[box_base];
    for(unsigned axis=0;axis<6u;++axis){f.Load(0,r[10]+axis*4u);f.Store(0,r[11]+axis*4u);}
    if(!s.cr6.gt)return;
    r[10]=Shift(r[8],2u);f.Load(8,r[11]);f.Load(9,r[11]+4u);f.Copy(13,0);r[7]=r[10]+r[9];
    f.Load(10,r[11]+8u);f.Load(11,r[11]+12u);f.Load(12,r[11]+16u);r[10]=r[9]+4u;Compare(s,r[10],r[7]);
    if(!s.cr6.eq)do{
        r[9]=m.ReadU32(Address(r[10]));r[10]+=4u;r[8]=Shift(r[9],1u);r[9]+=r[8];r[9]=Shift(r[9],3u);r[9]+=r[box_base];
        for(unsigned axis=0;axis<6u;++axis){f.Load(0,r[9]+axis*4u);f.CompareFloat(0,8u+axis);if(axis<3u?s.cr6.lt:s.cr6.gt)f.Copy(8u+axis,0);}
        Compare(s,r[10],r[7]);
    }while(!s.cr6.eq);
    for(unsigned axis=0;axis<6u;++axis)f.Store(8u+axis,r[11]+axis*4u);
}
void Constants(GuestMemory& m,NativeServices& native,Registers& s) {
    s.r[10]=0xffffffff82000000ull;s.r[11]=0xffffffff82000000ull;
    Bounds f{m,native,s};f.Load(6,s.r[10]+3428u);f.Load(7,s.r[11]+3596u);
}
void AllNodes(GuestMemory& m,NativeServices& native,Registers& s) {
    auto& r=s.r;Compare(s,r[3],0u);if(s.cr6.eq){r[3]=0u;return;}
    r[5]=m.ReadU32(Address(r[3]+16u));r[6]=m.ReadU32(Address(r[4]+72u));Compare(s,r[5],0u);
    r[11]=Shift(r[5],2u);r[11]+=r[5];r[4]=Shift(r[11],3u);Constants(m,native,s);
    if(!s.cr6.eq)do{
        r[11]=m.ReadU32(Address(r[3]+4u));r[4]-=40u;--r[5];r[11]+=r[4];Compare(s,r[5],0u);
        r[10]=r[11]-40u; // Ordinary-memory prefetch has no visible RAM effect.
        RefitNode(6u,m,native,s);Compare(s,r[5],0u);
    }while(!s.cr6.eq);
    r[3]=1u;
}
void CompareSignedZero(Registers& s,std::uint64_t word) {
    const auto value=std::bit_cast<std::int32_t>(Address(word));
    s.cr6={std::uint8_t(value<0),std::uint8_t(value>0),std::uint8_t(value==0),s.xer_so};
}
void DirtyNodes(GuestMemory& m,NativeServices& native,Registers& s) {
    auto& r=s.r;r[12]=s.lr;s.lr=0x82bda5f8u;
    for(unsigned n=26u;n<32u;++n)WriteU64(m,Address(r[1]-8u*(33u-n)),r[n]);
    m.WriteU32(Address(r[1]-8u),Address(r[12]));
    r[9]=m.ReadU32(Address(r[3]+8u));CompareSignedZero(s,r[9]);
    if(!s.cr6.eq){
        r[29]=m.ReadU32(Address(r[3]+12u));Compare(s,r[29],0u);
        if(!s.cr6.eq){
            r[10]=Shift(r[29],2u);r[11]=r[29]+1u;r[26]=r[10]+r[9];
            r[10]=Shift(r[11],2u);r[8]=r[29]+1u;r[11]+=r[10];
            r[10]=0xffffffff82000000ull;r[27]=Shift(r[11],8u);r[11]=0xffffffff82000000ull;
            r[28]=Shift(r[8],5u);r[30]=1u;Bounds f{m,native,s};f.Load(6,r[10]+3428u);f.Load(7,r[11]+3596u);
            do{
                r[26]-=4u;--r[29];r[28]-=32u;r[27]-=1280u;
                r[11]=m.ReadU32(Address(r[26]));Compare(s,r[11],0u);
                if(!s.cr6.eq){
                    r[4]=r[28];r[31]=32u;r[6]=r[27];
                    do{
                        --r[4];r[10]=m.ReadU32(Address(r[3]+8u));r[6]-=40u;
                        r[9]=Address(r[4])&31u;r[11]=(Address(r[4])>>3u)&0x1ffffffcu;--r[31];
                        r[10]=m.ReadU32(Address(r[11]+r[10]));r[9]=std::uint64_t(Address(r[30])<<unsigned(r[9]));r[10]&=r[9];CompareSignedZero(s,r[10]);
                        if(!s.cr6.eq){
                            r[10]=m.ReadU32(Address(r[3]+8u));r[8]=m.ReadU32(Address(r[10]+r[11]));r[9]=r[8]&~r[9];
                            m.WriteU32(Address(r[10]+r[11]),Address(r[9]));r[11]=m.ReadU32(Address(r[3]+4u));r[11]+=r[6];
                            RefitNode(5u,m,native,s);
                        }
                        Compare(s,r[31],0u);
                    }while(!s.cr6.eq);
                }
                Compare(s,r[29],0u);
            }while(!s.cr6.eq);
        }
    }
    for(unsigned n=26u;n<32u;++n)r[n]=ReadU64(m,Address(r[1]-8u*(33u-n)));
    r[12]=m.ReadU32(Address(r[1]-8u));s.lr=r[12];
}

}
bool Apply(GuestAddress entry,GuestMemory& m,NativeServices& native,Registers& s) {
    switch(entry){
    case 0x82bda248u:AllNodes(m,native,s);return true;
    case 0x82bda5f0u:DirtyNodes(m,native,s);return true;
    default:return false;
    }
}
}
