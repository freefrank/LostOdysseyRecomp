#include "config.h"
#include <cstdlib>
#include <cstring>
#include <os/platform.h>
#if LO_PLATFORM_SWITCH
#include <os/switch_platform.h>
#endif
#include <debug/fast_forward.h>
#include <atomic>
#include <gpu/dynamic_resolution.h>
#include <gpu/frame_rate.h>
#include <gpu/loading_pacing.h>
#include <filesystem>
#include <fstream>
#include <os/logger.h>
#include <os/user_paths.h>
#include <gpu/dlss_status_log.h>
#include <stdafx.h>
namespace settings
{
namespace
{
std::mutex mutex;
std::atomic<uint32_t> menuAnimationRate{1};
// One settings.ini line: no line breaks, bounded like the adapter/display names it stores.
std::string TextValue(std::string text)
{
    while (!text.empty() && (text.back() == '\r' || text.back() == '\n')) text.pop_back();
    return text.size() <= 256 && text.find_first_of("\r\n") == std::string::npos ? text : std::string{};
}
// A language pack id as the mods loader accepts it, in lower case; anything else selects no pack.
std::string PackId(std::string text)
{
    for (auto &c : text) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    const bool valid = text.size() <= 128 && text != "." && text != ".." &&
        std::all_of(text.begin(), text.end(), [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
        });
    return valid ? text : std::string{};
}
} // namespace
bool Docked()
{
#if LO_PLATFORM_SWITCH
    return os::switch_platform::IsDocked();
#else
    return false;
#endif
}
uint32_t RenderHeightOverride()
{
#if LO_PLATFORM_SWITCH
    // LO_SWITCH_RENDER_HEIGHT=540 or 720, or any height dynamic resolution renders at (360 to 1071 rows, a multiple
    // of 9: 360, 450, 504, ...), for measuring how a scene's cost follows its pixels.
    static const uint32_t height = [] {
        const char *text = std::getenv("LO_SWITCH_RENDER_HEIGHT");
        const uint32_t rows = text && *text ? uint32_t(std::strtoul(text, nullptr, 10)) : 0;
        return rows == 540 || rows == 720 || gpu::dynamic_resolution::IsDynamicHeight(rows) ? rows : 0u;
    }();
    return height;
#else
    return 0;
#endif
}
uint32_t SwitchMaximumHeight(const Config &config, bool docked)
{
    const uint32_t height = config.maximumHeight[docked ? 1 : 0];
    return height ? height : SwitchAutomaticHeight(docked);
}
uint32_t SwitchMinimumHeight(const Config &config, bool docked)
{
    const uint32_t base = SwitchMaximumHeight(config, docked);
    const uint32_t minimum = config.minimumHeight[docked ? 1 : 0];
    return gpu::dynamic_resolution::Floor(base, minimum ? minimum : gpu::dynamic_resolution::DefaultMinimum(base));
}
uint32_t DynamicResolutionMinimum(const Config &config)
{
#if LO_PLATFORM_SWITCH
    return config.minimumHeight[Docked() ? 1 : 0];
#else
    (void)config;
    return 0;
#endif
}
namespace
{
Config Validate(Config value)
{
    value.encounterRate = ValidEncounterRate(value.encounterRate);
    value.gpuDevice = TextValue(std::move(value.gpuDevice));
    value.displayName = TextValue(std::move(value.displayName));
    value.gameLanguagePack = PackId(std::move(value.gameLanguagePack));
    if (value.displayName.empty() || value.displayIndex > 63) value.displayIndex = 0;
#if !LO_PLATFORM_SWITCH // 540 is the Switch handheld scene height (see below)
    // 540 was a render resolution in development builds; it now means the original 720.
    if (value.internalResolution == 540) value.internalResolution = 720;
#endif
    if (value.internalResolution != 0 && value.internalResolution != InternalResolutionNative &&
#if LO_PLATFORM_SWITCH
        value.internalResolution != 540 &&
#endif
        value.internalResolution != 720 && value.internalResolution != 1080 &&
        value.internalResolution != 1440 && value.internalResolution != 2160)
        value.internalResolution = 0;
#if LO_PLATFORM_MACOS
    if (value.scalingQuality > ScalingMetalFx) value.scalingQuality = 1;
#else
    if (value.scalingQuality > 1) value.scalingQuality = 1;
#endif
    if (value.anisotropicFiltering != 0 && value.anisotropicFiltering != 2 && value.anisotropicFiltering != 4 &&
        value.anisotropicFiltering != 8 && value.anisotropicFiltering != 16) value.anisotropicFiltering = 0;
    value.depthOfFieldPercent = std::min(value.depthOfFieldPercent, 100u);
    value.cullingPercent = std::min(value.cullingPercent, 200u);
    value.vibrationPercent = std::min(value.vibrationPercent, 100u);
    value.audioMatrixRear = std::clamp((value.audioMatrixRear + 5) / 10 * 10, 90u, 150u);
    value.voiceVolume = std::min(value.voiceVolume, 100u);
    if (value.buttonPrompts > 2) value.buttonPrompts = 0;
    if (value.performanceOverlay > 2) value.performanceOverlay = 0;
    value.controllerMap = hid::input_map::ValidController(value.controllerMap);
    value.keyboardMap = hid::input_map::ValidKeyboard(value.keyboardMap);
    value.hdrPaperWhiteNits = std::clamp(value.hdrPaperWhiteNits, 80u, 400u);
    value.hdrPeakNits = std::clamp(value.hdrPeakNits, 80u, 10000u);
    value.hdrPeakNits = std::max(value.hdrPeakNits, value.hdrPaperWhiteNits);
    value.displayBrightness = std::clamp(value.displayBrightness, -20, 20);
    value.displayGamma = std::clamp(value.displayGamma, 50u, 150u);
    if (!gpu::upscaling::KnownUpscaler(value.upscaler)) value.upscaler = gpu::upscaling::Upscaler::Off;
    value.dlssQuality = gpu::upscaling::NormalizeDlssQuality(value.dlssQuality);
    if (value.dlssModel > 1) value.dlssModel = 0;
    value.fsrQuality = gpu::upscaling::NormalizeFsrQuality(value.fsrQuality);
    value.fsrSharpnessPercent = std::min(value.fsrSharpnessPercent, 100u);
    value.dlssNeuralRendering = std::min(value.dlssNeuralRendering, DlssNeuralRenderingMaxPasses);
    value.dlssNrPreset = std::min(value.dlssNrPreset, 3u);
    value.dlssNrStyle = std::min(value.dlssNrStyle, 2u);
    for (uint32_t *percent : {&value.dlssNrIntensity, &value.dlssNrGlobalTone, &value.dlssNrLocalTone, &value.dlssNrStructure})
        *percent = std::min(*percent, 200u);
    value.dlssNrSkin = std::clamp(value.dlssNrSkin, -100, 100);
    if (!framegen::KnownProvider(value.frameGenerationProvider))
        value.frameGenerationProvider = framegen::Provider::Off;
    if (value.frameGenerationMode != framegen::Mode::Fixed && value.frameGenerationMode != framegen::Mode::Dynamic)
        value.frameGenerationMode = framegen::Mode::Fixed;
    if (value.frameGenerationMultiplier < 2 || value.frameGenerationMultiplier > framegen::kMaxMultiplier)
        value.frameGenerationMultiplier = 2;
    if (value.frameGenerationTargetFps > 1000) value.frameGenerationTargetFps = 0;
    // XeSS-FG offers more than 2x only on Intel GPUs; like FSR it is fixed at 2x here.
    if (value.frameGenerationProvider == framegen::Provider::Fsr || value.frameGenerationProvider == framegen::Provider::MetalFx ||
        value.frameGenerationProvider == framegen::Provider::Xess)
    {
        value.frameGenerationMode = framegen::Mode::Fixed;
        value.frameGenerationMultiplier = 2;
        value.frameGenerationTargetFps = 0;
    }
    if (value.antialiasing > 3) value.antialiasing = 0;
    if (value.shadowResolution != 1 && value.shadowResolution != 2 && value.shadowResolution != 4)
        value.shadowResolution = 1;
    if (value.ambientOcclusion > 2) value.ambientOcclusion = 0;
    value.fxaa = value.antialiasing == 1;
    value.frameRate = gpu::frame_rate::Normalize(value.frameRate);
    if (value.loadingTransitions != 0) value.loadingTransitions = 2;
    if (value.debugLanguage >= DebugLanguageCount) value.debugLanguage = 0;
    if (value.fastForwardMode > 1) value.fastForwardMode = 0;
    namespace ff = debug_menu::fast_forward;
    if (value.fastForwardButton > uint32_t(ff::Button::LB)) value.fastForwardButton = 0;
    if (std::find(std::begin(ff::Rates), std::end(ff::Rates), value.fastForwardRate) == std::end(ff::Rates))
        value.fastForwardRate = 2;
    if (std::find(std::begin(MenuAnimationRates), std::end(MenuAnimationRates), value.menuAnimationRate) == std::end(MenuAnimationRates))
        value.menuAnimationRate = 1;
    if (value.uiLanguage >= std::size(UiLanguageNames))
        value.uiLanguage = 0;
    if (GameLanguageIds[GameLanguageIndex(value.gameLanguage)] != value.gameLanguage)
        value.gameLanguage = 1;
    if (uint32_t(value.windowMode) > 1)
        value.windowMode = WindowMode::Windowed;
    if (value.audioOutput > AudioOutputMatrix)
        value.audioOutput = AudioOutputStereo;
    if (!gpu::backend::Known(value.graphicsBackend))
        value.graphicsBackend = GraphicsBackend::Vulkan;
#if LO_PLATFORM_MACOS
    value.graphicsBackend = GraphicsBackend::Metal; // The only macOS backend.
#elif !defined(_WIN32)
    if (value.graphicsBackend != GraphicsBackend::Vulkan)
        value.graphicsBackend = GraphicsBackend::Vulkan;
#endif
    if (value.width < 640 || value.width > 7680 || value.height < 480 || value.height > 4320)
    {
        value.width = 1280;
        value.height = 720;
    }
#if LO_PLATFORM_ANDROID
    // The phone owns the surface and the menu hides the setting.
    value.aspectRatio = gpu::aspect_ratio::Mode::Auto;
#else
    if (uint32_t(value.aspectRatio) >= gpu::aspect_ratio::ModeCount)
        value.aspectRatio = gpu::aspect_ratio::Mode::Auto;
#endif
#if LO_PLATFORM_SWITCH
    // The Xbox 360 image: the game's own scene, retail shadow maps and none
    // of the host AA, AO, filtering, upscaling or HDR passes. The menu hides them.
    // The scene renders at Graphics → Maximum resolution for the current mode:
    // automatically 960x540 in both modes (the GPU-bound console gains about a
    // third in battle; 720p docked is a choice, not yet tested with deko3d); the
    // final pass scales to the 720p output (bilinear, or FSR 1).
    // gpu/video.cpp re-applies this when the console is docked or undocked.
    // LO_SWITCH_RENDER_HEIGHT (env.txt, RenderHeightOverride) wins over the menu.
    for (uint32_t *heights : {value.maximumHeight, value.minimumHeight})
        for (uint32_t mode = 0; mode < 2; ++mode)
            if (std::find(std::begin(SwitchSceneHeights), std::end(SwitchSceneHeights), heights[mode]) == std::end(SwitchSceneHeights))
                heights[mode] = 0;
    // A minimum above its maximum is the maximum: no dynamic range.
    for (uint32_t mode = 0; mode < 2; ++mode)
        value.minimumHeight[mode] = std::min(value.minimumHeight[mode], SwitchMaximumHeight(value, mode == 1));
    if (const uint32_t rows = RenderHeightOverride()) value.internalResolution = int(rows);
    else value.internalResolution = int(SwitchMaximumHeight(value, Docked()));
    value.shadowResolution = 1;
    value.antialiasing = 0;
    value.fxaa = false;
    value.ambientOcclusion = 0;
    value.anisotropicFiltering = 0;
    value.upscaler = gpu::upscaling::Upscaler::Off;
    value.dlssNeuralRendering = 0;
    value.frameGenerationProvider = framegen::Provider::Off;
    value.hdr = false;
    // The guest's display gamma ramp already darkens the blacks like the Xbox 360;
    // expanding 16-235 on top of it crushes everything below 32/255 to black.
    value.expandRgbRange = false;
    // 30 FPS, the game's own rate and every preset's. The screen has no VRR and the console does not hold 30 in
    // the heavy areas, so 60 would pace unevenly and halve dynamic resolution's GPU budget (frame_plan.cpp);
    // 90/120 cannot be shown at 60 Hz. The menu hides the row; a PC settings.ini's 60/90/120 becomes 30.
    // LO_FPS (env.txt) stays the diagnostic override.
    value.frameRate = gpu::frame_rate::kDefault;
#endif
#if LO_PLATFORM_MACOS
    // Metal always keeps display sync; the menu offers only ProMotion there.
    value.vsync = true;
#endif
    return value;
}
#if LO_PLATFORM_SWITCH
// A Spanish console (Spain or Latin America) starts the game, the settings and the debug menu in
// Spanish; each one picked in a menu stays as picked. The USA/Europe edition alone has Spanish game
// text (ConfigureGameLanguages runs before the settings are first read), and a language pack is a pick.
void FollowSystemLanguage(Config &value)
{
    static const bool spanish = [] {
        const bool result = os::switch_platform::SystemLanguageIsSpanish();
        LOG_NOTICE("settings: console language is {}Spanish", result ? "" : "not ");
        return result;
    }();
    if (!spanish) return;
    if (!value.uiLanguageChosen) value.uiLanguage = UiLanguageSpanish;
    if (!value.debugLanguageChosen) value.debugLanguage = DebugLanguageSpanish;
    if (!value.gameLanguageChosen && value.gameLanguagePack.empty() &&
        GameLanguageIds[GameLanguageIndex(GameLanguageSpanish)] == GameLanguageSpanish)
        value.gameLanguage = GameLanguageSpanish;
}
#endif
Config Read()
{
    Config value;
    bool hasAntialiasing = false;
    bool hasEncounterRate = false, legacyNoEncounters = false;
    bool hasHdrPeakAuto = false, hasHdrPeakNits = false, hasAspectRatio = false, hasVsync = false;
    bool hasLanguageChosen = false;
    bool hasSwitchFsr = false;
    const auto path = os::user_paths::SettingsPath();
    std::ifstream input(path);
    std::string key;
    while (std::getline(input, key))
    {
        auto equal = key.find('=');
        if (equal == std::string::npos)
            continue;
        const auto name = key.substr(0, equal);
        // Presence wins over the legacy key even if the new value is malformed.
        if (name == "antialiasing") { hasAntialiasing = true; value.antialiasing = 0; }
        if (name == "encounter_rate") { hasEncounterRate = true; value.encounterRate = EncounterRate::Normal; }
        if (name == "internal_resolution") value.internalResolution = 0;
        if (name == "fast_forward_button") value.fastForwardButton = 0;
        if (name == "anisotropic_filtering") value.anisotropicFiltering = 0;
        if (name == "aspect_ratio") hasAspectRatio = true;
        uint32_t number = 0;
        const auto digits = key.substr(equal + 1);
        if (name == "gpu_device" || name == "display_name" || name == "game_language_pack")
        {
            (name == "gpu_device" ? value.gpuDevice : name == "display_name" ? value.displayName : value.gameLanguagePack) =
                TextValue(digits);
            continue;
        }
        if (name == "controller_map" || name == "keyboard_map")
        {
            // "input:value" pairs; an invalid pair keeps that input's default.
            std::string_view text = digits;
            while (!text.empty() && (text.back() == '\r' || text.back() == '\n')) text.remove_suffix(1);
            if (name == "controller_map") value.controllerMap = hid::input_map::ParseController(text);
            else value.keyboardMap = hid::input_map::ParseKeyboard(text);
            continue;
        }
        if (name == "display_brightness" || name == "dlss_nr_skin")
        {
            // The signed values.
            int signedNumber = 0;
            auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), signedNumber);
            if (parsed.ec == std::errc{} && parsed.ptr == digits.data() + digits.size())
                (name == "display_brightness" ? value.displayBrightness : value.dlssNrSkin) = signedNumber;
            continue;
        }
        auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), number);
        if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size())
            continue;
        key.resize(equal);
        if (key == "ui_language")
            value.uiLanguage = number;
        else if (key == "debug_language")
            value.debugLanguage = number;
        else if ((key == "ui_language_chosen" || key == "game_language_chosen" || key == "debug_language_chosen") && number <= 1)
        {
            hasLanguageChosen = true;
            (key == "ui_language_chosen" ? value.uiLanguageChosen : key == "game_language_chosen" ? value.gameLanguageChosen
                                                                                                : value.debugLanguageChosen) = number == 1;
        }
        else if (key == "game_language")
            value.gameLanguage = number;
        else if (key == "width")
            value.width = number;
        else if (key == "height")
            value.height = number;
        else if (key == "aspect_ratio")
            value.aspectRatio = gpu::aspect_ratio::Mode(number);
        else if (key == "internal_resolution")
            value.internalResolution = number <= 2160 ? int(number) : 0;
        else if (key == "window_mode")
            value.windowMode = number == 2 ? WindowMode::Borderless : WindowMode(number); // 2 was exclusive fullscreen.
        else if (key == "display_index")
            value.displayIndex = number;
        else if (key == "graphics_backend")
            value.graphicsBackend = GraphicsBackend(number);
        else if (key == "antialiasing")
            value.antialiasing = number;
        else if (key == "shadow_resolution")
            value.shadowResolution = number;
        else if (key == "ambient_occlusion")
            value.ambientOcclusion = number;
        else if (key == "scaling_quality")
            value.scalingQuality = number;
        else if (key == "expand_rgb_range" && number <= 1)
            value.expandRgbRange = number == 1;
        else if (key == "display_gamma")
            value.displayGamma = number;
        else if (key == "anisotropic_filtering")
            value.anisotropicFiltering = number;
        else if (key == "depth_of_field")
            value.depthOfFieldPercent = number;
        else if (key == "bloom" && number <= 1)
            value.bloom = number == 1;
        else if (key == "motion_blur" && number <= 1)
            value.motionBlur = number == 1;
        else if (key == "dynamic_shadows" && number <= 2)
            value.dynamicShadows = number;
        else if (key == "culling")
            value.cullingPercent = number;
        else if (key == "vibration")
            value.vibrationPercent = number;
        else if (key == "button_prompts")
            value.buttonPrompts = number;
        else if (key == "upscaler")
            value.upscaler = gpu::upscaling::Upscaler(number);
        else if (key == "dlss_quality")
            value.dlssQuality = gpu::upscaling::DlssQuality(number);
        else if (key == "dlss_model")
            value.dlssModel = number;
        else if (key == "fsr_quality")
            value.fsrQuality = gpu::upscaling::FsrQuality(number);
        else if (key == "fsr_sharpness")
            value.fsrSharpnessPercent = number;
        else if (key == "dlss_neural_rendering")
            value.dlssNeuralRendering = number;
        else if (key == "dlss_nr_preset")
            value.dlssNrPreset = number;
        else if (key == "dlss_nr_style")
            value.dlssNrStyle = number;
        else if (key == "dlss_nr_intensity")
            value.dlssNrIntensity = number;
        else if (key == "dlss_nr_global_tone")
            value.dlssNrGlobalTone = number;
        else if (key == "dlss_nr_local_tone")
            value.dlssNrLocalTone = number;
        else if (key == "dlss_nr_structure")
            value.dlssNrStructure = number;
        else if (key == "dlss_nr_auto_mask" && number <= 1)
            value.dlssNrAutoMask = number == 1;
        else if (key == "frame_generation_provider")
            value.frameGenerationProvider = number <= uint32_t(framegen::Provider::Xess)
                ? framegen::Provider(number) : framegen::Provider::Off;
        else if (key == "frame_generation_mode")
            value.frameGenerationMode = number <= uint32_t(framegen::Mode::Dynamic)
                ? framegen::Mode(number) : framegen::Mode::Fixed;
        else if (key == "frame_generation_multiplier")
            value.frameGenerationMultiplier = number;
        else if (key == "frame_generation_target_fps")
            value.frameGenerationTargetFps = number;
        else if (key == "variable_refresh_rate")
            value.variableRefreshRate = number == 1;
        else if (key == "vsync" && number <= 1)
        {
            value.vsync = number == 1;
            hasVsync = true;
        }
        else if (key == "frame_rate")
            value.frameRate = number;
        else if (key == "loading_transitions")
            value.loadingTransitions = number;
        else if (key == "hdr" && number <= 1)
            value.hdr = number == 1;
        else if (key == "hdr_paper_white_nits")
            value.hdrPaperWhiteNits = number;
        else if (key == "hdr_peak_nits")
        {
            value.hdrPeakNits = number;
            hasHdrPeakNits = true;
        }
        else if (key == "hdr_peak_auto" && number <= 1)
        {
            value.hdrPeakAutomatic = number == 1;
            hasHdrPeakAuto = true;
        }
        else if (key == "fxaa")
            value.fxaa = number == 1;
        else if (key == "skip_shader_prebuild")
            value.skipShaderPrebuild = number == 1;
        else if (key == "achievement_notifications" && number <= 1)
            value.achievementNotifications = number == 1;
        else if (key == "achievement_sound" && number <= 1)
            value.achievementSound = number == 1;
        else if (key == "performance_overlay")
            value.performanceOverlay = number;
        else if (key == "save_anywhere" && number <= 1)
            value.saveAnywhere = number == 1;
        else if (key == "no_random_encounters" && number <= 1)
            legacyNoEncounters = number == 1;
        else if (key == "encounter_rate")
            value.encounterRate = ValidEncounterRate(EncounterRate(number));
        else if (key == "battle_exp_double" && number <= 1)
            value.battleExpDouble = number == 1;
        else if (key == "fast_forward" && number <= 1)
            value.fastForward = number == 1;
        else if (key == "fast_forward_mode")
            value.fastForwardMode = number;
        else if (key == "fast_forward_button")
            value.fastForwardButton = number;
        else if (key == "fast_forward_rate")
            value.fastForwardRate = number;
        else if (key == "fast_forward_keep_pitch" && number <= 1)
            value.fastForwardKeepAudioPitch = number == 1;
        else if (key == "menu_animation_rate")
            value.menuAnimationRate = number;
