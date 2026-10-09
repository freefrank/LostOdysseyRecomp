// Genuine upper body with shared previously validated growth/copy lowers.
#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/geometry_query_prepare61.h"
#include "lo_semantics/crt_copy_full_context.h"
#include <bit>
namespace query_prepare61_oracle {
using Registers=geometry_query_prepare61::Registers;
constexpr GuestAddress Query=0x30000u, Tree=0x31000u, Leaves=0x32000u,
    List=0x33000u, Mesh=0x34000u, Triangles=0x35000u, Vertices=0x36000u,
    Buffer=0x37000u, Old=0x38000u, Fresh=0x39000u, Vtable=0x3a000u, Ray=0x3b000u, Matrix=0x3c000u, Cached=0x3d000u;
constexpr GuestAddress Allocate=0x2a00u, Dispose=0x2b00u;
constexpr std::array<test::Region,7> Regions{{{0u,0x120000u},
    {0x82000000u,0x10000u},{0x82010000u,0x10000u},{0x82030000u,0x10000u},
    {0x820a0000u,0x10000u},{0x83216000u,0x1000u},{0x832df000u,0x1000u}}};
struct Native final: float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t value) override { PPCFPSCRRegister{}.setcsr(value); }
};
struct Guest final: crt_close_recursive_buffer_context::GuestServices {
    std::vector<std::array<std::uint64_t,72>> events;
    void CallIndirect(GuestAddress target,GuestMemory&,Registers& s) override {
        events.push_back(crt_full_oracle::Snapshot(s));
        if(target==Allocate) s.r[3]=Fresh;
        else if(target!=Dispose) throw std::runtime_error("unexpected triangle callback");
        s.r[8]^=0x1234567890abcdefull; s.fpr_bits[7]^=0x100u;
        s.cached_fp_control=0x9fc0u; PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    }
};
GuestMemory* active_memory=nullptr; Guest* active_guest=nullptr; Native native;
void Float(GuestMemory& m,GuestAddress a,float v){m.WriteU32(a,std::bit_cast<std::uint32_t>(v));}
void Seed(test::GuestWindow& window,unsigned mode){
    window.Fill(0xa5u); auto m=window.Memory();
    m.WriteU32(Query+4u,mode==1u?3u:0u);m.WriteU32(Query+8u,mode==0u?Tree:0u);m.WriteU32(Query+12u,Mesh);
    m.WriteU32(Tree+8u,4u);m.WriteU32(Cached,0u);
    m.WriteU32(Mesh+16u,Triangles);m.WriteU32(Mesh+20u,Vertices);
    for(unsigned i=0;i<3u;++i)m.WriteU32(Triangles+4u*i,i);
    constexpr std::array<float,9> xyz{0,0,0, 1,0,0, 0,1,0};
    for(unsigned i=0;i<xyz.size();++i)Float(m,Vertices+4u*i,xyz[i]);
    const std::array<float,6> ray{mode==2u?1.25f:.25f,mode==2u?2.25f:.25f,mode==2u?4.0f:1.0f,0,0,-1};
    for(unsigned i=0;i<ray.size();++i)Float(m,Ray+4u*i,ray[i]);
    for(unsigned i=0;i<16u;++i)Float(m,Matrix+4u*i,i%5u==0u?1.0f:0.0f);
    Float(m,Matrix+48u,1);Float(m,Matrix+52u,2);Float(m,Matrix+56u,3);
    m.WriteU32(Query+92u,mode<2u?Buffer:0u);
    m.WriteU32(Query+132u,mode==3u?0x7f7fffffu:std::bit_cast<std::uint32_t>(4.0f));Float(m,Query+136u,.0001f);
    m.WriteU8(Query+140u,0u);m.WriteU8(Query+141u,mode==1u?0u:1u);
    m.WriteU32(Buffer,mode==1u?0u:16u);m.WriteU32(Buffer+4u,8u);
    m.WriteU32(Buffer+8u,Old);Float(m,Buffer+12u,2.0f);
    Float(m,0x8201f9f0u,.5f);Float(m,0x82035cb8u,-.000001f);
    Float(m,0x82007784u,1.0f);Float(m,0x820a6b8cu,.000001f);
    Float(m,0x82000e50u,1.0f);m.WriteU32(0x832df554u,0u);m.WriteU32(0x83216624u,Vtable);
    m.WriteU32(Vtable,Allocate|3u);m.WriteU32(Vtable+12u,Dispose|3u);
}
void Check(unsigned mode){
    struct Restore{std::uint32_t csr=PPCFPSCRRegister{}.getcsr();~Restore(){PPCFPSCRRegister{}.setcsr(csr);}} restore;
    test::GuestWindow expected_window(Regions),actual_window(Regions);Seed(expected_window,mode);Seed(actual_window,mode);
    auto expected_memory=expected_window.Memory(), memory=actual_window.Memory();
    Registers initial{};for(unsigned n=0;n<32u;++n){initial.r[n]=0x1122334400000000ull+n;initial.fpr_bits[n]=0x3ff0000000000000ull+n;}
    initial.r[1]=0x8877665500080000ull;initial.r[3]=0xaabbccdd00000000ull|Query;initial.r[4]=Ray;initial.r[5]=mode==2u?Matrix:0u;initial.r[6]=mode==1u?Cached:0u;
    initial.lr=0x9988776681234567ull;initial.cached_fp_control=0x9fc0u;initial.xer_so=1u;
    PPCContext context{};crt_full_oracle::ToPpc(context,initial);Guest expected,actual;
    active_memory=&expected_memory;active_guest=&expected;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    __imp__sub_82BD5F28(context,expected_window.Bytes());const auto expected_csr=PPCFPSCRRegister{}.getcsr();
    active_memory=nullptr;active_guest=nullptr;auto state=initial;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if(!geometry_query_prepare61::Apply(0x82bd5f28u,memory,{actual,native},state))throw std::runtime_error("missing query prepare entry");
    auto want=crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context)),got=crt_full_oracle::Snapshot(state);
    for(unsigned n=0;n<want.size();++n)if(want[n]!=got[n]){
        std::fprintf(stderr,"query-prepare mode %u Full72 %u expected %016llx actual %016llx\n",mode,n,(unsigned long long)want[n],(unsigned long long)got[n]);
        throw std::runtime_error("query prepare registers mismatch");}
    if(!expected_window.EqualCommitted(actual_window))throw std::runtime_error("query prepare RAM mismatch");
    if(expected.events!=actual.events||expected_csr!=PPCFPSCRRegister{}.getcsr())throw std::runtime_error("query prepare callback/CSR mismatch");
    const auto hits=mode<2u?1u:0u;
    if(memory.ReadU32(Query+104u)!=hits||memory.ReadU32(Query+100u)!=hits||state.r[3]!=(mode<2u?1u:0u))throw std::runtime_error("query prepare outcome");
    if(mode<2u && memory.ReadU32(Query+80u)!=std::bit_cast<std::uint32_t>(1.0f))throw std::runtime_error("query hit distance");
    if(mode==1u && (actual.events.size()!=2u||memory.ReadU32(Buffer+8u)!=Fresh))throw std::runtime_error("query growth outcome");
    if(mode==2u && (memory.ReadU32(Query+16u)!=std::bit_cast<std::uint32_t>(.25f)||memory.ReadU32(Query+48u)!=std::bit_cast<std::uint32_t>(2.0f)))throw std::runtime_error("transformed query bounds");
    if(mode==3u && memory.ReadU32(Query+48u)!=std::bit_cast<std::uint32_t>(1.0f))throw std::runtime_error("unbounded query extent");
}
}
void OriginalQueryPrepare61Lower(std::uint32_t entry,PPCContext& c,std::uint8_t*){
    using namespace query_prepare61_oracle;auto s=crt_full_oracle::FromPpc(c);
    if(entry==0x82bd2870u)(void)reader_buffer_growth61::Apply(entry,*active_memory,{*active_guest,native},s);
    else if(entry==0x82b7a0b0u)(void)crt_copy_full_context::Apply(entry,*active_memory,s);
    else throw std::runtime_error("unexpected triangle lower");crt_full_oracle::ToPpc(c,s);
}
void OriginalQueryPrepare61Save(bool fp,PPCContext& c,std::uint8_t*){
    using namespace query_prepare61_oracle;const auto s=crt_full_oracle::FromPpc(c);auto& m=*active_memory;
    for(unsigned n=fp?28u:29u;n<32u;++n)recovery_abi::WriteU64(m,Address((fp?s.r[12]:s.r[1])-8u*(fp?32u-n:33u-n)),fp?s.fpr_bits[n]:s.r[n]);
    if(!fp)m.WriteU32(Address(s.r[1]-8u),Address(s.r[12]));
}
void OriginalQueryPrepare61Restore(bool fp,PPCContext& c,std::uint8_t*){
    using namespace query_prepare61_oracle;auto s=crt_full_oracle::FromPpc(c);auto& m=*active_memory;
    for(unsigned n=fp?28u:29u;n<32u;++n){const auto v=recovery_abi::ReadU64(m,Address((fp?s.r[12]:s.r[1])-8u*(fp?32u-n:33u-n)));if(fp)s.fpr_bits[n]=v;else s.r[n]=v;}
    if(!fp){s.r[12]=m.ReadU32(Address(s.r[1]-8u));s.lr=s.r[12];}crt_full_oracle::ToPpc(c,s);
}
int main(){try{for(unsigned mode=0;mode<4u;++mode)query_prepare61_oracle::Check(mode);std::puts("PASS geometry-query-prepare61 4 focused original-upper cases");return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
