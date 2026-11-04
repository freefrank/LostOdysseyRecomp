#pragma once
// The Switch's final upscale (video.cpp): LO_SWITCH_UPSCALER picks FSR 1 for a scene rendered smaller than the
// output (LO_SWITCH_RENDER_HEIGHT=360 under the 540 or 720 swap chain), Presentation::Draw applies it.
//   bilinear (default)  today's filter
//   fsr                 EASU into an intermediate of the output's size, then RCAS (AMD's FSR 1, two passes)
//   easu                EASU alone, one pass (SwitchWakerHD's FSR option when the picture is smaller)
//   rcas                RCAS at the scene's size, then the bilinear upscale
// LO_SWITCH_RCAS_STOPS: RCAS's sharpness in stops, 0 the sharpest (default 0.25, SwitchWakerHD's; AMD's 0.2).
// Without them, Graphics → FSR and FSR sharpness choose (Resolve); either variable, when set, wins over the menu.
#include "presentation.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace gpu::switch_upscaler
{
struct Choice
{
    SpatialUpscaler mode = SpatialUpscaler::Off;
    float rcasStops = 0.25f;
};
inline Choice Parse(const char* upscaler, const char* stops)
{
    Choice choice;
    if (upscaler) {
        if (std::strcmp(upscaler, "fsr") == 0) choice.mode = SpatialUpscaler::Fsr;
        else if (std::strcmp(upscaler, "easu") == 0) choice.mode = SpatialUpscaler::Easu;
        else if (std::strcmp(upscaler, "rcas") == 0) choice.mode = SpatialUpscaler::Rcas;
    }
    if (stops && *stops) {
        char* end = nullptr;
        const float value = std::strtof(stops, &end);
        if (end != stops && value >= 0.0f && value <= 8.0f) choice.rcasStops = value;
    }
    return choice;
}
inline Choice FromEnvironment()
{
    return Parse(std::getenv("LO_SWITCH_UPSCALER"), std::getenv("LO_SWITCH_RCAS_STOPS"));
}
// Graphics → FSR (fsr) and FSR sharpness: EASU then RCAS, sharpness p% at 2 - p/50 stops (FSR 2 and 3's mapping of
// their 0-1 sharpness: 100% is 0 stops, 88% 0.24); 0% is EASU alone. Off keeps the bilinear scale.
inline Choice FromSettings(bool fsr, uint32_t sharpnessPercent)
{
    Choice choice;
    if (!fsr) return choice;
    const uint32_t percent = std::min(sharpnessPercent, 100u);
    choice.mode = percent ? SpatialUpscaler::Fsr : SpatialUpscaler::Easu;
    choice.rcasStops = 2.0f - 0.02f * float(percent);
    return choice;
}
inline bool UpscalerFromEnvironment()
{
    static const bool set = [] { const char* value = std::getenv("LO_SWITCH_UPSCALER"); return value && *value; }();
    return set;
}
inline bool StopsFromEnvironment()
{
    static const bool set = [] { const char* value = std::getenv("LO_SWITCH_RCAS_STOPS"); return value && *value; }();
    return set;
}
// The upscaler a frame uses: the settings, with LO_SWITCH_UPSCALER's mode and LO_SWITCH_RCAS_STOPS's sharpness over them.
inline Choice Resolve(bool fsr, uint32_t sharpnessPercent)
{
    static const Choice environment = FromEnvironment();
    Choice choice = UpscalerFromEnvironment() ? environment : FromSettings(fsr, sharpnessPercent);
    if (StopsFromEnvironment()) choice.rcasStops = environment.rcasStops;
    return choice;
}
inline const char* Name(SpatialUpscaler mode)
{
    switch (mode) {
    case SpatialUpscaler::Fsr: return "fsr";
    case SpatialUpscaler::Easu: return "easu";
    case SpatialUpscaler::Rcas: return "rcas";
    default: return "bilinear";
    }
}
} // namespace gpu::switch_upscaler