#if LO_PLATFORM_SWITCH
        else if (key == "switch_fsr" && number <= 1)
        {
            hasSwitchFsr = true;
            value.switchFsr = number == 1;
        }
        else if (key == "dynamic_resolution" && number <= 1)
            value.dynamicResolution = number == 1;
        else if (key == "maximum_height_handheld" || key == "maximum_height_docked")
            value.maximumHeight[key == "maximum_height_docked" ? 1 : 0] = number;
        else if (key == "minimum_height_handheld" || key == "minimum_height_docked")
            value.minimumHeight[key == "minimum_height_docked" ? 1 : 0] = number;
        else if (key == "debug_deko3d_merge_barriers" && number <= 1)
            value.debug.deko3dMergeBarriers = number == 1;
        else if (key == "debug_deko3d_lean_barriers" && number <= 1)
            value.debug.deko3dLeanBarriers = number == 1;
        else if (key == "debug_deko3d_zcull" && number <= 1)
            value.debug.deko3dZcull = number == 1;
        else if (key == "debug_deko3d_depth_compression" && number <= 1)
            value.debug.deko3dDepthCompression = number == 1;
        else if (key == "debug_deko3d_color_compression" && number <= 1)
            value.debug.deko3dColorCompression = number == 1;
        else if (key == "debug_deko3d_vs_outputs" && number <= 1)
            value.debug.deko3dVsOutputs = number == 1;
        else if (key == "debug_server" && number <= 1)
            value.debug.debugServer = number == 1;
