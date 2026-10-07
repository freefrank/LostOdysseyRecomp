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
#include <gpu/frame_rate.h>
#include <gpu/frame_generation_settings.h>
#include <gpu/video.h>
#include <gpu/display_change.h>
#include <gpu/display_choice.h>
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
namespace hid {
bool playStationPrompts = false;
bool UsesPlayStationPrompts() { return playStationPrompts; }
void SetVibrationStrength(uint32_t) {}
void PreviewVibration() {}
}
namespace settings {
Config savedConfig;
Config GetConfig(){return savedConfig;}
bool SaveConfig(const Config& c){++saves;savedConfig=c;return true;}
void PreviewConfig(const Config& c){savedConfig=c;}
uint32_t GameLanguage(){return 1;}
void RequestMainMenuAfterSettingsClose(PPCContext&,uint8_t*,uint32_t){
    Check(false,"AF graphics fixture must not request a Settings title transition");
}
namespace language {
void TraceLookup(uint8_t*,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,bool){}
void TraceConfig(uint8_t*,uint32_t,const char*){}
}
namespace menu_assets { std::shared_ptr<const Assets> Cached(const std::filesystem::path&,uint32_t) noexcept {return {};} }
}
namespace gpu::video {
plume::RenderDevice* GetDevice(){return reinterpret_cast<plume::RenderDevice*>(1);}
FrameGenerationStatus fgStatus{};
FrameGenerationStatus GetFrameGenerationStatus(){return fgStatus;}
std::optional<gpu::backend::Backend> SelectedBackend(){return gpu::backend::Backend::Vulkan;}
DisplayChangeResult QueryDisplayChange(uint64_t){return DisplayChangeResult::Applied;}
settings::Config displayRequest; unsigned displayRequests=0;
uint64_t BeginDisplayChange(const settings::Config& c){displayRequest=c;++displayRequests;return 1;}
bool DisplayModeFailed(){return false;}
bool WindowModeOverridden(){return false;}
std::vector<std::string> GpuDeviceNames(){return {"GPU A","GPU B"};}
std::string ActiveGpuDeviceName(){return "GPU A";}
std::vector<display_choice::Display> displays{{"Display 1",0,0,1920,1080}};
std::vector<display_choice::Display> Displays(){return displays;}
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
namespace apu { bool surround=false; void SetSurround(bool s){surround=s;} uint32_t OutputChannels(){return surround?6:2;} }
namespace settings { bool SaveAudioOutput(uint32_t o){savedConfig.audioOutput=o;return true;} }
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
        const int count=tab==0?7:tab==1?5:tab==2?int(settings::GraphicsRow::Count):3;
        for(int row=0;row<count;++row){
            // A on HDR peak opens its calibration page by design.
            if(settings::graphics_menu::IsAction(tab,row) || (tab==2 && row==int(settings::GraphicsRow::HdrPeak)))continue;
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
    settings::tab=1;settings::row=4;tick(8);
    Check(settings::edit.audioOutput==1 && settings::savedConfig.audioOutput==1 && apu::surround && applies==1,
          "audio output switches live and saves without a guest apply");
    using settings::GraphicsRow;using gpu::upscaling::Upscaler;
    settings::tab=2;settings::row=int(GraphicsRow::AntiAliasing);settings::edit={};
    settings::edit.dlssQuality=gpu::upscaling::DlssQuality::Dlaa;
    settings::edit.fsrQuality=gpu::upscaling::FsrQuality::Balanced;
    // Windows adds XeSS as a seventh choice after FSR.
    const unsigned aaChoices=settings::graphics_menu::AaChoiceCount;
    for(unsigned i=1;i<=aaChoices;++i){
        tick(8);Check(settings::graphics_menu::AaChoice(settings::edit)==i%aaChoices,"unified AA/provider cycle");
        Check(settings::snapshot.rows[int(GraphicsRow::AntiAliasing)].selectedChoice==int(i%aaChoices),"published combined choice");
        Check(settings::snapshot.rows.back().name==L"Save graphics settings","Save is last");
        Check(settings::snapshot.rows[int(GraphicsRow::DlssQuality)].hidden==(i%aaChoices<4),"quality visibility");
        Check(settings::snapshot.rows[int(GraphicsRow::FsrSharpness)].hidden==(i%aaChoices!=5),"FSR sharpness visibility");
    }
    tick(4);Check(settings::edit.upscaler==(aaChoices>6?Upscaler::Xess:Upscaler::Fsr),"left from Off selects the last provider");
    if(aaChoices>6){
        Check(!settings::snapshot.rows[int(GraphicsRow::DlssQuality)].hidden &&
              settings::snapshot.rows[int(GraphicsRow::FsrSharpness)].hidden,"XeSS shows quality without FSR sharpening");
        tick(4);Check(settings::edit.upscaler==Upscaler::Fsr,"left from XeSS selects FSR");
    }
    Check(settings::edit.dlssQuality==gpu::upscaling::DlssQuality::Dlaa &&
          settings::edit.fsrQuality==gpu::upscaling::FsrQuality::Balanced,"provider-specific qualities preserved");
    // The scroll origin depends on the rows a platform shows; moving from
    // quality to sharpness must never scroll.
    settings::row=int(GraphicsRow::DlssQuality);tick();
    const int qualityScroll=settings::snapshot.scroll;
    tick(2);
    Check(settings::row==int(GraphicsRow::FsrSharpness) && settings::snapshot.scroll==qualityScroll,"sharpness directly follows quality");
    settings::edit.fsrSharpnessPercent=0;tick(4);Check(!settings::edit.fsrSharpnessPercent,"sharpness lower bound");
    tick(8);Check(settings::edit.fsrSharpnessPercent==1,"sharpness increments");
    settings::edit.fsrSharpnessPercent=100;tick(8);Check(settings::edit.fsrSharpnessPercent==100,"sharpness upper bound");
    settings::edit.upscaler=Upscaler::Off;settings::row=int(GraphicsRow::AntiAliasing);tick(2);
    Check(settings::row==int(GraphicsRow::AmbientOcclusion),"AO follows anti-aliasing");
    tick(2);Check(settings::row==int(GraphicsRow::AnisotropicFiltering),"navigation skips hidden provider rows to AF");
    tick(1);Check(settings::row==int(GraphicsRow::AmbientOcclusion),"reverse navigation skips hidden provider rows");
    settings::row=int(GraphicsRow::ShadowResolution);settings::edit.shadowResolution=1;
    for(uint32_t multiplier:{2u,4u,1u}) {
        tick(8);Check(settings::edit.shadowResolution==multiplier,"shadow multiplier cycles 1/2/4");
        Check(settings::snapshot.rows[int(GraphicsRow::ShadowResolution)].value==std::to_wstring(multiplier)+L"×",
              "shadow multiplier is published");
    }
    settings::row=int(GraphicsRow::AmbientOcclusion);settings::edit.ambientOcclusion=0;
    for(uint32_t mode:{1u,2u,0u}) {
        tick(8);Check(settings::edit.ambientOcclusion==mode,"AO cycles Off/SSAO/GTAO");
        Check(settings::snapshot.rows[int(GraphicsRow::AmbientOcclusion)].value==
              (mode==0?L"Off":mode==1?L"SSAO":L"GTAO"),"AO mode is published");
    }
    constexpr const wchar_t* shadowLabels[]={L"Shadow resolution",L"陰影解析度",L"シャドウ解像度",L"그림자 해상도",L"阴影分辨率"};
    constexpr const wchar_t* aoLabels[]={L"Ambient occlusion",L"環境光遮蔽",L"アンビエントオクルージョン",L"앰비언트 오클루전",L"环境光遮蔽"};
    for(uint32_t language=0;language<5;++language) {
        settings::edit.uiLanguage=language;settings::row=int(GraphicsRow::ShadowResolution);tick();
        Check(settings::snapshot.rows[int(GraphicsRow::ShadowResolution)].name==shadowLabels[language],
              "shadow selector translated in all UI languages");
        settings::row=int(GraphicsRow::AmbientOcclusion);tick();
        Check(settings::snapshot.rows[int(GraphicsRow::AmbientOcclusion)].name==aoLabels[language],
              "AO selector translated in all UI languages");
    }
    settings::edit.uiLanguage=0;
    // A VRR-only save must keep a Dynamic MFG preference read from settings.ini.
    settings::savedConfig=settings::edit;
    settings::savedConfig.frameGenerationProvider=framegen::Provider::Dlss;
    settings::savedConfig.frameGenerationMode=framegen::Mode::Dynamic;
    settings::savedConfig.frameGenerationMultiplier=4;
    settings::savedConfig.frameGenerationTargetFps=144;
    settings::edit=settings::savedConfig;
    settings::row=int(GraphicsRow::VariableRefreshRate);settings::edit.variableRefreshRate=false;
    const auto beforeVrr=settings::GetConfig();
    auto fgPreferenceRetained=[&]{const auto saved=settings::GetConfig();return
        saved.frameGenerationProvider==beforeVrr.frameGenerationProvider &&
        saved.frameGenerationMode==beforeVrr.frameGenerationMode &&
        saved.frameGenerationMultiplier==beforeVrr.frameGenerationMultiplier &&
        saved.frameGenerationTargetFps==beforeVrr.frameGenerationTargetFps;};
    tick(8);Check(settings::edit.variableRefreshRate,"VRR toggles on");
    Check(settings::GetConfig()==beforeVrr,"unsaved VRR does not change runtime settings");
    Check(settings::snapshot.rows[int(GraphicsRow::VariableRefreshRate)].value==L"On","VRR preference published");
    Check(!settings::restart::Required(beforeVrr,settings::edit),"VRR does not require restart");
    settings::row=int(GraphicsRow::Save);tick(0x1000);
    Check(settings::GetConfig().variableRefreshRate,"Save applies VRR preference");
    Check(fgPreferenceRetained(),"VRR-on Save preserves Dynamic MFG preference");
    settings::row=int(GraphicsRow::VariableRefreshRate);tick(4);
    Check(!settings::edit.variableRefreshRate,"VRR toggles off");
    settings::row=int(GraphicsRow::Save);tick(0x1000);
    Check(!settings::GetConfig().variableRefreshRate,"Save restores ordinary pacing");
    Check(fgPreferenceRetained(),"VRR-off Save preserves Dynamic MFG preference");
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
    // GPU choice: Automatic plus each adapter; a change saves and asks for a restart.
    settings::savedConfig=afSaved;settings::edit=afSaved;
    settings::row=int(GraphicsRow::Gpu);tick();
    Check(!settings::snapshot.rows[int(GraphicsRow::Gpu)].hidden && settings::snapshot.rows[int(GraphicsRow::Gpu)].choices.size()==3,
          "GPU row lists Automatic and both adapters");
    Check(settings::snapshot.rows[int(GraphicsRow::Display)].hidden,"one display hides the display row");
    tick(8);Check(settings::edit.gpuDevice=="GPU A","GPU cycles to the first adapter");
    tick(8);tick(8);Check(settings::edit.gpuDevice.empty(),"GPU cycle returns to Automatic");
    tick(4);Check(settings::edit.gpuDevice=="GPU B","GPU cycles backward");
    settings::row=int(GraphicsRow::Save);tick(0x1000);tick();
    Check(settings::restartPrompt && settings::GetConfig().gpuDevice=="GPU B","GPU change saves and asks for a restart");
    settings::restartPrompt=settings::savedRestartPrompt=false;settings::displayTicket=0;
    // Three monitors of one model: each is its own choice, told apart by number
    // and position, and Save asks the window thread to move to the chosen one.
    {
        using gpu::display_choice::Resolve;
        gpu::video::displays={{"M27P20",0,0,3840,2160},{"M27P20",3840,0,3840,2160},{"M27P20",-3840,0,3840,2160}};
        Check(Resolve(gpu::video::displays,"M27P20",2)==2 && Resolve(gpu::video::displays,"M27P20",1)==1 &&
              Resolve(gpu::video::displays,"M27P20",7)==0 && Resolve(gpu::video::displays,"Other",0)==-1 &&
              Resolve(gpu::video::displays,"",1)==-1,"identical names resolve by saved index");
        settings::savedConfig=afSaved;settings::edit=afSaved;
        settings::row=int(GraphicsRow::Display);tick();
        const auto& displayRow=settings::snapshot.rows[int(GraphicsRow::Display)];
        Check(!displayRow.hidden && displayRow.choices.size()==4,"three identical displays give Automatic plus three choices");
        Check(displayRow.choices[1]!=displayRow.choices[2] && displayRow.choices[2]!=displayRow.choices[3] &&
              displayRow.choices[1]!=displayRow.choices[3],"identical displays have distinct labels");
        Check(displayRow.choices[2].find(L"2: M27P20")==0 && displayRow.choices[2].find(L"3840, 0")!=std::wstring::npos,
              "display label carries number, name and position");
        tick(8);Check(settings::edit.displayName=="M27P20" && settings::edit.displayIndex==0,"first display chosen");
        tick(8);Check(settings::edit.displayIndex==1,"second identical display chosen");
        tick(8);Check(settings::edit.displayIndex==2,"third identical display chosen");
        Check(settings::snapshot.rows[int(GraphicsRow::Display)].selectedChoice==3,"third display shown as selected");
        const auto requests=gpu::video::displayRequests;
        settings::row=int(GraphicsRow::Save);tick(0x1000);
        Check(gpu::video::displayRequests==requests+1 && gpu::video::displayRequest.displayIndex==2 &&
              gpu::video::displayRequest.displayName=="M27P20","Save requests the move to the third display");
        Check(settings::GetConfig().displayIndex==2,"display choice saved");
        tick();Check(!settings::restartPrompt,"display choice needs no restart");
        settings::row=int(GraphicsRow::Display);tick();
        Check(settings::snapshot.rows[int(GraphicsRow::Display)].selectedChoice==3,"saved third display stays selected");
        tick(8);Check(settings::edit.displayName.empty() && settings::edit.displayIndex==0,"cycle returns to Automatic");
        settings::savedConfig.displayIndex=5;settings::edit=settings::savedConfig;tick();
        Check(settings::snapshot.rows[int(GraphicsRow::Display)].selectedChoice==1,"a missing index falls back to the first same-named display");
        gpu::video::displays={{"Display 1",0,0,1920,1080}};
        settings::restartPrompt=settings::savedRestartPrompt=false;settings::displayTicket=0;
    }
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
    settings::restartPrompt=settings::savedRestartPrompt=false;settings::displayTicket=0;
    tick(0x10);Check(saves==oldSaves+2,"Start on the focused Save row saves (keyboard Enter)");
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
    // A controller-style change must invalidate the raster cache without a guest tick.
    std::vector<uint32_t> promptPixels;
    uint64_t promptRevision = UINT64_MAX;
    Check(settings::DrawMenu(promptPixels,promptRevision,1280,720),"Xbox prompts render");
    const auto xboxPixels = promptPixels;
    const auto sameRevision = promptRevision;
    hid::playStationPrompts = true;
    Check(settings::DrawMenu(promptPixels,promptRevision,1280,720),"PlayStation prompts render");
    Check(promptRevision == sameRevision && promptPixels != xboxPixels,
        "controller style changes pixels with unchanged menu revision");
    hid::playStationPrompts = false;
    Check(settings::DrawMenu(promptPixels,promptRevision,1280,720),"Xbox prompts restore");
    Check(promptRevision == sameRevision && promptPixels == xboxPixels,
        "controller style round trip restores original pixels");
    // Exercise the real Vulkan FG menu and Save/restart flow without an SDK.
    settings::tab=2;settings::row=int(GraphicsRow::FrameGeneration);
    settings::edit=settings::savedConfig;
    settings::edit.graphicsBackend=settings::GraphicsBackend::Vulkan;
    settings::edit.frameGenerationProvider=framegen::Provider::Off;
    settings::edit.frameGenerationMode=framegen::Mode::Fixed;
    settings::edit.frameGenerationMultiplier=2;
    settings::edit.uiLanguage=0;
    settings::savedConfig=settings::edit;
    tick();
    const bool vulkanFg=gpu::frame_generation::VulkanCompiledProvider(framegen::Provider::Dlss);
    const bool vulkanFsr=gpu::frame_generation::VulkanCompiledProvider(framegen::Provider::Fsr);
    std::vector<std::wstring> expectedFg{L"Off"};
    if(vulkanFg) expectedFg.push_back(L"DLSS");
    if(vulkanFsr) expectedFg.push_back(L"FSR");
    Check(settings::snapshot.rows[int(GraphicsRow::FrameGeneration)].choices == expectedFg,
        "Vulkan FG offers exactly the compiled native providers");
    Check(settings::snapshot.rows[int(GraphicsRow::FrameGenerationMultiplier)].hidden,"Vulkan Off hides multiplier");
    if(vulkanFg) {
        tick(8);Check(settings::edit.frameGenerationProvider==framegen::Provider::Dlss,"Vulkan selects DLSS");
        Check(!settings::snapshot.rows[int(GraphicsRow::FrameGenerationMultiplier)].hidden,"Vulkan DLSS exposes multiplier");
        settings::row=int(GraphicsRow::FrameGenerationMultiplier);tick(4);
        Check(settings::edit.frameGenerationMultiplier==6,"Vulkan multiplier wraps to 6x");
        auto& status=gpu::video::fgStatus;
        status.phase=gpu::video::FrameGenerationPhase::RestartRequired;
        status.requested=framegen::Provider::Dlss;
        tick();Check(settings::snapshot.notice.find(L"Restart to enable Vulkan")!=std::wstring::npos,"startup Off reports restart");
        settings::row=int(GraphicsRow::Save);tick(0x1000);tick();
        Check(settings::restartPrompt && settings::snapshot.dialogMessage.find(L"Enabling or changing Vulkan frame generation")!=std::wstring::npos,
            "Save displays Vulkan restart reason");
        settings::restartPrompt=settings::savedRestartPrompt=false;settings::displayTicket=0;
        settings::row=int(GraphicsRow::Save);tick(0x1000);tick();
        Check(!settings::restartPrompt,"unchanged FG save after Later does not ask again");
        settings::row=int(GraphicsRow::FrameGenerationMultiplier);tick();
        Check(settings::snapshot.notice.find(L"Restart to enable Vulkan")!=std::wstring::npos,"FG notice still reports the pending restart");
        settings::restartPrompt=settings::savedRestartPrompt=false;settings::displayTicket=0;
        status.phase=gpu::video::FrameGenerationPhase::Ready;
        status.sessionProvider=status.applied=framegen::Provider::Dlss;
        settings::row=int(GraphicsRow::FrameGenerationMultiplier);tick(8);
        settings::row=int(GraphicsRow::Save);tick(0x1000);tick();
        Check(!settings::restartPrompt,"existing Vulkan session changes multiplier without restart");
        settings::row=int(GraphicsRow::FrameGeneration);tick(8);
        if(vulkanFsr) {
            Check(settings::edit.frameGenerationProvider==framegen::Provider::Fsr && settings::edit.frameGenerationMultiplier==2,
                "Vulkan DLSS to FSR switches to fixed 2x");
            Check(settings::snapshot.rows[int(GraphicsRow::FrameGenerationMultiplier)].hidden,"FSR hides unsupported multipliers");
            status.phase=gpu::video::FrameGenerationPhase::RestartRequired;status.requested=framegen::Provider::Fsr;
            settings::row=int(GraphicsRow::Save);tick(0x1000);tick();
            Check(settings::restartPrompt,"Vulkan SDK provider change requests restart");
            settings::restartPrompt=settings::savedRestartPrompt=false;settings::displayTicket=0;
            settings::row=int(GraphicsRow::FrameGeneration);tick(8);
        }
        Check(settings::edit.frameGenerationProvider==framegen::Provider::Off,"Vulkan provider cycle returns to Off");
        status.phase=gpu::video::FrameGenerationPhase::Off;status.requested=framegen::Provider::Off;
        settings::row=int(GraphicsRow::Save);tick(0x1000);tick();
        Check(!settings::restartPrompt,"existing Vulkan session turns off without restart");
    } else if(vulkanFsr) {
        tick(8);Check(settings::edit.frameGenerationProvider==framegen::Provider::Fsr,"FSR-only build selects FSR without Streamline");
        Check(settings::snapshot.rows[int(GraphicsRow::FrameGenerationMultiplier)].hidden,"FSR-only is fixed 2x");
        auto& status=gpu::video::fgStatus;
        status.phase=gpu::video::FrameGenerationPhase::RestartRequired;status.requested=framegen::Provider::Fsr;
        settings::row=int(GraphicsRow::Save);tick(0x1000);tick();
        Check(settings::restartPrompt,"FSR startup Off requires provider startup hooks");
        settings::restartPrompt=settings::savedRestartPrompt=false;settings::displayTicket=0;
    } else {
        tick(8);Check(settings::edit.frameGenerationProvider==framegen::Provider::Off,"unavailable build cannot enable Vulkan FG");
    }
    gpu::video::fgStatus={};
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
