#include <debug/cheats.h>
#include <debug/fast_forward.h>
#include <cassert>
#include <cstdio>
#include <limits>
#include <random>
#include <thread>
using namespace debug_menu::cheats;
unsigned checks=0;
void Check(bool ok) { ++checks; if(!ok) { std::fprintf(stderr,"FAIL check %u\n",checks); std::abort(); } }
void Put(Memory m,uint32_t p,uint32_t v,unsigned bytes=4) { const Write w{p,v,bytes}; Commit(m,{&w,1}); }
void Float(Memory m,uint32_t p,float v) { Put(m,p,std::bit_cast<uint32_t>(v)); }
std::vector<uint8_t> Fixture() {
    std::vector<uint8_t> bytes(data::DataSize);Memory m{bytes};
    Put(m,Gold,1000);
    for(unsigned i=0;i<5;++i)Put(m,Party+i*4,i);
    for(unsigned i=0;i<9;++i) {
        const auto b=i*data::CharacterStride;
        Float(m,Hp+b,10);Float(m,MaxHp+b,100+i);Float(m,Mp+b,5);Float(m,MaxMp+b,50+i);Float(m,Exp+b,42);
        Put(m,AccessoryCount+b,2,1);
    }
    for(const auto& e:data::Items)Float(m,e.value,7);
    for(const auto& e:data::Materials)Float(m,e.value,9);
    return bytes;
}
void Core() {
    auto bytes=Fixture();Memory m{bytes};Check(Validate(m));
    auto before=bytes;
    Check(!Plan(m,{Action::Item,0,UINT32_MAX,99}));Check(bytes==before);
    for(unsigned c=0;c<9;++c) {
        auto plan=Plan(m,{Action::Heal,c});Check(plan && plan->size()==2);Commit(m,*plan);
        Check(m.F32(Hp+c*data::CharacterStride)==100+c);Check(m.F32(Mp+c*data::CharacterStride)==50+c);
    }
    for(auto action:{Action::AllItems,Action::AllMaterials}) {
        auto plan=Plan(m,{action,0,0,99});Check(bool(plan));Commit(m,*plan);
        const auto list=action==Action::AllItems ? std::span<const data::Entry>(data::Items):std::span<const data::Entry>(data::Materials);
        for(auto e:list) { Check(m.F32(e.value)==99.0f);Check(m.U32(e.value)==0x42C60000u); }
    }
    auto plan=Plan(m,{Action::SetWeapon,3,0,data::Weapons[0].value});Check(bool(plan));
    const auto off=Weapon+3*data::CharacterStride;Put(m,off+2,0xAABB,2);Commit(m,*plan);
    Check(m.U16(off)==data::Weapons[0].value);Check(m.U16(off+2)==0xAABB);
    Check(!Plan(m,{Action::SetWeapon,3,0,0xDEAD}));
    Check(!Plan(m,{Action::SetAccessory,3,7,data::Accessories[0].value}));
    Check(!Plan(m,{Action::SetExp,9,0,99}));Check(!Plan(m,{Action::SetExp,0,0,100}));
    auto old=bytes;
    for(unsigned i=0;i<5000;++i) {
        std::mt19937 gen(i);
        Request r{Action(gen()%64),unsigned(gen()),unsigned(gen()),unsigned(gen())};
        auto p=Plan(m,r);
        if(p)for(const auto& w:*p)Check(uint64_t(w.offset)+w.bytes<=data::DataSize);
    }
    Check(bytes==old);
    for(unsigned c=0;c<9;++c) {
        const auto b=c*data::CharacterStride;
        for(const auto& e:data::LearnedSkills) { Put(m,b+e.value,5,1);Put(m,b+e.value+1,0xCD,1); }
        plan=Plan(m,{Action::LearnSupportedSkills,c});Check(plan && plan->size()==161);Commit(m,*plan);
        for(const auto& e:data::LearnedSkills) { Check(m.U8(b+e.value)==0x85);Check(m.U8(b+e.value+1)==0xCD); }
    }
    Put(m,Party,0);Put(m,Party+4,1);
    plan=Plan(m,{Action::SetFormation,0,1,0});Check(bool(plan));Commit(m,*plan);
    Check(m.U32(Party)==1 && m.U32(Party+4)==0);Check(Validate(m));
    old=bytes;Float(m,MaxHp,std::numeric_limits<float>::quiet_NaN());Check(!Validate(m));Check(!Plan(m,{Action::SetGold,0,0,5}));bytes=old;
    for(size_t size:{size_t(0),size_t(4),size_t(0x190),size_t(data::DataSize-1)})Check(!Validate(Memory{std::span(bytes).first(size)}));
    Session s;const Context ctx{0x100000,0x200000,0x300000};
    s.Tick(ctx,m,true);Check(s.Get().available && !s.Get().enabled);
    auto r=Request{Action::AddGold,0,0,100,s.Get().generation};Check(!s.Queue(r));
    s.Enable(true);Check(s.Queue(r));Check(!s.Queue(r));auto gold=m.U32(Gold);s.Tick(ctx,m,true);Check(m.U32(Gold)==gold+100);Check(s.Get().result==Result::Applied);
    Check(s.Queue(r));old=bytes;s.Tick({0x100000,0x200001,0x300000},m,true);Check(bytes==old);Check(s.Get().result==Result::Cancelled);
    r.generation=s.Get().generation;Check(s.Queue(r));s.Enable(false);s.Tick({0x100000,0x200001,0x300000},m,true);Check(bytes==old);
    s.Enable(true);r.generation=s.Get().generation;Check(s.Queue(r));s.Tick(ctx,m,false);Check(!s.Get().available);Check(bytes==old);
    Session concurrent;concurrent.Tick(ctx,m,true);
    std::thread reader([&]{for(int i=0;i<5000;++i)(void)concurrent.Get();});
    std::thread ui([&]{for(int i=0;i<5000;++i)concurrent.Enable(i%2==0);});
    for(int i=0;i<5000;++i)concurrent.Tick(ctx,m,true);
    reader.join();ui.join();
}
void Speed() {
    using namespace debug_menu::fast_forward;
    constexpr uint64_t ms=1000000;
    Control c;Check(c.clock.Read(100*ms)==100*ms);
    c.Sample(255,0,true,110*ms);Check(c.clock.rate==1);
    c.Enable(true,120*ms);c.Sample(255,0,true,130*ms);Check(c.clock.rate==1);
    c.Sample(0,0,true,140*ms);c.Sample(255,0,true,150*ms);
    Check(c.clock.Read(200*ms)==250*ms);
    c.Sample(0,0,true,210*ms);Check(c.clock.Read(220*ms)==280*ms);
    c.SetRate(8,220*ms);c.Sample(255,0,true,230*ms);
    Check(c.clock.Read(250*ms)==450*ms);
    c.Sample(255,255,true,260*ms);Check(c.clock.rate==1);
    Check(c.clock.Read(270*ms)==540*ms);
    c.Sample(255,0,false,280*ms);c.Sample(255,0,true,290*ms);Check(c.clock.rate==1);
    c.Sample(0,0,true,300*ms);c.Sample(255,0,true,310*ms);c.Pause(true,320*ms);
    const auto paused=c.clock.Read(320*ms);Check(c.clock.Read(1000*ms)==paused);
    c.Pause(false,1000*ms);c.Sample(255,0,true,1010*ms);Check(c.clock.rate==1);
    Check(c.clock.Read(1020*ms)==paused+20*ms);
    c.Sample(0,0,true,1030*ms);c.Sample(255,0,true,1040*ms);
    const auto start=c.clock.Read(1040*ms);
    Check(c.clock.Read(2040*ms)==start+250*ms*8+750*ms);Check(c.clock.rate==1);
    c.SetRate(0,2040*ms);Check(c.multiplier==8);c.SetRate(99,2040*ms);Check(c.multiplier==8);
    const auto value=c.clock.Read(2100*ms);Check(c.clock.Read(2000*ms)==value);
    Clock overflow;overflow.Read(UINT64_MAX-100);overflow.rate=8;overflow.lease=UINT64_MAX;
    Check(overflow.Read(UINT64_MAX)==UINT64_MAX);
    Control d;d.Enable(true,0);d.Sample(0,0,true,1);d.Sample(63,0,true,2);Check(d.clock.rate==1);
    d.Sample(64,0,true,3);Check(d.clock.rate==2);d.Sample(40,0,true,4);Check(d.clock.rate==2);
    d.Sample(32,0,true,5);Check(d.clock.rate==1);
    for(unsigned rate:Rates) {
        Control t;t.Enable(true,0);t.SetRate(rate,0);t.Sample(0,0,true,1);t.Sample(255,0,true,2);
        auto a=t.clock.Read(2);Check(t.clock.Read(1002)-a==1000*rate);
        t.Enable(false,1002);auto b=t.clock.Read(1002);Check(t.clock.Read(2002)-b==1000);
    }
}
int main() { Core();Speed();std::printf("PASS: %u core/clock checks; typed writes, no partial invalid writes, scene cancellation, queue concurrency, LT lifecycle\n",checks); }