#endif
        else if (key == "audio_output")
            value.audioOutput = number;
        else if (key == "audio_matrix_rear")
            value.audioMatrixRear = number;
        else if (key == "voice_volume")
            value.voiceVolume = number;
        else if (key == "automatic_updates")
        {
            // Unknown values keep the safe package default (enabled).
            if (number <= 1) value.automaticUpdates = number == 1;
        }
        else if (key == "ask_compact_game_data" && number <= 1)
            value.askCompactGameData = number == 1;
        else if (key == "discord_status" && number <= 1)
            value.discordStatus = number == 1;
        // Ignore the old debug_log key: saved defaults were indistinguishable from opt-outs.
        else if (key == "diagnostic_log" && number <= 1)
            value.debugLog = number == 1;
    }
    if (!hasAntialiasing) value.antialiasing = value.fxaa ? 1u : 0u;
    if (!hasEncounterRate && legacyNoEncounters) value.encounterRate = EncounterRate::None;
    // Profiles written before automatic peak detection use their stored peak
    // as an explicit choice. A fresh profile follows the current display.
    if (hasHdrPeakNits && !hasHdrPeakAuto) value.hdrPeakAutomatic = false;
    // Before the Aspect ratio setting, a 21:9 output resolution was the
    // Widescreen switch; keep that shape.
    if (!hasAspectRatio) value.aspectRatio = gpu::aspect_ratio::Migrated(value.width, value.height);
    // Before the VSync setting, 90/120 FPS presented without display sync.
    if (!hasVsync) value.vsync = value.frameRate <= 60;
    // Files from before the console's language was followed: a language other than the default was picked.
    if (!hasLanguageChosen)
    {
        value.uiLanguageChosen = value.uiLanguage != Config{}.uiLanguage;
        value.gameLanguageChosen = value.gameLanguage != Config{}.gameLanguage;
        value.debugLanguageChosen = value.debugLanguage != Config{}.debugLanguage;
    }
