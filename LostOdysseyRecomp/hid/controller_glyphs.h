#pragma once

namespace hid::prompts
{
// Physical SDL/XInput face positions. Confirmation and cancellation are game
// settings and do not change when a glyph is selected.
enum class Face { A, B, X, Y };
inline constexpr wchar_t Symbol(Face button)
{
    switch (button)
    {
    case Face::A: return L'\u00d7';
    case Face::B: return L'\u25cb';
    case Face::X: return L'\u25a1';
    case Face::Y: return L'\u25b3';
    }
    return L'?';
}

// Line art avoids missing characters in game/host bitmap fonts.
template <typename Line>
void DrawFace(Face face, int x, int y, int size, Line line)
{
    const auto p = [&](int v) { return v * size / 20; };
    const auto segment = [&](int ax, int ay, int bx, int by) {
        line(x + p(ax), y + p(ay), x + p(bx), y + p(by));
    };
    switch (face)
    {
    case Face::A:
        segment(3, 3, 17, 17); segment(17, 3, 3, 17); break;
    case Face::B:
        segment(7, 2, 13, 2); segment(13, 2, 18, 7);
        segment(18, 7, 18, 13); segment(18, 13, 13, 18);
        segment(13, 18, 7, 18); segment(7, 18, 2, 13);
        segment(2, 13, 2, 7); segment(2, 7, 7, 2); break;
    case Face::X:
        segment(3, 3, 17, 3); segment(17, 3, 17, 17);
        segment(17, 17, 3, 17); segment(3, 17, 3, 3); break;
    case Face::Y:
        segment(10, 2, 19, 18); segment(19, 18, 1, 18);
        segment(1, 18, 10, 2); break;
    }
}
}
