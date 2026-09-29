// Synthetic compiler/code-shape isolation, not game/GPU or frame-time evidence.
// Compare identical inputs through the pre-rework Copy loop and the current one.
#include "gpu/draw_state.h"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>
using gpu::renderer::DrawWords;
#if defined(_MSC_VER)
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__((noinline))
#endif
NOINLINE void Previous(DrawWords source, std::span<uint32_t> output)
{
    const size_t count = std::min(source.Size(), output.size());
    if (count && !source.HasLegacyFallback()) source.Copy(output); // unchanged native memcpy
    else for (size_t i = 0; i < count; ++i) output[i] = source.Read(i);
    std::fill(output.begin() + count, output.end(), 0);
}
NOINLINE void Current(DrawWords source, std::span<uint32_t> output) { source.Copy(output); }
struct Sample { double ns; uint64_t checksum; };
Sample Run(bool current, unsigned zeroStride, bool legacy)
{
    constexpr size_t bankWords = 1024, iterations = 100000;
    std::array<uint32_t, bankWords * 2> values{}, output{};
    std::array<uint8_t, bankWords * 8> mirror{};
    for (size_t i = 0; i < values.size(); ++i) {
        values[i] = zeroStride && i % zeroStride == 0 ? 0 : uint32_t(0x81000000u + i);
        const auto fallback = uint32_t(0x7fc01234u + i);
        for (size_t b = 0; b < 4; ++b) mirror[i * 4 + b] = uint8_t(fallback >> (24 - b * 8));
    }
    auto view = legacy ? DrawWords::Legacy(values, mirror.data()) : DrawWords(values);
    const auto vs = view.Subspan(0, bankWords), ps = view.Subspan(bankWords, bankWords);
    const std::span<uint32_t> out(output);
    auto copy = current ? Current : Previous;
    uint64_t checksum = 0;
    const auto start = std::chrono::steady_clock::now();
    for (size_t i = 0; i < iterations; ++i) {
        // Both implementations see precisely the same updates and full banks.
        values[10] ^= uint32_t(i);
        mirror[17] ^= uint8_t(i);
        copy(vs, out.first(bankWords)); copy(ps, out.subspan(bankWords));
        checksum += output[i % output.size()];
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    return {std::chrono::duration<double, std::nano>(elapsed).count() / iterations, checksum};
}
int main()
{
    std::puts("round,legacy_mmio,zero_stride,previous_ns,current_ns,checksum");
    for (unsigned round = 0; round < 3; ++round)
        for (bool legacy : {false, true})
            for (unsigned stride : {0u, 1u, 4u}) {
                Sample previous, current;
                if (round & 1) { current = Run(true, stride, legacy); previous = Run(false, stride, legacy); }
                else { previous = Run(false, stride, legacy); current = Run(true, stride, legacy); }
                if (previous.checksum != current.checksum) {
                    std::fputs("bulk-copy equivalence failed\n", stderr); return 1;
                }
                std::printf("%u,%u,%u,%.3f,%.3f,%llu\n", round, unsigned(legacy), stride,
                    previous.ns, current.ns, static_cast<unsigned long long>(current.checksum));
            }
}
