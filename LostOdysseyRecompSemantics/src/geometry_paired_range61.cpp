#include "lo_semantics/geometry_paired_range61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::geometry_paired_range61 {
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
Vector TripletTail(GuestMemory& m,std::uint64_t address,unsigned rotation=4u){
    Vector result{};const auto a=Address(address),n=a&15u,block=a&~15u;
    for(unsigned byte=0;byte<16u;++byte){const auto source=(byte+16u-rotation)%16u;
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
void VectorFrame(GuestMemory& m,Registers& s,VectorState& vs,bool restore) {
    // Verified __savevmx_119 / __restvmx_119: r12 base, r11=-144..-16.
    // Both helpers leave r11=-16 and return through their incoming LR.
    for(unsigned n=119;n<128u;++n){
        s.r[11]=std::uint64_t(-16ll*std::int64_t(128u-n));
        if(restore)vs.v[n]=RestoreVector(m,s.r[12]+s.r[11]);
        else SaveVector(m,s.r[12]+s.r[11],vs.v[n]);
    }
}
void Enter(GuestMemory& m,Registers& s,VectorState& vs) {
    auto& r=s.r;r[12]=s.lr;s.lr=0x82bd5918u;
    for(unsigned n=26;n<32u;++n)WriteU64(m,Address(r[1]-16u-8u*(31u-n)),r[n]);
    m.WriteU32(Address(r[1]-8u),Address(r[12]));
    r[12]=r[1]-64u;s.lr=0x82bd5920u;VectorFrame(m,s,vs,false);
    const auto caller=r[1];r[1]-=320u;m.WriteU32(Address(r[1]),Address(caller));
}
void Leave(GuestMemory& m,Registers& s,VectorState& vs) {
    auto& r=s.r;r[1]+=320u;r[12]=r[1]-64u;s.lr=0x82bd5ad4u;VectorFrame(m,s,vs,true);
    for(unsigned n=26;n<32u;++n)r[n]=ReadU64(m,Address(r[1]-16u-8u*(31u-n)));
    r[12]=m.ReadU32(Address(r[1]-8u));s.lr=r[12];
}
void Prepare(GuestMemory& m,Registers& s,VectorState& vs) {
    auto& r=s.r;auto& v=vs.v;r[30]=r[3];r[31]=r[4];r[28]=r[5];
    m.WriteU32(Address(r[1]+340u),Address(r[30]));m.WriteU32(Address(r[1]+348u),Address(r[31]));m.WriteU32(Address(r[1]+356u),Address(r[28]));
    r[11]=12u;r[10]=r[30]+28u;r[9]=r[30]+40u;r[7]=r[30]+120u;r[8]=r[30]+108u;r[6]=r[30]+16u;
    v[9].fill(0u);v[0]=TripletTail(m,r[10]+r[11]);v[13]=TripletTail(m,r[9]+r[11]);
    v[8]=TripletHead(m,r[10]);v[7]=TripletHead(m,r[9]);
    for(unsigned i=0;i<4u;++i){v[126][i]=v[8][i]|v[0][i];v[0][i]=v[7][i]|v[13][i];}
    v[11]=TripletTail(m,r[7]+r[11]);v[12]=TripletTail(m,r[8]+r[11]);v[10]=TripletTail(m,r[6]+r[11],0u);
    v[6]=TripletHead(m,r[8]);v[5]=TripletHead(m,r[7]);v[4]=TripletHead(m,r[6]);
    v[13]=v[11];v[11]={v[10][3],v[10][0],v[10][1],v[10][2]};
    v[122]={v[126][0],v[126][3],v[126][1],v[126][2]};
    for(unsigned i=0;i<4u;++i)v[124][i]=v[5][i]|v[13][i];
    v[121]={v[0][0],v[0][1],v[0][1],v[0][2]};
    for(unsigned i=0;i<4u;++i)v[125][i]=v[6][i]|v[12][i];
    v[120]={v[0][0],v[0][3],v[0][2],v[0][3]};
    for(unsigned i=0;i<4u;++i)v[123][i]=v[4][i]|v[11][i];
    Compare(s,r[31],r[28]);
}
void BoxTest(GuestMemory& m,Dependencies d,Registers& s,VectorState& vs) {
    auto& r=s.r;auto& v=vs.v;r[11]=r[31]+6u;
    v[0]=TripletTail(m,r[31]+r[29],8u);v[12]=TripletHead(m,r[31]);
    v[13]=TripletTail(m,r[11]+r[29],8u);v[11]=TripletHead(m,r[11]);
    m.WriteU32(Address(r[1]+84u),Address(r[31]));r[10]=m.ReadU32(Address(r[30]+96u));++r[10];m.WriteU32(Address(r[30]+96u),Address(r[10]));
    for(unsigned i=0;i<4u;++i){v[10][i]=v[127][i]<<(v[127][i]&31u);v[8][i]=v[10][i];v[0][i]|=v[12][i];v[13][i]|=v[11][i];}
    v[12].fill(0u);
    // High halfwords represent the three centers/half extents plus padding.
    // Sign extension is only for centers; extents are interleaved with zero.
    Vector centers{},extents{};
    for(unsigned i=0;i<4u;++i){
        const auto c=std::uint16_t(v[0][2u+i/2u]>>(16u*(i%2u)));
        centers[i]=Bits(float(std::bit_cast<std::int16_t>(c)));
        extents[i]=std::uint16_t(v[13][2u+i/2u]>>(16u*(i%2u)));
    }
    v[0]=centers;v[13]=extents;
    if((s.cached_fp_control&0x8040u)!=0x8040u){s.cached_fp_control|=0x8040u;d.fp.SetHostFpControl(s.cached_fp_control);}
    for(unsigned i=0;i<4u;++i)v[13][i]=Bits(float(v[13][i]));
    for(unsigned i=0;i<4u;++i)v[0][i]=Bits(Lane(v[0],i)*Lane(v[125],i));
    for(unsigned i=0;i<4u;++i)v[13][i]=Bits(Lane(v[13],i)*Lane(v[124],i));
    for(unsigned i=0;i<4u;++i)v[0][i]=Bits(Lane(v[123],i)-Lane(v[0],i));
    for(unsigned i=0;i<4u;++i)v[11][i]=v[0][i]&~v[10][i];
    for(unsigned i=0;i<4u;++i)v[10][i]=Bits(Lane(v[0],i)*Lane(v[126],i));
    for(unsigned i=0;i<4u;++i)v[9][i]=Bits(Lane(v[122],i)*Lane(v[0],i));
    for(unsigned i=0;i<4u;++i)v[11][i]=Lane(v[11],i)>Lane(v[13],i)?0xffffffffu:0u;
    for(unsigned i=0;i<4u;++i)v[10][i]=Lane(v[10],i)>=Lane(v[12],i)?0xffffffffu:0u;
    v[12]={v[0][0],v[0][3],v[0][1],v[0][2]};
    v[0]={v[13][0],v[13][3],v[13][2],v[13][3]};
    v[13]={v[13][0],v[13][1],v[13][1],v[13][2]};
    for(unsigned i=0;i<4u;++i)v[7][i]=Bits(Lane(v[126],i)*Lane(v[12],i));
    for(unsigned i=0;i<4u;++i)v[0][i]=Bits(Lane(v[0],i)*Lane(v[121],i));
    for(unsigned i=0;i<4u;++i)v[13][i]=Bits(Lane(v[13],i)*Lane(v[120],i));
    for(unsigned i=0;i<4u;++i)v[12][i]=v[11][i]&v[10][i];
    for(unsigned i=0;i<4u;++i)v[11][i]=Bits(Lane(v[7],i)-Lane(v[9],i));
    for(unsigned i=0;i<4u;++i)v[0][i]=Bits(Lane(v[0],i)+Lane(v[13],i));
    for(unsigned i=0;i<4u;++i)v[13][i]=v[11][i]&~v[8][i];
    for(unsigned i=0;i<4u;++i)v[0][i]=(Lane(v[13],i)>Lane(v[0],i)?0xffffffffu:0u)|v[12][i];
    v[0]={v[0][3],v[0][1],v[0][2],v[0][3]};
    unsigned equal=0u;for(unsigned i=0;i<4u;++i){v[0][i]=v[0][i]==v[119][i]?0xffffffffu:0u;if(v[0][i])++equal;}
    s.cr6={std::uint8_t(equal==4u),0u,std::uint8_t(equal==0u),0u};
}
bool Stop(GuestMemory& m,Registers& s) {
    s.r[11]=m.ReadU32(Address(s.r[30]+4u));s.r[11]&=5u;
    s.cr0={0u,std::uint8_t(s.r[11]>0u),std::uint8_t(s.r[11]==0u),s.xer_so};Compare(s,s.r[11],5u);return s.cr6.eq;
}
void Traverse(GuestMemory& m,Dependencies d,Registers& s,VectorState& vs) {
    Enter(m,s,vs);Prepare(m,s,vs);auto& r=s.r;auto& v=vs.v;
    if(s.cr6.lt){v[127].fill(0xffffffffu);r[29]=8u;v[119]={v[9][3],v[9][1],v[9][2],v[9][3]};r[26]=0u;r[27]=1u;
        LowerServices services(d.guest,vs);
        do {
            BoxTest(m,d,s,vs);
            r[11]=(s.cr6.lt<<7u)|(s.cr6.gt<<6u)|(s.cr6.eq<<5u)|(s.cr6.so<<4u);
            // Preserve the raw CR word before projecting the negated all-lanes flag.
            m.WriteU32(Address(r[1]+88u),Address(r[11]));r[11]=~r[11];
            r[11]=(Address(r[11])>>7u)&1u;CompareSigned(s,r[11]);
            if(!s.cr6.eq){
                r[11]=m.ReadU32(Address(r[31]+12u));m.WriteU32(Address(r[1]+80u),Address(r[26]));r[11]&=0x40000000u;CompareSigned(s,r[11]);
                if(s.cr6.eq){r[11]=m.ReadU32(Address(r[31]+16u));r[10]=(std::uint64_t(Address(r[11]))<<2u)&0xfffffffcu;r[11]+=r[10];r[11]=(std::uint64_t(Address(r[11]))<<2u)&0xfffffffcu;r[31]+=r[11];}
            }else{
                r[11]=m.ReadU32(Address(r[31]+12u));r[10]=r[11]&0x80000000u;CompareSigned(s,r[10]);
                if(!s.cr6.eq){
                    m.WriteU32(Address(r[1]+80u),Address(r[27]));r[4]=Address(r[11])&0x3fffffffu;r[3]=r[30];s.lr=0x82bd5af8u;
                    (void)geometry_math61::Apply(0x82bd4448u,m,{services,d.fp},s);if(Stop(m,s))break;
                    r[11]=m.ReadU32(Address(r[31]+12u));r[10]=r[11]&0x40000000u;CompareSigned(s,r[10]);
                    if(!s.cr6.eq){r[11]=Address(r[11])&0x3fffffffu;r[3]=r[30];r[4]=r[11]+1u;s.lr=0x82bd5b28u;
                        (void)geometry_math61::Apply(0x82bd4448u,m,{services,d.fp},s);if(Stop(m,s))break;}
                }
            }
            r[31]+=20u;m.WriteU32(Address(r[1]+348u),Address(r[31]));Compare(s,r[31],r[28]);
        }while(s.cr6.lt);
    }
    Leave(m,s,vs);
}
}
bool Apply(GuestAddress entry,GuestMemory& m,Dependencies d,Registers& s,VectorState& v) {
    if(entry!=0x82bd5910u)return false;Traverse(m,d,s,v);return true;
}
}
