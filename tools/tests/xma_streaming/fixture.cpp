// Exercises the entire production XMA translation unit. FFmpeg is replaced
// here only to compare its input packets byte-for-byte with the source frames.
#include "../../../LostOdysseyRecomp/apu/xma.cpp"
#include <cstdio>
#include <random>
#include <stdexcept>
#if LO_COUNT_CALLS
static void* packetFunction=nullptr;
static bool countPackets=false;
static unsigned packetCalls=0;
extern "C" __attribute__((no_instrument_function))
void __cyg_profile_func_enter(void* function,void*) {
    if(countPackets && function==packetFunction) ++packetCalls;
}
extern "C" __attribute__((no_instrument_function))
void __cyg_profile_func_exit(void*,void*) {}
#endif
namespace {
using namespace apu::xma;
unsigned checks=0;
void Check(bool ok,const char* why) { ++checks; if(!ok) throw std::runtime_error(why); }
void WriteBit(uint8_t* p,uint32_t bit,bool value) {
    const uint8_t mask=uint8_t(1u<<(7-bit%8));
    p[bit/8]=uint8_t((p[bit/8]&~mask)|(value?mask:0));
}
void WriteBits(uint8_t* p,uint32_t offset,uint32_t count,uint32_t value) {
    for(uint32_t i=0;i<count;++i) WriteBit(p,offset+i,(value>>(count-1-i))&1);
}
struct Stream {
    std::vector<std::vector<uint8_t>> packets,frames;
    explicit Stream(const std::vector<uint32_t>& lengths) {
        constexpr uint32_t payload=16352;
        uint32_t total=0;
        for(auto n:lengths) total+=n;
        packets.resize((total+payload-1)/payload,std::vector<uint8_t>(2048,0xff));
        std::vector<uint32_t> first(packets.size(),0x7fff),count(packets.size());
        uint32_t cursor=0;
        for(unsigned f=0;f<lengths.size();++f) {
            const auto len=lengths[f];
            std::vector<uint8_t> frame(1+(len+7)/8,0);
            frame[0]=uint8_t((((len+7)/8)*8-len)<<2);
            for(uint32_t b=0;b<len;++b)
                WriteBit(frame.data()+1,b,((b*19+f*11)>>2)&1);
            WriteBits(frame.data()+1,0,15,len);
            const bool more=f+1<lengths.size() && cursor/payload==(cursor+len)/payload;
            WriteBit(frame.data()+1,len-1,more);
            first[cursor/payload]=std::min(first[cursor/payload],cursor%payload);
            ++count[cursor/payload];
            for(uint32_t b=0;b<len;++b) {
                const uint32_t at=cursor+b;
                WriteBit(packets[at/payload].data(),32+at%payload,ReadBits(frame.data()+1,b,1));
            }
            frames.push_back(std::move(frame)); cursor+=len;
        }
        for(unsigned p=0;p<packets.size();++p) {
            WriteBits(packets[p].data(),0,6,count[p]);
            WriteBits(packets[p].data(),6,15,first[p]);
            packets[p][3]=0;
        }
    }
};
void Run(const std::vector<uint32_t>& lengths,unsigned split,unsigned lanes,bool delayed) {
    Stream s(lengths);
    std::vector<uint8_t> input;
    // Each non-selected lane deliberately contains unrelated frame data.
    for(unsigned p=0;p<s.packets.size();++p) {
        s.packets[p][3]=uint8_t(lanes-1);
        input.insert(input.end(),s.packets[p].begin(),s.packets[p].end());
        for(unsigned l=1;l<lanes;++l) input.insert(input.end(),2048,uint8_t(l*37));
    }
    const unsigned total=unsigned(input.size()/2048);
    split=std::min(split,total);
    if(!split) split=total;
    Check(input.size()<0x60000,"fixture input fits");
    std::copy_n(input.begin(),split*2048,g_memory.bytes.begin()+0x10000);
    std::copy(input.begin()+split*2048,input.end(),g_memory.bytes.begin()+0x80000);
    ContextData d{};
    ContextData::SetBits(d.d[0],0,12,split); d.setInput0Valid(true);
    ContextData::SetBits(d.d[1],0,12,total-split); d.setInput1Valid(total>split&&!delayed);
    ContextData::SetBits(d.d[1],27,2,3); ContextData::SetBits(d.d[1],29,1,1);
    d.d[5]=0x10000; d.d[6]=0x80000; d.setInputReadOffset(32);
    Context ctx;
    sentFrames.clear();
    bool refilled=!delayed||total==split;
    for(unsigned guard=0;guard<lengths.size()+16;++guard) {
        ProcessPacket(ctx,d);
        ctx.remainingSubframes=0;
        if(!d.inputValid(d.currentBuffer()) && !refilled) {
            d.setInput1Valid(true); refilled=true;
        }
        if(!d.anyInputValid()) break;
    }
    if(sentFrames!=s.frames) {
        std::fprintf(stderr,"packet mismatch: split=%u lanes=%u delayed=%u expected=%zu actual=%zu lengths=",
                     split,lanes,delayed,s.frames.size(),sentFrames.size());
        for(auto n:lengths) std::fprintf(stderr,"%u,",n);
        std::fprintf(stderr,"\n");
        Check(false,"production assembler changed frame bits or stream selection");
    }
    Check(true,"bit-exact frames");
    ctx.Reset();
}
void WaitingForRefill() {
    g_arrayGuest=0x1000;
    ContextData d{};
    ContextData::SetBits(d.d[0],22,5,8);
    ContextData::SetBits(d.d[1],29,1,1);
    d.setOutputValid(true);
    d.setInput0Valid(false); d.setInput1Valid(true);
    d.d[7]=0x40000; d.setInputReadOffset(0);
    d.Store(ContextHost(0));
    auto& ctx=g_contexts[0];
    ctx.allocated=true; ctx.enabled=true;
    sentFrames.clear();
#if LO_COUNT_CALLS
    packetFunction=reinterpret_cast<void*>(&ProcessPacket);
    packetCalls=0; countPackets=true;
#endif
    Work(0);
#if LO_COUNT_CALLS
    countPackets=false;
    Check(packetCalls==1,"waiting input spun under the XMA mutex instead of yielding");
#endif
    d.Load(ContextHost(0));
    Check(sentFrames.empty() && d.input1Valid() && d.outputValid(),
          "waiting for refill consumed or invalidated an unrelated buffer");
    // Pending PCM must still drain before the worker waits for more input.
    ctx.remainingSubframes=4; ctx.pcmOffset=0; ctx.pcm.fill(0x53);
    ctx.enabled=true;
    Work(0);
    d.Load(ContextHost(0));
    Check(!ctx.remainingSubframes && d.outputWriteOffset()==0 && !d.outputValid(),
          "pending stereo frame failed to fill the eight-block output ring");
    Check(std::all_of(g_memory.bytes.begin()+0x40000,g_memory.bytes.begin()+0x40800,
                     [](uint8_t v){return v==0x53;}),"pending PCM changed while waiting for input");
    // Preserve command retirement when neither input is valid (the runtime's
    // existing no-input kick regression relies on this completion signal).
    d.setInput1Valid(false); d.setOutputValid(true); d.Store(ContextHost(0));
    ctx.enabled=true;
    Work(0); d.Load(ContextHost(0));
    Check(!d.outputValid(),"an exhausted voice failed to retire its output");
    ctx.Reset(); ctx.allocated=false;
}
}
int main() {
    WaitingForRefill();
    // A length header may straddle a packet (including just one bit before it).
    for(unsigned tail=1;tail<24;++tail)
        for(unsigned split=1;split<5;++split)
            for(unsigned lanes=1;lanes<=3;++lanes)
                for(bool delayed:{false,true})
                    Run({16352-tail,1000,700,32700,600},split,lanes,delayed);
    std::mt19937 rng(645);
    for(unsigned n=0;n<200;++n) {
        std::vector<uint32_t> lengths;
        for(unsigned f=0;f<20;++f) lengths.push_back(600+rng()%7000);
        Run(lengths,1+rng()%8,1+rng()%3,n%2);
    }
    std::printf("PASS: %u XMA streaming checks\n",checks);
}
