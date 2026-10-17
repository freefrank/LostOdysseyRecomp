#!/usr/bin/env python3
"""Compile production settings and walking hooks with isolated guest/path services."""
import argparse
from pathlib import Path
import re

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--root', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()


def production(relative: str) -> str:
    text = (args.root / relative).read_text(encoding='utf-8')
    return re.sub(r'^#include[^\n]*\n', '', text, flags=re.M)


prefix = r'''
#include <settings/config.h>
#include <debug/fast_forward.h>
#include <debug/no_encounters.h>
#include <debug/battle_tour.h>
#include <gpu/frame_rate.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#ifdef _WIN32
#include <windows.h>
#endif
namespace os::user_paths {
bool UsePortableLayout() { return true; }
std::filesystem::path ConfigDir() { return {}; }
std::filesystem::path SettingsPath() { return "settings.ini"; }
}
#define LOG_INFO(...) ((void)0)
#define LOG_WARNING(...) ((void)0)
#define LOG_NOTICE(...) ((void)0)
namespace os::logger { void SetDebugLog(bool) {} }
namespace settings { void LogSettingsSaved(const Config&) {} }

// The guest body is a controlled stub; the compiled wrappers and config
// parser/writer below are extracted verbatim from production.
union Register { uint64_t u64=0; uint32_t u32; };
struct PPCContext { Register r3,r6; uint64_t lr=0; };
#define PPC_FUNC(name) void name(PPCContext& ctx,uint8_t* base)
std::unordered_map<uint32_t,uint32_t> guest;
#define PPC_LOAD_U32(address) guest[uint32_t(address)]
#define PPC_STORE_U32(address,value) (guest[uint32_t(address)]=uint32_t(value))
uint32_t originalDraw=1, originalCalls=0, wrapperCalls=0;
bool tourPending=false;
namespace debug_menu::battle_tour {
bool Pending() { return tourPending; }
bool Take(uint8_t*,int32_t& formation) {
    if(!tourPending)return false;
    tourPending=false; formation=42; return true;
}
void RestoreStage(uint8_t*) {}
}
'''

