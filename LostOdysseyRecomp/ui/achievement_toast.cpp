#include "achievement_toast.h"

#include <algorithm>
#include <cmath>
#include <cstring>

// Layout (720p pixels, multiplied by the scale):
//   pill box:   height 60 (the outline is part of it), width = textLeft + widest text + right padding
//   ring disk:  diameter 52, concentric with the pill's left end
//   shading:    capsule distance field -> shadow / dark outline / light rim / gradient body
//
// Per-frame cost is kept low by baking everything that does not depend on the time once per
// (scale, text widths, icon): the pill (left cap, right cap, one middle column that is
// stretched), the ring disk, the lit ring + glow, the orb and the icon variants. A frame is a
// few memcpy/row fills, one blit of the circle sprite, the (alpha-blended) text and a
// premultiplied -> straight conversion of the touched columns.
namespace ui::achievement_toast {
namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kDuration = 5.2;

// 720p metrics.
constexpr float kPillH = 60.f;
constexpr float kMargin = 18.f;     // shadow / glow room around the resting box
constexpr float kDiskR = 25.f;      // black disk radius
constexpr float kHalo = 14.f;       // circle sprite room beyond the pill radius
constexpr float kTextLeft = 66.f;   // from the pill's left edge
constexpr float kPadRight = 30.f;
constexpr float kTitleDy = -10.f;   // text line centres relative to the pill centre
constexpr float kDetailDy = 11.f;
constexpr float kOvershoot = 1.025f;

constexpr double kPopEnd = 0.12;
constexpr double kGrowEnd = 0.46;
constexpr double kSettleEnd = 0.60;
constexpr double kPulseStart = 4.60, kPulseEnd = 4.80;
constexpr double kCollapseStart = 4.90;
// One flip from the emblem to the achievement's icon, which then stays.
constexpr double kFlipCentres[] = {1.0};
constexpr double kFlipHalf = 0.15;

// ---- small helpers -----------------------------------------------------------------------------
inline float Clamp01(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
inline double Clamp01(double v) { return v < 0. ? 0. : (v > 1. ? 1. : v); }
inline float Cov(float d) { return Clamp01(0.5f - d); } // coverage from a signed distance in pixels
inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
inline double Smooth(double u) { u = Clamp01(u); return u * u * (3. - 2. * u); }
inline double EaseOutQuad(double u) { u = Clamp01(u); return 1. - (1. - u) * (1. - u); }
inline double EaseOutCubic(double u) { u = Clamp01(u); double v = 1. - u; return 1. - v * v * v; }

// Straight colour 0..1 + coverage -> premultiplied RGBA8 (colour never exceeds alpha).
inline uint32_t PackPM(float r, float g, float b, float a)
{
    a = Clamp01(a);
    uint32_t A = (uint32_t)(a * 255.f + 0.5f);
    uint32_t R = std::min(A, (uint32_t)(Clamp01(r) * a * 255.f + 0.5f));
    uint32_t G = std::min(A, (uint32_t)(Clamp01(g) * a * 255.f + 0.5f));
    uint32_t B = std::min(A, (uint32_t)(Clamp01(b) * a * 255.f + 0.5f));
    return R | (G << 8) | (B << 16) | (A << 24);
}

struct PM { float r, g, b, a; }; // premultiplied float
inline PM Over(PM s, PM d) { float k = 1.f - s.a; return {s.r + d.r * k, s.g + d.g * k, s.b + d.b * k, s.a + d.a * k}; }
inline uint32_t Pack(PM p) { return PackPM(p.a > 0.f ? p.r / p.a : 0.f, p.a > 0.f ? p.g / p.a : 0.f, p.a > 0.f ? p.b / p.a : 0.f, p.a); }
inline PM FromStraight(float r, float g, float b, float a) { return {r * a, g * a, b * a, a}; }

// Premultiplied src-over, two channels per 32-bit lane; truncating so a channel never exceeds 255.
inline uint32_t OverPM(uint32_t s, uint32_t d)
{
    uint32_t ia = 255u - (s >> 24);
    uint32_t rb = (d & 0x00FF00FFu) * ia;
    rb = ((rb + ((rb >> 8) & 0x00FF00FFu)) >> 8) & 0x00FF00FFu;
    uint32_t ag = ((d >> 8) & 0x00FF00FFu) * ia;
    ag = (ag + ((ag >> 8) & 0x00FF00FFu)) & 0xFF00FF00u;
    return s + (rb | ag);
}
// f = 0..256
inline uint32_t MulPM(uint32_t p, uint32_t f)
{
    uint32_t rb = (((p & 0x00FF00FFu) * f) >> 8) & 0x00FF00FFu;
    uint32_t ag = (((p >> 8) & 0x00FF00FFu) * f) & 0xFF00FF00u;
    return rb | ag;
}
inline uint32_t TintPM(uint32_t p, uint32_t f) { return (MulPM(p, f) & 0x00FFFFFFu) | (p & 0xFF000000u); }
inline uint32_t Lerp8(uint32_t a, uint32_t b, uint32_t f) // f = 0..256, lanes
{
    uint32_t rb = ((((a & 0x00FF00FFu) * (256u - f)) + ((b & 0x00FF00FFu) * f)) >> 8) & 0x00FF00FFu;
    uint32_t ag = ((((a >> 8) & 0x00FF00FFu) * (256u - f)) + (((b >> 8) & 0x00FF00FFu) * f)) & 0xFF00FF00u;
    return rb | ag;
}

struct View { uint32_t* p; int w, h; };

// Source-over blit of a premultiplied sprite at integer position (clipped).
void BlitOver(View dst, const uint32_t* src, int sw, int sh, int dx, int dy, uint32_t amul = 256)
{
    int x0 = std::max(0, dx), x1 = std::min(dst.w, dx + sw);
    int y0 = std::max(0, dy), y1 = std::min(dst.h, dy + sh);
    for (int y = y0; y < y1; y++) {
        const uint32_t* s = src + (size_t)(y - dy) * sw + (x0 - dx);
        uint32_t* d = dst.p + (size_t)y * dst.w + x0;
        for (int x = x0; x < x1; x++, s++, d++) {
            uint32_t p = *s;
            if (!(p >> 24)) continue;
            if (amul != 256) p = MulPM(p, amul);
            *d = OverPM(p, *d);
        }
    }
}

// Bilinear blit with scale about the sprite point (scx, scy) placed at (dcx, dcy).
// tint scales rgb only (0..256), amul scales everything (0..256).
void BlitXform(View dst, const uint32_t* src, int sw, int sh, float scx, float scy, float dcx, float dcy, float sx, float sy,
               uint32_t tint, uint32_t amul)
{
    int x0 = std::max(0, (int)std::floor(dcx - scx * sx) - 1), x1 = std::min(dst.w, (int)std::ceil(dcx + (sw - scx) * sx) + 1);
    int y0 = std::max(0, (int)std::floor(dcy - scy * sy) - 1), y1 = std::min(dst.h, (int)std::ceil(dcy + (sh - scy) * sy) + 1);
    float isx = 1.f / sx, isy = 1.f / sy;
    auto fetch = [&](int x, int y) -> uint32_t {
        return ((unsigned)x < (unsigned)sw && (unsigned)y < (unsigned)sh) ? src[(size_t)y * sw + x] : 0u;
    };
    for (int y = y0; y < y1; y++) {
        float v = ((float)y + 0.5f - dcy) * isy + scy - 0.5f;
        int iy = (int)std::floor(v);
        uint32_t fy = (uint32_t)((v - (float)iy) * 256.f);
        uint32_t* d = dst.p + (size_t)y * dst.w;
        for (int x = x0; x < x1; x++) {
            float u = ((float)x + 0.5f - dcx) * isx + scx - 0.5f;
            int ix = (int)std::floor(u);
            uint32_t fx = (uint32_t)((u - (float)ix) * 256.f);
            uint32_t a = Lerp8(fetch(ix, iy), fetch(ix + 1, iy), fx);
            uint32_t b = Lerp8(fetch(ix, iy + 1), fetch(ix + 1, iy + 1), fx);
            uint32_t p = Lerp8(a, b, fy);
            if (!(p >> 24)) continue;
            if (tint != 256) p = TintPM(p, tint);
            if (amul != 256) p = MulPM(p, amul);
            d[x] = OverPM(p, d[x]);
        }
    }
}

// ---- baked layers ------------------------------------------------------------------------------
struct Layer {
    int w = 0, h = 0;
    std::vector<uint32_t> px;
    void Init(int ww, int hh) { w = ww; h = hh; px.assign((size_t)ww * hh, 0u); }
};

struct Cache {
    bool valid = false;
    float scale = 0.f;
    uint32_t titleW = 0, detailW = 0, iconW = 0, iconH = 0;
    uint64_t iconHash = 0; // = Content::revision
    bool hasIcon = false;

    int D = 0, R = 0, m = 0, Wp = 0, Wfr = 0, Hfr = 0, textLeft = 0, CW = 0;
    Layer pill;                                  // resting width, with shadow
    Layer base, lit, orb, icon, restOrb, restIcon; // CW x CW, centred
};

thread_local Cache gCache;
thread_local std::vector<uint32_t> gScratch, gScene, gCircle;

void BakePill(Cache& c, float s)
{
    const int W = c.Wp + 2 * c.m, H = c.D + 2 * c.m;
    c.pill.Init(W, H);
    const float R = (float)c.R, cy = (float)c.m + R;
    const float x0 = (float)c.m + R, x1 = (float)(c.m + c.Wp) - R;
    const float sigma = 3.6f * s, shOff = 1.f * s;
    for (int y = 0; y < H; y++) {
        float py = (float)y + 0.5f;
        float vt = Clamp01((py - (float)c.m) / (float)c.D);
        float body = Lerp(0.285f, 0.205f, vt);
        float rim = Lerp(0.50f, 0.40f, vt);
        const float outline = 0.03f;
        for (int x = 0; x < W; x++) {
            float px = (float)x + 0.5f;
            float cx = std::min(std::max(px, x0), x1);
            float d = std::hypot(px - cx, py - cy) - R;
            float ds = std::hypot(px - cx, py - shOff - cy) - R;
            float sa = 0.85f * 0.5f * std::erfc(ds / (sigma * 1.41421356f));
            float col = Lerp(outline, rim, Cov(d + 3.5f * s));
            col = Lerp(col, body, Cov(d + 5.f * s));
            float a = Cov(d);
            PM fill = FromStraight(col, col, col, a);
            PM sh = {0.f, 0.f, 0.f, sa};
            c.pill.px[(size_t)y * W + x] = Pack(Over(fill, sh));
        }
    }
}

PM IconPixel(const Cache&, const uint32_t* icon, uint32_t iw, uint32_t ih, float rx, float ry, float r, float ri)
{
    float cov = Cov(r - ri);
    if (cov <= 0.f) return {0, 0, 0, 0};
    float f = (float)iw / (2.f * ri); // texels per output pixel
    float u = (rx + ri) / (2.f * ri) * (float)iw;
    float v = (ry + ri) / (2.f * ri) * (float)ih;
    auto tex = [&](int x, int y) -> PM {
        x = std::min(std::max(x, 0), (int)iw - 1); y = std::min(std::max(y, 0), (int)ih - 1);
        uint32_t p = icon[(size_t)y * iw + x];
        float a = (float)(p >> 24) / 255.f;
        return {(float)(p & 255) / 255.f * a, (float)((p >> 8) & 255) / 255.f * a, (float)((p >> 16) & 255) / 255.f * a, a};
    };
    PM acc = {0, 0, 0, 0};
    if (f > 1.f) { // box filter over the footprint
        float hu0 = u - f * 0.5f, hu1 = u + f * 0.5f, hv0 = v - f * 0.5f, hv1 = v + f * 0.5f;
        float wsum = 0.f;
        for (int y = (int)std::floor(hv0); y < (int)std::ceil(hv1); y++)
            for (int x = (int)std::floor(hu0); x < (int)std::ceil(hu1); x++) {
                float wx = std::min((float)x + 1.f, hu1) - std::max((float)x, hu0);
                float wy = std::min((float)y + 1.f, hv1) - std::max((float)y, hv0);
                float w = wx * wy;
                if (w <= 0.f) continue;
                PM t = tex(x, y);
                acc.r += t.r * w; acc.g += t.g * w; acc.b += t.b * w; acc.a += t.a * w;
                wsum += w;
            }
        if (wsum > 0.f) { acc.r /= wsum; acc.g /= wsum; acc.b /= wsum; acc.a /= wsum; }
    } else { // bilinear
        float uu = u - 0.5f, vv = v - 0.5f;
        int ix = (int)std::floor(uu), iy = (int)std::floor(vv);
        float fx = uu - (float)ix, fy = vv - (float)iy;
        PM t00 = tex(ix, iy), t10 = tex(ix + 1, iy), t01 = tex(ix, iy + 1), t11 = tex(ix + 1, iy + 1);
        auto mix = [&](float a, float b, float cc, float dd) { return Lerp(Lerp(a, b, fx), Lerp(cc, dd, fx), fy); };
        acc = {mix(t00.r, t10.r, t01.r, t11.r), mix(t00.g, t10.g, t01.g, t11.g), mix(t00.b, t10.b, t01.b, t11.b), mix(t00.a, t10.a, t01.a, t11.a)};
    }
    return {acc.r * cov, acc.g * cov, acc.b * cov, acc.a * cov};
}

// ---- the app icon's medallion, drawn procedurally ----------------------------------------------
// Geometry in the icon SVG's own units (centre 512,512; frame outer radius 374, enamel radius 282); the
// gradients follow assets/icon/icon_svgs.py (objectBoundingBox gradients of each stacked disc).
struct Stop { float t, r, g, b; };
constexpr float H(int v) { return (float)v / 255.f; }
constexpr Stop kRimHi[] = {{0.f, H(0xff), H(0xf0), H(0xc2)}, {.45f, H(0xc8), H(0x96), H(0x3f)}, {1.f, H(0x4a), H(0x30), H(0x10)}};
constexpr Stop kRimLo[] = {{0.f, H(0x5a), H(0x3c), H(0x14)}, {.55f, H(0xa7), H(0x78), H(0x2f)}, {1.f, H(0xf2), H(0xd3), H(0x8c)}};
constexpr Stop kBand[] = {{0.f, H(0xf7), H(0xdc), H(0x98)}, {.22f, H(0xc9), H(0x98), H(0x43)}, {.45f, H(0x8a), H(0x5f), H(0x22)},
                          {.62f, H(0xd0), H(0xa2), H(0x4e)}, {.82f, H(0x9a), H(0x6b), H(0x28)}, {1.f, H(0xe3), H(0xbf), H(0x72)}};
constexpr Stop kEnamel[] = {{0.f, H(0x3b), H(0x78), H(0xc2)}, {.55f, H(0x1b), H(0x42), H(0x77)}, {1.f, H(0x0a), H(0x1a), H(0x33)}};
constexpr Stop kSteel[] = {{0.f, 1.f, 1.f, 1.f}, {.42f, H(0xe2), H(0xe6), H(0xee)}, {.52f, H(0xb8), H(0xbf), H(0xcc)}, {1.f, H(0x6c), H(0x73), H(0x80)}};
constexpr Stop kGoldH[] = {{0.f, H(0xfb), H(0xe6), H(0xad)}, {.45f, H(0xd4), H(0xa5), H(0x4c)}, {1.f, H(0x7a), H(0x52), H(0x1c)}};
constexpr Stop kGoldBall[] = {{0.f, H(0xff), H(0xf3), H(0xc8)}, {.45f, H(0xd9), H(0xaa), H(0x4f)}, {1.f, H(0x6e), H(0x48), H(0x16)}};
constexpr Stop kGemBlue[] = {{0.f, H(0xc9), H(0xec), H(0xff)}, {.35f, H(0x3f), H(0x9b), H(0xe6)}, {1.f, H(0x0a), H(0x2f), H(0x63)}};
constexpr Stop kGemRed[] = {{0.f, H(0xff), H(0xd0), H(0xcc)}, {.4f, H(0xd2), H(0x3a), H(0x3a)}, {1.f, H(0x5a), H(0x0d), H(0x12)}};
constexpr Stop kLeather[] = {{0.f, H(0x2a), H(0x17), H(0x0a)}, {.45f, H(0x5c), H(0x38), H(0x18)}, {1.f, H(0x1e), H(0x10), H(0x07)}};

template <size_t N>
inline void Grad(const Stop (&st)[N], float t, float& r, float& g, float& b)
{
    t = Clamp01(t);
    size_t i = 1;
    while (i < N - 1 && t > st[i].t) i++;
    const Stop &a = st[i - 1], &c = st[i];
    float k = c.t > a.t ? Clamp01((t - a.t) / (c.t - a.t)) : 0.f;
    r = Lerp(a.r, c.r, k); g = Lerp(a.g, c.g, k); b = Lerp(a.b, c.b, k);
}
template <size_t N>
inline PM GradPM(const Stop (&st)[N], float t, float a = 1.f)
{
    float r, g, b; Grad(st, t, r, g, b);
    return FromStraight(r, g, b, a);
}
// diagonal objectBoundingBox gradient of a disc of radius R centred on (dx,dy)=0
inline float DiagT(float dx, float dy, float R) { return 0.5f + (dx + dy) / (4.f * R); }

inline float HashNoise(int x, int y)
{
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((h ^ (h >> 16)) & 1023u) / 1023.f;
}

// signed distances (negative inside), SVG units
inline float SdCircle(float x, float y, float cx, float cy, float r) { return std::hypot(x - cx, y - cy) - r; }
inline float SdRoundRect(float x, float y, float x0, float y0, float x1, float y1, float rad)
{
    float hx = (x1 - x0) * 0.5f, hy = (y1 - y0) * 0.5f;
    float qx = std::fabs(x - (x0 + x1) * 0.5f) - (hx - rad), qy = std::fabs(y - (y0 + y1) * 0.5f) - (hy - rad);
    return std::hypot(std::max(qx, 0.f), std::max(qy, 0.f)) + std::min(std::max(qx, qy), 0.f) - rad;
}
inline float SdBlade(float x, float y)
{
    static const float P[5][2] = {{512, 110}, {548, 198}, {544, 632}, {480, 632}, {476, 198}};
    float d = -1e9f;
    for (int i = 0; i < 5; i++) {
        const float *a = P[i], *b = P[(i + 1) % 5];
        float ex = b[0] - a[0], ey = b[1] - a[1], l = std::hypot(ex, ey);
        float nx = ey / l, ny = -ex / l;
        if (nx * (512.f - a[0]) + ny * (400.f - a[1]) > 0.f) { nx = -nx; ny = -ny; } // outward
        d = std::max(d, nx * (x - a[0]) + ny * (y - a[1]));
    }
    return d;
}
inline float SdGuard(float x, float y)
{
    float u = std::min(1.f, std::fabs(x - 512.f) / 136.f);
    float yt = 630.f + 12.f * u * u, yb = 664.f + 4.f * u * u;
    return std::max({yt - y, y - yb, std::fabs(x - 512.f) - 136.f});
}
inline float SdSegment(float x, float y, float ax, float ay, float bx, float by)
{
    float px = x - ax, py = y - ay, ex = bx - ax, ey = by - ay;
    float t = Clamp01((px * ex + py * ey) / (ex * ex + ey * ey));
    return std::hypot(px - ex * t, py - ey * t);
}

// painter helpers: draw a shape with fill + 4.5-unit dark stroke
struct Painter {
    float ppl; // pixels per local unit
    PM acc{0, 0, 0, 0};
    void Fill(float sd, PM colour) // colour is premultiplied at full coverage
    {
        float c = Cov(sd * ppl);
        if (c <= 0.f) return;
        acc = Over({colour.r * c, colour.g * c, colour.b * c, colour.a * c}, acc);
    }
    void Stroke(float sd, float halfWidth, PM colour)
    {
        float hw = halfWidth * ppl;
        float c = Cov(std::fabs(sd) * ppl - hw) * std::min(1.f, 2.f * hw);
        if (c <= 0.f) return;
        acc = Over({colour.r * c, colour.g * c, colour.b * c, colour.a * c}, acc);
    }
    void Shape(float sd, PM fill, bool stroke = true)
    {
        Fill(sd, fill);
        if (stroke) Stroke(sd, 2.25f, FromStraight(H(0x24), H(0x17), H(0x0a), 1.f));
    }
};
// The sword in its own local frame (tip up). ppl = pixels per local unit.
PM SwordSample(float x, float y, float ppl)
{
    Painter p{ppl};
    auto vgrad = [&](float y0, float y1) { return (y - y0) / (y1 - y0); };
    auto radial = [&](const auto& st, float x0, float y0, float w, float hh, float cxp, float cyp, float rp) {
        float cx = x0 + cxp * w, cy = y0 + cyp * hh;
        return GradPM(st, std::hypot(x - cx, y - cy) / (rp * w));
    };
    // blade
    float sb = SdBlade(x, y);
    p.Shape(sb, GradPM(kSteel, (x - 476.f) / 72.f));
    if (sb < -2.f) {
        p.Fill(std::max({236.f - y, y - 574.f, std::fabs(x - 512.f) - 6.f}), FromStraight(H(0x5f), H(0x66), H(0x72), 1.f));
        float dl = std::min(SdSegment(x, y, 512, 120, 482, 200), SdSegment(x, y, 482, 200, 486, 626));
        float dr = std::min(SdSegment(x, y, 512, 120, 542, 200), SdSegment(x, y, 542, 200, 538, 626));
        float hl = Cov((dl - 1.75f) * ppl) * std::min(1.f, 3.5f * ppl) * 0.9f;
        float sh = Cov((dr - 1.5f) * ppl) * std::min(1.f, 3.f * ppl) * 0.7f;
        if (hl > 0.f) p.acc = Over(FromStraight(1, 1, 1, hl), p.acc);
        if (sh > 0.f) p.acc = Over(FromStraight(H(0x3c), H(0x41), H(0x4b), sh), p.acc);
    }
    // guard, balls, grip, cuff, bands, pommel, gem
    p.Shape(SdGuard(x, y), GradPM(kGoldH, vgrad(628.f, 668.f)));
    p.Shape(SdCircle(x, y, 372, 655, 19), radial(kGoldBall, 353, 636, 38, 38, .38f, .32f, .70f));
    p.Shape(SdCircle(x, y, 652, 655, 19), radial(kGoldBall, 633, 636, 38, 38, .38f, .32f, .70f));
    float sg = SdRoundRect(x, y, 490, 672, 534, 794, 10);
    p.Shape(sg, GradPM(kLeather, (x - 490.f) / 44.f));
    if (sg < -2.f && y > 676.f && y < 786.f && x > 492.f && x < 532.f) {
        float v1 = std::fmod(y - 0.5f * (x - 486.f) - 672.f + 2000.f, 20.f), v2 = std::fmod(y + 0.5f * (x - 486.f) - 672.f + 2000.f, 20.f);
        float d1 = std::min(v1, 20.f - v1) * 0.894f, d2 = std::min(v2, 20.f - v2) * 0.894f;
        float a = Cov((std::min(d1, d2) - 2.5f) * ppl) * std::min(1.f, 5.f * ppl) * 0.85f;
        if (a > 0.f) p.acc = Over(FromStraight(H(0x8a), H(0x5a), H(0x2b), a), p.acc);
    }
    p.Shape(SdRoundRect(x, y, 482, 620, 542, 678, 16), GradPM(kGoldH, vgrad(620.f, 678.f)));
    p.Shape(SdRoundRect(x, y, 486, 670, 538, 682, 4), GradPM(kGoldH, vgrad(670.f, 682.f)));
    p.Shape(SdRoundRect(x, y, 486, 784, 538, 796, 4), GradPM(kGoldH, vgrad(784.f, 796.f)));
    p.Shape(SdCircle(x, y, 512, 826, 36), radial(kGoldBall, 476, 790, 72, 72, .38f, .32f, .70f));
    p.Shape(SdCircle(x, y, 512, 649, 18), radial(kGemBlue, 494, 631, 36, 36, .40f, .35f, .70f));
    p.Fill(std::hypot((x - 506.f) / 6.f, (y - 642.f) / 4.f) - 1.f, FromStraight(1, 1, 1, .9f)); // blue-gem glint
    float sr = SdCircle(x, y, 512, 826, 12);
    p.Fill(sr, radial(kGemRed, 500, 814, 24, 24, .40f, .35f, .70f));
    p.Stroke(sr, 1.5f, FromStraight(H(0x24), H(0x17), H(0x0a), 1.f));
    p.Fill(std::hypot((x - 509.f) / 4.f, (y - 822.f) / 3.f) - 1.f, FromStraight(1, 1, 1, .85f));
    return p.acc;
}
// Silhouette distance (local units) for the soft drop shadow.
float SwordSilhouette(float x, float y)
{
    float d = SdBlade(x, y);
    d = std::min(d, SdGuard(x, y));
    d = std::min(d, SdCircle(x, y, 372, 655, 19));
    d = std::min(d, SdCircle(x, y, 652, 655, 19));
    d = std::min(d, SdRoundRect(x, y, 482, 620, 542, 678, 16));
    d = std::min(d, SdRoundRect(x, y, 490, 672, 534, 794, 10));
    d = std::min(d, SdCircle(x, y, 512, 826, 36));
    return d;
}

constexpr float kTiltDeg = 20.f, kSwordScale = .74f;

// The enamel disc with its gloss, sword shadow and sword (coverage cut at the enamel radius).
PM EmblemPixel(float rx, float ry, float k /*px per SVG unit*/)
{
    const float X = rx / k, Y = ry / k; // relative to the centre, SVG units
    const float rEn = 282.f;
    float cov = Cov((std::hypot(X, Y) - rEn) * k);
    if (cov <= 0.f) return {0, 0, 0, 0};
    // enamel radial gradient: centre (46%, 40%) of the bbox, r 62%
    float ex = X - (0.46f - 0.5f) * 2.f * rEn, ey = Y - (0.40f - 0.5f) * 2.f * rEn;
    PM col = GradPM(kEnamel, std::hypot(ex, ey) / (0.62f * 2.f * rEn));
    { // thin inner gold ring (SVG r 216) with a dark inner edge
        float rr = std::hypot(X, Y);
        float a1 = Cov((std::fabs(rr - 210.f) - 1.5f) * k) * std::min(1.f, 3.f * k) * 0.5f;
        float a2 = Cov((std::fabs(rr - 216.f) - 2.5f) * k) * std::min(1.f, 5.f * k) * 0.9f;
        col = Over(FromStraight(H(0x06), H(0x12), H(0x23), a1), col);
        col = Over(FromStraight(H(0xe7), H(0xc7), H(0x7d), a2), col);
    }
    // gloss: ellipse (0,-120) 250x170, radial from the top-centre of its bbox, white .22 -> 0
    {
        float gx = X / 250.f, gy = (Y + 120.f) / 170.f; // inside the ellipse when |.|<1
        float e = std::hypot(gx, gy);
        if (e < 1.f + 1.f / (k * 170.f)) {
            float ec = Clamp01(0.5f - (e - 1.f) * 170.f * k);
            float tg = std::hypot(X / 500.f, (Y + 120.f + 170.f) / 340.f);
            float a = 0.22f * (1.f - Clamp01(tg)) * ec;
            col = Over(FromStraight(1, 1, 1, a), col);
        }
    }
    // sword frame: world -> local
    const float S = kSwordScale, ca = std::cos(kTiltDeg * (float)kPi / 180.f), sa = std::sin(kTiltDeg * (float)kPi / 180.f);
    auto toLocal = [&](float wx, float wy, float& qx, float& qy) { // inverse of rotate(+tilt) about the centre, then 1/S
        qx = 512.f + (wx * ca + wy * sa) / S;
        qy = 512.f + (-wx * sa + wy * ca) / S;
    };
    // soft drop shadow: sword silhouette offset by (18,22), blur sigma ~9 world units, opacity .6
    {
        float qx, qy;
        toLocal(X - 18.f, Y - 22.f, qx, qy);
        float d = SwordSilhouette(qx, qy) * S;
        if (d < 40.f) {
            float a = 0.6f * 0.5f * std::erfc(d / (9.f * 1.41421356f));
            col = Over(FromStraight(0, 0, 0, a), col);
        }
    }
    {
        float qx, qy;
        toLocal(X, Y, qx, qy);
        if (qy > 60.f && qy < 900.f && qx > 330.f && qx < 700.f) {
            PM sw = SwordSample(qx, qy, k * S);
            col = Over(sw, col);
        }
    }
    return {col.r * cov, col.g * cov, col.b * cov, col.a * cov};
}

void BakeCircle(Cache& c, const Content& content, float s)
{
    const int CW = c.CW;
    c.base.Init(CW, CW); c.lit.Init(CW, CW); c.orb.Init(CW, CW); c.icon.Init(CW, CW);
    const float cc = (float)CW * 0.5f;
    const float rd = kDiskR * s;            // frame outer radius (SVG 374)
    const float k = rd / 374.f;             // pixels per SVG unit
    const float rEn = 282.f * k;
    const bool fine = s >= 1.5f;
    for (int y = 0; y < CW; y++) {
        for (int x = 0; x < CW; x++) {
            float rx = (float)x + 0.5f - cc, ry = (float)y + 0.5f - cc;
            float r = std::hypot(rx, ry);
            float X = rx / k, Y = ry / k; // SVG units from the centre
            size_t i = (size_t)y * CW + x;

            // soft drop shadow of the frame (SVG: 18 down, blur 22, opacity .55)
            float dsh = (std::hypot(X, Y - 18.f) - 374.f) * k;
            PM base = {0, 0, 0, 0.55f * 0.5f * (float)std::erfc(dsh / (22.f * k * 1.41421356f))};
            // stacked discs, each with its own diagonal bounding-box gradient
            auto disc = [&](float R, PM colour) {
                float cv = Cov(r - R * k);
                if (cv > 0.f) base = Over({colour.r * cv, colour.g * cv, colour.b * cv, colour.a * cv}, base);
            };
            disc(374.f, GradPM(kRimHi, DiagT(X, Y, 374.f)));
            disc(364.f, GradPM(kRimLo, DiagT(X, Y, 364.f)));
            {
                PM band = GradPM(kBand, DiagT(X, Y, 356.f));
                float n = HashNoise(x, y);
                float dark = 1.f - 0.16f * n; // cheap grain
                band = {band.r * dark, band.g * dark, band.b * dark, 1.f};
                disc(356.f, band);
            }
            // gold diamonds on the band (rotated squares, half-size 13; tiny ones 6 at +-38 / +-142 deg)
            auto diamond = [&](float cx, float cy, float hsz) {
                float d = (std::fabs(X - cx) + std::fabs(Y - cy)) * 0.70710678f - hsz;
                float cvf = Cov(d * k);
                if (cvf <= 0.f) return;
                float bx = X - cx, by = Y - cy;
                PM g = GradPM(kGoldBall, std::hypot(bx / (2.f * hsz) + 0.12f, by / (2.f * hsz) + 0.18f) / 0.7f);
                float st = Cov((std::fabs(d) - 1.5f) * k) * std::min(1.f, 3.f * k);
                base = Over({g.r * cvf, g.g * cvf, g.b * cvf, cvf}, base);
                if (st > 0.f) base = Over(FromStraight(H(0x3a), H(0x24), H(0x0b), st), base);
            };
            if (r < 340.f * k && r > 306.f * k) {
                diamond(323.f, 0.f, 13.f); diamond(-323.f, 0.f, 13.f);
                if (fine) {
                    const float cs = 323.f * 0.78801075f, sn = 323.f * 0.61566148f; // cos/sin 38 deg
                    diamond(cs, sn, 6.f); diamond(-cs, sn, 6.f); diamond(cs, -sn, 6.f); diamond(-cs, -sn, 6.f);
                }
            }
            disc(294.f, GradPM(kRimLo, DiagT(X, Y, 294.f)));
            disc(288.f, GradPM(kRimHi, DiagT(X, Y, 288.f)));
            disc(282.f, FromStraight(0.02f, 0.03f, 0.05f, 1.f)); // dark behind the flipping centre
            c.base.px[i] = Pack(base);

            // pulse overlay: warm gold glow around the frame and a brightened band
            PM lit = {0, 0, 0, 0};
            {
                float bandCov = std::min(Cov(r - 374.f * k), Cov(282.f * k - r));
                lit = FromStraight(1.0f, 0.86f, 0.42f, 0.5f * bandCov);
                if (r > rd - 2.f * s) {
                    float gA = 0.8f * std::exp(-((r - rd) / (4.f * s)) * ((r - rd) / (4.f * s)));
                    gA *= Clamp01((r - (rd - 2.f * s)) / (2.f * s));
                    lit = Over(lit, FromStraight(1.0f, 0.72f, 0.22f, gA));
                }
            }
            c.lit.px[i] = Pack(lit);

            c.orb.px[i] = Pack(EmblemPixel(rx, ry, k));
        }
    }
    if (content.icon && content.iconWidth && content.iconHeight) {
        for (int y = 0; y < CW; y++)
            for (int x = 0; x < CW; x++) {
                float rx = (float)x + 0.5f - cc, ry = (float)y + 0.5f - cc;
                c.icon.px[(size_t)y * CW + x] = Pack(IconPixel(c, content.icon, content.iconWidth, content.iconHeight, rx, ry, std::hypot(rx, ry), rEn));
            }
    } else {
        c.icon.px = c.orb.px;
    }
    c.restOrb = c.base;
    c.restIcon = c.base;
    for (size_t i = 0; i < c.base.px.size(); i++) {
        c.restOrb.px[i] = OverPM(c.orb.px[i], c.base.px[i]);
        c.restIcon.px[i] = OverPM(c.icon.px[i], c.base.px[i]);
    }
}

void EnsureCache(const Content& content, float scale)
{
    Cache& c = gCache;
    bool hasIcon = content.icon && content.iconWidth && content.iconHeight;
    uint32_t tw = content.title ? content.titleWidth : 0, dw = content.detail ? content.detailWidth : 0;
    if (c.valid && c.scale == scale && c.iconHash == content.revision && c.titleW == tw && c.detailW == dw && c.hasIcon == hasIcon &&
        c.iconW == content.iconWidth && c.iconH == content.iconHeight)
        return;

    c.valid = true;
    c.scale = scale; c.titleW = tw; c.detailW = dw; c.hasIcon = hasIcon; c.iconHash = content.revision; // iconHash holds the revision
    c.iconW = content.iconWidth; c.iconH = content.iconHeight;

    c.R = std::max(2, (int)std::lround(kPillH * scale * 0.5f));
    c.D = 2 * c.R;
    c.m = (int)std::ceil(kMargin * scale);
    c.textLeft = (int)std::lround(kTextLeft * scale);
    c.Wp = std::max(2 * c.D, c.textLeft + (int)std::max(tw, dw) + (int)std::lround(kPadRight * scale));
    c.Wfr = (int)std::ceil((double)c.Wp * (kOvershoot + 0.005)) + 2 * c.m;
    c.Hfr = c.D + 2 * c.m;
    c.CW = 2 * (int)std::ceil((float)c.R + kHalo * scale);
    BakePill(c, scale);
    BakeCircle(c, content, scale);
}

// Text image blit (straight-alpha source) into the premultiplied scratch, clipped to x < clipX with a soft edge.
void BlitText(View dst, const uint32_t* img, int iw, int ih, int dx, int dy, int clipX, int soft)
{
    if (!img || iw <= 0 || ih <= 0) return;
    int xEnd = std::min({iw, dst.w - dx, clipX - dx});
    int xs = std::max(0, -dx);
    for (int y = std::max(0, -dy); y < ih && y + dy < dst.h; y++) {
        const uint32_t* s = img + (size_t)y * iw;
        uint32_t* d = dst.p + (size_t)(y + dy) * dst.w + dx;
        for (int x = xs; x < xEnd; x++) {
            uint32_t p = s[x];
            uint32_t a = p >> 24;
            if (!a) continue;
            int toClip = clipX - (dx + x); // pixels to the clip edge
            if (toClip < soft) a = a * (uint32_t)toClip / (uint32_t)soft;
            if (!a) continue;
            uint32_t r = ((p & 255) * a + 127) / 255, g = (((p >> 8) & 255) * a + 127) / 255, b = (((p >> 16) & 255) * a + 127) / 255;
            d[x] = OverPM(r | (g << 8) | (b << 16) | (a << 24), d[x]);
        }
    }
}

const uint32_t* RecipTable()
{
    static uint32_t tab[256];
    static bool init = [] {
        tab[0] = 0;
        for (uint32_t a = 1; a < 256; a++) tab[a] = (255u * 65536u + a / 2) / a;
        return true;
    }();
    (void)init;
    return tab;
}
} // namespace

double Duration() { return kDuration; }

Frame Render(const Content& content, double t, float scale)
{
    Frame f;
    if (!(scale > 0.f)) return f;
    EnsureCache(content, scale);
    Cache& c = gCache;
    f.width = (uint32_t)c.Wfr;
    f.height = (uint32_t)c.Hfr;
    f.offsetX = -(c.Wp / 2) - c.m;
    f.offsetY = -c.D - c.m;
    if (!(t >= 0.0) || t >= kDuration) return f;
    f.visible = true;

    const int W = c.Wfr, H = c.Hfr, m = c.m, R = c.R, D = c.D, L = m + R;

    // ---- timeline ----
    double w; // pill outer width (pixels), grows to the right from the circle
    double fade = 1.0;
    float popScale = 1.f;
    if (t < kPopEnd) {
        double p = t / kPopEnd;
        popScale = 1.15f - 0.15f * (float)EaseOutCubic(p);
        fade = EaseOutCubic(p);
        w = D;
    } else if (t < kGrowEnd) {
        w = D + ((double)c.Wp * kOvershoot - D) * EaseOutQuad((t - kPopEnd) / (kGrowEnd - kPopEnd));
    } else if (t < kSettleEnd) {
        w = (double)c.Wp * (kOvershoot - (kOvershoot - 1.0) * Smooth((t - kGrowEnd) / (kSettleEnd - kGrowEnd)));
    } else if (t < kCollapseStart) {
        w = c.Wp;
    } else {
        double u = Clamp01((t - kCollapseStart) / (kDuration - kCollapseStart));
        w = c.Wp + (double)(D - c.Wp) * Smooth(u);
        fade = 1.0 - Smooth(u);
    }
    int wi = std::max(D, (int)std::lround(w));

    // circle centre content state
    int state = 0;      // 0 orb, 1 icon
    float squash = 1.f; // horizontal scale of the centre
    float dark = 1.f;
    bool flipping = false;
    if (c.hasIcon) {
        for (double fc : kFlipCentres) {
            if (t >= fc + kFlipHalf) { state ^= 1; continue; }
            if (t >= fc - kFlipHalf) {
                double p = (t - (fc - kFlipHalf)) / (2 * kFlipHalf);
                float cv = (float)std::fabs(std::cos(kPi * p));
                if (p >= 0.5) state ^= 1;
                squash = std::max(cv, 0.03f);
                dark = 1.f - 0.75f * (1.f - cv);
                flipping = true;
            }
            break;
        }
    }
    float pulse = 0.f;
    if (t >= kPulseStart && t < kPulseEnd) pulse = (float)std::sin(kPi * (t - kPulseStart) / (kPulseEnd - kPulseStart));

    // ---- compose (premultiplied) into the scratch buffer ----
    gScratch.assign((size_t)W * H, 0u);
    View S{gScratch.data(), W, H};
    const int total = wi + 2 * m;
    {
        const uint32_t* sp = c.pill.px.data();
        const int sw = c.pill.w;
        for (int y = 0; y < H; y++) {
            const uint32_t* row = sp + (size_t)y * sw;
            uint32_t* d = S.p + (size_t)y * W;
            std::memcpy(d, row, (size_t)L * 4);
            std::fill(d + L, d + total - L, row[L]);
            std::memcpy(d + total - L, row + (sw - L), (size_t)L * 4);
        }
    }

    // text, revealed by the right edge
    {
        int clipX = m + wi - (int)std::lround(0.35 * R);
        int soft = std::max(1, (int)std::lround(4.f * scale));
        int tx = m + c.textLeft;
        int cy = m + R;
        if (content.title)
            BlitText(S, content.title, (int)content.titleWidth, (int)content.titleHeight, tx,
                     cy + (int)std::lround(kTitleDy * scale) - (int)content.titleHeight / 2, clipX, soft);
        if (content.detail)
            BlitText(S, content.detail, (int)content.detailWidth, (int)content.detailHeight, tx,
                     cy + (int)std::lround(kDetailDy * scale) - (int)content.detailHeight / 2, clipX, soft);
    }

    // circle
    {
        const int CW = c.CW;
        const int ox = L - CW / 2, oy = m + R - CW / 2;
        if (!flipping && pulse <= 0.f) {
            const Layer& rest = state ? c.restIcon : c.restOrb;
            BlitOver(S, rest.px.data(), CW, CW, ox, oy);
        } else {
            gCircle.assign((size_t)CW * CW, 0u);
            View C{gCircle.data(), CW, CW};
            BlitOver(C, c.base.px.data(), CW, CW, 0, 0);
            const Layer& centre = state ? c.icon : c.orb;
            if (flipping)
                BlitXform(C, centre.px.data(), CW, CW, CW * 0.5f, CW * 0.5f, CW * 0.5f, CW * 0.5f, squash, 1.f, (uint32_t)(dark * 256.f), 256);
            else
                BlitOver(C, centre.px.data(), CW, CW, 0, 0);
            if (pulse > 0.f) BlitOver(C, c.lit.px.data(), CW, CW, 0, 0, (uint32_t)(pulse * 256.f));
            float ps = 1.f + 0.08f * pulse;
            if (ps != 1.f)
                BlitXform(S, gCircle.data(), CW, CW, CW * 0.5f, CW * 0.5f, (float)L, (float)(m + R), ps, ps, 256, 256);
            else
                BlitOver(S, gCircle.data(), CW, CW, ox, oy);
        }
    }

    int cols = std::min(W, total + (int)(0.2 * L) + 1);
    if (popScale != 1.f) { // the whole (circle-sized) toast pops
        int pc = std::min(W, 2 * L + 2);
        gScene.assign((size_t)pc * H, 0u);
        for (int y = 0; y < H; y++) std::memcpy(&gScene[(size_t)y * pc], S.p + (size_t)y * W, (size_t)pc * 4);
        gScratch.assign((size_t)W * H, 0u);
        S.p = gScratch.data();
        BlitXform(S, gScene.data(), pc, H, (float)L, (float)(m + R), (float)L, (float)(m + R), popScale, popScale, 256, 256);
        cols = std::min(W, 2 * L + (int)(0.2 * L) + 2);
    }

    // ---- fade + convert to straight alpha ----
    uint32_t fm = (uint32_t)(Clamp01(fade) * 256.0);
    const uint32_t* recip = RecipTable();
    f.pixels.assign((size_t)W * H, 0u);
    for (int y = 0; y < H; y++) {
        const uint32_t* s = S.p + (size_t)y * W;
        uint32_t* o = f.pixels.data() + (size_t)y * W;
        for (int x = 0; x < cols; x++) {
            uint32_t p = s[x];
            if (fm < 256) p = MulPM(p, fm);
            uint32_t a = p >> 24;
            if (a == 0) continue;
            if (a == 255) { o[x] = p; continue; }
            uint32_t k = recip[a];
            uint32_t r = std::min(255u, ((p & 255) * k + 0x8000u) >> 16);
            uint32_t g = std::min(255u, (((p >> 8) & 255) * k + 0x8000u) >> 16);
            uint32_t b = std::min(255u, (((p >> 16) & 255) * k + 0x8000u) >> 16);
            o[x] = r | (g << 8) | (b << 16) | (a << 24);
        }
    }
    return f;
}

std::vector<float> SynthesizeChime(uint32_t sampleRate)
{
    const double total = 0.34;
    const size_t n = (size_t)(total * sampleRate);
    std::vector<float> out(n, 0.f);
    const double sr = (double)sampleRate;

    // part 1: soft downward blip at 0
    {
        double phase = 0.0;
        for (size_t i = 0; i < (size_t)(0.05 * sr) && i < n; i++) {
            double tt = (double)i / sr;
            double freq = 560.0 + 380.0 * std::exp(-tt / 0.012);
            phase += 2.0 * kPi * freq / sr;
            double env = (1.0 - std::exp(-tt / 0.0008)) * std::exp(-tt / 0.010);
            out[i] += (float)(0.30 * env * std::sin(phase));
        }
    }
    // part 2: bright bell 0.1 s later - three partials plus a soft body tone, ~0.2 s decay
    {
        struct Partial { double ratio, amp, tau; };
        const Partial parts[] = {{1.0, 0.50, 0.060}, {2.0, 0.30, 0.045}, {2.76, 0.14, 0.030}, {0.5, 0.12, 0.070}};
        const double f0 = 1568.0, start = 0.10;
        for (size_t i = (size_t)(start * sr); i < n; i++) {
            double tt = (double)i / sr - start;
            double att = 1.0 - std::exp(-tt / 0.0025);
            double v = 0.0;
            for (const Partial& p : parts)
                v += p.amp * std::exp(-tt / p.tau) * std::sin(2.0 * kPi * f0 * p.ratio * tt + (p.ratio == 2.76 ? 0.7 : 0.0));
            out[i] += (float)(att * v);
        }
    }
    // smooth end (last 15 ms), normalise to a 0.5 peak
    size_t fadeN = (size_t)(0.015 * sr);
    for (size_t i = 0; i < fadeN && i < n; i++) {
        double k = 0.5 * (1.0 + std::cos(kPi * (double)i / (double)fadeN)); // 1 -> 0
        out[n - 1 - i] *= (float)(1.0 - k);
    }
    float peak = 0.f;
    for (float v : out) peak = std::max(peak, std::fabs(v));
    if (peak > 0.f)
        for (float& v : out) v *= 0.5f / peak;
    return out;
}
}
