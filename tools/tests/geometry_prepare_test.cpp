#include "../../LostOdysseyRecomp/gpu/geometry_prepare.h"
#include <cstdio>
#include <cstdlib>

static unsigned checks;
static void Check(bool ok) { ++checks; if (!ok) std::abort(); }
static uint32_t OldSwap(uint32_t v, unsigned endian)
{
    switch (endian & 3) {
    case 1: return ((v & 0xFF00FF00u) >> 8) | ((v & 0x00FF00FFu) << 8);
    case 2: return ((v & 255) << 24) | ((v & 65280) << 8) | ((v >> 8) & 65280) | (v >> 24);
    case 3: return (v >> 16) | (v << 16);
    default: return v;
    }
}
int main()
{
    using namespace gpu::geometry_prepare;
    std::vector<uint8_t> bytes(65536 * 4 + 16);
    uint32_t random = 0x823400;
    for (auto& b : bytes) { random ^= random << 13; random ^= random >> 17; random ^= random << 5; b = uint8_t(random); }
    for (bool wide : {false, true}) for (unsigned endian = 0; endian < 8; ++endian)
        for (unsigned count : {0u, 1u, 3u, 7u, 16u, 31u, 1023u, 65536u})
            for (unsigned unaligned : {0u, 1u, 3u})
            {
                std::vector<uint32_t> output(count + 2, 0xDEADBEEF);
                const auto* src = bytes.data() + unaligned;
                ConvertIndices(src, output.data() + 1, count, wide, endian);
                Check(output.front() == 0xDEADBEEF && output.back() == 0xDEADBEEF);
                for (unsigned i = 0; i < count; ++i) {
                    uint32_t v;
                    if (wide) std::memcpy(&v, src + i * 4, 4);
                    else { uint16_t v16; std::memcpy(&v16, src + i * 2, 2); v = v16; }
                    v = OldSwap(v, endian);
                    if (!wide) v &= 0xFFFF;
                    Check(output[i + 1] == v);
                }
            }
    SampledContent sample;
    for (size_t size : {size_t(0), size_t(4), size_t(8), size_t(8192), size_t(8196), size_t(65536)})
    {
        sample.Capture(bytes.data(), size);
        Check(sample.Matches(bytes.data(), size));
        auto relocated = bytes;
        Check(sample.Matches(relocated.data(), size));
        Check(!sample.Matches(bytes.data(), size + 1));
        if (size <= 8192) {
            for (size_t i = 0; i < size; ++i) {
                bytes[i] ^= 1; Check(!sample.Matches(bytes.data(), size)); bytes[i] ^= 1;
            }
        } else {
            std::vector<size_t> locations{0, 511, size - 512, size - 1};
            for (size_t i = 0; i < 64; ++i) { locations.push_back(512 + i * ((size - 1024) / 64)); locations.push_back(locations.back() + 63); }
            for (auto i : locations) { bytes[i] ^= 1; Check(!sample.Matches(bytes.data(), size)); bytes[i] ^= 1; }
        }
        Check(sample.Matches(bytes.data(), size));
    }
    std::printf("geometry preparation: %u checks passed\n", checks);
}
