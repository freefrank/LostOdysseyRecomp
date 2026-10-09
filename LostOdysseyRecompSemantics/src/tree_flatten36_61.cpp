#include "lo_semantics/tree_flatten36_61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::tree_flatten36_61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
struct Flatten {
    GuestMemory& memory;
    float_triplet_transfer::NativeServices& native;
    Registers& state;
    std::uint32_t Word(std::uint64_t address){return memory.ReadU32(Address(address));}
    void WordStore(std::uint64_t address,std::uint64_t value){memory.WriteU32(Address(address),Address(value));}
    double Value(unsigned f)const{return std::bit_cast<double>(state.fpr_bits[f]);}
    void Load(unsigned f,std::uint64_t address){state.fpr_bits[f]=std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(Word(address))));}
    void Single(unsigned f,double value){state.fpr_bits[f]=std::bit_cast<std::uint64_t>(double(static_cast<float>(value)));}
    void Store(unsigned f,std::uint64_t address){WordStore(address,std::bit_cast<std::uint32_t>(static_cast<float>(Value(f))));}
    void CompareZero(std::uint64_t value){auto v=Address(value);state.cr6={0,std::uint8_t(v>0),std::uint8_t(v==0),state.xer_so};}
    void Coordinates(){
        auto& r=state.r;
        if(state.cached_fp_control&0x8040u){state.cached_fp_control&=~0x8040u;native.SetHostFpControl(state.cached_fp_control);}
        Load(0,r[31]);r[11]=(r[11]<<2u)&0xfffffffcu;
        Load(13,r[31]+12u);Single(13,Value(13)+Value(0));
        Load(10,r[31]+16u);Load(0,r[31]+20u);r[30]=r[11]+r[28];
        Load(12,r[31]+4u);r[11]=0xffffffff82020000ull;Load(11,r[31]+8u);
        Single(12,Value(10)+Value(12));Single(11,Value(0)+Value(11));Load(0,0x8201f9f0u);
        Single(13,Value(13)*Value(0));Store(13,r[30]);
        Single(12,Value(12)*Value(0));Store(12,r[30]+4u);
        Single(11,Value(11)*Value(0));Store(11,r[30]+8u);
        // Do not cache the original bounds across output stores. This second
        // read phase is visible when source and destination memory overlap.
        Load(12,r[31]);Load(13,r[31]+12u);Single(13,Value(13)-Value(12));
        Load(11,r[31]+16u);Load(12,r[31]+4u);Single(12,Value(11)-Value(12));
        Load(10,r[31]+20u);Load(11,r[31]+8u);Single(11,Value(10)-Value(11));
        Single(13,Value(13)*Value(0));Store(13,r[30]+12u);
        Single(12,Value(12)*Value(0));Store(12,r[30]+16u);
        Single(0,Value(11)*Value(0));Store(0,r[30]+20u);
    }
    void Visit(){
        auto& r=state.r;r[12]=state.lr;state.lr=0x82bdbc20u;
        for(unsigned i=27;i<32;++i)WriteU64(memory,Address(r[1]-16u-8u*(31u-i)),r[i]);
        WordStore(r[1]-8u,r[12]);const auto oldStack=r[1];r[1]-=128u;WordStore(r[1],oldStack);
        r[31]=r[6];r[11]=(r[4]<<3u)&0xfffffff8u;r[28]=r[3];r[11]+=r[4];r[29]=r[5];
        Coordinates();
        r[11]=Word(r[31]+24u);r[4]=Word(r[29]);r[11]&=0xfffffffeu;r[27]=r[4];CompareZero(r[11]);
        if(state.cr6.eq){
            r[11]=Word(r[31]+32u);r[11]=Word(r[11]);r[11]|=0x80000000u;WordStore(r[30]+24u,r[11]);
        }else{
            r[11]=r[4]+1u;r[5]=r[29];r[3]=r[28];WordStore(r[29],r[11]);WordStore(r[30]+24u,r[4]);
            r[11]=Word(r[31]+24u);r[6]=r[11]&0xfffffffeu;state.lr=0x82bdbd10u;Visit();
            r[4]=Word(r[29]);r[11]=r[4]+1u;WordStore(r[29],r[11]);WordStore(r[30]+28u,r[4]);
            r[11]=Word(r[31]+24u);r[11]&=0xfffffffeu;CompareZero(r[11]);r[6]=r[11]+40u;
            if(state.cr6.eq)r[6]=0u;
            r[5]=r[29];r[3]=r[28];state.lr=0x82bdbd44u;Visit();
            r[11]=Word(r[29]);r[11]-=r[27];WordStore(r[30]+32u,r[11]);
        }
        r[1]+=128u;
        for(unsigned i=27;i<32;++i)r[i]=ReadU64(memory,Address(r[1]-16u-8u*(31u-i)));
        r[12]=Word(r[1]-8u);state.lr=r[12];
    }
};
}
bool Apply(GuestAddress entry,GuestMemory& memory,float_triplet_transfer::NativeServices& native,Registers& state){
    if(entry!=0x82bdbc18u)return false;
    Flatten{memory,native,state}.Visit();return true;
}
}
