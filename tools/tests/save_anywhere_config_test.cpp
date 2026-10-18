// Bounded production config and debug-toggle fixture; run from an isolated working directory.
#include <stdafx.h>
#include <os/logger.h>
#include <gpu/dlss_status_log.h>
#undef LOG_INFO
#define LOG_INFO(...) ((void)0)
#undef LOG_WARNING
#define LOG_WARNING(...) ((void)0)
#include "../../LostOdysseyRecomp/settings/config.cpp"
#include "../../LostOdysseyRecomp/debug/save_anywhere.cpp"

extern "C" PPC_FUNC(__imp__sub_822E0E10) {}
extern "C" PPC_FUNC(__imp__sub_82876EA8) {}

namespace
{
void Check(bool ok, const char* message)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

std::string Contents()
{
    std::ifstream input("settings.ini");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void Write(const char* text)
{
    std::ofstream output("settings.ini", std::ios::trunc);
    output << text;
    Check(bool(output), "write isolated settings.ini");
}

void CheckFreshProcess(const wchar_t* executable, const char* expected, uint32_t menuRate = 1, uint32_t loadingMode = 2)
{
    std::wstring command = L"\"" + std::wstring(executable) + L"\" --restore " +
        (expected[0] == '1' ? L"1" : L"0") + L" " + std::to_wstring(menuRate) + L" " + std::to_wstring(loadingMode);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    Check(CreateProcessW(executable, command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr,
        &startup, &process) != FALSE, "start isolated restart check");
    Check(WaitForSingleObject(process.hProcess, 10000) == WAIT_OBJECT_0, "restart check completed");
    DWORD exitCode = 1;
    Check(GetExitCodeProcess(process.hProcess, &exitCode) && exitCode == 0, "startup restoration");
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    Check(_putenv_s("LO_FAST_LOADING", "") == 0, "clear loading override for persisted settings checks");
    if (argc == 5 && std::wstring_view(argv[1]) == L"--restore")
    {
        const auto menuRate = uint32_t(std::wcstoul(argv[3], nullptr, 10));
        const auto loadingMode = uint32_t(std::wcstoul(argv[4], nullptr, 10));
        Check(settings::MenuAnimationRate() == menuRate, "cold hook read restores camp-menu transition speed");
        const bool expected = std::wstring_view(argv[2]) == L"1";
        Check(settings::GetConfig().saveAnywhere == expected, "startup config flag");
        Check(debug_menu::SaveAnywhereEnabled() == expected, "startup debug switch");
        Check(settings::GetConfig().menuAnimationRate == menuRate, "startup camp-menu config field");
        Check(settings::GetConfig().loadingTransitions == loadingMode && gpu::loading::Enabled() == (loadingMode != 0),
            "startup restores and publishes the saved loading pace");
        return 0;
    }
    Check(argc == 1, "unexpected arguments");
    wchar_t executable[32768]{};
    Check(GetModuleFileNameW(nullptr, executable, DWORD(std::size(executable))) != 0, "own executable path");
    Check(!std::filesystem::exists("settings.ini"), "run in empty isolated directory");
    Check(!settings::GetConfig().saveAnywhere && !debug_menu::SaveAnywhereEnabled(), "missing file defaults off");
    Check(settings::GetConfig().menuAnimationRate == 1 && settings::MenuAnimationRate() == 1,
        "missing menu animation preference keeps the original speed");
    Check(settings::Config{}.loadingTransitions == 2 && settings::GetConfig().loadingTransitions == 2 && gpu::loading::Enabled(),
        "missing loading preference keeps the current Fast behavior");
    Check(settings::GetConfig().shadowResolution == 1 && settings::GetConfig().ambientOcclusion == 0,
        "missing shadow and AO keys retain original rendering");
    CheckFreshProcess(executable, "0");

    Write("width=1600\n");
    Check(!settings::Read().saveAnywhere, "missing key defaults off");
    CheckFreshProcess(executable, "0");
    Write("save_anywhere=2\n");
    Check(!settings::Read().saveAnywhere, "invalid key defaults off");
    Write("no_random_encounters=1\n");
    Check(settings::Read().encounterRate == settings::EncounterRate::None, "legacy no random encounters loads None");
    Write("no_random_encounters=2\n");
    Check(settings::Read().encounterRate == settings::EncounterRate::Normal, "invalid legacy value defaults Normal");
    Write("no_random_encounters=1\nencounter_rate=1\nbattle_exp_double=1\n");
    Check(settings::Read().encounterRate == settings::EncounterRate::Half && settings::Read().battleExpDouble,
        "new encounter rate overrides legacy and EXP loads independently");
    Write("encounter_rate=0\nno_random_encounters=1\nbattle_exp_double=2\n");
    Check(settings::Read().encounterRate == settings::EncounterRate::Normal && !settings::Read().battleExpDouble,
        "new key precedence is independent of ordering; invalid EXP defaults off");
    Write("no_random_encounters=1\nencounter_rate=garbage\n");
    Check(settings::Read().encounterRate == settings::EncounterRate::Normal, "malformed new rate overrides legacy safely");
    Check(!settings::Read().fastForward && settings::Read().fastForwardMode == 0 && settings::Read().fastForwardRate == 2 &&
          settings::Read().fastForwardKeepAudioPitch,
        "missing fast-forward keys default to off, Hold, 2x, Keep pitch");
    Write("fast_forward=1\nfast_forward_mode=1\nfast_forward_rate=6\n");
    Check(settings::Read().fastForward && settings::Read().fastForwardMode == 1 && settings::Read().fastForwardRate == 6,
        "fast-forward keys read from INI");
    Write("fast_forward=2\nfast_forward_mode=7\nfast_forward_rate=5\n");
    Check(!settings::Read().fastForward && settings::Read().fastForwardMode == 0 && settings::Read().fastForwardRate == 2,
        "invalid fast-forward values default to off, Hold, 2x");
    Check(settings::Read().menuAnimationRate == 1, "missing menu animation key defaults to Normal");
    for (const auto rate : settings::MenuAnimationRates)
    {
        Write(("menu_animation_rate=" + std::to_string(rate) + "\n").c_str());
        Check(settings::Read().menuAnimationRate == rate, "supported menu animation multiplier read from INI");
    }
    for (const auto* text : {"menu_animation_rate=0\n", "menu_animation_rate=3\n",
                            "menu_animation_rate=4294967295\n", "menu_animation_rate=-1\n",
                            "menu_animation_rate=bad\n"})
    {
        Write(text);
        Check(settings::Read().menuAnimationRate == 1, "invalid menu animation multiplier defaults to Normal");
    }

    Check(settings::Read().loadingTransitions == 2, "missing loading pace key defaults to Fast");
    for (const auto mode : {0u, 2u})
    {
        Write(("loading_transitions=" + std::to_string(mode) + "\n").c_str());
        Check(settings::Read().loadingTransitions == mode, "supported loading pace reads from INI");
    }
    for (const auto* text : {"loading_transitions=1\n", "loading_transitions=3\n",
                            "loading_transitions=4294967295\n", "loading_transitions=-1\n",
                            "loading_transitions=bad\n"})
    {
        Write(text);
        Check(settings::Read().loadingTransitions == 2, "unsupported loading pace defaults to Fast");
    }
    const auto beforeLoadingPreview = settings::GetConfig();
    auto loading = beforeLoadingPreview;
    loading.loadingTransitions = 0;
    settings::PreviewConfig(loading);
    Check(settings::GetConfig().loadingTransitions == 0 && !gpu::loading::Enabled() && settings::Read().loadingTransitions == 2,
        "Original loading preview publishes without committing to disk");
    settings::PreviewConfig(beforeLoadingPreview);
    Check(settings::GetConfig().loadingTransitions == 2 && gpu::loading::Enabled(),
        "cancelling loading preview restores the saved pace");
    for (const auto mode : {0u, 2u})
    {
        loading = settings::GetConfig();
        loading.loadingTransitions = mode;
        Check(settings::SaveConfig(loading) && settings::GetConfig().loadingTransitions == mode &&
              settings::Read().loadingTransitions == mode && gpu::loading::Enabled() == (mode != 0) &&
              Contents().find("loading_transitions=" + std::to_string(mode) + "\n") != std::string::npos,
            "loading pace saves, publishes and survives disk readback");
        CheckFreshProcess(executable, "0", 1, mode);
    }
    Check(std::filesystem::create_directory("settings.ini.tmp"), "block loading settings temporary file");
    loading.loadingTransitions = 0;
    Check(!settings::SaveConfig(loading) && settings::GetConfig().loadingTransitions == 2 &&
          settings::Read().loadingTransitions == 2 && gpu::loading::Enabled(),
        "failed loading save preserves runtime and disk pace");
    Check(std::filesystem::remove("settings.ini.tmp"), "remove loading settings temporary-file blocker");

    Write("internal_resolution=540\n");
    Check(settings::Read().internalResolution == 720, "the removed 540 render resolution loads as 720");
    Write("internal_resolution=900\n");
    Check(settings::Read().internalResolution == 0, "an unknown render resolution follows the output");
    Check(settings::Read().vibrationPercent == 100, "missing vibration key keeps retail strength");
    Write("vibration=40\n");
    Check(settings::Read().vibrationPercent == 40, "vibration strength read from INI");
    Write("vibration=250\n");
    Check(settings::Read().vibrationPercent == 100, "vibration strength is bounded to 100");
    Write("vibration=-5\n");
    Check(settings::Read().vibrationPercent == 100, "malformed vibration keeps retail strength");
    Check(settings::Read().audioOutput == settings::AudioOutputStereo, "missing audio output key keeps stereo");
    Write("audio_output=1\n");
    Check(settings::Read().audioOutput == settings::AudioOutputSurround, "5.1 audio output read from INI");
    Check(settings::Read().audioMatrixRear == 110, "missing rear angle keeps Pro Logic II's 110 degrees");
    Write("audio_output=2\naudio_matrix_rear=124\n");
    Check(settings::Read().audioOutput == settings::AudioOutputMatrix && settings::Read().audioMatrixRear == 120,
          "matrix surround read from INI, rear angle rounded to a 10 degree step");
    Write("audio_output=3\naudio_matrix_rear=400\n");
    Check(settings::Read().audioOutput == settings::AudioOutputStereo && settings::Read().audioMatrixRear == 150,
          "unknown audio output falls back to stereo, rear angle capped at 150");
    Check(settings::Read().voiceVolume == 100, "missing voice volume keeps 100");
    Write("voice_volume=64\n");
    Check(settings::Read().voiceVolume == 64, "voice volume read from INI");
    Write("voice_volume=300\n");
    Check(settings::Read().voiceVolume == 100, "voice volume is bounded to 100");

    Write("shadow_resolution=2\nambient_occlusion=1\n");
    Check(settings::Read().shadowResolution == 2 && settings::Read().ambientOcclusion == 1,
        "shadow 2x and SSAO read from INI");
    Write("shadow_resolution=4\nambient_occlusion=2\n");
    Check(settings::Read().shadowResolution == 4 && settings::Read().ambientOcclusion == 2,
        "shadow 4x and GTAO read from INI");
    Write("shadow_resolution=3\nambient_occlusion=3\n");
    Check(settings::Read().shadowResolution == 1 && settings::Read().ambientOcclusion == 0,
        "unsupported shadow and AO values return to defaults");
    Write("shadow_resolution=-1\nambient_occlusion=invalid\n");
    Check(settings::Read().shadowResolution == 1 && settings::Read().ambientOcclusion == 0,
        "malformed shadow and AO values return to defaults");

    debug_menu::SetSaveAnywhereEnabled(true);
    Check(debug_menu::SaveAnywhereEnabled() && settings::GetConfig().saveAnywhere,
        "runtime enable takes effect and updates config");
    Check(settings::Read().saveAnywhere && Contents().find("save_anywhere=1\n") != std::string::npos,
        "enabled value survives disk readback");
    CheckFreshProcess(executable, "1");

    settings::Config preview = settings::GetConfig();
    preview.width = 2000;
    settings::PreviewConfig(preview);
    debug_menu::SetSaveAnywhereEnabled(false);
    Check(!debug_menu::SaveAnywhereEnabled() && !settings::GetConfig().saveAnywhere,
        "runtime disable takes effect");
    Check(settings::Read().width == 1280, "toggle does not commit unconfirmed graphics preview");
    Check(!settings::Read().saveAnywhere && Contents().find("save_anywhere=0\n") != std::string::npos,
        "disabled value survives disk readback");
    CheckFreshProcess(executable, "0");

    debug_menu::SetSaveAnywhereEnabled(true);
    Check(settings::SaveEncounterRate(settings::EncounterRate::Half) &&
          settings::GetConfig().encounterRate == settings::EncounterRate::Half &&
          Contents().find("encounter_rate=1\n") != std::string::npos,
        "Half encounter rate persists");
    Check(settings::SaveBattleExpDouble(true) && settings::GetConfig().battleExpDouble &&
          Contents().find("battle_exp_double=1\n") != std::string::npos,
        "battle EXP preference persists");
    Check(settings::SaveFastForward(true, 1, 8, false) && settings::GetConfig().fastForward &&
          settings::GetConfig().fastForwardRate == 8 && !settings::GetConfig().fastForwardKeepAudioPitch &&
          Contents().find("fast_forward=1\nfast_forward_mode=1\nfast_forward_rate=8\nfast_forward_keep_pitch=0\n") != std::string::npos,
        "fast-forward choices persist");
    auto menuPreview = settings::GetConfig();
    menuPreview.width = 2200;
    menuPreview.encounterRate = settings::EncounterRate::Normal;
    menuPreview.battleExpDouble = false;
    settings::PreviewConfig(menuPreview);
    for (const auto rate : settings::MenuAnimationRates)
    {
        Check(settings::SaveMenuAnimationRate(rate) && settings::GetConfig().menuAnimationRate == rate &&
              settings::MenuAnimationRate() == rate && settings::Read().menuAnimationRate == rate &&
              Contents().find("menu_animation_rate=" + std::to_string(rate) + "\n") != std::string::npos,
            "menu animation multiplier persists and publishes to the hook");
        Check(settings::Read().width == 1280, "menu animation save does not commit a graphics preview");
        Check(settings::Read().encounterRate == settings::EncounterRate::Half && settings::Read().battleExpDouble,
            "menu animation save retains encounter rate and battle EXP over a graphics preview");
        CheckFreshProcess(executable, "1", rate);
    }
    Check(settings::SaveEncounterRate(settings::EncounterRate::None) && settings::SaveBattleExpDouble(false) &&
          settings::Read().menuAnimationRate == 4 && settings::MenuAnimationRate() == 4,
        "encounter and battle EXP saves retain the persisted and live animation rate");
    Check(settings::SaveEncounterRate(settings::EncounterRate::Half) && settings::SaveBattleExpDouble(true),
        "restore encounter and battle EXP preferences for subsequent save checks");
    Check(std::filesystem::create_directory("settings.ini.tmp"), "block atomic settings temporary file");
    Check(!settings::SaveMenuAnimationRate(2) && settings::GetConfig().menuAnimationRate == 4 &&
          settings::MenuAnimationRate() == 4 && settings::Read().menuAnimationRate == 4,
        "failed menu animation save preserves the previous runtime and disk choice");
    Check(std::filesystem::remove("settings.ini.tmp"), "remove settings temporary-file blocker");
    menuPreview = settings::GetConfig();
    menuPreview.menuAnimationRate = 1;
    settings::PreviewConfig(menuPreview);
    Check(settings::GetConfig().menuAnimationRate == 4 && settings::MenuAnimationRate() == 4,
        "graphics preview retains the debug menu animation choice");
    Check(settings::SaveAudioOutput(settings::AudioOutputSurround) &&
          settings::GetConfig().audioOutput == settings::AudioOutputSurround &&
          Contents().find("audio_output=1\n") != std::string::npos, "audio output persists on its own");
    settings::Config graphics = settings::GetConfig();
    graphics.audioOutput = settings::AudioOutputStereo;
    graphics.width = 1800;
    graphics.shadowResolution = 4;
    graphics.ambientOcclusion = 2;
    graphics.vibrationPercent = 30;
    graphics.menuAnimationRate = 1;
    Check(settings::SaveConfig(graphics), "save ordinary settings");
    Check(settings::Read().shadowResolution == 4 && settings::Read().ambientOcclusion == 2 &&
          Contents().find("shadow_resolution=4\nambient_occlusion=2\n") != std::string::npos,
        "shadow and AO choices roundtrip through stable INI keys");
    Check(settings::GetConfig().vibrationPercent == 30 && settings::Read().vibrationPercent == 30 &&
          Contents().find("vibration=30\n") != std::string::npos, "vibration strength roundtrips");
    Check(settings::Read().saveAnywhere, "ordinary save retains debug-only preference");
    Check(settings::Read().encounterRate == settings::EncounterRate::Half && settings::Read().battleExpDouble,
        "ordinary save retains encounter rate and battle EXP");
    Check(settings::Read().fastForward && settings::Read().fastForwardMode == 1 && settings::Read().fastForwardRate == 8 &&
          !settings::Read().fastForwardKeepAudioPitch,
        "ordinary save retains fast-forward choices");
    Check(settings::GetConfig().menuAnimationRate == 4 && settings::MenuAnimationRate() == 4 &&
          settings::Read().menuAnimationRate == 4, "ordinary save retains menu animation speed");
    Check(settings::Read().audioOutput == settings::AudioOutputSurround, "ordinary save retains the audio output choice");
    Check(settings::SaveDebugLanguage(1) && settings::Read().saveAnywhere,
        "debug language save retains save-anywhere preference");
    for (const auto fps : gpu::frame_rate::kNativeRates)
    {
        auto native = settings::GetConfig();
        native.frameRate = fps;
        Check(settings::SaveConfig(native), "save native frame-rate preset");
        Check(settings::GetConfig().frameRate == fps && settings::Read().frameRate == fps,
            "native frame rate survives runtime validation and INI readback");
        Check(Contents().find("frame_rate=" + std::to_string(fps) + "\n") != std::string::npos,
            "native frame rate is persisted as FPS, not menu index");
    }
    settings::Config fg = settings::GetConfig();
    fg.frameGenerationProvider = framegen::Provider::Dlss;
    fg.frameGenerationMode = framegen::Mode::Dynamic;
    fg.frameGenerationMultiplier = 4;
    fg.frameGenerationTargetFps = 144;
    Check(settings::SaveConfig(fg), "save frame-generation settings");
    const auto restoredFg = settings::Read();
    Check(restoredFg.frameGenerationProvider == framegen::Provider::Dlss &&
          restoredFg.frameGenerationMode == framegen::Mode::Dynamic &&
          restoredFg.frameGenerationMultiplier == 4 && restoredFg.frameGenerationTargetFps == 144,
        "frame-generation settings survive disk readback");
    fg.frameGenerationProvider = framegen::Provider::Fsr;
    Check(settings::SaveConfig(fg), "save FSR frame-generation settings");
    const auto restoredFsr = settings::Read();
    Check(restoredFsr.frameGenerationProvider == framegen::Provider::Fsr &&
          restoredFsr.frameGenerationMode == framegen::Mode::Fixed &&
          restoredFsr.frameGenerationMultiplier == 2 && restoredFsr.frameGenerationTargetFps == 0,
        "FSR is normalized to fixed 2x");
    Write("frame_generation_provider=257\nframe_generation_mode=257\nframe_generation_multiplier=99\nframe_generation_target_fps=9999\n");
    const auto malformedFg = settings::Read();
    Check(malformedFg.frameGenerationProvider == framegen::Provider::Off &&
          malformedFg.frameGenerationMode == framegen::Mode::Fixed &&
          malformedFg.frameGenerationMultiplier == 2 && malformedFg.frameGenerationTargetFps == 0,
        "malformed frame-generation settings use safe defaults");
    Write("hdr=2\nhdr_paper_white_nits=-1\nhdr_peak_nits=garbage\n");
    const auto malformedHdr = settings::Read();
    Check(!malformedHdr.hdr && malformedHdr.hdrPaperWhiteNits == 203 && malformedHdr.hdrPeakNits == 1000 &&
          malformedHdr.hdrPeakAutomatic,
        "invalid HDR values retain safe defaults");
    Write("hdr=1\nhdr_paper_white_nits=1\nhdr_peak_nits=4294967295\n");
    const auto boundedHdr = settings::Read();
    Check(boundedHdr.hdr && boundedHdr.hdrPaperWhiteNits == 80 && boundedHdr.hdrPeakNits == 10000 &&
          !boundedHdr.hdrPeakAutomatic, "legacy HDR peak migrates as manual and is bounded on read");
    settings::Config hdr = settings::GetConfig();
    hdr.hdr = true;
    hdr.hdrPaperWhiteNits = 225;
    hdr.hdrPeakAutomatic = false;
    hdr.hdrPeakNits = 1200;
    Check(settings::SaveConfig(hdr), "save HDR preferences");
    Check(settings::Read().hdr && settings::Read().hdrPaperWhiteNits == 225 &&
          !settings::Read().hdrPeakAutomatic && settings::Read().hdrPeakNits == 1200 &&
          Contents().find("hdr=1\nhdr_paper_white_nits=225\nhdr_peak_auto=0\nhdr_peak_nits=1200\n") != std::string::npos,
        "HDR preferences roundtrip with stable keys");
    hdr.hdrPeakAutomatic = true;
    Check(settings::SaveConfig(hdr) && settings::Read().hdrPeakAutomatic && settings::Read().hdrPeakNits == 1200,
        "automatic peak retains the manual value for later selection");
    settings::Config previewHdr = hdr;
    previewHdr.hdrPaperWhiteNits = 260;
    settings::PreviewConfig(previewHdr);
    Check(settings::GetConfig().hdrPaperWhiteNits == 260 && settings::Read().hdrPaperWhiteNits == 225,
        "HDR preview does not commit to disk");
    Check(settings::SaveDebugLanguage(0) && settings::Read().hdrPaperWhiteNits == 225,
        "unrelated settings save preserves persisted HDR values over a preview");
    std::puts("PASS isolated save-anywhere toggle, INI roundtrip and fresh-process restore");
    return 0;
}
