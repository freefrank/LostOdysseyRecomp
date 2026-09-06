#pragma once
#include <cstdint>
namespace settings
{
enum class WindowMode : uint32_t
{
    Windowed,
    Borderless,
    Exclusive
};
// Stable persisted IDs: retain the original EN/TW UI values.
inline constexpr const wchar_t *UiLanguageNames[] = {L"English", L"繁體中文", L"日本語", L"한국어", L"简体中文"};
// Guest table at 832455F0 maps these IDs to INT/JPN/KOR/CHI/SCH.
inline constexpr uint32_t GameLanguageIds[] = {1, 2, 7, 8, 9};
inline constexpr const wchar_t *GameLanguageNames[] = {L"English", L"日本語", L"한국어", L"繁體中文", L"简体中文"};
inline uint32_t GameLanguageIndex(uint32_t id)
{
    for (uint32_t i = 0; i < 5; ++i)
        if (GameLanguageIds[i] == id)
            return i;
    return 0;
}
struct Config
{
    uint32_t uiLanguage = 0;
    uint32_t gameLanguage = 1;
    uint32_t width = 1280, height = 720;
    WindowMode windowMode = WindowMode::Windowed;
    bool fxaa = false;
    bool operator==(const Config &) const = default;
};
Config GetConfig();
void PreviewConfig(const Config &config);
// Atomic replacement, preserving the previous file if writing fails.
bool SaveConfig(const Config &config);
uint32_t GameLanguage();
} // namespace settings
