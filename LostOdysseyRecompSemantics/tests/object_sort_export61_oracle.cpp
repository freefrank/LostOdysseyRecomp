#include "object_sort_engine61_oracle_fixture.h"
#include "lo_semantics/object_sort_export61.h"
#include "lo_semantics/crt_close_next61.h"
namespace object_sort_export61_oracle {
using namespace sort_engine61_oracle;
constexpr GuestAddress ExportOwner=0x36000u,Object=0x37000u,Cells=0x38000u;
constexpr GuestAddress GlobalService=0x39000u,GlobalTable=0x3a000u,Destination=0x40000u,Descriptor=0x41000u;
constexpr std::array<test::Region,5> ExportRegions{{{0u,0x120000u},{0x82000000u,0x1000u},
    {0x821ba000u,0x1000u},{0x83216000u,0x1000u},{0x832dc000u,0x4000u}}};
struct Output final:crt_close_block_output_context::ErrorOutputServices {
    void CallOutput(GuestMemory&,Full&)override{throw std::runtime_error("file output outside export fixture");}
};
struct ExportEnvironment {
    sort_engine61_oracle::Environment engine;Output output;
    explicit ExportEnvironment(test::GuestWindow& window):engine(window){}
    object_sort_export61::Dependencies Deps(){return {engine.Deps(),output};}
};
ExportEnvironment* active_export=nullptr;
void SeedExport(test::GuestWindow& window) {
    Seed(window,{}, {9u,8u,7u},2u);auto m=window.Memory();
    m.WriteU32(Writer+4u,Node);m.WriteU32(Writer+8u,0u);m.WriteU32(Writer+12u,0u);
    m.WriteU32(ExportOwner+184u,Object);m.WriteU32(Object+88u,2u);
    m.WriteU32(Object+104u,0u);m.WriteU32(Object+108u,Cells);
    m.WriteU32(0x832df548u,GlobalService);m.WriteU32(GlobalService,GlobalTable);
    m.WriteU32(GlobalTable+8u,AllocateTarget|1u);m.WriteU32(GlobalTable+20u,FreeTarget|3u);
}
// Independent upper contract: serialize through the already accepted dispatcher
// into a known borrowed one-block writer, then read its actual payload bytes.
std::vector<std::uint8_t> ReferencePayload() {
    RestoreHost restore;test::GuestWindow window(ExportRegions);SeedExport(window);ExportEnvironment env(window);
    auto s=Initial(2u);s.r[3]=Object;s.r[4]=0u;s.r[5]=Writer;
    auto& m=env.engine.stream.memory;
    (void)object_sort_dispatch61::Apply(0x82bafec0u,m,env.Deps(),s);
    s.r[3]=Writer;(void)crt_close_next61::Apply(0x82bd0900u,m,env.engine.guest,s);
    const auto count=Address(s.r[3]);if(count<12u||count>4096u)throw std::runtime_error("unexpected reference payload size");
    std::vector<std::uint8_t> bytes(count);for(unsigned i=0;i<count;++i)bytes[i]=m.ReadU8(Data+i);return bytes;
}
void Check(unsigned scenario,const std::vector<std::uint8_t>& payload) {
    RestoreHost restore;test::GuestWindow original(ExportRegions),recovered(ExportRegions);
    SeedExport(original);SeedExport(recovered);
    const auto seed=[&](test::GuestWindow& window){auto m=window.Memory();
        if(scenario==0u)m.WriteU32(ExportOwner+184u,0u);
        m.WriteU32(Descriptor,static_cast<std::uint32_t>(payload.size())+(scenario==2u?1u:0u));m.WriteU32(Descriptor+4u,Destination);
    };seed(original);seed(recovered);
    ExportEnvironment expected(original),actual(recovered);auto state=Initial(2u);
    state.r[3]=0xaabbccdd00000000ull|ExportOwner;state.r[4]=0x1122334400000000ull|Descriptor;
    const auto initial=state;PPCContext context{};crt_full_oracle::ToPpc(context,state);
    active_export=&expected;PPCFPSCRRegister{}.setcsr(state.cached_fp_control);
    const auto entry=scenario<2u?0x82b9df18u:0x82b9dfa0u;
    if(scenario<2u)__imp__sub_82B9DF18(context,original.Bytes());else __imp__sub_82B9DFA0(context,original.Bytes());
    const auto expected_csr=PPCFPSCRRegister{}.getcsr();active_export=nullptr;PPCFPSCRRegister{}.setcsr(state.cached_fp_control);
    if(!object_sort_export61::Apply(entry,actual.engine.stream.memory,actual.Deps(),state))throw std::runtime_error("missing export entry");
    const auto want=crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context)),got=crt_full_oracle::Snapshot(state);
    if(want!=got||!original.EqualCommitted(recovered)||expected.engine.guest.events!=actual.engine.guest.events||expected_csr!=PPCFPSCRRegister{}.getcsr()) {
        std::fprintf(stderr,"export case %u state=%u RAM=%u events=%u CSR=%x/%x\n",scenario,want==got,original.EqualCommitted(recovered),expected.engine.guest.events==actual.engine.guest.events,expected_csr,PPCFPSCRRegister{}.getcsr());
        for(unsigned i=0;i<want.size();++i)if(want[i]!=got[i])std::fprintf(stderr," state[%u]=%llx/%llx\n",i,(unsigned long long)want[i],(unsigned long long)got[i]);
        throw std::runtime_error("export Full72/RAM/callback mismatch");
    }
    const auto result=scenario==1u?payload.size():scenario==3u?1u:0u;
    if(state.r[3]!=result||state.r[1]!=initial.r[1]||state.lr!=0x81234567u||state.r[31]!=initial.r[31]||state.r[30]!=initial.r[30])throw std::runtime_error("export return/frame contract");
    auto& m=actual.engine.stream.memory;
    for(unsigned i=0;i<payload.size();++i)if(m.ReadU8(Destination+i)!=(scenario==3u?payload[i]:0xa5u))throw std::runtime_error("export destination contract");
    if(scenario==0u&&!actual.engine.guest.events.empty())throw std::runtime_error("absent object allocated storage");
}
}
void OriginalObjectSortExport61Lower(std::uint32_t entry,PPCContext& context,std::uint8_t*) {
    using namespace object_sort_export61_oracle;auto s=crt_full_oracle::FromPpc(context);
    if(!object_sort_export61::ApplyAcceptedLower(entry,active_export->engine.stream.memory,active_export->Deps(),s))throw std::runtime_error("missing accepted export lower");
    crt_full_oracle::ToPpc(context,s);
}
int main(){try{const auto payload=object_sort_export61_oracle::ReferencePayload();for(unsigned n=0;n<4u;++n)object_sort_export61_oracle::Check(n,payload);
    std::puts("PASS object-sort-export61 4 actual-upper/shared-lower cases");
    std::puts("LIMIT empty-object payload; CRT/dispatcher lowers shared, native/file output/faults/concurrency and gameplay remain unvalidated");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
