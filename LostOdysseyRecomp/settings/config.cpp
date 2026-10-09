#include "config.h"
#include <debug/fast_forward.h>
#include <gpu/frame_rate.h>
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
// One settings.ini line: no line breaks, bounded like the adapter/display names it stores.
std::string TextValue(std::string text)
{
    while (!text.empty() && (text.back() == '\r' || text.back() == '\n')) text.pop_back();
    return text.size() <= 256 && text.find_first_of("\r\n") == std::string::npos ? text : std::string{};
}
Config Validate(Config value)
{
    value.gpuDevice = TextValue(std::move(value.gpuDevice));
    value.displayName = TextValue(std::move(value.displayName));
    if (value.displayName.empty() || value.displayIndex > 63) value.displayIndex = 0;
    if (value.internalResolution != 0 && value.internalResolution != InternalResolutionNative &&
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
    value.audioMatrixPhase = std::min((value.audioMatrixPhase + 7) / 15 * 15, 180u);
    if (value.buttonPrompts > 2) value.buttonPrompts = 0;
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
    if (value.debugLanguage > 1) value.debugLanguage = 0;
    if (value.fastForwardMode > 1) value.fastForwardMode = 0;
    namespace ff = debug_menu::fast_forward;
    if (std::find(std::begin(ff::Rates), std::end(ff::Rates), value.fastForwardRate) == std::end(ff::Rates))
        value.fastForwardRate = 2;
    if (value.uiLanguage > 4)
        value.uiLanguage = 0;
    if (GameLanguageIds[GameLanguageIndex(value.gameLanguage)] != value.gameLanguage)
        value.gameLanguage = 1;
    if (uint32_t(value.windowMode) > 1)
        value.windowMode = WindowMode::Windowed;
    if (value.audioOutput > AudioOutputMatrix)
        value.audioOutput = AudioOutputStereo;
    if (!gpu::backend::Known(value.graphicsBackend))
#ifdef _WIN32
        value.graphicsBackend = GraphicsBackend::D3D12;
#else
        value.graphicsBackend = GraphicsBackend::Vulkan;
#endif
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
    return value;
}
Config Read()
{
    Config value;
    bool hasAntialiasing = false;
    bool hasHdrPeakAuto = false, hasHdrPeakNits = false, hasAspectRatio = false;
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
        if (name == "internal_resolution") value.internalResolution = 0;
        if (name == "anisotropic_filtering") value.anisotropicFiltering = 0;
        if (name == "aspect_ratio") hasAspectRatio = true;
        uint32_t number = 0;
        const auto digits = key.substr(equal + 1);
        if (name == "gpu_device" || name == "display_name")
        {
            (name == "gpu_device" ? value.gpuDevice : value.displayName) = TextValue(digits);
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
        else if (key == "dynamic_shadows" && number <= 1)
            value.dynamicShadows = number == 1;
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
        else if (key == "frame_rate")
            value.frameRate = number;
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
        else if (key == "save_anywhere" && number <= 1)
            value.saveAnywhere = number == 1;
        else if (key == "no_random_encounters" && number <= 1)
            value.noRandomEncounters = number == 1;
        else if (key == "fast_forward" && number <= 1)
            value.fastForward = number == 1;
        else if (key == "fast_forward_mode")
            value.fastForwardMode = number;
        else if (key == "fast_forward_rate")
            value.fastForwardRate = number;
        else if (key == "audio_output")
            value.audioOutput = number;
        else if (key == "audio_matrix_phase")
            value.audioMatrixPhase = number;
        else if (key == "automatic_updates")
        {
            // Unknown values keep the safe package default (enabled).
            if (number <= 1) value.automaticUpdates = number == 1;
        }
        else if (key == "debug_log" && number <= 1)
            value.debugLog = number == 1;
    }
    if (!hasAntialiasing) value.antialiasing = value.fxaa ? 1u : 0u;
    // Profiles written before automatic peak detection use their stored peak
    // as an explicit choice. A fresh profile follows the current display.
    if (hasHdrPeakNits && !hasHdrPeakAuto) value.hdrPeakAutomatic = false;
    // Before the Aspect ratio setting, a 21:9 output resolution was the
    // Widescreen switch; keep that shape.
    if (!hasAspectRatio) value.aspectRatio = gpu::aspect_ratio::Migrated(value.width, value.height);
    return Validate(value);
}
Config &Current()
{
    static Config config = Read();
    return config;
}
// Debug-menu choices are saved on their own, never through a graphics save.
void CopyFastForward(Config &to, const Config &from)
{
    to.fastForward = from.fastForward;
    to.fastForwardMode = from.fastForwardMode;
    to.fastForwardRate = from.fastForwardRate;
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
    merged.saveAnywhere = Current().saveAnywhere;
    merged.noRandomEncounters = Current().noRandomEncounters;
    CopyFastForward(merged, Current());
    merged.audioOutput = Current().audioOutput;
    Current() = merged;
}
uint32_t GameLanguage()
{
    static const uint32_t language = GetConfig().gameLanguage;
    return language;
}
static bool WriteConfig(const Config &value)
{
    const auto path = os::user_paths::SettingsPath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    const auto temporary = path.parent_path() / (path.filename().string() + ".tmp");
    std::ofstream output(temporary, std::ios::trunc);
    output << "ui_language=" << value.uiLanguage << "\ngame_language=" << value.gameLanguage
           << "\nwidth=" << value.width << "\nheight=" << value.height << "\nwindow_mode=" << uint32_t(value.windowMode)
           << "\naspect_ratio=" << uint32_t(value.aspectRatio)
           << "\ndisplay_name=" << value.displayName << "\ndisplay_index=" << value.displayIndex
           << "\ngraphics_backend=" << uint32_t(value.graphicsBackend)
           << "\ngpu_device=" << value.gpuDevice
           << "\ndebug_language=" << value.debugLanguage
           << "\nantialiasing=" << value.antialiasing << "\nframe_rate=" << value.frameRate
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
           << "\ndynamic_shadows=" << (value.dynamicShadows ? 1 : 0)
           << "\nculling=" << value.cullingPercent
           << "\nvibration=" << value.vibrationPercent
           << "\nbutton_prompts=" << value.buttonPrompts
            << "\nupscaler=" << uint32_t(value.upscaler) << "\ndlss_quality=" << uint32_t(value.dlssQuality)
           << "\ndlss_model=" << value.dlssModel
           << "\nfsr_quality=" << uint32_t(value.fsrQuality)
           << "\nfsr_sharpness=" << value.fsrSharpnessPercent
           << "\ndlss_neural_rendering=" << value.dlssNeuralRendering
           << "\ndlss_nr_preset=" << value.dlssNrPreset << "\ndlss_nr_style=" << value.dlssNrStyle
           << "\ndlss_nr_intensity=" << value.dlssNrIntensity << "\ndlss_nr_global_tone=" << value.dlssNrGlobalTone
           << "\ndlss_nr_local_tone=" << value.dlssNrLocalTone << "\ndlss_nr_structure=" << value.dlssNrStructure
           << "\ndlss_nr_skin=" << value.dlssNrSkin << "\ndlss_nr_auto_mask=" << (value.dlssNrAutoMask ? 1 : 0)
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
           << "\ndebug_log=" << (value.debugLog ? 1 : 0)
           << "\naudio_output=" << value.audioOutput
           << "\naudio_matrix_phase=" << value.audioMatrixPhase
           << "\nskip_shader_prebuild=" << (value.skipShaderPrebuild ? 1 : 0)
           << "\nsave_anywhere=" << (value.saveAnywhere ? 1 : 0)
           << "\nno_random_encounters=" << (value.noRandomEncounters ? 1 : 0)
           << "\nfast_forward=" << (value.fastForward ? 1 : 0)
           << "\nfast_forward_mode=" << value.fastForwardMode
           << "\nfast_forward_rate=" << value.fastForwardRate << '\n';
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
    value.saveAnywhere = Current().saveAnywhere;
    value.noRandomEncounters = Current().noRandomEncounters;
    CopyFastForward(value, Current());
    value.audioOutput = Current().audioOutput;
    if (!WriteConfig(value)) return false;
    Current() = value;
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
bool SaveDebugLanguage(uint32_t language)
{
    std::lock_guard lock(mutex);
    // Merge with disk, not an unrelated unconfirmed graphics preview.
    auto persisted = Read();
    persisted.debugLanguage = language <= 1 ? language : 0;
    if (!WriteConfig(persisted)) return false;
    Current().debugLanguage = persisted.debugLanguage;
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
bool SaveNoRandomEncounters(bool enabled)
{
    std::lock_guard lock(mutex);
    auto persisted = Read();
    persisted.noRandomEncounters = enabled;
    if (!WriteConfig(persisted)) return false;
    Current().noRandomEncounters = enabled;
    return true;
}
bool SaveFastForward(bool enabled, uint32_t mode, uint32_t rate)
{
    std::lock_guard lock(mutex);
    auto persisted = Read();
    persisted.fastForward = enabled;
    persisted.fastForwardMode = mode;
    persisted.fastForwardRate = rate;
    persisted = Validate(persisted);
    if (!WriteConfig(persisted)) return false;
    CopyFastForward(Current(), persisted);
    return true;
}
} // namespace settings
