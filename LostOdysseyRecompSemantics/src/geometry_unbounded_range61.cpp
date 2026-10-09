#include "lo_semantics/geometry_unbounded_range61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::geometry_unbounded_range61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
void Compare(Registers& s,std::uint64_t a,std::uint64_t b){
    s.cr6={std::uint8_t(Address(a)<Address(b)),std::uint8_t(Address(a)>Address(b)),std::uint8_t(Address(a)==Address(b)),s.xer_so};
}
void CompareSigned(Registers& s,std::uint64_t a){
    const auto x=std::bit_cast<std::int32_t>(Address(a));
    s.cr6={std::uint8_t(x<0),std::uint8_t(x>0),std::uint8_t(x==0),s.xer_so};
}
float Lane(const Vector& v,unsigned i){return std::bit_cast<float>(v[i]);}
std::uint32_t Bits(float value){return std::bit_cast<std::uint32_t>(value);}
// The source reads triplets through two partially masked 16-byte windows.
// Retain the otherwise unused fourth lane and zero padding, since volatile
// vector bits are observable. Only ordinary guest RAM behavior is claimed.
Vector TripletHead(GuestMemory& m,std::uint64_t address){
    Vector result{};const auto a=Address(address),n=a&15u,block=a&~15u;
    for(unsigned byte=n;byte<16u;++byte)
        result[byte/4u]|=std::uint32_t(m.ReadU8(block+15u+n-byte))<<(8u*(byte%4u));
    return result;
}
Vector TripletTail(GuestMemory& m,std::uint64_t address){
    Vector result{};const auto a=Address(address),n=a&15u,block=a&~15u;
    for(unsigned byte=0;byte<16u;++byte){const auto source=(byte+12u)%16u;
        if(source<n)result[byte/4u]|=std::uint32_t(m.ReadU8(block+n-1u-source))<<(8u*(byte%4u));}
    return result;
}
void SaveVector(GuestMemory& m,std::uint64_t address,const Vector& v){
    const auto a=Address(address)&~15u;for(unsigned i=0;i<4u;++i)m.WriteU32(a+4u*i,v[3u-i]);
}
Vector RestoreVector(GuestMemory& m,std::uint64_t address){
    Vector result{};const auto a=Address(address)&~15u;for(unsigned i=0;i<4u;++i)result[3u-i]=m.ReadU32(a+4u*i);return result;
}
// Preserve scalar lower reuse while exposing the complete borrowed vector state
// at its dynamic guest-service boundary. The scalar lower itself uses no VMX.
struct LowerServices final:crt_close_recursive_buffer_context::GuestServices{
    geometry_unbounded_range61::GuestServices& guest;VectorState& vectors;
    LowerServices(geometry_unbounded_range61::GuestServices& g,VectorState& v):guest(g),vectors(v){}
    void CallIndirect(GuestAddress target,GuestMemory& memory,Registers& s)override{guest.CallIndirect(target,memory,s,vectors);}
};
void Enter(GuestMemory& m,Registers& s,VectorState& vs){
    auto& r=s.r;auto& v=vs.v;r[12]=s.lr;s.lr=0x82bd6e30u;
    for(unsigned n=26;n<32u;++n)WriteU64(m,Address(r[1]-16u-8u*(31u-n)),r[n]);
    m.WriteU32(Address(r[1]-8u),Address(r[12]));
    r[12]=std::uint64_t(-112ll);SaveVector(m,r[1]+r[12],v[125]);
    r[12]=std::uint64_t(-96ll);SaveVector(m,r[1]+r[12],v[126]);
    r[12]=std::uint64_t(-80ll);SaveVector(m,r[1]+r[12],v[127]);
    const auto caller=r[1];r[1]-=208u;m.WriteU32(Address(r[1]),Address(caller));
}
void Leave(GuestMemory& m,Registers& s,VectorState& vs){
    auto& r=s.r;auto& v=vs.v;r[1]+=208u;
    r[0]=std::uint64_t(-112ll);v[125]=RestoreVector(m,r[1]+r[0]);
    r[0]=std::uint64_t(-96ll);v[126]=RestoreVector(m,r[1]+r[0]);
    r[0]=std::uint64_t(-80ll);v[127]=RestoreVector(m,r[1]+r[0]);
    for(unsigned n=26;n<32u;++n)r[n]=ReadU64(m,Address(r[1]-16u-8u*(31u-n)));
    r[12]=m.ReadU32(Address(r[1]-8u));s.lr=r[12];
}
// SAT uses origin-center, ray direction, and box extents. The permutation
// forms the YZ/ZX/XY cross-product tests; the fourth lane is retained exactly.
void BoxTest(GuestMemory& m,Dependencies deps,Registers& s,VectorState& vs){
    auto& r=s.r;auto& v=vs.v;
    r[9]=r[30]+16u;r[11]=r[30]+28u;r[8]=r[30]+40u;r[10]=r[31]+12u;
    v[0]=TripletTail(m,r[31]+r[29]);v[9]=TripletHead(m,r[31]);
    v[11]=TripletTail(m,r[9]+r[29]);for(unsigned i=0;i<4u;++i)v[0][i]|=v[9][i];
    v[6]=TripletHead(m,r[9]);v[13]=TripletTail(m,r[11]+r[29]);
    v[10]=TripletTail(m,r[8]+r[29]);v[8]=TripletHead(m,r[11]);
    for(unsigned i=0;i<4u;++i)v[9][i]=v[6][i]|v[11][i];
    v[12]=TripletTail(m,r[10]+r[29]);v[5]=TripletHead(m,r[8]);
    for(unsigned i=0;i<4u;++i)v[13][i]|=v[8][i];
    v[7]=TripletHead(m,r[10]);for(unsigned i=0;i<4u;++i)v[11][i]=v[5][i]|v[10][i];
    r[11]=m.ReadU32(Address(r[30]+96u));
    if((s.cached_fp_control&0x8040u)!=0x8040u){s.cached_fp_control|=0x8040u;deps.fp.SetHostFpControl(s.cached_fp_control);}
    for(unsigned i=0;i<4u;++i)v[0][i]=Bits(Lane(v[9],i)-Lane(v[0],i));
    for(unsigned i=0;i<4u;++i)v[6][i]=v[127][i]<<(v[127][i]&31u);
    for(unsigned i=0;i<4u;++i)v[12][i]|=v[7][i];
    ++r[11];
    v[10]={v[13][0],v[13][3],v[13][1],v[13][2]};
    for(unsigned i=0;i<4u;++i)v[5][i]=v[127][i]<<(v[127][i]&31u);
    v[7]={v[11][0],v[11][1],v[11][1],v[11][2]};
    v[11]={v[11][0],v[11][3],v[11][2],v[11][3]};
    v[8]={v[12][0],v[12][1],v[12][1],v[12][2]};
    v[9]={v[12][0],v[12][3],v[12][2],v[12][3]};
    m.WriteU32(Address(r[30]+96u),Address(r[11]));
    for(unsigned i=0;i<4u;++i)v[11][i]=Bits(Lane(v[8],i)*Lane(v[11],i));
    for(unsigned i=0;i<4u;++i)v[9][i]=Bits(Lane(v[9],i)*Lane(v[7],i));
    v[4]={v[0][0],v[0][3],v[0][1],v[0][2]};
    for(unsigned i=0;i<4u;++i)v[3][i]=Bits(Lane(v[0],i)*Lane(v[13],i));
    for(unsigned i=0;i<4u;++i)v[10][i]=Bits(Lane(v[10],i)*Lane(v[0],i));
    for(unsigned i=0;i<4u;++i)v[0][i]&=~v[6][i];
    for(unsigned i=0;i<4u;++i)v[13][i]=Bits(Lane(v[13],i)*Lane(v[4],i));
    for(unsigned i=0;i<4u;++i)v[0][i]=Lane(v[0],i)>Lane(v[12],i)?0xffffffffu:0u;
    for(unsigned i=0;i<4u;++i)v[12][i]=Bits(Lane(v[9],i)+Lane(v[11],i));
    for(unsigned i=0;i<4u;++i)v[8][i]=Lane(v[3],i)>=Lane(v[126],i)?0xffffffffu:0u;
    for(unsigned i=0;i<4u;++i)v[13][i]=Bits(Lane(v[13],i)-Lane(v[10],i));
    for(unsigned i=0;i<4u;++i)v[0][i]&=v[8][i];
    for(unsigned i=0;i<4u;++i)v[13][i]&=~v[5][i];
    for(unsigned i=0;i<4u;++i)v[13][i]=Lane(v[13],i)>Lane(v[12],i)?0xffffffffu:0u;
    for(unsigned i=0;i<4u;++i)v[0][i]|=v[13][i];
    v[0]={v[0][3],v[0][1],v[0][2],v[0][3]};
    unsigned equal=0u;for(unsigned i=0;i<4u;++i){v[0][i]=v[0][i]==v[125][i]?0xffffffffu:0u;if(v[0][i])++equal;}
    s.cr6={std::uint8_t(equal==4u),0u,std::uint8_t(equal==0u),0u};
}
void Traverse(GuestMemory& m,Dependencies deps,Registers& s,VectorState& vs){
    Enter(m,s,vs);auto& r=s.r;auto& v=vs.v;r[31]=r[4];r[26]=r[5];r[30]=r[3];
    Compare(s,r[31],r[26]);
    if(s.cr6.lt){v[126].fill(0u);r[29]=12u;v[127].fill(0xffffffffu);
        v[125]={v[126][3],v[126][1],v[126][2],v[126][3]};
        LowerServices services(deps.guest,vs);
        do{
            BoxTest(m,deps,s,vs);
            r[10]=(s.cr6.lt<<7u)|(s.cr6.gt<<6u)|(s.cr6.eq<<5u)|(s.cr6.so<<4u);
            r[11]=m.ReadU32(Address(r[31]+24u));r[27]=(Address(r[10])>>7u)&1u;r[28]=Address(r[11])&0x80000000u;
            CompareSigned(s,r[28]);
            if(!s.cr6.eq){CompareSigned(s,r[27]);if(!s.cr6.eq){
                r[4]=Address(r[11])&0x3fffffffu;r[3]=r[30];s.lr=0x82bd6f6cu;
                (void)geometry_math61::Apply(0x82bd4448u,m,{services,deps.fp},s);
                r[11]=m.ReadU32(Address(r[30]+4u));r[11]&=5u;
                s.cr0={0u,std::uint8_t(r[11]>0u),std::uint8_t(r[11]==0u),s.xer_so};
                Compare(s,r[11],5u);if(s.cr6.eq)break;
            }}
            CompareSigned(s,r[27]);
            if(s.cr6.eq){CompareSigned(s,r[28]);if(s.cr6.eq){
                r[11]=m.ReadU32(Address(r[31]+32u));r[10]=Address(r[11])<<3u;
                r[11]+=r[10];r[11]=Address(r[11])<<2u;r[31]+=r[11];
            }}
            r[31]+=36u;Compare(s,r[31],r[26]);
        }while(s.cr6.lt);
    }
    Leave(m,s,vs);
}
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies deps,Registers& s,VectorState& vectors){
    if(entry!=0x82bd6e28u)return false;
    Traverse(memory,deps,s,vectors);return true;
}
}
