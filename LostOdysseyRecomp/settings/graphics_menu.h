#pragma once
#include "config.h"
#include "menu.h"
#include <algorithm>

namespace settings::graphics_menu
{
// UI indices only. Keep the persisted AA/provider/quality IDs independent.
inline constexpr uint32_t AaChoiceCount = 6;
inline uint32_t AaChoice(const Config& config)
{
    using gpu::upscaling::Upscaler;
    if (config.upscaler == Upscaler::Dlss) return 4;
    if (config.upscaler == Upscaler::Fsr) return 5;
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
        config.upscaler = choice == 4 ? Upscaler::Dlss : Upscaler::Fsr;
        // Retain legacy AA as the renderer's unsupported-scene fallback.
        // The existing frame plan selects one temporal consumer, not both.
    }
}
inline bool IsAction(int tab, int row)
{
    return (tab == 0 && (row == GameRestoreRow || row == GameMainMenuRow)) ||
           (tab == 2 && (row == int(GraphicsRow::Brightness) || row == int(GraphicsRow::Save))) ||
           (tab == 3 && row == 3);
}
static_assert(int(GraphicsRow::DlssQuality) + 1 == int(GraphicsRow::FsrSharpness));
static_assert(int(GraphicsRow::FsrSharpness) + 1 == int(GraphicsRow::AnisotropicFiltering));
static_assert(int(GraphicsRow::Save) + 1 == int(GraphicsRow::Count));
}
