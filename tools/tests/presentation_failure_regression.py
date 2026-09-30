"""Compile the production CPU-upload presentation function with fault-injecting API fakes."""
import argparse
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[2]
PRELUDE=r'''
#include "gpu/display_change.h"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>
using gpu::video::DisplayChangeTracker;
using gpu::video::DisplayChangeResult;
using gpu::video::DisplayCompletion;
#define LOG_WARNING(...) ((void)0)
static void Check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
namespace plume {
struct RenderTexture{};
struct RenderCommandSemaphore{};struct Fence{};
struct RenderBufferDesc {uint64_t bytes;static auto UploadBuffer(uint64_t n){return RenderBufferDesc{n};}};
struct RenderTextureDesc {template<class... T> static auto Texture2D(T...){return RenderTextureDesc{};}};
struct RenderBuffer {
    std::vector<uint8_t> bytes;bool fail=false;
    explicit RenderBuffer(size_t n,bool f):bytes(n),fail(f){}
    void* map(){return fail?nullptr:bytes.data();}void unmap(){}
};
enum class RenderBarrierStage{COPY,NONE};enum class RenderTextureLayout{COPY_DEST,PRESENT};
struct RenderTextureBarrier{RenderTextureBarrier(RenderTexture* p,RenderTextureLayout){Check(p,"null texture used");}};
struct RenderBox{template<class... T> RenderBox(T...){}};
struct RenderTextureCopyLocation {
    static int Subresource(RenderTexture*){return 0;}
    template<class... T>static int PlacedFootprint(T...){return 0;}
};
struct RenderCommandList {
    int begins=0,ends=0;
    void begin(){++begins;}void end(){++ends;}
    template<class... T>void barriers(T...){}
    template<class... T>void copyTextureRegion(T...){}
};
}
struct PresentationOptions{};
struct Device {
    int failure=0;
    auto createBuffer(plume::RenderBufferDesc d)->std::unique_ptr<plume::RenderBuffer>{
        if(failure==1)return {};return std::make_unique<plume::RenderBuffer>(size_t(d.bytes),failure==3);
    }
    auto createTexture(plume::RenderTextureDesc)->std::unique_ptr<plume::RenderTexture>{
        if(failure==2)return {};if(failure==4)throw std::runtime_error("injected allocation exception");
        return std::make_unique<plume::RenderTexture>();
    }
} device;
struct SwapChain {
    bool empty=false,failAcquire=false;int acquires=0;
    plume::RenderTexture back;
    bool isEmpty(){return empty;}
    bool acquireTexture(plume::RenderCommandSemaphore*,uint32_t* index){++acquires;*index=0;return !failAcquire;}
    auto getTexture(uint32_t){return &back;}uint32_t getWidth(){return 4;}uint32_t getHeight(){return 4;}
    template<class... T>bool present(T...){return true;}
} swap;
struct Presentation {template<class... T>void Draw(T...){}} presentation;
struct Queue {template<class... T>void executeCommandLists(T...){}} queue;
Device* g_device=&device;SwapChain* g_swapChain=&swap;Presentation* g_presentation=&presentation;Queue* g_queue=&queue;
std::unique_ptr<plume::RenderCommandList> g_commandList=std::make_unique<plume::RenderCommandList>();
std::unique_ptr<plume::RenderCommandSemaphore> g_acquireSemaphore=std::make_unique<plume::RenderCommandSemaphore>();
std::unique_ptr<plume::Fence> g_fence=std::make_unique<plume::Fence>();
std::unique_ptr<plume::RenderBuffer> g_uploadBuffer;
std::unique_ptr<plume::RenderTexture> g_cpuFrame;
uint64_t g_uploadCapacity=0,g_completedPresentCount=0;
uint32_t g_cpuWidth=0,g_cpuHeight=0,g_lastPresentedImage=0;
bool g_available=true,g_initializing=false,g_presentPending=false,g_hasPresentedImage=false;
std::atomic<bool> g_displayFailed{false};
constexpr int kSwapChainFormat=0;
DisplayChangeTracker g_displayChanges;
void WaitForPresentGpu(){}
void RecordPresentedSnapshot(plume::RenderTexture*){}
plume::RenderCommandSemaphore* PresentSemaphore(uint32_t){return g_acquireSemaphore.get();}
'''
TAIL=r'''
void Reset(){device.failure=0;swap={};g_uploadBuffer.reset();g_cpuFrame.reset();g_uploadCapacity=0;
    g_cpuWidth=g_cpuHeight=0;g_commandList->begins=g_commandList->ends=0;g_displayChanges.Reset();}
int main()try{
    const std::vector<uint32_t> pixels(16,0x12345678);
    for(int failure:{1,2,3,4}){
        Reset();device.failure=failure;auto ticket=g_displayChanges.Begin(4,4,0);
        Check(!UploadAndPresentPixels(pixels,4,4,false,ticket,{}),"fault unexpectedly presented");
        Check(g_displayChanges.Query(ticket)==DisplayChangeResult::Failed,"fault left Pending");
        Check(swap.acquires==0 && g_commandList->begins==0,"fault acquired/opened GPU work");
        Check(!g_uploadBuffer && !g_cpuFrame && !g_uploadCapacity && !g_cpuWidth,"fault committed invalid resource dimensions");
    }
    Reset();auto ticket=g_displayChanges.Begin(4,4,0);
    Check(UploadAndPresentPixels(pixels,4,4,false,ticket,{}),"success failed");
    Check(g_displayChanges.Query(ticket)==DisplayChangeResult::Applied,"success not applied");
    Check(swap.acquires==1 && g_commandList->begins==1 && g_commandList->ends==1,"unbalanced submit");
    Reset();g_uploadBuffer=std::make_unique<plume::RenderBuffer>(512,false);g_uploadCapacity=512;
    g_cpuFrame=std::make_unique<plume::RenderTexture>();g_cpuWidth=g_cpuHeight=2;
    auto* oldUpload=g_uploadBuffer.get();auto* oldFrame=g_cpuFrame.get();device.failure=2;
    ticket=g_displayChanges.Begin(4,4,0);
    Check(!UploadAndPresentPixels(pixels,4,4,false,ticket,{}),"resize failure succeeded");
    Check(g_uploadBuffer.get()==oldUpload && g_cpuFrame.get()==oldFrame && g_uploadCapacity==512 && g_cpuWidth==2,"failed resize lost old resources");
    Reset();swap.failAcquire=true;ticket=g_displayChanges.Begin(4,4,0);
    Check(!UploadAndPresentPixels(pixels,4,4,false,ticket,{}) && g_displayChanges.Query(ticket)==DisplayChangeResult::Failed,"acquire failure left Pending");
    Reset();swap.empty=true;ticket=g_displayChanges.Begin(4,4,0);
    Check(!UploadAndPresentPixels(pixels,4,4,false,ticket,{}) && g_displayChanges.Query(ticket)==DisplayChangeResult::Pending,"minimize lost retry transaction");
    Reset();g_presentation=nullptr;device.failure=2;ticket=g_displayChanges.Begin(4,4,0);
    Check(UploadAndPresentPixels(pixels,4,4,false,ticket,{}),"direct-copy path needlessly allocated CPU texture");
    std::cout<<"PASS production presentation faults: buffer/texture/map/exception, state rollback, acquire, minimized retry, direct copy\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
'''
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx',default='clang++');parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--sanitize',action='store_true');args=parser.parse_args()
    out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
    source=(ROOT/'LostOdysseyRecomp/gpu/video.cpp').read_text()
    start='    static bool UploadAndPresentPixels(const std::vector<uint32_t>& pixels, uint32_t width, uint32_t height,\n                                       bool isMenu, uint64_t displayTicket, const PresentationOptions& presentationOptions)\n    {'
    if source.count(start)!=1:raise RuntimeError('production upload definition changed')
    begin=source.index(start);end=source.index('\n#endif',begin)
    cpp=out/'present.cpp';cpp.write_text(PRELUDE+source[begin:end]+TAIL)
    flags=['-std=c++20','-pthread','-I'+str(ROOT/'LostOdysseyRecomp')]
    flags+=['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'] if args.sanitize else ['-O2']
    build=subprocess.run([args.cxx,*flags,str(cpp),'-o',str(out/'present')],capture_output=True,text=True,timeout=120)
    (out/'build.log').write_text(build.stdout+build.stderr)
    if build.returncode:print(build.stderr);return build.returncode
    run=subprocess.run([str(out/'present')],capture_output=True,text=True,timeout=15)
    (out/'run.log').write_text(run.stdout+run.stderr);print(run.stdout+run.stderr,end='');return run.returncode
if __name__=='__main__':raise SystemExit(main())
