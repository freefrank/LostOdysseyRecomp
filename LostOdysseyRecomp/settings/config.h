#pragma once
#include <cstdint>
#include <filesystem>
#include <span>
#include "gpu/backend_selection.h"
#include "gpu/upscaling_plan.h"
#include "../../shared/frame_generation/core.h"
namespace settings
{
enum class WindowMode : uint32_t
{
    Windowed,
    Borderless,
    Exclusive
};
using GraphicsBackend = gpu::backend::Backend;
// Stable persisted IDs: retain the original EN/TW UI values.
inline constexpr const wchar_t *UiLanguageNames[] = {L"English", L"繁體中文", L"日本語", L"한국어", L"简体中文"};
// Guest table at 832455F0 maps IDs 1-9 to INT/JPN/DEU/FRA/SPA/ITA/KOR/CHI/SCH.
inline constexpr uint32_t AsiaLanguageIds[] = {1, 2, 7, 8, 9};
inline constexpr const wchar_t *AsiaLanguageNames[] = {L"English", L"日本語", L"한국어", L"繁體中文", L"简体中文"};
inline constexpr uint32_t EuropeLanguageIds[] = {1, 2, 3, 4, 5, 6};
inline constexpr const wchar_t *EuropeLanguageNames[] = {L"English", L"日本語", L"Deutsch", L"Français", L"Español", L"Italiano"};
// Selected once before first-run setup and guest threads start.
inline std::span<const uint32_t> GameLanguageIds = AsiaLanguageIds;
inline std::span<const wchar_t *const> GameLanguageNames = AsiaLanguageNames;
void ConfigureGameLanguages(const std::filesystem::path &xexPath);
inline uint32_t GameLanguageIndex(uint32_t id)
{
    for (uint32_t i = 0; i < GameLanguageIds.size(); ++i)
        if (GameLanguageIds[i] == id)
            return i;
    return 0;
}
struct Config
{
    uint32_t uiLanguage = 0;
    uint32_t debugLanguage = 0; // Independent tool UI: 0 English, 1 Simplified Chinese.
    uint32_t gameLanguage = 1;
    uint32_t width = 1280, height = 720;
    int internalResolution = 0; // 0 follows output (up to 4K); 720/1080/1440/2160 select scene height.
    WindowMode windowMode = WindowMode::Windowed;
#ifdef _WIN32
    GraphicsBackend graphicsBackend = GraphicsBackend::D3D12; // Applied on the next process start.
#else
    GraphicsBackend graphicsBackend = GraphicsBackend::Vulkan; // Applied on the next process start.
#endif
    uint32_t antialiasing = 0; // 0 Off, 1 FXAA, 2 SMAA, 3 experimental camera-based TAA.
    uint32_t frameRate = 30;
    bool variableRefreshRate = false; // Opt-in VRR-friendly presentation; does not enable monitor/driver VRR.
    uint32_t scalingQuality = 1; // 0 bilinear, 1 bicubic spatial resampling.
    bool expandRgbRange = false; // Expand game image RGB 16-235 to 0-255 at presentation.
    uint32_t anisotropicFiltering = 0; // 0 Off, otherwise 2/4/8/16x. Applied live by the renderer.
    gpu::upscaling::Upscaler upscaler = gpu::upscaling::Upscaler::Off;
    gpu::upscaling::DlssQuality dlssQuality = gpu::upscaling::DlssQuality::Quality;
    gpu::upscaling::FsrQuality fsrQuality = gpu::upscaling::FsrQuality::Quality;
    uint32_t fsrSharpnessPercent = 0; // 0 disables FSR RCAS; 1-100 sets its strength.
    framegen::Provider frameGenerationProvider = framegen::Provider::Off; // D3D12; reconciled at the next presentation boundary.
    framegen::Mode frameGenerationMode = framegen::Mode::Fixed;
    uint32_t frameGenerationMultiplier = 2; // Requested total output frames per rendered frame.
    uint32_t frameGenerationTargetFps = 0; // Dynamic mode: 0 asks the SDK to use the display rate.
    bool fxaa = false; // Legacy serialized mirror; antialiasing is authoritative.
    bool automaticUpdates = true;
    bool skipShaderPrebuild = false;
    bool saveAnywhere = false; // Debug-only preference; defaults off for existing profiles.
    bool operator==(const Config &) const = default;
};
Config GetConfig();
void PreviewConfig(const Config &config);
// Atomic replacement, preserving the previous file if writing fails.
bool SaveConfig(const Config &config);
bool SaveDebugLanguage(uint32_t language);
bool SaveSaveAnywhere(bool enabled);
uint32_t GameLanguage();
} // namespace settings
