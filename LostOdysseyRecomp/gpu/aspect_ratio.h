#pragma once
#include <algorithm>
#include <cstdint>

// Settings -> Graphics -> Aspect ratio. Auto renders at the output's own shape
// (Hor+ on wider and taller outputs). A fixed shape frames the scene, UI and
// movies for that shape on any window; presentation centres the image with black
// bars instead of stretching it.
namespace gpu::aspect_ratio
{
// Persisted ids (settings.ini aspect_ratio).
enum class Mode : uint32_t { Auto = 0, Wide = 1, Ultrawide = 2, Standard = 3 };
inline constexpr uint32_t ModeCount = 4;
struct Ratio { uint32_t width = 0, height = 0; };
// 21:9 is the 64:27 of 2560x1080 and 5120x2160 panels.
inline constexpr Ratio RatioOf(Mode mode)
{
    switch (mode) {
    case Mode::Wide: return {16, 9};
    case Mode::Ultrawide: return {64, 27};
    case Mode::Standard: return {4, 3};
    default: return {};
    }
}
// An output within 2% of the chosen shape keeps its own (3440x1440 for 21:9,
// 1366x768 for 16:9) rather than showing bars a few pixels wide.
inline constexpr uint32_t TolerancePercent = 2;
struct Extent
{
    uint32_t width = 0, height = 0;
    bool operator==(const Extent&) const = default;
};
// The largest rectangle of the chosen shape inside the output, in even pixels;
// the whole output for Auto.
inline constexpr Extent Fit(Mode mode, uint32_t width, uint32_t height)
{
    const auto ratio = RatioOf(mode);
    if (!ratio.width || !width || !height) return {width, height};
    const uint64_t output = uint64_t(width) * ratio.height, target = uint64_t(height) * ratio.width;
    if (output * 100 <= target * (100 + TolerancePercent) && output * 100 >= target * (100 - TolerancePercent))
        return {width, height};
    const auto even = [](uint64_t value, uint32_t limit) {
        return (std::min)(limit, (std::max)(2u, uint32_t(value) & ~1u));
    };
    if (output > target) // Wider than the shape: bars left and right.
        return {even((uint64_t(height) * ratio.width + ratio.height / 2) / ratio.height, width), height};
    return {width, even((uint64_t(width) * ratio.height + ratio.width / 2) / ratio.width, height)};
}
// 4:3 shows the console's narrower horizontal view. Auto on a tall output keeps
// the 16:9 width and shows more above and below.
inline constexpr bool NarrowsView(Mode mode) { return mode == Mode::Standard; }
// Settings saved before this setting existed: a 21:9-shaped output resolution
// (the old Widescreen switch) keeps 21:9, anything else is Auto.
inline constexpr Mode Migrated(uint32_t width, uint32_t height)
{
    return uint64_t(width) * 10 >= uint64_t(height) * 20 && uint64_t(width) * 10 < uint64_t(height) * 26
        ? Mode::Ultrawide : Mode::Auto;
}
} // namespace gpu::aspect_ratio
