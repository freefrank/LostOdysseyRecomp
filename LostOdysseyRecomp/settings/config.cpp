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
    if (value.uiLanguage > 4)
        value.uiLanguage = 0;
    if (GameLanguageIds[GameLanguageIndex(value.gameLanguage)] != value.gameLanguage)
        value.gameLanguage = 1;
    if (uint32_t(value.windowMode) > 2)
        value.windowMode = WindowMode::Windowed;
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
    std::ifstream input("settings.ini");
    std::string key;
    while (std::getline(input, key))
    {
        auto equal = key.find('=');
        if (equal == std::string::npos)
            continue;
        uint32_t number = 0;
        const auto digits = key.substr(equal + 1);
        auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), number);
        if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size())
            continue;
        key.resize(equal);
        if (key == "ui_language")
            value.uiLanguage = number;
        else if (key == "game_language")
            value.gameLanguage = number;
        else if (key == "width")
            value.width = number;
        else if (key == "height")
            value.height = number;
        else if (key == "window_mode")
            value.windowMode = WindowMode(number);
        else if (key == "fxaa")
            value.fxaa = number == 1;
    }
    return Validate(value);
}
Config &Current()
{
    static Config config = Read();
    return config;
}
} // namespace
Config GetConfig()
{
    std::lock_guard lock(mutex);
    return Current();
}
void PreviewConfig(const Config &value)
{
    std::lock_guard lock(mutex);
    Current() = Validate(value);
}
uint32_t GameLanguage()
{
    static const uint32_t language = GetConfig().gameLanguage;
    return language;
}
bool SaveConfig(const Config &requested)
{
    std::lock_guard lock(mutex);
    const Config value = Validate(requested);
    std::ofstream output("settings.ini.tmp", std::ios::trunc);
    output << "ui_language=" << value.uiLanguage << "\ngame_language=" << value.gameLanguage
           << "\nwidth=" << value.width << "\nheight=" << value.height << "\nwindow_mode=" << uint32_t(value.windowMode)
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
    Current() = value;
    LOG_INFO("settings saved: {}x{} mode={} FXAA={} language={} (game language applies at restart)", value.width,
             value.height, uint32_t(value.windowMode), value.fxaa, value.gameLanguage);
    return true;
}
} // namespace settings
