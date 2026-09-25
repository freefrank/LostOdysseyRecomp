#include "config.h"
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
Config Validate(Config value)
{
    if (value.internalResolution != 0 && value.internalResolution != 720 && value.internalResolution != 1080 &&
        value.internalResolution != 1440 && value.internalResolution != 2160)
        value.internalResolution = 0;
    if (value.scalingQuality > 1) value.scalingQuality = 1;
    if (value.anisotropicFiltering != 0 && value.anisotropicFiltering != 2 && value.anisotropicFiltering != 4 &&
        value.anisotropicFiltering != 8 && value.anisotropicFiltering != 16) value.anisotropicFiltering = 0;
    if (!gpu::upscaling::KnownUpscaler(value.upscaler)) value.upscaler = gpu::upscaling::Upscaler::Off;
    value.dlssQuality = gpu::upscaling::NormalizeDlssQuality(value.dlssQuality);
    value.fsrQuality = gpu::upscaling::NormalizeFsrQuality(value.fsrQuality);
    value.fsrSharpnessPercent = std::min(value.fsrSharpnessPercent, 100u);
    if (value.antialiasing > 3) value.antialiasing = 0;
    value.fxaa = value.antialiasing == 1;
    if (value.frameRate != 30 && value.frameRate != 60 && value.frameRate != 120) value.frameRate = 30;
    if (value.debugLanguage > 1) value.debugLanguage = 0;
    if (value.uiLanguage > 4)
        value.uiLanguage = 0;
    if (GameLanguageIds[GameLanguageIndex(value.gameLanguage)] != value.gameLanguage)
        value.gameLanguage = 1;
    if (uint32_t(value.windowMode) > 2)
        value.windowMode = WindowMode::Windowed;
    if (!gpu::backend::Known(value.graphicsBackend))
#ifdef _WIN32
        value.graphicsBackend = GraphicsBackend::D3D12;
#else
        value.graphicsBackend = GraphicsBackend::Vulkan;
#endif
#ifndef _WIN32
    if (value.graphicsBackend == GraphicsBackend::D3D12 || value.graphicsBackend == GraphicsBackend::D3D11)
        value.graphicsBackend = GraphicsBackend::Vulkan;
#endif
    if (value.width < 640 || value.width > 7680 || value.height < 480 || value.height > 4320)
    {
        value.width = 1280;
        value.height = 720;
    }
    return value;
}
Config Read()
{
    Config value;
    bool hasAntialiasing = false;
    const auto path = os::user_paths::UsePortableLayout() ? std::filesystem::path("settings.ini") : os::user_paths::ConfigDir() / "settings.ini";
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
        uint32_t number = 0;
        const auto digits = key.substr(equal + 1);
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
        else if (key == "internal_resolution")
            value.internalResolution = number <= 2160 ? int(number) : 0;
        else if (key == "window_mode")
            value.windowMode = WindowMode(number);
        else if (key == "graphics_backend")
            value.graphicsBackend = GraphicsBackend(number);
        else if (key == "antialiasing")
            value.antialiasing = number;
        else if (key == "scaling_quality")
            value.scalingQuality = number;
        else if (key == "anisotropic_filtering")
            value.anisotropicFiltering = number;
        else if (key == "upscaler")
            value.upscaler = gpu::upscaling::Upscaler(number);
        else if (key == "dlss_quality")
            value.dlssQuality = gpu::upscaling::DlssQuality(number);
        else if (key == "fsr_quality")
            value.fsrQuality = gpu::upscaling::FsrQuality(number);
        else if (key == "fsr_sharpness")
            value.fsrSharpnessPercent = number;
        else if (key == "frame_rate")
            value.frameRate = number;
        else if (key == "fxaa")
            value.fxaa = number == 1;
        else if (key == "skip_shader_prebuild")
            value.skipShaderPrebuild = number == 1;
        else if (key == "save_anywhere" && number <= 1)
            value.saveAnywhere = number == 1;
        else if (key == "automatic_updates")
        {
            // Unknown values keep the safe package default (enabled).
            if (number <= 1) value.automaticUpdates = number == 1;
        }
    }
    if (!hasAntialiasing) value.antialiasing = value.fxaa ? 1u : 0u;
    return Validate(value);
}
Config &Current()
{
    static Config config = Read();
    return config;
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
    LOG_INFO("game edition: {}; {} game languages", europe ? "USA/Europe" : "Asia/default", GameLanguageIds.size());
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
    Current() = merged;
}
uint32_t GameLanguage()
{
    static const uint32_t language = GetConfig().gameLanguage;
    return language;
}
static bool WriteConfig(const Config &value)
{
    const auto path = os::user_paths::UsePortableLayout() ? std::filesystem::path("settings.ini") : os::user_paths::ConfigDir() / "settings.ini";
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    const auto temporary = path.parent_path() / (path.filename().string() + ".tmp");
    std::ofstream output(temporary, std::ios::trunc);
    output << "ui_language=" << value.uiLanguage << "\ngame_language=" << value.gameLanguage
           << "\nwidth=" << value.width << "\nheight=" << value.height << "\nwindow_mode=" << uint32_t(value.windowMode)
           << "\ngraphics_backend=" << uint32_t(value.graphicsBackend)
           << "\ndebug_language=" << value.debugLanguage
           << "\nantialiasing=" << value.antialiasing << "\nframe_rate=" << value.frameRate
           << "\nscaling_quality=" << value.scalingQuality
           << "\nanisotropic_filtering=" << value.anisotropicFiltering
           << "\nupscaler=" << uint32_t(value.upscaler) << "\ndlss_quality=" << uint32_t(value.dlssQuality)
           << "\nfsr_quality=" << uint32_t(value.fsrQuality)
           << "\nfsr_sharpness=" << value.fsrSharpnessPercent
           << "\ninternal_resolution=" << value.internalResolution
           << "\nfxaa=" << value.fxaa << "\nautomatic_updates=" << value.automaticUpdates
           << "\nskip_shader_prebuild=" << (value.skipShaderPrebuild ? 1 : 0)
           << "\nsave_anywhere=" << (value.saveAnywhere ? 1 : 0) << '\n';
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
    if (!WriteConfig(value)) return false;
    Current() = value;
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
} // namespace settings
