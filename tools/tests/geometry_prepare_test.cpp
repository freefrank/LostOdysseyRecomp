#include "../../LostOdysseyRecomp/gpu/geometry_prepare.h"
#include <cstdio>
#include <cstdlib>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

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
static void TestSampleBlock64()
{
    using gpu::geometry_prepare::EqualSampleBlock64;
    uint8_t left[64 + 15], right[64 + 15];
    for (unsigned leftOffset = 0; leftOffset < 16; ++leftOffset)
        for (unsigned rightOffset = 0; rightOffset < 16; ++rightOffset)
        {
            auto* a = left + leftOffset;
            auto* b = right + rightOffset;
            for (unsigned i = 0; i < 64; ++i) a[i] = b[i] = uint8_t(i * 37 + 0xA5);
            Check(EqualSampleBlock64(a, b));
            Check(EqualSampleBlock64(a, a));
            for (unsigned i = 0; i < 64; ++i)
                for (uint8_t bit : {uint8_t(1), uint8_t(0x80)})
                {
                    b[i] ^= bit;
                    Check(!EqualSampleBlock64(a, b));
                    b[i] ^= bit;
                }
            // Mismatches at the same SIMD lane in different blocks must not cancel.
            b[0] ^= 1;
            b[16] ^= 1;
            b[32] ^= 0x80;
            b[48] ^= 0x80;
            Check(!EqualSampleBlock64(a, b));
            b[0] ^= 1;
            b[16] ^= 1;
            b[32] ^= 0x80;
            b[48] ^= 0x80;
            Check(EqualSampleBlock64(a, b));
        }
#ifdef _WIN32
    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);
    const size_t page = systemInfo.dwPageSize;
    auto* leftPages = static_cast<uint8_t*>(VirtualAlloc(nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    auto* rightPages = static_cast<uint8_t*>(VirtualAlloc(nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Check(leftPages && rightPages);
    DWORD oldProtection;
    Check(VirtualProtect(leftPages + page, page, PAGE_NOACCESS, &oldProtection) != 0);
    Check(VirtualProtect(rightPages + page, page, PAGE_NOACCESS, &oldProtection) != 0);
    auto* a = leftPages + page - 64;
    auto* b = rightPages + page - 64;
    for (unsigned i = 0; i < 64; ++i) a[i] = b[i] = uint8_t(i * 37 + 0xA5);
    Check(EqualSampleBlock64(a, b));
    for (unsigned i = 0; i < 64; ++i)
    {
        a[i] ^= 1;
        Check(!EqualSampleBlock64(a, b));
        a[i] ^= 1;
        b[i] ^= 0x80;
        Check(!EqualSampleBlock64(a, b));
        b[i] ^= 0x80;
    }
    Check(EqualSampleBlock64(a, b));
    Check(VirtualFree(leftPages, 0, MEM_RELEASE) != 0);
    Check(VirtualFree(rightPages, 0, MEM_RELEASE) != 0);
#endif
}
int main(int argc, char** argv)
{
    using namespace gpu::geometry_prepare;
    const bool sampleBlockOnly = argc == 2 && std::strcmp(argv[1], "--sample-block-only") == 0;
    if (argc != 1 && !sampleBlockOnly) return 2;
    TestSampleBlock64();
    if (sampleBlockOnly)
    {
        std::printf("sample block64 equality: %u checks passed\n", checks);
        return 0;
    }
    std::vector<uint8_t> bytes(65536 * 4 + 16);
    uint32_t random = 0x823400;
    for (auto& b : bytes) { random ^= random << 13; random ^= random >> 17; random ^= random << 5; b = uint8_t(random); }
    CopyDwordsSwapped(nullptr, nullptr, 0, 2);
    for (unsigned endian = 0; endian < 8; ++endian)
        for (unsigned count : {0u, 1u, 2u, 3u, 4u, 5u, 7u, 8u, 9u, 15u, 16u, 17u, 31u, 32u, 33u, 1023u, 65536u})
            for (unsigned srcOffset : {0u, 1u, 3u, 7u, 15u})
                for (unsigned dstOffset : {0u, 1u, 3u, 7u, 15u})
                {
                    const size_t begin = 16 + dstOffset, end = begin + size_t(count) * 4;
                    std::vector<uint8_t> output(end + 16, 0xA5);
                    const auto* src = bytes.data() + srcOffset;
                    CopyDwordsSwapped(output.data() + begin, src, count, endian);
                    for (size_t i = 0; i < begin; ++i) Check(output[i] == 0xA5);
                    for (size_t i = end; i < output.size(); ++i) Check(output[i] == 0xA5);
                    for (unsigned i = 0; i < count; ++i) {
                        uint32_t input, actual;
                        std::memcpy(&input, src + i * 4, 4);
                        std::memcpy(&actual, output.data() + begin + i * 4, 4);
                        Check(actual == OldSwap(input, endian));
                    }
                }
#ifdef _WIN32
    // Put the exact source/destination extent against inaccessible pages, so
    // SIMD tail overreads and writes fail even when the written values match.
    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);
    const size_t page = systemInfo.dwPageSize;
    auto* guardedSource = static_cast<uint8_t*>(VirtualAlloc(nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    auto* guardedDestination = static_cast<uint8_t*>(VirtualAlloc(nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Check(guardedSource && guardedDestination);
    DWORD oldProtection;
    Check(VirtualProtect(guardedSource + page, page, PAGE_NOACCESS, &oldProtection) != 0);
    Check(VirtualProtect(guardedDestination + page, page, PAGE_NOACCESS, &oldProtection) != 0);
    for (unsigned count = 0; count <= 33; ++count) for (unsigned endian = 0; endian < 8; ++endian)
    {
        auto* src = guardedSource + page - count * 4;
        auto* dst = guardedDestination + page - count * 4;
        if (count) std::memcpy(src, bytes.data(), count * 4);
        CopyDwordsSwapped(dst, src, count, endian);
        for (unsigned i = 0; i < count; ++i) {
            uint32_t input, actual;
            std::memcpy(&input, src + i * 4, 4);
            std::memcpy(&actual, dst + i * 4, 4);
            Check(actual == OldSwap(input, endian));
        }
    }
    Check(VirtualFree(guardedSource, 0, MEM_RELEASE) != 0);
    Check(VirtualFree(guardedDestination, 0, MEM_RELEASE) != 0);
#endif
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
    for (size_t size : {size_t(0), size_t(4), size_t(8), size_t(8192), size_t(8196), size_t(16384), size_t(65536)})
    {
        sample.Capture(bytes.data(), size);
        Check(sample.Matches(bytes.data(), size));
        auto relocated = bytes;
        Check(sample.Matches(relocated.data(), size));
        Check(!sample.Matches(bytes.data(), size + 1));
        for (size_t i = 0; i < size; ++i) {
            bytes[i] ^= 1; Check(!sample.Matches(bytes.data(), size)); bytes[i] ^= 1;
        }
        Check(sample.Matches(bytes.data(), size));
    }
    std::printf("geometry preparation: %u checks passed\n", checks);
}
