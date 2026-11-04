#pragma once
// The Switch's final upscale (video.cpp): LO_SWITCH_UPSCALER picks FSR 1 for a scene rendered smaller than the
// output (LO_SWITCH_RENDER_HEIGHT=360 under the 540 or 720 swap chain), Presentation::Draw applies it.
//   bilinear (default)  today's filter
//   fsr                 EASU into an intermediate of the output's size, then RCAS (AMD's FSR 1, two passes)
//   easu                EASU alone, one pass (SwitchWakerHD's FSR option when the picture is smaller)
//   rcas                RCAS at the scene's size, then the bilinear upscale
// LO_SWITCH_RCAS_STOPS: RCAS's sharpness in stops, 0 the sharpest (default 0.25, SwitchWakerHD's; AMD's 0.2).
#include "presentation.h"
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
