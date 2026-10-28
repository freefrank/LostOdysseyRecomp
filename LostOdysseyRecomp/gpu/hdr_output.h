#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>

namespace gpu::hdr
{
inline float DecodeHalf(uint16_t bits)
{
    const uint32_t sign = uint32_t(bits & 0x8000u) << 16;
    const uint32_t exponent = (bits >> 10) & 31;
    const uint32_t fraction = bits & 1023;
    if (!exponent) {
        const float value = std::ldexp(float(fraction), -24);
        return sign ? -value : value;
    }
    const uint32_t result = sign | (exponent == 31 ? 0x7f800000u : (exponent + 112) << 23) | (fraction << 13);
    return std::bit_cast<float>(result);
}

// The same content is expressed in absolute scRGB units on Windows and in
// units of the system SDR white on Metal. A floating-point SDR fallback still
// needs linear pixels, even when the display has no HDR headroom.
struct OutputTransform
{
    bool linear = false;
    bool active = false;
    float scale = 1.0f;
    float peakRatio = 1.0f;
    bool pq = false;
};

inline float FinitePositive(float value, float fallback)
{
    return std::isfinite(value) && value > 0.0f ? value : fallback;
}

inline OutputTransform MakeOutput(bool linear, bool edr, bool active,
                                  float paperWhiteNits, float peakNits, bool pq = false)
{
    OutputTransform result;
    result.linear = linear;
    result.active = linear && active;
    result.pq = pq;
    if (!result.active) return result;
    const float white = std::clamp(FinitePositive(paperWhiteNits, 203.0f), 80.0f, 400.0f);
    // Automatic defaults are resolved by the settings layer. Do not silently
    // cap a manual calibration override to a potentially inaccurate report.
    const float peak = std::clamp(FinitePositive(peakNits, 1000.0f), white, 10000.0f);
    result.scale = pq ? white : edr ? 1.0f : white / 80.0f;
    result.peakRatio = std::max(1.0f, peak / white);
    return result;
}

inline float DecodePq(float value)
{
    constexpr float m1 = 2610.0f / 16384.0f, m2 = 2523.0f / 32.0f;
    constexpr float c1 = 3424.0f / 4096.0f, c2 = 2413.0f / 128.0f, c3 = 2392.0f / 128.0f;
    const float p = std::pow(std::clamp(value, 0.0f, 1.0f), 1.0f / m2);
    return 10000.0f * std::pow(std::max(p - c1, 0.0f) / std::max(c2 - c3 * p, 0.000001f), 1.0f / m1);
}

inline std::array<float, 3> DecodePq10(uint32_t pixel, bool blueLowBits = false)
{
    const float a = DecodePq(float(pixel & 1023u) / 1023.0f);
    const float g = DecodePq(float((pixel >> 10) & 1023u) / 1023.0f);
    const float b = DecodePq(float((pixel >> 20) & 1023u) / 1023.0f);
    const float r2020 = blueLowBits ? b : a, b2020 = blueLowBits ? a : b;
    // BT.2020 to the original BT.709 working gamut, in absolute nits.
    return {1.660491f*r2020-0.587641f*g-0.072850f*b2020,
            -0.124550f*r2020+1.132900f*g-0.008349f*b2020,
            -0.018151f*r2020-0.100579f*g+1.118730f*b2020};
}

// Preserve the original diffuse range, then compress the excess smoothly.
// Scaling all channels by the same factor preserves hue through the shoulder.
inline std::array<float, 3> MapLinear(std::array<float, 3> color, float peakRatio)
{
    for (auto& channel : color)
        channel = std::isfinite(channel) ? std::clamp(channel, 0.0f, 65504.0f) : 0.0f;
    const float maximum = std::max({color[0], color[1], color[2]});
    if (maximum <= 1.0f) return color;
    const float range = std::max(0.0f, FinitePositive(peakRatio, 1.0f) - 1.0f);
    const float excess = maximum - 1.0f;
    const float mapped = 1.0f + (range > 0.0f ? range * (excess / (range + excess)) : 0.0f);
    for (auto& channel : color) channel *= mapped / maximum;
    return color;
}

// Every linear output carries sRGB-decoded diffuse pixels. A clipped SDR
// preview re-encodes that range with the sRGB curve, as the compositor does.
inline float EncodeSrgb(float linear)
{
    return linear <= 0.0031308f ? linear * 12.92f : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
}

inline uint32_t PreviewRgba(float red, float green, float blue, float outputScale)
{
    // Diagnostic PNG/PPM consumers are SDR. Clearly mark these as SDR previews
    // in the caller; the original FP16 readback must never be interpreted as RGBA8.
    const float scale = FinitePositive(outputScale, 1.0f);
    const auto encode = [scale](float value) {
        const float linear = std::isfinite(value) ? std::clamp(value / scale, 0.0f, 1.0f) : 0.0f;
        return uint32_t(std::lround(EncodeSrgb(linear) * 255.0f));
    };
    return encode(red) | (encode(green) << 8) | (encode(blue) << 16) | 0xff000000u;
}
} // namespace gpu::hdr
