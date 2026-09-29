#include <stdafx.h>
#include "gpu/native_mesh.h"
#include <iostream>
#include <random>

extern "C" PPC_FUNC(__imp__sub_823C6860);
extern "C" PPC_FUNC(__imp__sub_827B56B0);
extern uint64_t oracle_sdk_helper_calls;
namespace gpu::native_frontend { bool TrySubmit(PPCContext&, uint8_t*, bool); }
using namespace gpu::native_frontend;
constexpr uint32_t kDevice = 0x100000, kStream = 0x200000, kResource = 0x105000;
static void Check(bool value, const char* message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
static uint32_t Get(const uint8_t* base, uint32_t address)
{
    uint32_t value; std::memcpy(&value,base+address,4);
    return gpu::native_command::GuestWord(value);
}
static void Put(uint8_t* base, uint32_t address, uint32_t value)
{
    value=gpu::native_command::GuestWord(value); std::memcpy(base+address,&value,4);
}
static void Put64(uint8_t* base,uint32_t address,uint64_t value)
{
    Put(base,address,uint32_t(value>>32)); Put(base,address+4,uint32_t(value));
}
struct Result
{
    std::vector<uint32_t> registers = std::vector<uint32_t>(0x5003,0xdeadbeef);
    uint32_t draws = 0, coherencyWaits = 0;
    std::vector<uint32_t> synchronization;
    void Write(uint32_t index,uint32_t value) {
        registers[index]=value;
        if(index==0x2007 || (index>=0x0a2f && index<=0x0a31) ||
           (index>=0x2388 && index<0x23a0) || (index>=0x4900 && index<0x4928)) {
            synchronization.push_back(index); synchronization.push_back(value);
        }
        if(index==0x0a31) registers[index]|=0x80000000u;
    }
    void Wait(uint32_t info,uint32_t address,uint32_t reference,uint32_t mask,uint32_t interval) {
        Check(info==3 && address==0xa31 && reference==0 && mask==0x80000000u && interval==8,
              "only the SDK's fixed coherency wait belongs to this fixture");
        synchronization.insert(synchronization.end(),{0xffffffff,info,address,reference,mask,interval});
        registers[address]&=~0x80000000u; // same completion as CP's shared wait executor
        Check((registers[address]&mask)==reference,"coherency wait predicate"); ++coherencyWaits;
    }
};
static Result ReadLegacy(const uint8_t* base,uint32_t end,bool predicate)
{
    Result result;
    for(uint32_t cursor=kStream+4;cursor<=end;) {
        const auto header=Get(base,cursor);cursor+=4;
        const auto type=header>>30;
        if(type==2)continue;
        const auto count=((header>>16)&0x3fff)+1;
        Check(cursor+count*4-4<=end,"legacy oracle packet bound");
        if(type==0) {
            const auto first=header&0x7fff;
            Check(first+count<=result.registers.size(),"legacy oracle register bound");
            for(uint32_t i=0;i<count;++i)result.Write(first+i,Get(base,cursor+i*4));
        } else if(type==3) {
            const auto opcode=(header>>8)&0x7f;
            Check(opcode==0x22 || opcode==0x3c,"unexpected legacy opcode in accepted SDK path");
            if(opcode==0x3c) {
                Check(count==5 && !(header&1),"stream wait size and unpredicated ordering");
                result.Wait(Get(base,cursor),Get(base,cursor+4),Get(base,cursor+8),Get(base,cursor+12),Get(base,cursor+16));
            } else if(predicate || !(header&1)) {
                const auto initiator=Get(base,cursor+4);
                result.registers[0x21fc]=initiator;
                if(((initiator>>6)&3)==0) {
                    Check(count==4,"indexed oracle packet size");
                    result.registers[0x21fa]=Get(base,cursor+8);
                    result.registers[0x21fb]=Get(base,cursor+12);
                } else Check(count==2,"auto oracle packet size");
                ++result.draws;
            }
        } else Check(false,"unexpected legacy packet type");
        cursor+=count*4;
    }
    return result;
}
static Result ReadNative(const uint8_t* base,uint32_t end,bool predicate)
{
    Check(Get(base,kStream+4)==kMesh,"actual producer did not skip SDK flush");
    const auto* body=reinterpret_cast<const uint32_t*>(base+kStream+8);
    Check(end-kStream==body[0]*4,"native cursor publication");
    DecodedMesh mesh;
    Check(Decode(std::span(body,body[0]-1),mesh),"actual producer payload validation");
    Result result;
    ApplyDeltas(mesh,[&](uint32_t index,uint32_t value){result.Write(index,value);},
        [&](uint32_t control) {
            result.Write(0x2007,control);result.Write(0xa31,0x10000);
            result.Write(0xa2f,0);result.Write(0xa30,4096);
            result.Wait(3,0xa31,0,0x80000000u,8);
        });
    if(predicate) {
        result.registers[0x21fc]=mesh.draw.initiator;
        if(mesh.draw.Indexed()) {
            result.registers[0x21fa]=mesh.draw.dmaBase;
            result.registers[0x21fb]=mesh.draw.dmaSize;
        }
        ++result.draws;
    }
    return result;
}
int main()
{
    Check(meshEnabled,"run oracle with LO_NATIVE_FRONTEND=mesh");
#if defined(_WIN32)
    auto* base=static_cast<uint8_t*>(VirtualAlloc(nullptr,0x100000000ull,MEM_RESERVE,PAGE_READWRITE));
    Check(base!=nullptr,"reserve synthetic guest address space");
    Check(VirtualAlloc(base,0x400000,MEM_COMMIT,PAGE_READWRITE)!=nullptr,"commit synthetic SDK and stream pages");
    Check(VirtualAlloc(base+0x83302000,0x1000,MEM_COMMIT,PAGE_READWRITE)!=nullptr,"commit synthetic current-device page");
#else
#error This bounded SDK oracle currently uses Windows virtual guest pages.
#endif
    Put(base,0x83302a38,kDevice);
    std::mt19937_64 random(0x686056b0);
    uint64_t compared=0;
    for(uint32_t trial=0;trial<128;++trial) {
        std::memset(base+kDevice,0,0x6000);
        for(auto layout:kGroups)
            for(uint32_t i=0;i<layout.fields*layout.wordsPerField;++i)
                Put(base,kDevice+layout.guestOffset+i*4,uint32_t(random()));
        const bool indexed=(trial&1)!=0;
        const bool index32=(trial&2)!=0;
        const uint32_t count=(trial<4 || trial%4==0)?65535u:trial+1;
        DirtyState dirty{random(),random(),random()&~(15ull<<17),random(),random(),0};
        if(trial==0)dirty={};
        Put64(base,kDevice,dirty.vertex);Put64(base,kDevice+8,dirty.pixel);
        Put64(base,kDevice+16,dirty.main);Put64(base,kDevice+24,dirty.fetchRaster);
        Put64(base,kDevice+32,dirty.misc);Put64(base,kDevice+40,0);
        Put(base,kDevice+48,kStream);Put(base,kDevice+52,0x3ffff0);Put(base,kDevice+56,0x3ff000);
        Put(base,kDevice+12428,kResource);
        Put(base,kResource,(index32?0x80000000u:0u)|((trial&3)<<29));
        Put(base,kResource+24,0xa0080040u+(trial%3)*2);
        std::array<uint8_t,0x6000> original;std::memcpy(original.data(),base+kDevice,original.size());
        PPCContext context{};context.r1.u64=0x80000;context.r3.u64=kDevice;context.r4.u64=trial%6+1;
        context.r5.u64=indexed?123:37;context.r6.u64=indexed?3:count;context.r7.u64=count;
        context.lr=0x12345678;
        const auto input=context;
        if(indexed)__imp__sub_823C6860(context,base);else __imp__sub_827B56B0(context,base);
        const auto legacyEnd=Get(base,kDevice+48);
        const auto legacyPass=ReadLegacy(base,legacyEnd,true),legacySkip=ReadLegacy(base,legacyEnd,false);
        std::memcpy(base+kDevice,original.data(),original.size());context=input;
        const auto before=oracle_sdk_helper_calls;
        Check(TrySubmit(context,base,indexed),"qualified ordinary SDK path rejected");
        Check(oracle_sdk_helper_calls==before,"accepted native path called SDK state flush");
        Check(std::memcmp(&context,&input,sizeof(context))==0,"native producer changed guest ABI context");
        const auto nativeEnd=Get(base,kDevice+48);
        const auto nativePass=ReadNative(base,nativeEnd,true),nativeSkip=ReadNative(base,nativeEnd,false);
        if(nativePass.registers!=legacyPass.registers || nativeSkip.registers!=legacySkip.registers) {
            for(size_t i=0;i<legacyPass.registers.size();++i)
                if(nativePass.registers[i]!=legacyPass.registers[i])
                    std::cerr<<"trial "<<trial<<" reg "<<std::hex<<i<<" native "<<nativePass.registers[i]<<" legacy "<<legacyPass.registers[i]<<std::dec<<'\n';
            Check(false,"native state or predication differs from canonical SDK output");
        }
        Check(nativePass.draws==legacyPass.draws && nativeSkip.draws==legacySkip.draws,"draw count and predicate contract");
        Check(nativePass.synchronization==legacyPass.synchronization &&
              nativeSkip.synchronization==legacySkip.synchronization,"stream/coherency/bool ordering differs from SDK");
        Check(nativePass.coherencyWaits==legacyPass.coherencyWaits &&
              nativeSkip.coherencyWaits==legacySkip.coherencyWaits,"predicated draw must not suppress preceding wait");
        for(uint32_t offset=0;offset<40;offset+=4)Check(Get(base,kDevice+offset)==0,"dirty acknowledgement incomplete");
        // Decline a shader-preparation transition before guest mutation.
        Put64(base,kDevice+16,1ull<<17);
        std::array<uint8_t,0x6000> rejected;std::memcpy(rejected.data(),base+kDevice,rejected.size());
        context=input;
        Check(!TrySubmit(context,base,indexed),"shader transition must retain legacy preparation");
        Check(std::memcmp(rejected.data(),base+kDevice,rejected.size())==0 &&
              std::memcmp(&context,&input,sizeof(context))==0,"rejected producer had guest-visible effects");
        ++compared;
    }
    VirtualFree(base,0,MEM_RELEASE);
    std::cout<<"SDK_ORACLE_PASS cases="<<compared<<" predicates=pass,skip stream_wait_order=verified original_helpers="<<oracle_sdk_helper_calls
             <<" native_flush_calls=0 source=canonical_generated_sdk gpu_or_game_validation=false\n";
}
