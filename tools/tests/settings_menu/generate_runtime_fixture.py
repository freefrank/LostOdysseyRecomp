#!/usr/bin/env python3
"""Compile the entire production menu.cpp against synthetic service boundaries.

No game images, GPU, SDL window, user settings or save data are accessed.
Only direct includes are substituted; input/publish/dispatch bodies are unchanged.
"""
from pathlib import Path
import argparse
import re

PREAMBLE = r'''
#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <settings/graphics_menu.h>
#include <settings/menu_render.h>
#include <settings/menu_assets.h>
#include <settings/translations.h>
#include <settings/restart.h>
#include <settings/language_selection.h>
#include <gpu/frame_plan.h>
#include <gpu/display_change.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#define LO_GPU_PLUME 1
#define LOG_INFO(...) ((void)0)
struct Reg { union { uint64_t u64=0; uint32_t u32; }; };
struct PPCContext { Reg r3,r4; uint64_t lr=0, cookie=1234; };
#define PPC_FUNC(name) void name(PPCContext& ctx, uint8_t* base)
uint32_t Load32(const uint8_t* base,uint32_t p) { return uint32_t(base[p])<<24|uint32_t(base[p+1])<<16|uint32_t(base[p+2])<<8|base[p+3]; }
void Store32(uint8_t* base,uint32_t p,uint32_t v) { for(int i=0;i<4;++i)base[p+i]=uint8_t(v>>(24-i*8)); }
#define PPC_LOAD_U32(p) Load32(base,uint32_t(p))
#define PPC_STORE_U32(p,v) Store32(base,uint32_t(p),uint32_t(v))
#define PPC_LOAD_U16(p) (uint16_t(base[uint32_t(p)])<<8|base[uint32_t(p)+1])
#define PPC_LOAD_U8(p) base[uint32_t(p)]
unsigned checks=0,saves=0,applies=0,closes=0,consents=0;
void Check(bool v,const char* message) { ++checks;if(!v){fprintf(stderr,"FAIL: %s\n",message);std::exit(1);} }
struct FileSystem { static std::filesystem::path GetGameRoot(){return {};} };
namespace settings {
Config savedConfig;
Config GetConfig(){return savedConfig;}
bool SaveConfig(const Config& c){++saves;savedConfig=c;return true;}
void PreviewConfig(const Config& c){savedConfig=c;}
uint32_t GameLanguage(){return 1;}
namespace language {
void TraceLookup(uint8_t*,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,bool){}
void TraceConfig(uint8_t*,uint32_t,const char*){}
}
namespace menu_assets { std::shared_ptr<const Assets> Cached(const std::filesystem::path&,uint32_t) noexcept {return {};} }
}
namespace gpu::video {
void* GetDevice(){return reinterpret_cast<void*>(1);}
std::optional<gpu::backend::Backend> SelectedBackend(){return gpu::backend::Backend::Vulkan;}
DisplayChangeResult QueryDisplayChange(uint64_t){return DisplayChangeResult::Applied;}
uint64_t BeginDisplayChange(const settings::Config&){return 1;}
bool DisplayModeFailed(){return false;}
bool WindowModeOverridden(){return false;}
}
namespace gpu::frame_plan {
DlssEffectSnapshot CurrentDlssEffect(){return {};}
std::optional<UpscalerExecutionObservation> CurrentUpscalerExecution(){return {};}
}
namespace gpu::taa_collection {
const wchar_t* Label(uint32_t){return L"Collection";}
const wchar_t* Message(uint32_t){return L"Message";}
int Consent(){return 0;} bool Enabled(){return false;}
bool SetConsent(bool){++consents;return true;}
}
'''
TEST = r'''
extern "C" PPC_FUNC(__imp__sub_822F19B0) {}
extern "C" PPC_FUNC(__imp__sub_82481BE8) {ctx.r3.u64=0;}
extern "C" PPC_FUNC(__imp__sub_82870E38) {++applies;ctx.cookie=0;}
extern "C" PPC_FUNC(__imp__sub_828710A0) {}
extern "C" PPC_FUNC(__imp__sub_82889E50) {++closes;}
int main(int argc, char** argv) {
    constexpr uint32_t Menu=0x10000,ConfigData=0x21000;
    const size_t size=size_t(1)<<32;
#ifdef _WIN32
    auto* base=static_cast<uint8_t*>(VirtualAlloc(nullptr,size,MEM_RESERVE,PAGE_NOACCESS));
    Check(base!=nullptr,"reserve synthetic guest space");
    for(uint32_t p:{0x10000u,0x20000u,0x83260000u,0x83360000u})
        Check(VirtualAlloc(base+p,0x10000,MEM_COMMIT,PAGE_READWRITE)!=nullptr,"commit fixture pages");
#else
    auto* base=static_cast<uint8_t*>(mmap(nullptr,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE,-1,0));
    Check(base!=MAP_FAILED,"sparse synthetic guest space");
#endif
    PPC_STORE_U32(0x8326A068,0x20000);PPC_STORE_U32(0x20004,0x20100);PPC_STORE_U32(0x20118,ConfigData);
    PPC_STORE_U32(Menu+4,4);
    auto tick=[&](uint16_t input=0){settings::pending=input;PPCContext ctx;ctx.r3.u64=Menu;
        sub_822F19B0(ctx,base);Check(ctx.r3.u32==Menu && ctx.cookie==1234,"guest helper calls preserve context");};
    tick();Check(settings::active,"menu opens");
    uint16_t neutral=0;settings::FilterInput(neutral,0,0);
    // A must not cycle ANY ordinary setting, not just the new graphics choice.
    for(int tab=0;tab<4;++tab){
        settings::tab=tab;
        const int count=tab==0?7:tab==1?3:tab==2?int(settings::GraphicsRow::Count):3;
        for(int row=0;row<count;++row){
            if(settings::graphics_menu::IsAction(tab,row))continue;
            settings::row=row;auto before=settings::edit;uint32_t words[8];
            for(int i=0;i<8;++i)words[i]=PPC_LOAD_U32(ConfigData+i*4);
            auto oldSaves=saves,oldApplies=applies;
            tick(0x1000);
            Check(settings::edit==before && oldSaves==saves && oldApplies==applies,"confirm does not modify a choice");
            for(int i=0;i<8;++i)Check(words[i]==PPC_LOAD_U32(ConfigData+i*4),"A leaves guest settings unchanged");
        }
    }
    settings::tab=0;settings::row=0;tick(8);
    Check(PPC_LOAD_U32(ConfigData)==1 && applies==1,"right still applies gameplay setting");
    using settings::GraphicsRow;using gpu::upscaling::Upscaler;
    settings::tab=2;settings::row=int(GraphicsRow::AntiAliasing);settings::edit={};
    settings::edit.dlssQuality=gpu::upscaling::DlssQuality::Dlaa;
    settings::edit.fsrQuality=gpu::upscaling::FsrQuality::Balanced;
    for(unsigned i=1;i<=6;++i){
        tick(8);Check(settings::graphics_menu::AaChoice(settings::edit)==i%6,"unified AA/provider cycle");
        Check(settings::snapshot.rows[int(GraphicsRow::AntiAliasing)].selectedChoice==int(i%6),"published combined choice");
        Check(settings::snapshot.rows.back().name==L"Save graphics settings","Save is last");
        Check(settings::snapshot.rows[int(GraphicsRow::DlssQuality)].hidden==(i%6<4),"quality visibility");
        Check(settings::snapshot.rows[int(GraphicsRow::FsrSharpness)].hidden==(i%6!=5),"FSR sharpness visibility");
    }
    tick(4);Check(settings::edit.upscaler==Upscaler::Fsr,"left from Off selects FSR");
    Check(settings::edit.dlssQuality==gpu::upscaling::DlssQuality::Dlaa &&
          settings::edit.fsrQuality==gpu::upscaling::FsrQuality::Balanced,"provider-specific qualities preserved");
    settings::row=int(GraphicsRow::DlssQuality);tick(2);
    Check(settings::row==int(GraphicsRow::FsrSharpness) && settings::snapshot.scroll==0,"sharpness directly follows quality");
    settings::edit.fsrSharpnessPercent=0;tick(4);Check(!settings::edit.fsrSharpnessPercent,"sharpness lower bound");
    tick(8);Check(settings::edit.fsrSharpnessPercent==1,"sharpness increments");
    settings::edit.fsrSharpnessPercent=100;tick(8);Check(settings::edit.fsrSharpnessPercent==100,"sharpness upper bound");
    settings::edit.upscaler=Upscaler::Off;settings::row=int(GraphicsRow::AntiAliasing);tick(2);
    Check(settings::row==int(GraphicsRow::AnisotropicFiltering),"navigation skips hidden provider rows to AF");
    tick(1);Check(settings::row==int(GraphicsRow::AntiAliasing),"reverse navigation skips hidden rows");
    // AF changes only after Save, does not require a restart, and survives all levels.
    settings::row=int(GraphicsRow::AnisotropicFiltering);settings::edit.anisotropicFiltering=0;
    const auto afSaved=settings::GetConfig();
    for(uint32_t level:{2u,4u,8u,16u,0u}) {
        tick(8);Check(settings::edit.anisotropicFiltering==level,"AF cycles forward");
        Check(settings::GetConfig().anisotropicFiltering==afSaved.anisotropicFiltering,"unsaved AF not applied");
    }
    tick(4);Check(settings::edit.anisotropicFiltering==16,"AF cycles backward");
    auto afAfter=afSaved;afAfter.anisotropicFiltering=16;
    Check(!settings::restart::Required(afSaved,afAfter),"AF needs no restart");
    settings::edit=afAfter;settings::row=int(GraphicsRow::Save);tick(0x1000);
    Check(settings::GetConfig().anisotropicFiltering==16,"Save applies AF");
    settings::savedConfig=afSaved;settings::edit=afSaved;
    settings::edit.upscaler=Upscaler::Fsr;settings::row=int(GraphicsRow::AntiAliasing);tick();
    auto click=[&](int row,float x,bool reverse){
        int visible=0;for(int i=0;i<row;++i)visible+=!settings::snapshot.rows[i].hidden;
        settings::PointerClick(x,150.f+43.f*(visible-settings::snapshot.scroll)+12,reverse);
        tick();
    };
    click(int(GraphicsRow::AntiAliasing),420,false);
    Check(settings::edit.upscaler==Upscaler::Dlss,"left arrow mouse cell adjusts left");
    click(int(GraphicsRow::AntiAliasing),1000,false);
    Check(settings::edit.upscaler==Upscaler::Fsr,"right mouse cell adjusts right");
    click(int(GraphicsRow::AntiAliasing),900,true);
    Check(settings::edit.upscaler==Upscaler::Dlss,"right-click reverses choice");
    auto oldSaves=saves;
    tick(0x1010);Check(settings::row==int(GraphicsRow::Save) && saves==oldSaves,"Start+A focuses Save without saving");
    tick(0x1000);Check(saves==oldSaves+1,"A on Save still confirms");
    // No changed display or backend in subsequent mouse Save.
    settings::restartPrompt=false;settings::savedRestartPrompt=false;settings::displayTicket=0;
    tick();oldSaves=saves;click(int(GraphicsRow::Save),700,false);
    Check(saves==oldSaves+1,"mouse Save stays a confirm action");
    settings::restartPrompt=false;settings::savedRestartPrompt=false;settings::displayTicket=0;
    settings::tab=2;settings::row=0;settings::waitForRelease=false;
    uint16_t input=0;settings::FilterInput(input,0,0);
    input=2;Check(settings::FilterInput(input,0,0) && input==0,"input is consumed");
    Check(settings::pending.load()==2,"fresh navigation enqueues immediately without debounce wait");
    auto edge=settings::pending.exchange(0);tick(edge);Check(settings::row==1,"first following guest tick updates focus");
    input=2;settings::FilterInput(input,0,0);Check(!settings::pending.load(),"held input does not repeat immediately");
    input=0;settings::FilterInput(input,0,0);input=2;settings::FilterInput(input,0,0);
    Check(settings::pending.load()==2,"quick release/repress not swallowed by repeat timer");settings::pending=0;
    input=0;settings::FilterInput(input,0,0);settings::swapConfirm=true;
    input=0x2000;settings::FilterInput(input,0,0);Check(settings::pending.exchange(0)==0x1000,"swapped B is confirm");
    input=0;settings::FilterInput(input,0,0);input=0x1000;settings::FilterInput(input,0,0);
    Check(settings::pending.exchange(0)==0x2000,"swapped A is back");settings::swapConfirm=false;
    settings::tab=3;settings::row=4;tick(0x1000);Check(!settings::collectionPrompt && !consents,"A does not toggle collection");
    tick(8);Check(settings::collectionPrompt,"right enables collection confirmation");
    settings::collectionChoice=1;tick(0x1000);Check(!settings::collectionPrompt && consents==1,"A still confirms modal dialog");
    if (argc > 1) {
        std::filesystem::create_directories(argv[1]);
        for (uint32_t language : {0u,4u}) {
            settings::tab=2;settings::row=int(GraphicsRow::FsrSharpness);
            settings::edit.upscaler=Upscaler::Fsr;settings::edit.uiLanguage=language;
            settings::edit.fsrSharpnessPercent=50;tick();
            std::vector<uint32_t> pixels;
            Check(settings::RasterizeMenu(settings::snapshot,1280,720,pixels),"actual published graphics snapshot renders");
            std::ofstream f(std::filesystem::path(argv[1])/(language?"zh.ppm":"en.ppm"),std::ios::binary);
            f << "P6\n1280 720\n255\n";
            for(auto c:pixels){char rgb[]={char(c),char(c>>8),char(c>>16)};f.write(rgb,3);}
        }
    }
    printf("PASS: %u actual menu hook/input/snapshot checks (synthetic services, no game)\n",checks);
#ifdef _WIN32
    VirtualFree(base,0,MEM_RELEASE);
#else
    munmap(base,size);
#endif
}
'''

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[3])
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    source=(args.root/'LostOdysseyRecomp/settings/menu.cpp').read_text(encoding='utf-8')
    body=re.sub(r'^#include[^\n]*\n','',source,flags=re.M)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(PREAMBLE+body+TEST,encoding='utf-8')

if __name__=='__main__':main()
