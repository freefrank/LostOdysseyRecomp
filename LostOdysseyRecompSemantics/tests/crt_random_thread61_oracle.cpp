// Four original upper bodies; accepted CRT/heap lowers are shared explicitly.
#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/crt_random_thread61.h"
#include "lo_semantics/memory_fill.h"
#include <algorithm>
#include <bit>

namespace random_thread61_oracle {
using Full=crt_random_thread61::Registers;
using HeapRegisters=raw_allocation_context::Registers;
constexpr GuestAddress AllocHeap=0x10000u,AllocBlock=0x100000u;
constexpr GuestAddress AllocDescriptor=0x360000u,AllocSentinel=0x370000u;
constexpr GuestAddress Getter=0x2a00u,Setter=0x2a04u,TlsRecord=0x90000u;
constexpr std::array<test::Region,6> RandomRegions{{{0,0x400000u},
    {0x831e0000u,0x10000u},{0x83214000u,0x3000u},
    {0x83245000u,0x1000u},{0x832d3000u,0x2000u},{0x83378000u,0x3000u}}};
using AllocationEvent=std::array<std::uint64_t,7>;
struct HeapBoundary final : heap_allocation_context::BoundaryServices
{
    std::vector<AllocationEvent> events;
    static AllocationEvent Record(GuestAddress entry,const HeapRegisters& state)
    {return {entry,state.r[1],state.lr,state.r[3],state.r[4],
        state.r[5],state.r[30]};}
    void CallDirect(GuestAddress entry,GuestMemory& memory,
        HeapRegisters& state) override
    {
        events.push_back(Record(entry,state));
        switch(entry)
        {
        case 0x827cc428u:state.r[3]=0u;return;
        case 0x82b7bc40u:
            state.r[3]=FillGuestMemory(memory,Address(state.r[3]),
                Address(state.r[4]),Address(state.r[5]));return;
        default:throw std::runtime_error("unexpected record heap direct");
        }
    }
    void CallNative(GuestAddress entry,GuestMemory&,
        HeapRegisters& state) override
    {
        events.push_back(Record(entry,state));
        switch(entry)
        {
        case 0x830da07cu:state.r[3]=1u;return;
        case 0x830d9c6cu:case 0x830d9c7cu:return;
        default:throw std::runtime_error("unexpected record heap native");
        }
    }
};

void SeedList(GuestMemory& memory,std::uint32_t units)
{
    const auto head=AllocHeap+(units+48u)*8u,node=AllocBlock+8u;
    memory.WriteU16(AllocBlock,static_cast<std::uint16_t>(units));
    memory.WriteU16(AllocBlock+2u,0u);
    memory.WriteU8(AllocBlock+4u,0u);
    memory.WriteU8(AllocBlock+5u,0u);
    memory.WriteU32(head,node);memory.WriteU32(head+4u,node);
    memory.WriteU32(node,head);memory.WriteU32(node+4u,head);
    const auto word=AllocHeap+((units>>5u)+88u)*4u;
    memory.WriteU32(word,memory.ReadU32(word)|(1u<<(units&31u)));
    memory.WriteU32(AllocHeap+48u,memory.ReadU32(AllocHeap+48u)+units);
    const auto next=AllocBlock+units*16u;
    memory.WriteU16(next,1u);
    memory.WriteU16(next+2u,static_cast<std::uint16_t>(units));
    memory.WriteU8(next+4u,0u);
    memory.WriteU8(next+5u,1u);
}

void SeedHeap(GuestMemory& memory) {
    memory.WriteU32(0x83245708u,AllocHeap);
    memory.WriteU32(0x832d3aecu,
        0u);
    memory.WriteU32(0x832d3ae8u,
        0u);
    memory.WriteU32(0x83378e80u,0u);
    memory.WriteU32(0x83215210u,0u);
    memory.WriteU32(AllocHeap+20u,0u);
    memory.WriteU32(AllocHeap+24u,1u);
    memory.WriteU32(AllocHeap+28u,0xffffu);
    memory.WriteU32(AllocHeap+48u,0u);
    memory.WriteU32(AllocHeap+96u,AllocDescriptor);
    memory.WriteU32(AllocDescriptor+44u,0x340000u);
    memory.WriteU32(AllocDescriptor+64u,AllocBlock);
    memory.WriteU32(AllocHeap+1408u,AllocSentinel);
    memory.WriteU8(AllocHeap+379u,1u);
    for(unsigned units=0;units<128u;++units)
    {
        const auto head=AllocHeap+(units+48u)*8u;
        memory.WriteU32(head,head);memory.WriteU32(head+4u,head);
    }
    for(unsigned word=0;word<4u;++word)
        memory.WriteU32(AllocHeap+(88u+word)*4u,0u);
    memory.WriteU32(AllocHeap+88u,AllocHeap+88u);
    memory.WriteU32(AllocHeap+92u,AllocHeap+88u);
    SeedList(memory,14u);
}
struct Unused final : crt_record_allocation_context::HandlerServices,crt_free_context::LowerCalls {
    void CallNewHandler(GuestAddress,GuestMemory&,Full&)override{throw std::runtime_error("unexpected random allocation handler");}
    void Call(GuestAddress,GuestMemory&,crt_free_context::Registers&)override{throw std::runtime_error("random release failure path outside fixture");}
};
using CallbackEvent=std::array<std::uint64_t,73>;
struct Thread final : crt_random_thread61::Services {
    unsigned scenario;GuestAddress record;std::vector<CallbackEvent> events;
    explicit Thread(unsigned which):scenario(which),record(which==1u?0u:TlsRecord){}
    void Observe(std::uint32_t target,GuestMemory& m,Full& s) {
        CallbackEvent event{};const auto snapshot=crt_full_oracle::Snapshot(s);
        event[0]=target;std::copy(snapshot.begin(),snapshot.end(),event.begin()+1);events.push_back(event);
        // An actual mutable callback, including fields absent from typed TLS.
        s.r[8]=0xaabbccdd00000000ull+events.size();s.fpr_bits[7]^=0x18u;
        s.cr1={1,0,0,1};s.cr7={0,1,0,0};m.WriteU32(ErrorState+352u,0xbadu);
    }
    void KeTlsGetValue(GuestMemory& m,Full& s)override {
        Observe(0x830da144u,m,s);s.r[3]=scenario==1u?0u:Getter|1u;
    }
    void KeTlsSetValue(GuestMemory& m,Full& s)override {
        Observe(0x830da154u,m,s);s.r[3]=1u;
    }
    void CallIndirect(GuestAddress target,GuestMemory& m,Full& s)override {
        Observe(target,m,s);
        if(target==Getter)s.r[3]=record;
        else if(target==Setter){record=Address(s.r[4]);s.r[3]=1u;}
        else throw std::runtime_error("unexpected TLS guest target");
    }
    void FatalRuntimeError(GuestMemory&,Full&)override{throw std::runtime_error("fatal path outside random fixture");}
};
struct EnvironmentCase {
    Services stream;HeapBoundary heap;Unused unused;Thread thread;
    EnvironmentCase(test::GuestWindow& window,unsigned scenario):stream(window,Mode::LockedWrite),thread(scenario){}
    crt_random_thread61::Dependencies Deps(){return {{Dependencies(stream),heap,unused},unused,thread};}
};
EnvironmentCase* current=nullptr;
void SeedRandom(test::GuestWindow& window,unsigned scenario) {
    Seed(window,Mode::LockedWrite);auto m=window.Memory();SeedHeap(m);
    m.WriteU32(Environment+336u,0u);m.WriteU32(Environment+256u,ErrorState);
    m.WriteU32(ErrorState+332u,0x1357u);m.WriteU32(ErrorState+352u,0x12345678u);
    m.WriteU32(0x83214d74u,7u);m.WriteU32(0x83214d78u,9u);
    m.WriteU32(0x832d3adcu,Getter|1u);m.WriteU32(0x832d3ae0u,Setter|3u);
    m.WriteU32(TlsRecord+20u,scenario==2u?0xffffffffu:1u);
}
void Check(unsigned scenario) {
    test::GuestWindow original(RandomRegions),recovered(RandomRegions);
    SeedRandom(original,scenario);SeedRandom(recovered,scenario);
    EnvironmentCase expected(original,scenario),actual(recovered,scenario);
    Full initial{};for(unsigned i=0;i<32u;++i){initial.r[i]=0x1122334400000000ull+i;initial.fpr_bits[i]=0x3ff0000000000000ull+i;}
    initial.r[1]=0x8877665500000000ull|Stack;initial.r[13]=0xaabbccdd00000000ull|Environment;
    initial.lr=0xabcdef0181234567ull;initial.ctr=0x5566778899aabbccull;
    initial.cached_fp_control=0x9fc0u;initial.xer_so=1;initial.xer_ca=1;initial.cr0.lt=1;initial.cr6.gt=1;
    PPCContext context{};crt_full_oracle::ToPpc(context,initial);auto state=initial;
    // A direct existing-TLS call exposes the thread reader's own return state;
    // the outer random entry otherwise replaces its r12 in its epilogue.
    const bool direct_record=scenario==3u;
    const auto entry=direct_record?0x822ca048u:0x82bd2c48u;
    current=&expected;
    if(direct_record)__imp__sub_822CA048(context,original.Bytes());
    else __imp__sub_82BD2C48(context,original.Bytes());
    current=nullptr;
    if(!crt_random_thread61::Apply(entry,actual.stream.memory,actual.Deps(),state))throw std::runtime_error("random entry absent");
    const auto before=crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context)),after=crt_full_oracle::Snapshot(state);
    if(before!=after || !original.EqualCommitted(recovered) || expected.thread.events!=actual.thread.events ||
        expected.heap.events!=actual.heap.events || expected.stream.events!=actual.stream.events) {
        std::fprintf(stderr,"random case %u state=%u RAM=%u callbacks=%u heap=%u\n",scenario,before==after,original.EqualCommitted(recovered),expected.thread.events==actual.thread.events,expected.heap.events==actual.heap.events);
        for(unsigned i=0;i<before.size();++i)if(before[i]!=after[i])std::fprintf(stderr," state[%u]=%llX/%llX\n",i,(unsigned long long)before[i],(unsigned long long)after[i]);
        throw std::runtime_error("random Full72/RAM/callback mismatch");
    }
    const auto seed=scenario==2u?0xffffffffu:1u;
    const auto next=std::uint32_t(seed*214013u+2531011u);
    const auto record=actual.thread.record;
    const auto expected_result=direct_record?TlsRecord:((next>>16u)&0x7fffu);
    const auto expected_seed=direct_record?seed:next;
    if(state.r[3]!=expected_result || actual.stream.memory.ReadU32(record+20u)!=expected_seed ||
        actual.stream.memory.ReadU32(ErrorState+352u)!=0x12345678u || state.r[1]!=initial.r[1] || state.lr!=0x81234567u)
        throw std::runtime_error("random independent result/error/frame mismatch");
    if(direct_record && state.r[12]!=0x81234567u)
        throw std::runtime_error("thread record return scratch mismatch");
    if(scenario==1u && (record!=AllocBlock+16u || actual.stream.memory.ReadU32(record)!=0x1357u ||
        actual.stream.memory.ReadU32(record+4u)!=0xffffffffu || actual.stream.memory.ReadU32(record+92u)!=0x83215478u))
        throw std::runtime_error("new thread record initialization mismatch");
    for(unsigned i=29;i<32u;++i)if(state.r[i]!=initial.r[i])throw std::runtime_error("random nonvolatile mismatch");
}
}
void OriginalRandomThread61Lower(std::uint32_t entry,PPCContext& context,std::uint8_t*) {
    using namespace random_thread61_oracle;auto state=crt_full_oracle::FromPpc(context);
    if(entry==0x82b7bed8u)current->thread.FatalRuntimeError(current->stream.memory,state);
    else if(!crt_random_thread61::ApplyAcceptedLower(entry,current->stream.memory,current->Deps(),state))throw std::runtime_error("missing random accepted lower");
    crt_full_oracle::ToPpc(context,state);
}
void OriginalRandomThread61Indirect(std::uint32_t target,PPCContext& context,std::uint8_t*) {
    using namespace random_thread61_oracle;auto state=crt_full_oracle::FromPpc(context);
    current->thread.CallIndirect(target,current->stream.memory,state);crt_full_oracle::ToPpc(context,state);
}
void OriginalRandomThread61Save(PPCContext& context,std::uint8_t*) {
    const auto state=crt_full_oracle::FromPpc(context);
    auto& memory=random_thread61_oracle::current->stream.memory;
    for(unsigned i=29u;i<32u;++i)
        recovery_abi::WriteU64(memory,Address(state.r[1]-16u-8u*(31u-i)),state.r[i]);
    memory.WriteU32(Address(state.r[1]-8u),Address(state.r[12]));
}
void OriginalRandomThread61Restore(PPCContext& context,std::uint8_t*) {
    auto state=crt_full_oracle::FromPpc(context);
    auto& memory=random_thread61_oracle::current->stream.memory;
    for(unsigned i=29u;i<32u;++i)
        state.r[i]=recovery_abi::ReadU64(memory,Address(state.r[1]-16u-8u*(31u-i)));
    state.r[12]=memory.ReadU32(Address(state.r[1]-8u));state.lr=state.r[12];
    crt_full_oracle::ToPpc(context,state);
}
int main() {
    try{for(unsigned scenario=0;scenario<4u;++scenario)random_thread61_oracle::Check(scenario);
        std::puts("PASS crt-random-thread61 4 actual-upper/shared-lower cases");
        std::puts("LIMIT native TLS/indirect guest targets remain mutable services; heap/CRT lowers shared; failed allocation/TLS install, fatal runtime and concurrency unverified");return 0;
    }catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}
}
