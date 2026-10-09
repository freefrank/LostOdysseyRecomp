#include "lo_semantics/geometry_quantized_unbounded61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::geometry_quantized_unbounded61 {
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
    geometry_quantized_unbounded61::GuestServices& guest;VectorState& vectors;
    LowerServices(geometry_quantized_unbounded61::GuestServices& g,VectorState& v):guest(g),vectors(v){}
    void CallIndirect(GuestAddress target,GuestMemory& memory,Registers& s)override{guest.CallIndirect(target,memory,s,vectors);}
};
void Enter(GuestMemory& m,Registers& s,VectorState& vs){
    auto& r=s.r;auto& v=vs.v;r[12]=s.lr;s.lr=0x82bd6fd8u;
    for(unsigned n=26;n<32u;++n)WriteU64(m,Address(r[1]-16u-8u*(31u-n)),r[n]);
    m.WriteU32(Address(r[1]-8u),Address(r[12]));
    r[12]=std::uint64_t(-112ll);SaveVector(m,r[1]+r[12],v[125]);
    r[12]=std::uint64_t(-96ll);SaveVector(m,r[1]+r[12],v[126]);
    r[12]=std::uint64_t(-80ll);SaveVector(m,r[1]+r[12],v[127]);
    const auto caller=r[1];r[1]-=272u;m.WriteU32(Address(r[1]),Address(caller));
}
void Leave(GuestMemory& m,Registers& s,VectorState& vs){
    auto& r=s.r;auto& v=vs.v;r[1]+=272u;
    r[0]=std::uint64_t(-112ll);v[125]=RestoreVector(m,r[1]+r[0]);
    r[0]=std::uint64_t(-96ll);v[126]=RestoreVector(m,r[1]+r[0]);
    r[0]=std::uint64_t(-80ll);v[127]=RestoreVector(m,r[1]+r[0]);
    for(unsigned n=26;n<32u;++n)r[n]=ReadU64(m,Address(r[1]-16u-8u*(31u-n)));
    r[12]=m.ReadU32(Address(r[1]-8u));s.lr=r[12];
}
double Float(const Registers& s,unsigned n){return std::bit_cast<double>(s.fpr_bits[n]);}
void Float(Registers& s,unsigned n,double value){s.fpr_bits[n]=std::bit_cast<std::uint64_t>(value);}
double LoadFloat(GuestMemory& m,std::uint64_t a){return std::bit_cast<float>(m.ReadU32(Address(a)));}
void StoreFloat(GuestMemory& m,std::uint64_t a,double value){m.WriteU32(Address(a),Bits(static_cast<float>(value)));}
std::uint64_t SignedHalf(std::uint64_t value){return std::uint64_t(std::int64_t(std::bit_cast<std::int16_t>(std::uint16_t(value))));}
// Quantized centers are signed halfwords; extents are unsigned. Keep the six
// integer spill slots, int64->double->single conversions and scaled float stores
// visible. The following box test reads these guest stack triplets.
void ExpandBox(GuestMemory& m,Dependencies deps,Registers& s,VectorState& vs){
    auto& r=s.r;auto& v=vs.v;
    r[11]=m.ReadU16(Address(r[30]));
    if(s.cached_fp_control&0x8040u){s.cached_fp_control&=~0x8040u;deps.fp.SetHostFpControl(s.cached_fp_control);}
    Float(s,0,LoadFloat(m,r[31]+108u));r[9]=m.ReadU16(Address(r[30]+2u));
    Float(s,13,LoadFloat(m,r[31]+112u));r[11]=SignedHalf(r[11]);r[8]=m.ReadU16(Address(r[30]+4u));
    r[9]=SignedHalf(r[9]);r[7]=m.ReadU16(Address(r[30]+6u));r[8]=SignedHalf(r[8]);
    r[6]=m.ReadU16(Address(r[30]+8u));r[7]=std::uint64_t(std::int64_t(std::bit_cast<std::int32_t>(Address(r[7]))));
    r[5]=m.ReadU16(Address(r[30]+10u));r[6]=std::uint64_t(std::int64_t(std::bit_cast<std::int32_t>(Address(r[6]))));
    Float(s,12,LoadFloat(m,r[31]+116u));WriteU64(m,Address(r[1]+80u),r[11]);
    r[5]=std::uint64_t(std::int64_t(std::bit_cast<std::int32_t>(Address(r[5]))));WriteU64(m,Address(r[1]+88u),r[9]);
    Float(s,11,LoadFloat(m,r[31]+120u));WriteU64(m,Address(r[1]+96u),r[8]);
    Float(s,10,LoadFloat(m,r[31]+124u));WriteU64(m,Address(r[1]+104u),r[7]);
    r[10]=r[1]+128u;WriteU64(m,Address(r[1]+112u),r[6]);r[11]=r[1]+144u;
    WriteU64(m,Address(r[1]+120u),r[5]);Float(s,9,LoadFloat(m,r[31]+128u));r[9]=r[1]+128u;
    for(unsigned i=0;i<4u;++i)v[8][i]=v[127][i]<<(v[127][i]&31u);
    r[8]=r[1]+144u;
    // The original interleaves reads and conversion, then narrows all six.
    s.fpr_bits[8]=ReadU64(m,Address(r[1]+80u));s.fpr_bits[7]=ReadU64(m,Address(r[1]+88u));
    Float(s,8,double(std::bit_cast<std::int64_t>(s.fpr_bits[8])));s.fpr_bits[6]=ReadU64(m,Address(r[1]+96u));
    Float(s,7,double(std::bit_cast<std::int64_t>(s.fpr_bits[7])));s.fpr_bits[5]=ReadU64(m,Address(r[1]+104u));
    Float(s,6,double(std::bit_cast<std::int64_t>(s.fpr_bits[6])));s.fpr_bits[4]=ReadU64(m,Address(r[1]+112u));
    Float(s,5,double(std::bit_cast<std::int64_t>(s.fpr_bits[5])));s.fpr_bits[3]=ReadU64(m,Address(r[1]+120u));
    Float(s,4,double(std::bit_cast<std::int64_t>(s.fpr_bits[4])));Float(s,3,double(std::bit_cast<std::int64_t>(s.fpr_bits[3])));
    for(unsigned n=8;n>=3u;--n)Float(s,n,static_cast<float>(Float(s,n)));
    Float(s,0,static_cast<float>(Float(s,0)*Float(s,8)));StoreFloat(m,r[1]+128u,Float(s,0));
    Float(s,0,static_cast<float>(Float(s,7)*Float(s,13)));StoreFloat(m,r[1]+132u,Float(s,0));
    Float(s,0,static_cast<float>(Float(s,6)*Float(s,12)));StoreFloat(m,r[1]+136u,Float(s,0));
    Float(s,0,static_cast<float>(Float(s,5)*Float(s,11)));StoreFloat(m,r[1]+144u,Float(s,0));
    Float(s,0,static_cast<float>(Float(s,4)*Float(s,10)));StoreFloat(m,r[1]+148u,Float(s,0));
    Float(s,0,static_cast<float>(Float(s,3)*Float(s,9)));StoreFloat(m,r[1]+152u,Float(s,0));
}
// Separating-axis ray/box rejection. Operands remain raw lane bits between
// operations, preserving both volatile vectors and each single rounding stage.
void BoxTest(GuestMemory& m,Dependencies deps,Registers& s,VectorState& vs){
    auto& r=s.r;auto& v=vs.v;
    ExpandBox(m,deps,s,vs);
    v[0]=TripletTail(m,r[10]+r[29]);r[10]=r[31]+28u;
    v[13]=TripletTail(m,r[11]+r[29]);r[11]=r[31]+16u;
    v[12]=TripletHead(m,r[9]);r[9]=r[31]+40u;v[11]=TripletHead(m,r[8]);
    for(unsigned i=0;i<4u;++i)v[10][i]=v[12][i]|v[0][i];
    v[12]=TripletTail(m,r[10]+r[29]);v[7]=TripletHead(m,r[10]);v[0]=TripletTail(m,r[11]+r[29]);
    for(unsigned i=0;i<4u;++i)v[13][i]|=v[11][i];
    v[9]=TripletHead(m,r[11]);r[11]=m.ReadU32(Address(r[31]+96u));
    v[11]=TripletTail(m,r[9]+r[29]);++r[11];v[6]=TripletHead(m,r[9]);
    m.WriteU32(Address(r[31]+96u),Address(r[11]));
    for(unsigned i=0;i<4u;++i)v[0][i]|=v[9][i];
    for(unsigned i=0;i<4u;++i)v[12][i]|=v[7][i];
    for(unsigned i=0;i<4u;++i)v[5][i]=v[127][i]<<(v[127][i]&31u);
    // Scalar dequantization disabled flushing; this VMX boundary writes it
    // unconditionally, matching the source even if the cached bits already agree.
    s.cached_fp_control|=0x8040u;deps.fp.SetHostFpControl(s.cached_fp_control);
    for(unsigned i=0;i<4u;++i)v[0][i]=Bits(Lane(v[0],i)-Lane(v[10],i));
    for(unsigned i=0;i<4u;++i)v[11][i]|=v[6][i];
    v[10]={v[12][0],v[12][3],v[12][1],v[12][2]};
    v[9]={v[11][0],v[11][1],v[11][1],v[11][2]};
    v[11]={v[11][0],v[11][3],v[11][2],v[11][3]};
    for(unsigned i=0;i<4u;++i)v[8][i]=v[0][i]&~v[8][i];
    v[7]={v[0][0],v[0][3],v[0][1],v[0][2]};
    for(unsigned i=0;i<4u;++i)v[6][i]=Bits(Lane(v[0],i)*Lane(v[12],i));
    for(unsigned i=0;i<4u;++i)v[10][i]=Bits(Lane(v[10],i)*Lane(v[0],i));
    v[0]={v[13][0],v[13][3],v[13][2],v[13][3]};
    for(unsigned i=0;i<4u;++i)v[8][i]=Lane(v[8],i)>Lane(v[13],i)?0xffffffffu:0u;
    v[13]={v[13][0],v[13][1],v[13][1],v[13][2]};
    for(unsigned i=0;i<4u;++i)v[12][i]=Bits(Lane(v[12],i)*Lane(v[7],i));
    for(unsigned i=0;i<4u;++i)v[0][i]=Bits(Lane(v[0],i)*Lane(v[9],i));
    for(unsigned i=0;i<4u;++i)v[13][i]=Bits(Lane(v[13],i)*Lane(v[11],i));
    for(unsigned i=0;i<4u;++i)v[7][i]=Lane(v[6],i)>=Lane(v[126],i)?0xffffffffu:0u;
    for(unsigned i=0;i<4u;++i)v[11][i]=Bits(Lane(v[12],i)-Lane(v[10],i));
    for(unsigned i=0;i<4u;++i)v[0][i]=Bits(Lane(v[0],i)+Lane(v[13],i));
    for(unsigned i=0;i<4u;++i)v[12][i]=v[8][i]&v[7][i];
    for(unsigned i=0;i<4u;++i)v[13][i]=v[11][i]&~v[5][i];
    for(unsigned i=0;i<4u;++i)v[0][i]=Lane(v[13],i)>Lane(v[0],i)?0xffffffffu:0u;
    for(unsigned i=0;i<4u;++i)v[0][i]|=v[12][i];
    v[0]={v[0][3],v[0][1],v[0][2],v[0][3]};
    unsigned equal=0u;for(unsigned i=0;i<4u;++i){v[0][i]=v[0][i]==v[125][i]?0xffffffffu:0u;if(v[0][i])++equal;}
    s.cr6={std::uint8_t(equal==4u),0u,std::uint8_t(equal==0u),0u};
}
void Traverse(GuestMemory& m,Dependencies deps,Registers& s,VectorState& vs){
    Enter(m,s,vs);auto& r=s.r;auto& v=vs.v;r[30]=r[4];r[26]=r[5];r[31]=r[3];
    Compare(s,r[30],r[26]);
    if(s.cr6.lt){v[126].fill(0u);r[29]=12u;v[127].fill(0xffffffffu);
        v[125]={v[126][3],v[126][1],v[126][2],v[126][3]};
        LowerServices services(deps.guest,vs);
        do{
            BoxTest(m,deps,s,vs);
            r[10]=(s.cr6.lt<<7u)|(s.cr6.gt<<6u)|(s.cr6.eq<<5u)|(s.cr6.so<<4u);
            r[11]=m.ReadU32(Address(r[30]+12u));r[27]=(Address(r[10])>>7u)&1u;r[28]=Address(r[11])&0x80000000u;
            CompareSigned(s,r[28]);
            if(!s.cr6.eq){CompareSigned(s,r[27]);if(!s.cr6.eq){
                r[4]=Address(r[11])&0x3fffffffu;r[3]=r[31];s.lr=0x82bd71f8u;
                (void)geometry_math61::Apply(0x82bd4448u,m,{services,deps.fp},s);
                r[11]=m.ReadU32(Address(r[31]+4u));r[11]&=5u;
                s.cr0={0u,std::uint8_t(r[11]>0u),std::uint8_t(r[11]==0u),s.xer_so};
                Compare(s,r[11],5u);if(s.cr6.eq)break;
            }}
            CompareSigned(s,r[27]);
            if(s.cr6.eq){CompareSigned(s,r[28]);if(s.cr6.eq){
                r[11]=m.ReadU32(Address(r[30]+20u));r[10]=Address(r[11])<<1u;
                r[11]+=r[10];r[11]=Address(r[11])<<3u;r[30]+=r[11];
            }}
            r[30]+=24u;Compare(s,r[30],r[26]);
        }while(s.cr6.lt);
    }
    Leave(m,s,vs);
}
}
bool Apply(GuestAddress entry,GuestMemory& memory,Dependencies deps,Registers& s,VectorState& vectors){
    if(entry!=0x82bd6fd0u)return false;
    Traverse(memory,deps,s,vectors);return true;
}
}
