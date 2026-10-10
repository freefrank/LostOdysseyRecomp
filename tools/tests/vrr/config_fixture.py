#!/usr/bin/env python3
"""Compile the real config parser/writer with isolated path/log service boundaries."""
import argparse
from pathlib import Path
import re
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--root', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
source = (a.root / 'LostOdysseyRecomp/settings/config.cpp').read_text(encoding='utf-8')
source = re.sub(r'^#include[^\n]*\n', '', source, flags=re.M)
prefix = r'''
#include <settings/config.h>
#include <debug/fast_forward.h>
#include <gpu/frame_rate.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif
namespace os::user_paths {
bool UsePortableLayout() { return true; }
std::filesystem::path ConfigDir() { return {}; }
std::filesystem::path SettingsPath() { return "settings.ini"; }
}
#define LOG_INFO(...) ((void)0)
namespace settings { void LogSettingsSaved(const Config&) {} }
'''
test = r'''
void Check(bool ok, const char* reason) {
    if (!ok) { std::cerr << reason << '\n'; std::exit(1); }
}
void Write(const std::string& text) { std::ofstream("settings.ini") << text; }
int main() {
    namespace fs = std::filesystem;
    const auto previous = fs::current_path();
    const auto scratch = fs::temp_directory_path() / ("lo-vrr-config-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Check(fs::create_directory(scratch), "unique isolated scratch directory");
    fs::current_path(scratch);
    Check(!settings::GetConfig().variableRefreshRate, "legacy/missing preference defaults off");
    for (auto text : {"", "variable_refresh_rate=0\n", "variable_refresh_rate=2\n",
                      "variable_refresh_rate=-1\n", "variable_refresh_rate=bad\n"}) {
        Write(text); Check(!settings::Read().variableRefreshRate, "invalid VRR input cannot enable it");
    }
    Write("variable_refresh_rate=1\n");
    Check(settings::Read().variableRefreshRate, "VRR key parsed");
    for (auto rate : gpu::frame_rate::kNativeRates) for (bool enabled : {false,true}) {
        auto config = settings::GetConfig();
        config.frameRate = rate; config.variableRefreshRate = enabled;
        config.frameGenerationProvider = framegen::Provider::Dlss;
        config.frameGenerationMode = framegen::Mode::Dynamic;
        config.frameGenerationMultiplier = 4; config.frameGenerationTargetFps = 144;
        Check(settings::SaveConfig(config), "VRR save succeeds");
        const auto disk = settings::Read();
        Check(disk.variableRefreshRate == enabled && disk.frameRate == rate,
            "real INI round trip preserves VRR and native rate");
        Check(disk.frameGenerationTargetFps == 144 && disk.frameGenerationMultiplier == 4,
            "VRR does not overwrite user's FG preferences");
        auto preview = config; preview.variableRefreshRate = !enabled;
        settings::PreviewConfig(preview);
        Check(settings::SaveDebugLanguage(1), "debug-only save");
        Check(settings::Read().variableRefreshRate == enabled, "debug save cannot persist unrelated preview");
        Check(settings::SaveSaveAnywhere(true), "debug toggle save");
        Check(settings::Read().variableRefreshRate == enabled, "other saves retain disk VRR setting");
    }
    for (auto provider : {framegen::Provider::Fsr, framegen::Provider::MetalFx, framegen::Provider::Xess}) {
        auto config=settings::GetConfig(); config.frameGenerationProvider=provider;
        config.frameGenerationMode=framegen::Mode::Dynamic;
        config.frameGenerationMultiplier=6; config.frameGenerationTargetFps=144;
        Check(settings::SaveConfig(config), "fixed-only FG provider saves");
        const auto disk=settings::Read();
        Check(disk.frameGenerationProvider==provider && disk.frameGenerationMode==framegen::Mode::Fixed &&
            disk.frameGenerationMultiplier==2 && !disk.frameGenerationTargetFps,
            "FSR/MetalFX/XeSS normalize to fixed 2x without discarding saved provider");
    }
    Write("frame_generation_provider=3\n");
    Check(settings::Read().frameGenerationProvider==framegen::Provider::MetalFx,"MetalFX has stable INI value 3");
    Write("frame_generation_provider=4\n");
    Check(settings::Read().frameGenerationProvider==framegen::Provider::Xess,"XeSS has stable INI value 4");
    Write("frame_generation_provider=5\n");
    Check(settings::Read().frameGenerationProvider==framegen::Provider::Off,"unknown FG provider rejected");
    Write("game_language_pack=PT-BR\n");
    Check(settings::Read().gameLanguagePack=="pt-br","language pack id read in lower case");
    Write("game_language_pack=../pt\n");
    Check(settings::Read().gameLanguagePack.empty(),"invalid language pack id selects none");
    {
        auto config=settings::GetConfig(); config.gameLanguagePack="pt-br";
        Check(settings::SaveConfig(config) && settings::Read().gameLanguagePack=="pt-br","language pack id saved");
    }
    fs::current_path(previous);
    fs::remove_all(scratch);
    std::cout << "VRR production config parse/save/preview checks passed (isolated paths)\n";
}
'''
a.output.parent.mkdir(parents=True, exist_ok=True)
a.output.write_text(prefix + source + test, encoding='utf-8')
