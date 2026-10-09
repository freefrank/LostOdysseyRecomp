#include "object_sort_engine61_oracle_fixture.h"
#include "lo_semantics/object_grid_read61.h"
#include "lo_semantics/crt_reader_cleanup_callers_context.h"
#include "lo_semantics/crt_context_adapter.h"

namespace object_grid61_oracle {
using Registers=object_grid_read61::Registers;
using Environment=sort_engine61_oracle::Environment;
constexpr GuestAddress Grid=0x40000u,Reader=0x41000u,Node=0x42000u,Data=0x43000u,Cells=0x44000u,Path=0x45000u;
constexpr std::array<test::Region,6> Regions{{{0u,0x120000u},{0x82000000u,0x1000u},
    {0x821ba000u,0x1000u},{0x82baf000u,0x1000u},{0x83216000u,0x1000u},{0x832dc000u,0x4000u}}};
Environment* active_environment=nullptr;
object_grid_read61::Dependencies Deps(Environment& env) {
    const auto d=env.Deps();return {d.sort,d.fp};
}
void Check(unsigned mode) {
    sort_engine61_oracle::RestoreHost restore;
    std::vector<unsigned> bits;
    const auto append=[&](std::uint32_t value,unsigned width) {
        for(unsigned n=width;n!=0u;--n)bits.push_back((value>>(n-1u))&1u);
    };
    if(mode==2u) {
        append(1u,1u); // implicit color 0
        append(2u,32u);append(31u,5u);append(0u,5u);append(0u,5u);append(0u,5u);append(1u,5u);
        append(0u,1u);append(7u,32u); // explicit color 7
        append(1u,32u);append(31u,5u);append(0u,5u);append(1u,5u);append(0u,5u);
    }
    append(0u,1u);append(0xffffffffu,32u);
    if(mode==2u) for(unsigned i=0u;i<8u;++i)append(i==0u||i==4u?0u:1u,1u);
    test::GuestWindow original(Regions),recovered(Regions);
    const auto seed=[&](test::GuestWindow& window) {
        window.Fill(0xa5u);auto memory=window.Memory();
        memory.WriteU32(Grid+88u,4u);memory.WriteU32(Grid+104u,mode==1u?0u:8u);memory.WriteU32(Grid+108u,Cells);
        memory.WriteU32(Reader,Node);memory.WriteU8(Reader+24u,0u);memory.WriteU32(Node,Data);
        memory.WriteU32(Node+4u,0u);memory.WriteU32(Node+8u,512u);memory.WriteU8(Path,0u);
        for(unsigned i=0u;i<(bits.size()+7u)/8u;++i) {
            std::uint8_t value=0u;
            for(unsigned j=0u;j<8u;++j)if(i*8u+j<bits.size())value|=std::uint8_t(bits[i*8u+j]<<(7u-j));
            memory.WriteU8(Data+i,value);
        }
        memory.WriteU32(0x832df554u,0u);memory.WriteU32(0x83216624u,sort_engine61_oracle::Vtable);
        memory.WriteU32(sort_engine61_oracle::Vtable,sort_engine61_oracle::AllocateTarget|1u);
        memory.WriteU32(sort_engine61_oracle::Vtable+12u,sort_engine61_oracle::FreeTarget|3u);
        memory.WriteU32(0x82000e50u,std::bit_cast<std::uint32_t>(1.f));
        memory.WriteU32(0x821baa74u,std::bit_cast<std::uint32_t>(2.f));
        for(unsigned op=0u;op<32u;++op)memory.WriteU32(0x82baf77cu+op*4u,0x82001000u+op*4u);
    };
    seed(original);seed(recovered);Environment expected(original),actual(recovered);
    auto initial=sort_engine61_oracle::Initial(4u);initial.r[3]=Grid;initial.r[4]=mode==0u?Path:0u;initial.r[5]=Reader;
    PPCContext context{};crt_full_oracle::ToPpc(context,initial);auto state=initial;
    active_environment=&expected;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    __imp__sub_82BB2098(context,original.Bytes());const auto expected_host=PPCFPSCRRegister{}.getcsr();
    active_environment=nullptr;PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if(!object_grid_read61::Apply(0x82bb2098u,actual.stream.memory,Deps(actual),state))throw std::runtime_error("missing grid read");
    if(crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context))!=crt_full_oracle::Snapshot(state) ||
        !original.EqualCommitted(recovered) || expected.guest.events!=actual.guest.events ||
        expected_host!=PPCFPSCRRegister{}.getcsr())throw std::runtime_error("grid read Full72/RAM/callback/host-control mismatch");
    if(state.r[3]!=(mode==0u?0u:1u) || state.r[1]!=initial.r[1] || state.lr!=0x81234567u ||
        actual.stream.memory.ReadU32(Node+4u)!=(mode==0u?0u:(bits.size()+7u)/8u) ||
        actual.guest.events.size()!=(mode==2u?4u:0u))throw std::runtime_error("grid read path/stream/ownership mismatch");
    if(mode==2u) {
        constexpr std::array<std::uint32_t,8> cells{{0x80000000u,0u,0xffffffffu,0xffffffffu,0x80000007u,0xffffffffu,0xffffffffu,0xffffffffu}};
        for(unsigned i=0u;i<cells.size();++i)if(actual.stream.memory.ReadU32(Cells+i*4u)!=cells[i])
            throw std::runtime_error("grid color scatter/occupancy mismatch");
    }
}
void CheckSeek() {
    test::GuestWindow original(Regions),recovered(Regions);
    const auto seed=[](test::GuestWindow& window) {
        window.Fill(0xa5u);auto m=window.Memory();m.WriteU32(Reader,Node);m.WriteU32(Node,Data);m.WriteU32(Node+4u,0u);m.WriteU32(Node+8u,16u);
    };
    for(unsigned offset:{3u,16u}) {
        seed(original);seed(recovered);Environment env(recovered);
        auto state=sort_engine61_oracle::Initial(4u);state.r[3]=Reader;state.r[4]=offset;
        PPCContext context{};crt_full_oracle::ToPpc(context,state);__imp__sub_82BD0A30(context,original.Bytes());
        if(!object_grid_read61::Apply(0x82bd0a30u,env.stream.memory,Deps(env),state) ||
            crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context))!=crt_full_oracle::Snapshot(state) || !original.EqualCommitted(recovered))
            throw std::runtime_error("bounded seek Full72/RAM mismatch");
        if(env.stream.memory.ReadU32(Node+4u)!=(offset==3u?3u:0u))throw std::runtime_error("bounded seek strict capacity mismatch");
    }
}
}
void OriginalObjectGrid61Save(unsigned first,PPCContext& c,std::uint8_t*) {
    auto& m=object_grid61_oracle::active_environment->stream.memory;const auto r=crt_full_oracle::Gprs(c);
    for(unsigned i=first;i<=31u;++i)WriteU64(m,Address(c.r1.u64-16u-8u*(31u-i)),r[i]->u64);
    m.WriteU32(Address(c.r1.u64-8u),c.r12.u32);
}
void OriginalObjectGrid61Restore(unsigned first,PPCContext& c,std::uint8_t*) {
    auto& m=object_grid61_oracle::active_environment->stream.memory;const auto r=crt_full_oracle::Gprs(c);
    for(unsigned i=first;i<=31u;++i)r[i]->u64=ReadU64(m,Address(c.r1.u64-16u-8u*(31u-i)));
    c.r12.u64=m.ReadU32(Address(c.r1.u64-8u));c.lr=c.r12.u64;
}
void OriginalObjectGrid61Indirect(GuestAddress target,PPCContext& c,std::uint8_t*) {
    auto& env=*object_grid61_oracle::active_environment;auto s=crt_full_oracle::FromPpc(c);
    env.guest.CallIndirect(target,env.stream.memory,s);crt_full_oracle::ToPpc(c,s);
}
// File loading/open/close are accepted boundaries, outside these borrowed-reader
// cases. The named bridge makes that narrower validation scope explicit.
void OriginalObjectGrid61FileLower(GuestAddress entry,PPCContext& c,std::uint8_t*) {
    auto& env=*object_grid61_oracle::active_environment;auto s=crt_full_oracle::FromPpc(c);
    const auto d=object_grid61_oracle::Deps(env);auto& m=env.stream.memory;
    switch(entry) {
    case 0x82df2150u:(void)crt_stream_close_shared_lower::Apply(entry,m,d.reader.accepted,s);break;
    case 0x82b85d88u: {
        auto stream=crt_context_adapter::ToStream(s);(void)crt_stream_bulk_close_routes::Apply(entry,m,d.reader.accepted.close,stream);
        crt_context_adapter::FromStream(s,stream);break;
    }
    case 0x82bd0d28u:(void)crt_close_reader_callers_context::Apply(entry,m,d.reader,s);break;
    case 0x82bd0df0u:(void)crt_reader_cleanup_callers_context::Apply(entry,m,d.reader.guest,s);break;
    default:throw std::runtime_error("unexpected file reader lower");
    }
    crt_full_oracle::ToPpc(c,s);
}
int main() {
    try {
        for(unsigned mode=0u;mode<3u;++mode)object_grid61_oracle::Check(mode);
        object_grid61_oracle::CheckSeek();
        std::puts("PASS object-grid-read61 3 group/path cases and 2 bounded seeks");return 0;
    }catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}
}
