#!/usr/bin/env python3
"""Compile actual cheats.cpp with synthetic SDL/allocator boundaries; no game/GPU.

Linux-only sparse guest address-space fixture. This verifies the native adapter
against deliberately constructed data, not a real game's live data.
"""
from __future__ import annotations
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile

PREAMBLE = r'''
#include <debug/cheats.h>
#include <debug/fast_forward.h>
#include <host_ui/host_ui.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <thread>
#include <sys/mman.h>
using namespace std::chrono_literals;
unsigned checks=0;
void Check(bool b,const char* message) { ++checks;if(!b) {fprintf(stderr,"FAIL: %s\n",message);abort();} }
uint32_t Load32(const uint8_t* b,uint32_t p) { return (uint32_t(b[p])<<24)|(uint32_t(b[p+1])<<16)|(uint32_t(b[p+2])<<8)|b[p+3]; }
#define PPC_LOAD_U32(p) Load32(base,uint32_t(p))
struct PPCRegister { union {uint64_t u64=0;uint32_t u32;}; };
struct PPCContext {PPCRegister r3,ctr;uint64_t cookie=0;};
constexpr uint32_t PPC_CODE_BASE=0x82000000, PPC_CODE_SIZE=0x01000000;
struct XexLoader {static inline uint32_t s_imageBase=0x82000000,s_imageSize=0x01400000;};
uint32_t returnedPlayData=0x100000;
unsigned getterCalls=0;
void Getter(PPCContext& ctx,uint8_t*) {
    Check(ctx.r3.u32==0x400000 && ctx.ctr.u32==0x82001000,"native getter called with engine and entry");
    ++getterCalls;ctx.r3.u64=returnedPlayData;ctx.cookie=0;ctx.ctr.u64=0;
}
struct MemoryDispatch {
    using Function=void(PPCContext&,uint8_t*);
    Function* FindFunction(uint32_t entry) {return entry==0x82001000 ? Getter:nullptr;}
} g_memory;
struct Allocator {
    struct Region {} virtualRegion;
    struct Allocation {uint32_t address,size;};
    std::vector<Allocation> allocations;
    bool FindAllocation(Region&,uint32_t p,uint32_t& address,uint32_t& size) {
        for(auto a:allocations) if(p>=a.address && uint64_t(p)<uint64_t(a.address)+a.size) {
            address=a.address;size=a.size;return true;
        }
        return false;
    }
} g_pageAllocator;
struct SDL_Window {uint32_t id=1,flags=0;bool alive=true;} window,other{2,0,true};
SDL_Window* focused=&window;
constexpr uint32_t SDL_WINDOW_MINIMIZED=0x40,SDL_WINDOW_HIDDEN=0x8;
SDL_Window* SDL_GetKeyboardFocus() {return focused;}
uint32_t SDL_GetWindowID(SDL_Window* w) {return w->id;}
uint32_t SDL_GetWindowFlags(SDL_Window* w) {return w->flags;}
SDL_Window* SDL_GetWindowFromID(uint32_t id) {return id==window.id && window.alive ? &window : nullptr;}
enum SDL_GameControllerAxis {SDL_CONTROLLER_AXIS_TRIGGERLEFT,SDL_CONTROLLER_AXIS_TRIGGERRIGHT};
struct SDL_GameController {int16_t lt=0,rt=0;bool attached=true;} pads[2];
int joystickCount=1;
int instanceBase=100;
int SDL_NumJoysticks() {return joystickCount;}
int SDL_JoystickGetDeviceInstanceID(int i) {return instanceBase+i;}
SDL_GameController* SDL_GameControllerFromInstanceID(int id) {return &pads[id-instanceBase];}
bool SDL_GameControllerGetAttached(SDL_GameController* c) {return c->attached;}
int16_t SDL_GameControllerGetAxis(SDL_GameController* c,SDL_GameControllerAxis a) {return a==SDL_CONTROLLER_AXIS_TRIGGERLEFT ? c->lt:c->rt;}
namespace settings {bool open=false;bool IsOpen() {return open;} }
namespace debug_menu {bool overlay=false;bool IsOverlayVisible() {return overlay;} }
'''
TEST = r'''
int main() {
    using namespace debug_menu;
    using namespace debug_menu::cheats;
    constexpr size_t AddressSpace=size_t(1)<<32;
    auto* base=static_cast<uint8_t*>(mmap(nullptr,AddressSpace,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE,-1,0));
    Check(base!=MAP_FAILED,"sparse 4 GiB allocation");
    auto put=[&](uint32_t p,uint32_t v) {for(unsigned i=0;i<4;++i)base[p+i]=uint8_t(v>>(24-i*8));};
    constexpr uint32_t play=0x100000,world=0x200000,level=0x300000;
    constexpr uint32_t engine=0x400000,vtable=0x83200000;
    g_pageAllocator.allocations={{play,data::DataSize+8},{world,0x1000},{level,0x1000},{engine,0x1000}};
    put(0x83315FB4,engine);put(engine,vtable);put(vtable+0x160,0x82001000);
    put(0x83318744,world);put(world+0x50,level);put(0x832CB6B4,0);
    PPCContext ctx;ctx.r3.u64=engine;ctx.ctr.u64=0xA5A5;ctx.cookie=0x12345678;
    auto tick=[&] {Tick(ctx,base);Check(ctx.r3.u64==engine && ctx.ctr.u64==0xA5A5 && ctx.cookie==0x12345678,"guest register context fully restored");};
    Memory memory{{base+play+8,data::DataSize}};
    auto val=[&](uint32_t p,uint32_t v) {put(play+8+p,v);};
    for(unsigned i=0;i<5;++i)val(Party+i*4,i);
    for(unsigned n=0;n<9;++n) {
        auto b=n*data::CharacterStride;
        val(MaxHp+b,std::bit_cast<uint32_t>(100.f));val(MaxMp+b,std::bit_cast<uint32_t>(50.f));
    }
    tick();Check(session.Get().available,"getter-returned typed layout bound");
    session.Enable(true);
    Check(session.Queue({Action::SetGold,0,0,12345,session.Get().generation}),"queue gold");
    tick();Check(memory.U32(Gold)==12345,"actual runtime applies on guest tick");
    // Out-of-battle contract and scene-identity cancellation.
    Check(session.Queue({Action::SetGold,0,0,54321,session.Get().generation}),"queue before battle");
    put(0x832CB6B4,2);tick();Check(memory.U32(Gold)==12345 && !session.Get().available,"battle rejects writes");
    put(0x832CB6B4,0);std::this_thread::sleep_for(260ms);tick();
    Check(session.Get().available,"field recovered");
    base[0x831EAD88]=7;Check(session.SetEditor(true),"enable retail flag request");
    tick();Check(base[0x831EAD88]==0 && session.Get().editorApplied,"retail flag applied");
    session.SetEditor(false);tick();Check(base[0x831EAD88]==7 && !session.Get().editorApplied,"retail flag restored");
    session.SetEditor(true);tick();base[0x831EAD88]=9;session.SetEditor(false);tick();
    Check(base[0x831EAD88]==9,"new game-authored retail flag preserved");
    Check(session.Queue({Action::SetGold,0,0,54321,session.Get().generation}),"queue before allocation invalidation");
    g_pageAllocator.allocations[0].size=data::DataSize;tick(); // too short by eight bytes
    Check(!session.Get().available && memory.U32(Gold)==12345,"reject undersized allocation without writes");
    g_pageAllocator.allocations[0].size=data::DataSize+8;
    std::this_thread::sleep_for(260ms);tick();Check(session.Get().available,"allocation restored");
    Check(session.Queue({Action::SetGold,0,0,54321,session.Get().generation}),"queue before root switch");
    returnedPlayData=play+4;tick();Check(!session.Get().available && memory.U32(Gold)==12345,"root change invalidates pending write");
    returnedPlayData=play;std::this_thread::sleep_for(260ms);tick();
    const auto callsBefore=getterCalls;
    Check(session.Queue({Action::SetGold,0,0,999,session.Get().generation}),"queue before invalid getter");
    put(vtable+0x160,0x10);tick();
    Check(getterCalls==callsBefore && !session.Get().available && memory.U32(Gold)==12345,"invalid guest function rejected without call or write");
    put(vtable+0x160,0x82001000);
    // Actual native SDL adapter: each replacement is only an external service.
    fast_forward::Enable(true);PollHostControls();pads[0].lt=32767;PollHostControls();
    Check(fast_forward::GetStatus().active,"held LT boosts");
    focused=nullptr;PollHostControls();Check(!fast_forward::GetStatus().active,"focus loss stops immediately");
    focused=&window;PollHostControls();Check(!fast_forward::GetStatus().active,"held LT on regain is not rearmed");
    pads[0].lt=0;PollHostControls();pads[0].lt=32767;PollHostControls();Check(fast_forward::GetStatus().active,"fresh LT press boosts");
    pads[0].rt=32767;PollHostControls();Check(!fast_forward::GetStatus().active,"LT+RT does not boost");
    pads[0].rt=0;PollHostControls();Check(fast_forward::GetStatus().active,"single LT restores boost");
    settings::open=true;PollHostControls();Check(!fast_forward::GetStatus().active,"settings stops boost");
    settings::open=false;PollHostControls();Check(!fast_forward::GetStatus().active,"settings close requires release");
    pads[0].lt=0;PollHostControls();pads[0].lt=32767;PollHostControls();
    overlay=true;PollHostControls();Check(!fast_forward::GetStatus().active,"F1 stops boost");overlay=false;
    pads[0].lt=0;PollHostControls();pads[0].lt=32767;PollHostControls();
    pads[0].attached=false;PollHostControls();Check(!fast_forward::GetStatus().active,"unplug stops boost");
    pads[0].attached=true;PollHostControls();Check(!fast_forward::GetStatus().active,"replug held LT is not armed");
    pads[0].lt=0;PollHostControls();pads[0].lt=32767;PollHostControls();
    ++instanceBase;PollHostControls();Check(!fast_forward::GetStatus().active,"new instance cannot inherit armed trigger");
    pads[0].lt=0;PollHostControls();pads[0].lt=32767;PollHostControls();
    window.flags=SDL_WINDOW_MINIMIZED;PollHostControls();Check(!fast_forward::GetStatus().active,"minimized stops boost");window.flags=0;
    focused=&other;PollHostControls();Check(!fast_forward::GetStatus().active,"other SDL window not eligible");
    focused=&window;pads[0].lt=0;PollHostControls();pads[0].lt=32767;PollHostControls();
    Check(fast_forward::GetStatus().active,"active before lease timeout");
    std::this_thread::sleep_for(270ms);Check(!fast_forward::GetStatus().active,"stalled SDL pump expires boost");
    host_ui::SetGamePaused(true);auto frozen=host_ui::GetActiveGameTimeNs();std::this_thread::sleep_for(2ms);
    Check(frozen==host_ui::GetActiveGameTimeNs(),"shared guest clock freezes with overlay pause");
    host_ui::SetGamePaused(false);Check(host_ui::GetActiveGameTimeNs()>=frozen,"resume clock monotonic");
    host_ui::RequestStop();PollHostControls();Check(!fast_forward::GetStatus().active,"shutdown stops boost");
    munmap(base,AddressSpace);
    printf("PASS: %u actual adapter checks; synthetic allocations, retail flag lifecycle, SDL focus/menu/hotplug/lease\n",checks);
}
'''

