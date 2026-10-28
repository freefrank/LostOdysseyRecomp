#include <gpu/shader/rect_list_hlsl.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <numeric>
#include <random>
#include <stdexcept>

namespace rect = xenos::rect_list;
static uint64_t checks = 0;
static void Require(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

// An independent reference for the old six-corner encoding.
static std::vector<uint32_t> Original(const std::vector<uint32_t>& source,
    bool indexed, uint32_t count, uint32_t base, uint32_t& skipped) {
    std::vector<uint32_t> result;
    skipped = 0;
    const size_t rectangles = (indexed ? source.size() : count) / 3;
    for (size_t i = 0; i < rectangles; ++i) {
        const uint32_t first = indexed ? source[3 * i] : uint32_t(3 * i);
        if (indexed && (source[3 * i + 1] != first + 1 || source[3 * i + 2] != first + 2)) {
            ++skipped; continue;
        }
        const uint32_t vertex = first + base;
        if (vertex > rect::kMaxFirstVertex) { ++skipped; continue; }
        for (uint32_t c = 0; c < 6; ++c) result.push_back(vertex * 8 + c);
    }
    return result;
}

static void Check(const std::vector<uint32_t>& source, bool indexed, uint32_t count, uint32_t base) {
    uint32_t skipped = 0;
    const auto original = Original(source, indexed, count, base, skipped);
    for (bool reuse : {false, true}) {
        auto indices = source;
        std::vector<uint32_t> scratch(100, UINT32_MAX);
        bool useIndices = indexed;
        const auto actualSkipped = rect::ExpandIndices(indices, scratch, useIndices, count, base, reuse);
        Require(useIndices && actualSkipped == skipped, "original skip and indexed-draw behavior");
        Require(indices.size() == original.size(), "same triangle/index count");
        for (size_t i = 0; i < indices.size(); ++i) {
            const uint32_t before = original[i], after = indices[i];
            Require((before >> 3) == (after >> 3), "same guest vertex triple");
            const uint32_t oldCorner = before & 7, newCorner = after & 7;
            // XeRectListPick: 1/4 choose b, 2/3 choose d, 0 chooses a,
            // 5 synthesizes b+d-a. PointSize always comes from s0.
            const auto output = [](uint32_t c) { return c == 4 ? 1u : c == 3 ? 2u : c; };
            Require(output(oldCorner) == output(newCorner), "same output at each primitive vertex");
            if (!reuse) Require(before == after, "opt-out reproduces the original exact indices");
        }
        for (size_t i = 0; i < indices.size(); i += 6) {
            std::array<uint32_t, 6> unique{};
            std::copy_n(indices.begin() + i, 6, unique.begin());
            std::sort(unique.begin(), unique.end());
            Require(std::unique(unique.begin(), unique.end()) - unique.begin() == (reuse ? 4 : 6),
                "four unique shader IDs instead of six");
        }
    }
    auto defaults = source;
    std::vector<uint32_t> scratch;
    bool useIndices = indexed;
    rect::ExpandIndices(defaults, scratch, useIndices, count, base);
    for (size_t i = 0; i + 5 < defaults.size(); i += 6)
        Require(defaults[i + 1] == defaults[i + 4] && defaults[i + 2] == defaults[i + 3], "reuse enabled by default");
}

int main() {
    try {
        for (uint32_t base : {0u, 1u, 7u, rect::kMaxFirstVertex - 2, rect::kMaxFirstVertex,
            rect::kMaxFirstVertex + 1, UINT32_MAX}) {
            for (uint32_t count = 0; count < 100; ++count) {
                Check({}, false, count, base);
                std::vector<uint32_t> indices(count);
                std::iota(indices.begin(), indices.end(), 0);
                Check(indices, true, 99, base); // Indexed path uses actual input length.
            }
            Check({0, 2, 1, 3, 4, 5, 6, 8, 7}, true, 9, base);
            Check({rect::kMaxFirstVertex, rect::kMaxFirstVertex + 1, rect::kMaxFirstVertex + 2}, true, 3, base);
            Check({UINT32_MAX - 1, UINT32_MAX, 0}, true, 3, base);
            Check({3, 4, 5, 0, 1, 2, 3, 4, 5}, true, 9, base); // Ordering and repeated rectangles.
        }
        std::mt19937 rng(0x750);
        for (unsigned trial = 0; trial < 5000; ++trial) {
            std::vector<uint32_t> source(rng() % 100);
            for (size_t i = 0; i + 2 < source.size(); i += 3) {
                source[i] = rng() % 1000000;
                source[i + 1] = source[i] + 1;
                source[i + 2] = source[i] + (trial % 5 ? 2 : 3);
            }
            Check(source, true, uint32_t(source.size()), rng() % 10000);
        }
        std::printf("rect list: %llu checks passed\n", static_cast<unsigned long long>(checks));
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what()); return 1;
    }
}
