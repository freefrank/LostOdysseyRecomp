#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/owned_tree_reorder_support61.h"
#include "lo_semantics/owned_tree_visit61.h"
#include "lo_semantics/reader_buffer_growth61.h"
#include <algorithm>
#include <bit>
namespace tree_reorder61_oracle {
using Full=owned_tree_reorder_support61::Registers;
constexpr GuestAddress Owner=0x30000,Nodes=0x31000,User=0x32000,Boxes=0x33000,Descriptors=0x34000,Indices=0x35000,Payload=0x36000,Fresh=0x37000,Table=0x38000;
constexpr GuestAddress Alloc=0x2a00,Free=0x2b00;
constexpr std::array<test::Region,4> Regions{{{0,0x120000},{0x82000000,0x1000},{0x83216000,0x1000},{0x832df000,0x1000}}};
struct Native final:float_triplet_transfer::NativeServices{void SetHostFpControl(std::uint32_t value)override{PPCFPSCRRegister{}.setcsr(value);}};
std::uint8_t* active_bytes=nullptr;
struct Guest final:manager_release_context61::GuestServices{
    unsigned mode=0;std::vector<std::array<std::uint64_t,73>> events;
    void CallDirect(GuestAddress,GuestMemory&,Full&)override{throw std::runtime_error("unexpected tree manager setup");}
    void CallIndirect(GuestAddress target,GuestMemory&,Full& s)override{
        if(target==0x82bd1b50u||target==0x82bd2168u||target==0x82bd1b78u){PPCContext c{};crt_full_oracle::ToPpc(c,s);if(target==0x82bd1b50u)__imp__sub_82BD1B50(c,active_bytes);else if(target==0x82bd2168u)__imp__sub_82BD2168(c,active_bytes);else __imp__sub_82BD1B78(c,active_bytes);s=crt_full_oracle::FromPpc(c);return;}
        std::array<std::uint64_t,73> event{};const auto snapshot=crt_full_oracle::Snapshot(s);std::copy(snapshot.begin(),snapshot.end(),event.begin());event[72]=target;events.push_back(event);
        if(target==Alloc){if(s.r[4]!=8u||s.r[5]!=64u)throw std::runtime_error("leaf append allocation size");s.r[3]=Fresh;}
        else if(target==Free){s.r[3]=0x11223344feedfaceull;if(mode==3u)s.r[30]=0x1122334400000007ull;}
        else throw std::runtime_error("unexpected tree external callback");
        s.r[8]^=0x123456789abcdef0ull;s.fpr_bits[15]^=0x180u;s.cr1={1,0,0,1};s.cr7={0,1,0,0};
    }
};
GuestMemory* active_memory=nullptr;Guest* active_guest=nullptr;Native* active_fp=nullptr;
void Lower(GuestAddress entry,GuestMemory& m,Guest& guest,Native& fp,Full& s){
    if(entry==0x82bda0a8u||entry==0x82bda1a0u)(void)owned_tree_visit61::Apply(entry,m,guest,s);
    else if(entry==0x82bd1558u)(void)transform_owner_routes61::Apply(entry,m,{guest,fp},s);
    else if(entry==0x82bd0798u)(void)crt_close_recursive_buffer_context::Apply(entry,m,guest,s);
    else if(entry==0x82bd2870u)(void)reader_buffer_growth61::Apply(entry,m,{guest,fp},s);
    else throw std::runtime_error("unexpected reorder lower");
}
void Seed(test::GuestWindow& window,unsigned mode){window.Fill(0xa5u);auto m=window.Memory();m.WriteU32(Owner+4,Nodes);m.WriteU32(Owner+12,0);m.WriteU32(Owner+16,0);m.WriteU32(Owner+32,Payload);m.WriteU32(Owner+24,Boxes+4);
    for(unsigned n=0;n<3;++n){auto node=Nodes+40*n;m.WriteU32(node+24,n==0?(Nodes+40)|1u:0u);m.WriteU32(node+32,Indices+4*n);m.WriteU32(node+36,n+1);for(unsigned c=0;c<6;++c)m.WriteU32(node+4*c,std::bit_cast<std::uint32_t>(static_cast<float>(10*n+c)));}
    m.WriteU32(User,0);m.WriteU32(User+4,mode==2?0:Boxes);m.WriteU32(User+8,mode==2?0:Descriptors);m.WriteU32(User+12,mode==2?std::bit_cast<std::uint32_t>(2.f):Indices);
    m.WriteU32(0x82000e50,std::bit_cast<std::uint32_t>(1.f));m.WriteU32(0x832df554,0);m.WriteU32(0x83216624,Table);m.WriteU32(Table,Alloc|3u);m.WriteU32(Table+12,Free|1u);
}
void Check(unsigned mode){struct Restore{std::uint32_t value=PPCFPSCRRegister{}.getcsr();~Restore(){PPCFPSCRRegister{}.setcsr(value);}} restore;
    test::GuestWindow original(Regions),recovered(Regions);Seed(original,mode);Seed(recovered,mode);auto om=original.Memory(),rm=recovered.Memory();Full initial{};for(unsigned n=0;n<32;++n){initial.r[n]=0x1122334400000000ull+n;initial.fpr_bits[n]=0x3ff0000000000000ull+n;}initial.r[1]=0x9988776600000000ull|Stack;initial.r[3]=0xaabbccdd00000000ull|Owner;initial.r[4]=mode==0?0x82bd1b50u:mode==1?0x82bd2168u:0x82bd1b78u;initial.r[5]=User;initial.lr=0x1122334481234567ull;initial.cached_fp_control=0x9fc0;initial.xer_so=1;
    Guest expected,actual;expected.mode=actual.mode=mode;Native fp;active_memory=&om;active_guest=&expected;active_fp=&fp;active_bytes=original.Bytes();PPCContext c{};crt_full_oracle::ToPpc(c,initial);PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if(mode<2)__imp__sub_82BDB1C0(c,original.Bytes());else if(mode==2)__imp__sub_82BDB208(c,original.Bytes());else __imp__sub_82BD20F0(c,original.Bytes());const auto expected_csr=PPCFPSCRRegister{}.getcsr();active_memory=nullptr;active_guest=nullptr;active_fp=nullptr;
    auto s=initial;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);const GuestAddress entry=mode<2?0x82bdb1c0u:mode==2?0x82bdb208u:0x82bd20f0u;(void)owned_tree_reorder_support61::Apply(entry,rm,{actual,fp},s);
    const auto want=crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),got=crt_full_oracle::Snapshot(s);if(want!=got||!original.EqualCommitted(recovered)||expected.events!=actual.events||expected_csr!=PPCFPSCRRegister{}.getcsr()){std::fprintf(stderr,"reorder support case %u state=%u RAM=%u events=%u\n",mode,want==got,original.EqualCommitted(recovered),expected.events==actual.events);for(unsigned n=0;n<want.size();++n)if(want[n]!=got[n])std::fprintf(stderr," state[%u]=%llx/%llx\n",n,(unsigned long long)want[n],(unsigned long long)got[n]);throw std::runtime_error("tree reorder support mismatch");}
    if(mode<2&&(s.r[3]!=2u||rm.ReadU32(User)!=2u))throw std::runtime_error("leaf count/depth contract");
    if(mode==1&&(rm.ReadU32(Descriptors)!=17u||rm.ReadU32(Descriptors+4)!=34u||rm.ReadU32(Boxes)!=std::bit_cast<std::uint32_t>(10.f)||rm.ReadU32(Boxes+44)!=std::bit_cast<std::uint32_t>(25.f)))throw std::runtime_error("leaf export layout");
    if(mode==2&&(rm.ReadU32(User+4)!=2u||rm.ReadU32(User+8)!=Fresh||rm.ReadU32(Fresh)!=Nodes+40||rm.ReadU32(Fresh+4)!=Nodes+80||actual.events.size()!=1))throw std::runtime_error("leaf append/growth contract");
    if(mode==3&&(actual.events.size()!=2||Address(actual.events[0][4])!=Payload||Address(actual.events[1][4])!=Boxes||rm.ReadU32(Owner+32)!=7||rm.ReadU32(Owner+24)!=7||rm.ReadU32(Owner+20)!=7||rm.ReadU32(Owner+28)!=7))throw std::runtime_error("tree storage live-clear contract");
    if(s.r[1]!=initial.r[1]||s.lr!=0x81234567u)throw std::runtime_error("tree support return frame");
}
}
// Shared accepted traversal invokes the genuine fixed callbacks through this
// fixture guest adapter; only external allocation/disposal is scripted.
void OriginalTreeReorder61Lower(std::uint32_t entry,PPCContext& c,std::uint8_t*){using namespace tree_reorder61_oracle;auto s=crt_full_oracle::FromPpc(c);Lower(entry,*active_memory,*active_guest,*active_fp,s);crt_full_oracle::ToPpc(c,s);}
void OriginalTreeReorder61Save(PPCContext& c,std::uint8_t*){using namespace tree_reorder61_oracle;auto s=crt_full_oracle::FromPpc(c);for(unsigned n=29;n<32;++n)WriteU64(*active_memory,Address(s.r[1]-8u*(33u-n)),s.r[n]);active_memory->WriteU32(Address(s.r[1]-8u),Address(s.r[12]));}
void OriginalTreeReorder61Restore(PPCContext& c,std::uint8_t*){using namespace tree_reorder61_oracle;auto s=crt_full_oracle::FromPpc(c);for(unsigned n=29;n<32;++n)s.r[n]=ReadU64(*active_memory,Address(s.r[1]-8u*(33u-n)));s.r[12]=active_memory->ReadU32(Address(s.r[1]-8u));s.lr=s.r[12];crt_full_oracle::ToPpc(c,s);}
void OriginalTreeReorder61Guest(std::uint32_t target,PPCContext& c,std::uint8_t*){using namespace tree_reorder61_oracle;auto s=crt_full_oracle::FromPpc(c);active_guest->CallIndirect(target,*active_memory,s);crt_full_oracle::ToPpc(c,s);}
int main(){try{for(unsigned n=0;n<4;++n)tree_reorder61_oracle::Check(n);std::puts("PASS owned-tree-reorder-support61 4 genuine upper/shared-lower cases");return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