def main() -> None:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[3])
    parser.add_argument('--cxx',default='clang++')
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--out',type=Path)
    args=parser.parse_args()
    runtime=args.root.resolve()/'LostOdysseyRecomp'
    source=(runtime/'debug/cheats.cpp').read_text(encoding='utf-8')
    if source.count('void Tick(PPCContext& ctx, uint8_t* base)')!=1 or source.count('void PollHostControls()')!=1:
        raise RuntimeError('Production adapter boundaries changed')
    body=re.sub(r'^#include[^\n]*\n','',source,flags=re.M)
    output=args.out or Path(tempfile.mkdtemp(prefix='lo-cheats-adapter-'))
    output.mkdir(parents=True,exist_ok=True)
    cpp=output/'adapter.cpp';binary=output/'adapter'
    cpp.write_text(PREAMBLE+body+TEST,encoding='utf-8')
    flags=['-std=c++20','-O1','-g','-pthread','-Wall','-Wextra','-Werror','-I'+str(runtime)]
    if args.sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
    subprocess.run([args.cxx,*flags,str(cpp),'-o',str(binary)],check=True,timeout=90)
    env={k:v for k,v in os.environ.items() if not k.startswith('LO_')}
    env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1'
    subprocess.run([str(binary)],check=True,timeout=30,env=env)

if __name__=='__main__':main()
