#include <gpu/hdr_output.h>
#include <cassert>
#include <cmath>
#include <limits>

int main()
{
    using namespace gpu::hdr;
    assert(DecodeHalf(0x3c00) == 1.0f);
    assert(DecodeHalf(0xc000) == -2.0f);
    assert(DecodeHalf(1) == std::ldexp(1.0f, -24));
    assert(std::isinf(DecodeHalf(0x7c00)) && std::isnan(DecodeHalf(0x7e00)));
    const auto windows = MakeOutput(true, false, true, 200, 1000);
    assert(windows.active && windows.linear && windows.scale == 2.5f && windows.peakRatio == 5);
    const auto metal = MakeOutput(true, true, true, 200, 1000);
    assert(metal.active && metal.scale == 1 && metal.peakRatio == 5);
    const auto sdrDisplay = MakeOutput(true, false, false, 200, 1000);
    assert(sdrDisplay.linear && !sdrDisplay.active && sdrDisplay.scale == 1 && sdrDisplay.peakRatio == 1);
    const auto unsupported = MakeOutput(false, false, true, 200, 1000);
    assert(!unsupported.linear && !unsupported.active);
    const auto invalid = MakeOutput(true, true, true, NAN, INFINITY);
    assert(std::isfinite(invalid.scale) && std::isfinite(invalid.peakRatio));
    const auto pq = MakeOutput(true, false, true, 200, 1000, true);
    assert(pq.pq && pq.scale == 200 && pq.peakRatio == 5);
    assert(DecodePq(0) == 0 && std::abs(DecodePq(1)-10000) < 1);
    assert(std::abs(DecodePq(0.7518271f)-1000) < 0.1f);
    const auto packed = DecodePq10(1023u | (1023u<<10) | (1023u<<20));
    for (float channel : packed) assert(std::abs(channel-10000) < 1);
    const auto red = DecodePq10(1023u), swapped = DecodePq10(1023u<<20,true);
    assert(red == swapped);
    assert((MapLinear({0.18f, 0.5f, 1.0f}, 5) == std::array<float, 3>{0.18f, 0.5f, 1.0f}));
    float previous = 1;
    for (float input = 1.01f; input < 60000; input *= 1.1f)
    {
        const auto mapped = MapLinear({input, input * 0.5f, input * 0.25f}, 5);
        assert(mapped[0] > previous && mapped[0] < 5);
        assert(std::abs(mapped[1] / mapped[0] - 0.5f) < 0.00001f);
        assert(std::abs(mapped[2] / mapped[0] - 0.25f) < 0.00001f);
        previous = mapped[0];
    }
    const auto invalidColor = MapLinear({-1, NAN, INFINITY}, 5);
    assert((invalidColor == std::array<float, 3>{0, 0, 0}));
    assert(PreviewRgba(2.5f, 2.5f, 2.5f, 2.5f) == 0xffffffffu);
    assert(PreviewRgba(0, 0, 0, 1) == 0xff000000u);
    // Active and inactive linear outputs use sRGB for the diffuse range. The
    // SDR preview must round-trip every level at each output's white scale.
    for (uint32_t level = 0; level < 256; ++level)
    {
        const float encoded = float(level) / 255.0f;
        const float linear = encoded <= 0.04045f ? encoded / 12.92f : std::pow((encoded + 0.055f) / 1.055f, 2.4f);
        for (float scale : {1.0f, 203.0f/80.0f, 203.0f})
            assert(PreviewRgba(linear*scale, linear*scale, linear*scale, scale) ==
                (0xff000000u | level | (level << 8) | (level << 16)));
    }
}
