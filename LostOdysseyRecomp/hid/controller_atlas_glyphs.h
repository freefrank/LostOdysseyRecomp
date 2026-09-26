#pragma once

#include "controller_glyphs.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace hid::prompts::atlas
{
    // Icon_Page_0 in rpmenurescommon_int.xxx and Font Icon's Texture2D_1 in
    // rpfontscommon_int.xxx share these measured button tiles. Both full RGBA
    // fingerprints are checked before this painter is called. Rects are [x, x + width) x
    // [y, y + height). `rect` is the full 36x36 atlas tile (not guest UV data);
    // `artwork` is its measured nontransparent pixel boundary. The latter is
    // only used to limit the CPU paint loop. Never use artwork as a UV rectangle.
    // Do not apply to similarly sized atlases without identifying the asset.
    struct Rect { int x, y, width, height; };
    inline constexpr int Width = 256, Height = 128;
    struct FaceCell { Rect rect; Rect artwork; Face face; };
    inline constexpr std::array<FaceCell, 4> FaceCells{{
        {{0, 72, 36, 36}, {2, 74, 32, 34}, Face::A},    // A -> cross
        {{36, 72, 36, 36}, {38, 74, 32, 34}, Face::B},  // B -> circle
        {{72, 72, 36, 36}, {74, 74, 32, 34}, Face::X},  // X -> square
        {{108, 72, 36, 36}, {110, 74, 32, 34}, Face::Y} // Y -> triangle
    }};
    struct ShoulderCell { Rect rect; Rect artwork; char side; char number; };
    inline constexpr std::array<ShoulderCell, 4> ShoulderCells{{
        {{0, 0, 36, 36}, {2, 4, 32, 28}, 'L', '1'},    // LB -> L1
        {{36, 0, 36, 36}, {38, 4, 33, 28}, 'R', '1'},  // RB -> R1
        {{72, 0, 36, 36}, {76, 2, 28, 32}, 'L', '2'},  // LT -> L2
        {{108, 0, 36, 36}, {112, 2, 28, 32}, 'R', '2'} // RT -> R2
    }};
    // These two atlas tiles are the guest's BACK/START button prompts. BACK
    // is the left-side Select/Share/Create button; START is the right-side
    // Options/Menu button. Coordinates here are atlas pixels, not guest UVs.
    enum class SystemButton { Back, Start };
    struct SystemCell { Rect rect; Rect artwork; SystemButton button; };
    inline constexpr std::array<SystemCell, 2> SystemCells{{
        {{108, 36, 36, 36}, {111, 38, 30, 32}, SystemButton::Back},
        {{144, 36, 36, 36}, {146, 38, 32, 32}, SystemButton::Start}
    }};

    namespace detail
    {
        struct Segment { float x1, y1, x2, y2; };

        inline float Coverage(float x, float y, Segment segment, float radius)
        {
            const float dx = segment.x2 - segment.x1, dy = segment.y2 - segment.y1;
            const float length2 = dx * dx + dy * dy;
            const float t = length2 ? std::clamp(((x - segment.x1) * dx + (y - segment.y1) * dy) / length2, 0.0f, 1.0f) : 0.0f;
            const float distance = std::hypot(x - (segment.x1 + t * dx), y - (segment.y1 + t * dy));
            return std::clamp(radius + 0.5f - distance, 0.0f, 1.0f);
        }

        inline void Mix(uint8_t* rgb, std::array<uint8_t, 3> ink, float coverage)
        {
            for (int channel = 0; channel < 3; ++channel)
                rgb[channel] = uint8_t(std::lround(rgb[channel] * (1.0f - coverage) + ink[channel] * coverage));
        }

        inline std::array<uint8_t, 3> Ink(Face face)
        {
            switch (face)
            {
            case Face::A: return {133, 207, 255}; // cross, blue
            case Face::B: return {255, 153, 164}; // circle, red
            case Face::X: return {230, 174, 245}; // square, pink
            case Face::Y: return {174, 244, 174}; // triangle, green
            }
            return {255, 255, 255};
        }

        inline std::array<uint8_t, 3> Button(Face face)
        {
            switch (face)
            {
            case Face::A: return {40, 67, 99};
            case Face::B: return {99, 51, 65};
            case Face::X: return {78, 56, 96};
            case Face::Y: return {45, 87, 64};
            }
            return {40, 50, 64};
        }

        inline void FacePixel(uint8_t* output, FaceCell cell, int x, int y)
        {
            const float px = float(x - cell.artwork.x) + 0.5f, py = float(y - cell.artwork.y) + 0.5f;
            const float distance = std::hypot(px - 16.0f, py - 17.0f);
            // Draw inside the source artwork's existing alpha silhouette; the
            // dark bezel and gently lit face replace the Xbox button colors.
            const float light = std::clamp((17.0f - distance) / 15.0f, 0.0f, 1.0f);
            const auto tint = Button(cell.face);
            const auto body = distance > 14.7f ? std::array<uint8_t, 3>{8, 11, 16}
                : distance > 13.2f ? std::array<uint8_t, 3>{20, 29, 42}
                : std::array<uint8_t, 3>{uint8_t(tint[0] + 14 * light), uint8_t(tint[1] + 18 * light), uint8_t(tint[2] + 19 * light)};
            std::copy(body.begin(), body.end(), output);
            float coverage = 0.0f;
            DrawFace(cell.face, cell.artwork.x + 4, cell.artwork.y + 5, 24, [&](int ax, int ay, int bx, int by) {
                coverage = std::max(coverage, Coverage(x + 0.5f, y + 0.5f,
                    {float(ax), float(ay), float(bx), float(by)}, 1.45f));
            });
            Mix(output, Ink(cell.face), coverage);
        }

        // Two-character shoulder legends, drawn from line segments instead of
        // font code points. At an 18px display these retain roughly 1px strokes.
        template <typename Line>
        void Letter(char glyph, float x, float y, float scale, Line line)
        {
            const auto stroke = [&](float ax, float ay, float bx, float by) {
                line(Segment{x + ax * scale, y + ay * scale, x + bx * scale, y + by * scale});
            };
            switch (glyph)
            {
            case 'L': stroke(0, 0, 0, 8); stroke(0, 8, 5, 8); break;
            case 'R': stroke(0, 8, 0, 0); stroke(0, 0, 4, 0); stroke(4, 0, 5, 1);
                      stroke(5, 1, 5, 3); stroke(5, 3, 4, 4); stroke(4, 4, 0, 4);
                      stroke(2, 4, 5, 8); break;
            case '1': stroke(1, 2, 3, 0); stroke(3, 0, 3, 8); stroke(0, 8, 5, 8); break;
            case '2': stroke(0, 1, 2, 0); stroke(2, 0, 4, 0); stroke(4, 0, 5, 1);
                      stroke(5, 1, 5, 3); stroke(5, 3, 0, 8); stroke(0, 8, 5, 8); break;
            }
        }

        inline void ShoulderPixel(uint8_t* output, ShoulderCell cell, int x, int y)
        {
            const auto& rect = cell.artwork;
            const float px = float(x - rect.x) + 0.5f, py = float(y - rect.y) + 0.5f;
            const float rim = std::min({px, py, rect.width - px, rect.height - py});
            // Rounded boundary comes from the source alpha. Repaint only its
            // RGB so the old LB/RB/LT/RT letterforms cannot show through.
            const float topLight = 1.0f - py / rect.height;
            const auto body = rim < 2.2f ? std::array<uint8_t, 3>{33, 38, 46}
                : std::array<uint8_t, 3>{uint8_t(155 + 42 * topLight), uint8_t(161 + 42 * topLight), uint8_t(172 + 42 * topLight)};
            std::copy(body.begin(), body.end(), output);
            const float scale = rect.width < 30 ? 1.35f : 1.65f;
            const float first = float(rect.x) + (rect.width - 13.0f * scale) * 0.5f;
            const float top = float(rect.y) + (rect.height - 8.0f * scale) * 0.5f;
            float coverage = 0.0f;
            const auto stroke = [&](Segment s) {
                coverage = std::max(coverage, Coverage(x + 0.5f, y + 0.5f, s, 1.1f));
            };
            Letter(cell.side, first, top, scale, stroke);
            Letter(cell.number, first + 8.0f * scale, top, scale, stroke);
            Mix(output, {22, 29, 39}, coverage);
        }

        inline void SystemPixel(uint8_t* output, SystemCell cell, int x, int y)
        {
            const float px = float(x - cell.rect.x) + 0.5f;
            const float py = float(y - cell.rect.y) + 0.5f;
            const float distance = std::hypot(px - 18.0f, py - 18.0f);
            // Rebuild the target tile so the old BACK/START text (which had
            // its own alpha silhouette) cannot remain visible below the icon.
            // All other tiles, including their alpha, stay byte-for-byte intact.
            output[3] = uint8_t(std::lround(255.0f * std::clamp(16.3f - distance, 0.0f, 1.0f)));
            if (!output[3]) { output[0] = output[1] = output[2] = 0; return; }
            const float light = std::clamp((16.0f - distance) / 15.0f, 0.0f, 1.0f);
            const auto body = distance > 14.5f ? std::array<uint8_t, 3>{12, 18, 27}
                : std::array<uint8_t, 3>{uint8_t(44 + 15 * light), uint8_t(58 + 19 * light), uint8_t(76 + 20 * light)};
            std::copy(body.begin(), body.end(), output);

            float coverage = 0.0f;
            if (cell.button == SystemButton::Back)
            {
                // Three connected nodes form a recognizable Share/Create
                // symbol across PS4 and PS5; no tiny console-specific text.
                coverage = std::max(Coverage(px, py, {14.0f, 16.4f, 21.0f, 11.2f}, 0.8f),
                                    Coverage(px, py, {14.0f, 19.6f, 21.0f, 24.8f}, 0.8f));
                for (auto node : std::array<std::array<float, 2>, 3>{{{11.5f, 18.0f}, {24.0f, 9.5f}, {24.0f, 26.5f}}})
                    coverage = std::max(coverage,
                        std::clamp(2.85f + 0.5f - std::hypot(px - node[0], py - node[1]), 0.0f, 1.0f));
            }
            else
            {
                // A three-line Options/Menu mark remains legible at 18px.
                for (float row : {12.0f, 18.0f, 24.0f})
                    coverage = std::max(coverage, Coverage(px, py, {11.0f, row, 25.0f, row}, 1.5f));
            }
            Mix(output, {219, 231, 243}, coverage);
        }
    } // namespace detail

    // Input and output are full, linear, unpremultiplied RGBA8 256x128 atlases;
    // stride is bytes per row, at least 256*4. Copies exactly 1024 bytes/row,
    // leaves output row padding alone, and changes the four face and four
    // shoulder artworks plus the two BACK/START tiles only. Alpha is unchanged
    // everywhere except BACK/START: their source text needs to be removed and
    // replaced with the new icon silhouettes. Input and output may be the same
    // pointer with equal strides; otherwise they must not overlap. Returns
    // false without writing for invalid pointers/strides.
    // BC3 encoding, mip regeneration and guest texture lifecycle are caller work.
    inline bool PatchPlayStationAtlasRgba(const uint8_t* source, size_t sourceStride,
                                          uint8_t* destination, size_t destinationStride,
                                          int width, int height)
    {
        if (!source || !destination || width != Width || height != Height ||
            sourceStride < size_t(Width) * 4 || destinationStride < size_t(Width) * 4 ||
            sourceStride > std::numeric_limits<size_t>::max() / size_t(Height) ||
            destinationStride > std::numeric_limits<size_t>::max() / size_t(Height) ||
            (source == destination && sourceStride != destinationStride)) return false;

        for (int y = 0; y < Height; ++y)
            if (source != destination)
                std::memcpy(destination + size_t(y) * destinationStride,
                            source + size_t(y) * sourceStride, size_t(Width) * 4);

        const auto paint = [&](auto cell, auto draw) {
            const Rect rect = cell.artwork;
            for (int y = rect.y; y < rect.y + rect.height; ++y)
                for (int x = rect.x; x < rect.x + rect.width; ++x)
                {
                    uint8_t* pixel = destination + size_t(y) * destinationStride + size_t(x) * 4;
                    if (pixel[3] != 0) draw(pixel, cell, x, y);
                }
        };
        for (auto cell : FaceCells) paint(cell, detail::FacePixel);
        for (auto cell : ShoulderCells) paint(cell, detail::ShoulderPixel);
        for (auto cell : SystemCells)
            for (int y = cell.rect.y; y < cell.rect.y + cell.rect.height; ++y)
                for (int x = cell.rect.x; x < cell.rect.x + cell.rect.width; ++x)
                    detail::SystemPixel(destination + size_t(y) * destinationStride + size_t(x) * 4, cell, x, y);
        return true;
    }
} // namespace hid::prompts::atlas
