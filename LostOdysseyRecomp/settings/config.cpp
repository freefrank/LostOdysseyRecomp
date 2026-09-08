#include "config.h"
#include <filesystem>
#include <fstream>
#include <os/logger.h>
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
    if (uint32_t(value.graphicsBackend) > uint32_t(GraphicsBackend::Vulkan))
        value.graphicsBackend = GraphicsBackend::D3D12;
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
    std::ifstream input("settings.ini");
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
        else if (key == "frame_rate")
            value.frameRate = number;
        else if (key == "fxaa")
            value.fxaa = number == 1;
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
    Current() = merged;
}
uint32_t GameLanguage()
{
    static const uint32_t language = GetConfig().gameLanguage;
    return language;
}
static bool WriteConfig(const Config &value)
{
    std::ofstream output("settings.ini.tmp", std::ios::trunc);
    output << "ui_language=" << value.uiLanguage << "\ngame_language=" << value.gameLanguage
           << "\nwidth=" << value.width << "\nheight=" << value.height << "\nwindow_mode=" << uint32_t(value.windowMode)
           << "\ngraphics_backend=" << uint32_t(value.graphicsBackend)
           << "\ndebug_language=" << value.debugLanguage
           << "\nantialiasing=" << value.antialiasing << "\nframe_rate=" << value.frameRate
           << "\nscaling_quality=" << value.scalingQuality
           << "\ninternal_resolution=" << value.internalResolution
           << "\nfxaa=" << value.fxaa << '\n';
    output.flush();
    if (!output)
        return false;
    output.close();
    if (!output)
        return false;
#ifdef _WIN32
    if (!MoveFileExW(L"settings.ini.tmp", L"settings.ini", MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        return false;
#else
    std::error_code error;
    std::filesystem::rename("settings.ini.tmp", "settings.ini", error);
    if (error)
        return false;
#endif
    LOG_INFO("settings saved: {}x{} internal_resolution={} mode={} backend={} AA={} language={} (backend/game language apply at restart)",
             value.width, value.height, value.internalResolution, uint32_t(value.windowMode),
             uint32_t(value.graphicsBackend), value.antialiasing, value.gameLanguage);
    return true;
}
bool SaveConfig(const Config &requested)
{
    std::lock_guard lock(mutex);
    auto value = Validate(requested);
    value.debugLanguage = Current().debugLanguage;
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
} // namespace settings
