#pragma once

// Independent double-precision oracle. Do not implement this in terms of the
// production HLSL or its CPU preview helpers: those are the code under test.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace hdr_reference {
using Color = std::array<double, 3>;
inline double Srgb(double signal) {
    signal = std::max(signal, 0.0);
    return signal <= 0.04045 ? signal / 12.92 : std::pow((signal + 0.055) / 1.055, 2.4);
}
inline double Decode(double signal, bool extended) {
    // The display ramp has already run. Preserve SDR's display response below
    // white; retain the existing extended gamma-2.2 highlight encoding above it.
    return extended && signal > 1 ? std::pow(signal, 2.2) : Srgb(signal);
}
inline Color Shoulder(Color color, double peakRatio) {
    for (auto& c : color) c = std::clamp(c, 0.0, 65504.0);
    const double maximum = *std::max_element(color.begin(), color.end());
    if (maximum <= 1) return color;
    const double range = std::max(peakRatio - 1, 0.0);
    const double mapped = 1 + (range > 0 ? range * (maximum - 1) / (range + maximum - 1) : 0);
    for (auto& c : color) c *= mapped / maximum;
    return color;
}
inline double Pq(double nits) {
    const double p = std::pow(std::clamp(nits / 10000, 0.0, 1.0), 2610.0/16384);
    return std::pow((3424.0/4096 + (2413.0/128)*p) / (1 + (2392.0/128)*p), 2523.0/32);
}
inline Color EncodePq(Color nits) {
    const auto c = nits;
    nits = {0.627404*c[0]+0.329283*c[1]+0.043313*c[2],
            0.069097*c[0]+0.919540*c[1]+0.011362*c[2],
            0.016391*c[0]+0.088013*c[1]+0.895595*c[2]};
    for (auto& channel : nits) channel = Pq(channel);
    return nits;
}
inline uint32_t GuestEntry(unsigned level, bool identity = false) {
    // Synthetic reconstruction of the documented #209 sRGB-decode/BT.709-
    // encode LUT, quantized to the actual DC_LUT_30_COLOR 10-bit protocol.
    const double l = Srgb(double(level)/255);
    const double v = identity ? double(level)/255 : l < 0.018 ? 4.5*l : 1.099*std::pow(l,0.45)-0.099;
    return uint32_t(std::lround(v*1023));
}
inline double Lut(double v, bool guest, int brightness, unsigned gamma) {
    const double x = std::clamp(v, 0.0, 1.0)*255;
    const unsigned i = std::min(unsigned(x),254u);
    const auto sample = [&](unsigned j) {
        if (guest) return double(GuestEntry(j))/1023;
        const double black = double(-brightness)/200;
        // GPU uploads float table entries; interpolation stays independent here.
        return double(float(std::pow(std::clamp((double(j)/255-black)/(1-black),0.0,1.0),100.0/gamma)));
    };
    return sample(i)*(1-(x-i)) + sample(i+1)*(x-i) + std::max(v-1,0.0);
}
inline Color Adjust(Color c, bool guest, bool expand, int brightness=0, unsigned gamma=100) {
    for (auto& v : c) {
        if (guest) v=Lut(v,true,0,100);
        if (expand) v=std::max((v-16.0/255)*(255.0/219),0.0);
        if (brightness || gamma!=100) v=Lut(v,false,brightness,gamma);
    }
    return c;
}
inline Color Output(Color c, bool linear, bool extended, double scale, double peakRatio, bool pq) {
    if (linear) {
        for (auto& v : c) v=Decode(v,extended);
        if (extended) c=Shoulder(c,peakRatio);
        for (auto& v : c) v*=scale;
    }
    return pq ? EncodePq(c) : c;
}
inline double Luminance(Color c) { return .2126*c[0]+.7152*c[1]+.0722*c[2]; }
inline std::vector<Color> Samples() {
    std::vector<Color> result;
    // Complete 8-bit ramp, then a dense near-black ramp and mixed-channel
    // synthetic CGI colours (not samples captured from copyrighted footage).
    for (unsigned i=0;i<256;++i) { double v=double(i)/255; result.push_back({v,v,v}); }
    for (unsigned i=0;i<256;++i) { double v=double(i)/(255*16); result.push_back({v,v*.6,v*.2}); }
    for (auto c : {Color{.01,.02,.04}, Color{.025,.08,.15}, Color{.12,.06,.025},
                   Color{.18,.2,.25}, Color{.4,.3,.15}, Color{0,0,0}, Color{1,1,1},
                   Color{2,2,2}, Color{4,4,4}, Color{.03,.1,2}, Color{8,.6,.02},
                   Color{16,4,1}, Color{1.0001,.9999,.04045}, Color{65504,65504,65504}})
        result.push_back(c);
    return result;
}
} // namespace hdr_reference
