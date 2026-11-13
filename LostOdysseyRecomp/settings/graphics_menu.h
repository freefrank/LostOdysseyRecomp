#pragma once
#include "config.h"
#include "menu.h"
#include <os/platform.h>
#include <algorithm>
#include <iterator>

namespace settings::graphics_menu
{
inline bool HdrAvailable(GraphicsBackend backend)
{
#ifdef _WIN32
    return backend == GraphicsBackend::D3D12 || backend == GraphicsBackend::Vulkan;
#elif LO_PLATFORM_MACOS
    return backend == GraphicsBackend::Metal;
#else
    return backend == GraphicsBackend::Vulkan;
#endif
}
// UI indices only. Keep the persisted AA/provider/quality IDs independent.
// macOS offers MetalFX Temporal; Android has no NGX and builds FSR optionally.
#if LO_PLATFORM_MACOS
inline constexpr uint32_t AaChoiceCount = 5;
#elif LO_PLATFORM_ANDROID
#if defined(LO_HAS_FSR) && LO_HAS_FSR
inline constexpr bool AndroidFsrAvailable = true;
#else
inline constexpr bool AndroidFsrAvailable = false;
#endif
inline constexpr uint32_t AaChoiceCount = AndroidFsrAvailable ? 5 : 4;
#elif defined(_WIN32) && defined(LO_HAS_XESS) && LO_HAS_XESS
// Intel XeSS is D3D12-only; only XeSS-enabled Windows builds offer it.
inline constexpr uint32_t AaChoiceCount = 7;
#else
inline constexpr uint32_t AaChoiceCount = 6;
#endif
inline uint32_t AaChoice(const Config& config)
{
    using gpu::upscaling::Upscaler;
#if LO_PLATFORM_MACOS
    // A DLSS/FSR value from another platform's settings shows the AA it falls back to.
    if (config.upscaler == Upscaler::MetalFx) return 4;
#elif LO_PLATFORM_ANDROID
    // NGX is unavailable here; a saved desktop request shows the AA fallback.
    if (AndroidFsrAvailable && config.upscaler == Upscaler::Fsr) return 4;
#else
    if (config.upscaler == Upscaler::Dlss) return 4;
    if (config.upscaler == Upscaler::Fsr) return 5;
    if (AaChoiceCount > 6 && config.upscaler == Upscaler::Xess) return 6;
#endif
    return std::min(config.antialiasing, 3u);
}
inline void SelectAa(Config& config, uint32_t choice)
{
    using gpu::upscaling::Upscaler;
    if (choice >= AaChoiceCount) return;
    if (choice < 4) {
        config.upscaler = Upscaler::Off;
        config.antialiasing = choice;
        config.fxaa = choice == 1;
    } else {
#if LO_PLATFORM_MACOS
        config.upscaler = Upscaler::MetalFx;
#elif LO_PLATFORM_ANDROID
        config.upscaler = Upscaler::Fsr;
#else
        config.upscaler = choice == 4 ? Upscaler::Dlss : choice == 5 ? Upscaler::Fsr : Upscaler::Xess;
#endif
        // Retain legacy AA as the renderer's unsupported-scene fallback.
        // The existing frame plan selects one temporal consumer, not both.
    }
}
// Render resolution choices, in menu order (Config::internalResolution values).
#if LO_PLATFORM_MACOS
inline constexpr int RenderResolutions[] = {0, 720, 1080, 1440, 2160, InternalResolutionNative};
#else
inline constexpr int RenderResolutions[] = {0, 720, 1080, 1440, 2160};
#endif
inline uint32_t RenderResolutionChoice(const Config& config)
{
    for (uint32_t i = 0; i < std::size(RenderResolutions); ++i)
        if (RenderResolutions[i] == config.internalResolution) return i;
    return 0;
}
// Graphics preset: a shortcut for the rows it sets, shown as the preset those
// rows match (Custom otherwise). Nothing of its own is saved. Both render the
// original 720p at 30 FPS; Low-end also turns off the game's costliest effects
// for weak hardware, Original keeps their retail state.
enum class Preset : uint32_t { Original, LowEnd, Custom };
inline constexpr uint32_t PresetChoiceCount = 2; // Custom is shown, never chosen
inline void ApplyPreset(Config& config, Preset preset)
{
    const bool low = preset == Preset::LowEnd;
    config.internalResolution = 720;
    config.frameRate = 30;
    config.dynamicShadows = low ? 0 : 1;
    config.motionBlur = !low;
    config.depthOfFieldPercent = low ? 0 : 100;
    config.bloom = !low;
    config.scalingQuality = low ? 0 : 1; // Standard (bilinear) or High (bicubic)
}
inline Preset PresetOf(const Config& config)
{
    for (const Preset preset : {Preset::Original, Preset::LowEnd})
    {
        Config applied = config;
        ApplyPreset(applied, preset);
        if (applied == config) return preset;
    }
    return Preset::Custom;
}
// Dynamic shadows row: On, Low, Off (settings values 1, 2, 0).
inline constexpr uint32_t DynamicShadowsValues[] = {1, 2, 0};
inline uint32_t DynamicShadowsChoice(const Config& config)
{
    for (uint32_t i = 0; i < std::size(DynamicShadowsValues); ++i)
        if (DynamicShadowsValues[i] == config.dynamicShadows) return i;
    return 0;
}
inline constexpr uint32_t ShadowResolutions[] = {1, 2, 4};
inline uint32_t ShadowResolutionChoice(const Config& config)
{
    for (uint32_t i = 0; i < std::size(ShadowResolutions); ++i)
        if (ShadowResolutions[i] == config.shadowResolution) return i;
    return 0;
}
inline bool IsAction(int tab, int row)
{
    return (tab == 0 && (row == GameControlsRow || row == GameRestoreRow || row == GameMainMenuRow)) ||
           (tab == 2 && (row == int(GraphicsRow::NeuralRenderingTuning) || row == int(GraphicsRow::HdrCalibration) ||
                         row == int(GraphicsRow::Brightness) || row == int(GraphicsRow::Save))) ||
           (tab == 3 && (row == SystemAchievementsRow || row == SystemModsRow || row == SystemImportRow || row == SystemAddToSteamRow || row == SystemShaderBundleRow ||
                         row == SystemShaderPreparationRow || row == SystemAndroidDiagnosticsRow ||
                         row == SystemSaveRow));
}
static_assert(int(GraphicsRow::DlssQuality) + 1 == int(GraphicsRow::DlssModel));
static_assert(int(GraphicsRow::DlssModel) + 1 == int(GraphicsRow::FsrSharpness));
// The preset opens the rendering group, after the display rows.
static_assert(int(GraphicsRow::OutputResolution) + 1 == int(GraphicsRow::Preset));
static_assert(int(GraphicsRow::Preset) + 1 == int(GraphicsRow::RenderResolution));
static_assert(int(GraphicsRow::RenderResolution) + 1 == int(GraphicsRow::ShadowResolution));
static_assert(int(GraphicsRow::ShadowResolution) + 1 == int(GraphicsRow::DynamicShadows));
// Upscaling, neural rendering and frame generation stay one group.
static_assert(int(GraphicsRow::AntiAliasing) + 1 == int(GraphicsRow::DlssQuality));
static_assert(int(GraphicsRow::FsrSharpness) + 1 == int(GraphicsRow::DlssNeuralRendering));
static_assert(int(GraphicsRow::DlssNeuralRendering) + 1 == int(GraphicsRow::NeuralRenderingTuning));
static_assert(int(GraphicsRow::NeuralRenderingTuning) + 1 == int(GraphicsRow::FrameGeneration));
static_assert(int(GraphicsRow::FrameGeneration) + 1 == int(GraphicsRow::FrameGenerationMultiplier));
static_assert(int(GraphicsRow::FrameGenerationMultiplier) + 1 == int(GraphicsRow::AmbientOcclusion));
static_assert(int(GraphicsRow::AmbientOcclusion) + 1 == int(GraphicsRow::AnisotropicFiltering));
static_assert(int(GraphicsRow::FrameRate) + 1 == int(GraphicsRow::LoadingTransitions));
static_assert(int(GraphicsRow::LoadingTransitions) + 1 == int(GraphicsRow::VariableRefreshRate));
static_assert(int(GraphicsRow::VariableRefreshRate) + 1 == int(GraphicsRow::PerformanceOverlay));
// Each sub-page entry sits right under its setting.
static_assert(int(GraphicsRow::HdrPeak) + 1 == int(GraphicsRow::HdrCalibration));
static_assert(int(GraphicsRow::HdrCalibration) + 1 == int(GraphicsRow::Brightness));
static_assert(int(GraphicsRow::Save) + 1 == int(GraphicsRow::Count));
}