#if LO_PLATFORM_SWITCH
    // Files from before the FSR row saved fsr_sharpness=0 while the row was hidden: take the Switch's default.
    if (!hasSwitchFsr) value.fsrSharpnessPercent = Config{}.fsrSharpnessPercent;
    FollowSystemLanguage(value);
#endif
    (void)hasSwitchFsr;
    return Validate(value);
}
Config &Current()
{
    static Config config = [] {
        auto value = Read();
        menuAnimationRate.store(value.menuAnimationRate, std::memory_order_relaxed);
        gpu::loading::SetMode(value.loadingTransitions);
        return value;
    }();
    return config;
}
// Debug-menu choices are saved on their own, never through a graphics save.
void CopyFastForward(Config &to, const Config &from)
{
    to.fastForward = from.fastForward;
    to.fastForwardMode = from.fastForwardMode;
    to.fastForwardButton = from.fastForwardButton;
    to.fastForwardRate = from.fastForwardRate;
    to.fastForwardKeepAudioPitch = from.fastForwardKeepAudioPitch;
}
} // namespace
void ConfigureGameLanguages(const std::filesystem::path &xexPath)
{
    // Read bounded XEX execution metadata directly; works for manually extracted
    // folders as well as the installer. Import-time SHA256 validates the build.
    std::ifstream input(xexPath, std::ios::binary);
    std::array<unsigned char, 65536> data{};
    input.read(reinterpret_cast<char *>(data.data()), data.size());
    const size_t size = size_t(input.gcount());
    auto be32 = [&](size_t offset) -> uint32_t {
        return (uint32_t(data[offset]) << 24) | (uint32_t(data[offset + 1]) << 16) |
               (uint32_t(data[offset + 2]) << 8) | data[offset + 3];
    };
    bool europe = false;
    if (size >= 24 && memcmp(data.data(), "XEX2", 4) == 0)
    {
        const uint32_t count = be32(20);
        if (count <= 1024 && 24 + size_t(count) * 8 <= size)
            for (uint32_t i = 0; i < count; ++i)
                if (be32(24 + i * 8) == 0x40006)
                {
                    const size_t offset = be32(28 + i * 8);
                    if (offset + 24 > size) break;
                    constexpr uint32_t mediaIds[] = {0x368DE6DD, 0x1888BE4E, 0x6DD59D08, 0x0C0E80B5};
                    const auto disc = data[offset + 18];
                    europe = be32(offset + 12) == 0x4D5307FA && be32(offset + 4) == 3 &&
                             disc >= 1 && disc <= 4 && data[offset + 19] == 4 && be32(offset) == mediaIds[disc - 1];
                    break;
                }
    }
    GameLanguageIds = europe ? std::span<const uint32_t>(EuropeLanguageIds) : std::span<const uint32_t>(AsiaLanguageIds);
    GameLanguageNames = europe ? std::span<const wchar_t *const>(EuropeLanguageNames) : std::span<const wchar_t *const>(AsiaLanguageNames);
    LOG_NOTICE("game edition: {}; {} game languages", europe ? "USA/Europe" : "Asia/default", GameLanguageIds.size());
}
Config GetConfig()
{
    std::lock_guard lock(mutex);
    return Current();
}
void PreviewConfig(const Config &value)
{
    std::lock_guard lock(mutex);
    auto merged = Validate(value);
    merged.debugLanguage = Current().debugLanguage;
    merged.debugLanguageChosen = Current().debugLanguageChosen;
    merged.saveAnywhere = Current().saveAnywhere;
    merged.encounterRate = Current().encounterRate;
    merged.battleExpDouble = Current().battleExpDouble;
    CopyFastForward(merged, Current());
    merged.menuAnimationRate = Current().menuAnimationRate;
    merged.debug = Current().debug;
    merged.audioOutput = Current().audioOutput;
    Current() = merged;
    gpu::loading::SetMode(merged.loadingTransitions);
}
uint32_t GameLanguage()
{
    static const uint32_t language = GetConfig().gameLanguage;
    return language;
}
std::string GameLanguagePack()
{
    static const std::string pack = [] {
        const auto id = GetConfig().gameLanguagePack;
        for (const auto &choice : GameLanguageChoices())
            if (!id.empty() && choice.pack == id && choice.id == GameLanguage()) return id;
        return std::string{};
    }();
    return pack;
}
static bool WriteConfig(const Config &value)
{
    const auto path = os::user_paths::SettingsPath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    const auto temporary = path.parent_path() / (path.filename().string() + ".tmp");
    std::ofstream output(temporary, std::ios::trunc);
    output << "ui_language=" << value.uiLanguage << "\ngame_language=" << value.gameLanguage
           << "\ngame_language_pack=" << value.gameLanguagePack
           << "\nwidth=" << value.width << "\nheight=" << value.height << "\nwindow_mode=" << uint32_t(value.windowMode)
           << "\naspect_ratio=" << uint32_t(value.aspectRatio)
           << "\ndisplay_name=" << value.displayName << "\ndisplay_index=" << value.displayIndex
           << "\ngraphics_backend=" << uint32_t(value.graphicsBackend)
           << "\ngpu_device=" << value.gpuDevice
           << "\ndebug_language=" << value.debugLanguage
           << "\nantialiasing=" << value.antialiasing << "\nframe_rate=" << value.frameRate
           << "\nloading_transitions=" << value.loadingTransitions
           << "\nshadow_resolution=" << value.shadowResolution
           << "\nambient_occlusion=" << value.ambientOcclusion
           << "\nscaling_quality=" << value.scalingQuality
           << "\nexpand_rgb_range=" << (value.expandRgbRange ? 1 : 0)
           << "\ndisplay_brightness=" << value.displayBrightness
           << "\ndisplay_gamma=" << value.displayGamma
           << "\nanisotropic_filtering=" << value.anisotropicFiltering
           << "\ndepth_of_field=" << value.depthOfFieldPercent
           << "\nbloom=" << (value.bloom ? 1 : 0)
           << "\nmotion_blur=" << (value.motionBlur ? 1 : 0)
           << "\ndynamic_shadows=" << value.dynamicShadows
           << "\nculling=" << value.cullingPercent
           << "\nvibration=" << value.vibrationPercent
           << "\nbutton_prompts=" << value.buttonPrompts
           << "\ncontroller_map=" << hid::input_map::FormatController(value.controllerMap)
           << "\nkeyboard_map=" << hid::input_map::FormatKeyboard(value.keyboardMap)
            << "\nupscaler=" << uint32_t(value.upscaler) << "\ndlss_quality=" << uint32_t(value.dlssQuality)
           << "\ndlss_model=" << value.dlssModel
           << "\nfsr_quality=" << uint32_t(value.fsrQuality)
           << "\nfsr_sharpness=" << value.fsrSharpnessPercent
           << "\ndlss_neural_rendering=" << value.dlssNeuralRendering
           << "\ndlss_nr_preset=" << value.dlssNrPreset << "\ndlss_nr_style=" << value.dlssNrStyle
           << "\ndlss_nr_intensity=" << value.dlssNrIntensity << "\ndlss_nr_global_tone=" << value.dlssNrGlobalTone
           << "\ndlss_nr_local_tone=" << value.dlssNrLocalTone << "\ndlss_nr_structure=" << value.dlssNrStructure
           << "\ndlss_nr_skin=" << value.dlssNrSkin << "\ndlss_nr_auto_mask=" << (value.dlssNrAutoMask ? 1 : 0)
           << "\nvsync=" << (value.vsync ? 1 : 0)
           << "\nvariable_refresh_rate=" << (value.variableRefreshRate ? 1 : 0)
           << "\nhdr=" << (value.hdr ? 1 : 0)
           << "\nhdr_paper_white_nits=" << value.hdrPaperWhiteNits
           << "\nhdr_peak_auto=" << (value.hdrPeakAutomatic ? 1 : 0)
           << "\nhdr_peak_nits=" << value.hdrPeakNits
           << "\nframe_generation_provider=" << uint32_t(value.frameGenerationProvider)
           << "\nframe_generation_mode=" << uint32_t(value.frameGenerationMode)
           << "\nframe_generation_multiplier=" << value.frameGenerationMultiplier
           << "\nframe_generation_target_fps=" << value.frameGenerationTargetFps
           << "\ninternal_resolution=" << value.internalResolution
           << "\nfxaa=" << value.fxaa << "\nautomatic_updates=" << value.automaticUpdates
           << "\ndiscord_status=" << (value.discordStatus ? 1 : 0)
           << "\nask_compact_game_data=" << (value.askCompactGameData ? 1 : 0)
           << "\ndiagnostic_log=" << (value.debugLog ? 1 : 0)
           << "\naudio_output=" << value.audioOutput
           << "\naudio_matrix_rear=" << value.audioMatrixRear
           << "\nvoice_volume=" << value.voiceVolume
           << "\nskip_shader_prebuild=" << (value.skipShaderPrebuild ? 1 : 0)
           << "\nachievement_notifications=" << (value.achievementNotifications ? 1 : 0)
           << "\nachievement_sound=" << (value.achievementSound ? 1 : 0)
           << "\nperformance_overlay=" << value.performanceOverlay
           << "\nsave_anywhere=" << (value.saveAnywhere ? 1 : 0)
           << "\nencounter_rate=" << uint32_t(value.encounterRate)
           << "\nbattle_exp_double=" << (value.battleExpDouble ? 1 : 0)
           << "\nfast_forward=" << (value.fastForward ? 1 : 0)
           << "\nfast_forward_mode=" << value.fastForwardMode
           << "\nfast_forward_button=" << value.fastForwardButton
           << "\nfast_forward_rate=" << value.fastForwardRate
           << "\nfast_forward_keep_pitch=" << (value.fastForwardKeepAudioPitch ? 1 : 0)
           << "\nmenu_animation_rate=" << value.menuAnimationRate << '\n';
#if LO_PLATFORM_SWITCH
    const auto &debug = value.debug;
    output << "debug_deko3d_merge_barriers=" << (debug.deko3dMergeBarriers ? 1 : 0)
           << "\ndebug_deko3d_lean_barriers=" << (debug.deko3dLeanBarriers ? 1 : 0)
           << "\ndebug_deko3d_zcull=" << (debug.deko3dZcull ? 1 : 0)
           << "\ndebug_deko3d_depth_compression=" << (debug.deko3dDepthCompression ? 1 : 0)
           << "\ndebug_deko3d_color_compression=" << (debug.deko3dColorCompression ? 1 : 0)
           << "\ndebug_deko3d_vs_outputs=" << (debug.deko3dVsOutputs ? 1 : 0)
           << "\ndebug_server=" << (debug.debugServer ? 1 : 0)
           << "\nswitch_fsr=" << (value.switchFsr ? 1 : 0)
           << "\ndynamic_resolution=" << (value.dynamicResolution ? 1 : 0)
           << "\nmaximum_height_handheld=" << value.maximumHeight[0]
           << "\nmaximum_height_docked=" << value.maximumHeight[1]
           << "\nminimum_height_handheld=" << value.minimumHeight[0]
           << "\nminimum_height_docked=" << value.minimumHeight[1]
           << "\nui_language_chosen=" << (value.uiLanguageChosen ? 1 : 0)
           << "\ngame_language_chosen=" << (value.gameLanguageChosen ? 1 : 0)
           << "\ndebug_language_chosen=" << (value.debugLanguageChosen ? 1 : 0) << '\n';
#endif
    output.flush();
    if (!output)
        return false;
    output.close();
    if (!output)
        return false;
#ifdef _WIN32
    if (!MoveFileExW(temporary.wstring().c_str(), path.wstring().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        return false;
#else
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error)
        return false;
#endif
    LogSettingsSaved(value);
    return true;
}
bool SaveConfig(const Config &requested)
{
    std::lock_guard lock(mutex);
    auto value = Validate(requested);
    value.debugLanguage = Current().debugLanguage;
    value.debugLanguageChosen = Current().debugLanguageChosen;
    // A language other than the one in use was picked: it no longer follows the console.
    value.uiLanguageChosen = Current().uiLanguageChosen || value.uiLanguage != Current().uiLanguage;
    value.gameLanguageChosen = Current().gameLanguageChosen || value.gameLanguage != Current().gameLanguage ||
                               value.gameLanguagePack != Current().gameLanguagePack;
    value.saveAnywhere = Current().saveAnywhere;
    value.encounterRate = Current().encounterRate;
    value.battleExpDouble = Current().battleExpDouble;
    CopyFastForward(value, Current());
    value.menuAnimationRate = Current().menuAnimationRate;
    value.debug = Current().debug;
    value.audioOutput = Current().audioOutput;
    if (!WriteConfig(value)) return false;
    Current() = value;
    gpu::loading::SetMode(value.loadingTransitions);
    os::logger::SetDebugLog(value.debugLog);
    return true;
}
bool SaveAudioOutput(uint32_t output)
{
    std::lock_guard lock(mutex);
    // Merge with the persisted settings, not a pending graphics preview.
    auto persisted = Read();
    persisted.audioOutput = output <= AudioOutputMatrix ? output : AudioOutputStereo;
    if (!WriteConfig(persisted)) return false;
    Current().audioOutput = persisted.audioOutput;
    return true;
}
bool SaveDisplayChoice(const std::string &name, uint32_t index)
{
    std::lock_guard lock(mutex);
    // Merge with the persisted settings, not a pending graphics preview.
    auto persisted = Read();
    persisted.displayName = name;
    persisted.displayIndex = index;
    persisted = Validate(persisted);
    if (!WriteConfig(persisted)) return false;
    Current().displayName = persisted.displayName;
    Current().displayIndex = persisted.displayIndex;
    return true;
}
bool SaveGraphicsBackend(GraphicsBackend backend)
{
    std::lock_guard lock(mutex);
    // Merge with the persisted settings, not a pending graphics preview.
    auto persisted = Read();
    persisted.graphicsBackend = backend;
    persisted = Validate(persisted);
    if (!WriteConfig(persisted)) return false;
    Current().graphicsBackend = persisted.graphicsBackend;
    return true;
}
bool SaveDebugLanguage(uint32_t language)
{
    std::lock_guard lock(mutex);
    // Merge with disk, not an unrelated unconfirmed graphics preview.
    auto persisted = Read();
    const uint32_t chosen = language < DebugLanguageCount ? language : 0;
    persisted.debugLanguageChosen = persisted.debugLanguageChosen || chosen != persisted.debugLanguage;
    persisted.debugLanguage = chosen;
    if (!WriteConfig(persisted)) return false;
    Current().debugLanguage = persisted.debugLanguage;
    Current().debugLanguageChosen = persisted.debugLanguageChosen;
    return true;
}
bool SaveSaveAnywhere(bool enabled)
{
    std::lock_guard lock(mutex);
    // Merge with the persisted settings, not a pending graphics preview.
    auto persisted = Read();
    persisted.saveAnywhere = enabled;
    if (!WriteConfig(persisted)) return false;
    Current().saveAnywhere = enabled;
    return true;
}
bool SaveEncounterRate(EncounterRate rate)
{
    std::lock_guard lock(mutex);
    auto persisted = Read();
    persisted.encounterRate = ValidEncounterRate(rate);
    if (!WriteConfig(persisted)) return false;
    Current().encounterRate = persisted.encounterRate;
    return true;
}
bool SaveBattleExpDouble(bool enabled)
{
    std::lock_guard lock(mutex);
    auto persisted = Read();
    persisted.battleExpDouble = enabled;
    if (!WriteConfig(persisted)) return false;
    Current().battleExpDouble = enabled;
    return true;
}
bool SaveFastForward(bool enabled, uint32_t mode, uint32_t rate, bool keepAudioPitch, uint32_t button)
{
    std::lock_guard lock(mutex);
    auto persisted = Read();
    persisted.fastForward = enabled;
    persisted.fastForwardMode = mode;
    persisted.fastForwardButton = button;
    persisted.fastForwardRate = rate;
    persisted.fastForwardKeepAudioPitch = keepAudioPitch;
    persisted = Validate(persisted);
    if (!WriteConfig(persisted)) return false;
    CopyFastForward(Current(), persisted);
    return true;
}
bool SaveMenuAnimationRate(uint32_t rate)
{
    std::lock_guard lock(mutex);
    // Keep unconfirmed graphics previews out of this debug-only save.
    auto persisted = Read();
    persisted.menuAnimationRate = rate;
    persisted = Validate(persisted);
    if (!WriteConfig(persisted)) return false;
    Current().menuAnimationRate = persisted.menuAnimationRate;
    menuAnimationRate.store(persisted.menuAnimationRate, std::memory_order_relaxed);
    return true;
}
bool SaveDebugOptions(const DebugOptions &options)
{
    std::lock_guard lock(mutex);
    // Merge with the persisted settings, not a pending graphics preview.
    auto persisted = Read();
    persisted.debug = options;
    if (!WriteConfig(persisted)) return false;
    Current().debug = options;
    return true;
}
uint32_t MenuAnimationRate()
{
    Current(); // Initialize the persisted choice once before reading its atomic mirror.
    return menuAnimationRate.load(std::memory_order_relaxed);
}
} // namespace settings