test = r'''
extern "C" PPC_FUNC(__imp__sub_829E3268) {
    ++originalCalls;
    ctx.r3.u64=originalDraw;
    if(originalDraw==1) {
        PPC_STORE_U32(0x832C95C0+564,123);
        PPC_STORE_U32(0x832C95C0+580,0);
    }
}
extern "C" PPC_FUNC(__imp__sub_829E3048) {}
void Check(bool ok,const char* reason) {
    if(!ok) { std::fprintf(stderr,"FAIL: %s\n",reason); std::exit(1); }
}
void Write(const std::string& value) { std::ofstream("settings.ini") << value; }
uint32_t Draw(uint32_t result=1,uint32_t caller=0x829E31F8) {
    PPCContext ctx; ctx.lr=caller; ctx.r6.u32=0x1000;
    originalDraw=result;
    ++wrapperCalls;
    sub_829E3268(ctx,nullptr);
    Check(ctx.lr==caller,"original caller retained");
    return ctx.r3.u32;
}
int main() {
    namespace fs=std::filesystem;
    const auto previous=fs::current_path();
    const auto scratch=fs::temp_directory_path()/("lo-encounter-config-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Check(fs::create_directory(scratch),"unique isolated scratch directory");
    fs::current_path(scratch);
    using settings::EncounterRate;
    Check(settings::GetConfig().encounterRate==EncounterRate::Normal &&
        !settings::GetConfig().battleExpDouble,"safe defaults for existing profiles");
    Check(settings::GetConfig().discordStatus,"Discord status defaults On for existing profiles");
    for(auto text : {"", "discord_status=2\n", "discord_status=-1\n", "discord_status=bad\n"}) {
        Write(text); Check(settings::Read().discordStatus,"missing/invalid Discord setting keeps On default");
    }
    for(bool enabled : {false,true}) {
        Write(enabled ? "discord_status=1\n" : "discord_status=0\n");
        auto discord=settings::Read();
        Check(discord.discordStatus==enabled && settings::SaveConfig(discord) &&
            settings::GetConfig().discordStatus==enabled && settings::Read().discordStatus==enabled,
            "Discord status parses and roundtrips both values");
    }
    Check(settings::GetConfig().fastForwardKeepAudioPitch,"missing fast-forward audio key keeps pitch");
    for(auto text : {"", "fast_forward_keep_pitch=2\n", "fast_forward_keep_pitch=-1\n",
                    "fast_forward_keep_pitch=4294967295\n", "fast_forward_keep_pitch=bad\n"}) {
        Write(text); Check(settings::Read().fastForwardKeepAudioPitch,"missing/invalid fast-forward audio keeps pitch");
    }
    for(bool keep : {false,true}) {
        Write(keep ? "fast_forward_keep_pitch=1\n" : "fast_forward_keep_pitch=0\n");
        Check(settings::Read().fastForwardKeepAudioPitch==keep,"fast-forward audio key parses both modes");
        Check(settings::SaveFastForward(true,1,8,keep) && settings::Read().fastForwardKeepAudioPitch==keep &&
            settings::GetConfig().fastForwardKeepAudioPitch==keep,"fast-forward audio roundtrip retains selected mode");
        auto graphics=settings::GetConfig(); graphics.fastForwardKeepAudioPitch=!keep;
        Check(settings::SaveConfig(graphics) && settings::Read().fastForwardKeepAudioPitch==keep,
            "graphics save preserves debug audio mode");
        settings::PreviewConfig(graphics);
        Check(settings::GetConfig().fastForwardKeepAudioPitch==keep,"graphics preview preserves debug audio mode");
        Check(fs::create_directory("settings.ini.tmp"),"block atomic settings temporary file");
        Check(!settings::SaveFastForward(true,1,8,!keep) && settings::Read().fastForwardKeepAudioPitch==keep &&
            settings::GetConfig().fastForwardKeepAudioPitch==keep,"failed audio-mode save preserves disk and runtime choice");
        Check(fs::remove("settings.ini.tmp"),"remove atomic settings blocker");
    }
    Write("no_random_encounters=1\n");
    Check(settings::Read().encounterRate==EncounterRate::None,"legacy 1 migrates to None");
    for(auto text : {"no_random_encounters=0\n","no_random_encounters=2\n",
        "no_random_encounters=-1\n","no_random_encounters=bad\n"}) {
        Write(text); Check(settings::Read().encounterRate==EncounterRate::Normal,"invalid/disabled legacy key uses Normal");
    }
    for(auto text : {"no_random_encounters=1\nencounter_rate=1\n",
                    "encounter_rate=1\nno_random_encounters=1\n"}) {
        Write(text); Check(settings::Read().encounterRate==EncounterRate::Half,"new key wins in either order");
    }
    for(auto text : {"no_random_encounters=1\nencounter_rate=bad\n",
        "encounter_rate=3\nno_random_encounters=1\n","encounter_rate=-1\n",
        "encounter_rate=4294967295\n"}) {
        Write(text); Check(settings::Read().encounterRate==EncounterRate::Normal,"invalid new key cannot inherit legacy None");
    }
    for(auto text : {"", "battle_exp_double=0\n", "battle_exp_double=2\n",
                    "battle_exp_double=-1\n", "battle_exp_double=bad\n"}) {
        Write(text); Check(!settings::Read().battleExpDouble,"missing/invalid battle EXP is off");
    }
    Write("width=1600\nbattle_exp_double=1\n");
    Check(settings::Read().battleExpDouble,"battle EXP key parsed");
    auto preview=settings::GetConfig(); preview.width=2000;
    settings::PreviewConfig(preview);
    Check(settings::SaveBattleExpDouble(true) && settings::GetConfig().battleExpDouble,
        "EXP preference persists independently");
    for(auto rate : {EncounterRate::Normal,EncounterRate::Half,EncounterRate::None}) {
        Check(settings::SaveEncounterRate(rate),"encounter preference saved");
        Check(settings::Read().encounterRate==rate && settings::Read().battleExpDouble,
            "rate round trip retains independent EXP setting");
        Check(settings::Read().width==1600,"debug save does not commit a graphics preview");
    }
    auto graphics=settings::GetConfig();
    graphics.encounterRate=EncounterRate::Normal; graphics.battleExpDouble=false;
    Check(settings::SaveConfig(graphics),"ordinary settings save");
    Check(settings::Read().encounterRate==EncounterRate::None && settings::Read().battleExpDouble,
        "ordinary settings save preserves debug preferences");
    settings::PreviewConfig(graphics);
    Check(settings::GetConfig().encounterRate==EncounterRate::None && settings::GetConfig().battleExpDouble,
        "graphics preview cannot overwrite debug preferences");

    // Execute the real wrapper, including the successful-draw original-body
    // side effects. Interleaved zero draws and other callers cannot consume
    // Half's alternation; tour and Every Step have priority.
    debug_menu::SetEncounterRate(EncounterRate::Normal);
    unsigned normal=0,half=0,none=0;
    for(unsigned i=0;i<100;++i) normal+=Draw();
    debug_menu::SetEncounterRate(EncounterRate::Half);
    for(unsigned i=0;i<100;++i) {
        Check(Draw(0)==0,"failed walking draw remains zero");
        Check(Draw(1,0x12345678)==1,"scripted/other caller never suppressed");
        const auto result=Draw();
        Check(result==(i&1),"Half deterministically skips first and allows second");
        half+=result;
        Check(PPC_LOAD_U32(0x832C95C0+564)==123 && PPC_LOAD_U32(0x832C95C0+580)==0,
            "original threshold/distance reset survives suppressed encounters");
    }
    debug_menu::SetEncounterRate(EncounterRate::None);
    for(unsigned i=0;i<100;++i) none+=Draw();
    Check(normal==100 && half==50 && none==0,"100 successful draws give 100/50/0 battles");
    Check(Draw(1,0x829E31F4)==1,"None leaves every other call site unchanged");
    Check(Draw(2)==2,"non-battle original result is preserved");
    tourPending=true;
    Check(Draw(0)==1 && PPC_LOAD_U32(0x1000)==42,"tour overrides None and writes its formation");
    debug_menu::SetEncounterRate(EncounterRate::Half);
    Check(Draw()==0,"Half restarts deterministically after a mode change");
    tourPending=true;
    Check(Draw(0)==1,"tour overrides Half");
    Check(Draw()==1,"tour does not consume Half's alternation");
    debug_menu::SetEncounterEveryStepEnabled(true);
    Check(debug_menu::GetEncounterRate()==EncounterRate::Half && Draw()==1 &&
        settings::Read().encounterRate==EncounterRate::Half,
        "Every Step permits the original battle without persisting over Half");
    debug_menu::SetEncounterRate(EncounterRate::Half);
    Check(!debug_menu::EncounterEveryStepEnabled() && Draw()==0,"Half disables Every Step");
    debug_menu::SetEncounterEveryStepEnabled(true);
    debug_menu::SetEncounterRate(EncounterRate::None);
    Check(!debug_menu::EncounterEveryStepEnabled() && Draw()==0,"None disables Every Step");
    debug_menu::SetEncounterEveryStepEnabled(true);
    Check(debug_menu::GetEncounterRate()==EncounterRate::None && Draw()==1 &&
        settings::Read().encounterRate==EncounterRate::None,
        "Every Step overrides None without changing the saved preference");
    debug_menu::SetEncounterRate(EncounterRate::Normal);
    Check(!debug_menu::EncounterEveryStepEnabled() && Draw()==1,"Normal disables Every Step");
    Check(originalCalls==wrapperCalls,"every wrapper invocation runs the original draw");
    fs::current_path(previous); fs::remove_all(scratch);
    std::printf("PASS production encounter hook/config: 100 successful walking draws -> Normal=%u Half=%u None=%u; caller/tour/Every Step and migration verified\n",normal,half,none);
}
'''

args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(prefix + production('LostOdysseyRecomp/settings/config.cpp') +
    production('LostOdysseyRecomp/debug/no_encounters.cpp') + test, encoding='utf-8')
